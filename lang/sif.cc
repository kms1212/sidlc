#include <sif.hh>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <ast.hh>
#include <uuid.h>

namespace {

constexpr uint8_t k_sif_magic[4] = {'S', 'I', 'F', 0};
constexpr uint32_t k_sif_format_version = 0;
constexpr size_t k_hash_size = 32;
constexpr size_t k_uuid_size = 16;

enum class ExprKind : uint8_t {
    String = 1,
    Number = 2,
    Identifier = 3,
};

enum class TypeKind : uint8_t {
    Named = 1,
    Ptr = 2,
    Array = 3,
};

class Sha256 {
  private:
    uint32_t state[8] = {
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U,
    };
    uint8_t buffer[64] = {};
    uint64_t bit_count = 0;
    size_t buffer_size = 0;

    static uint32_t rotr(uint32_t value, uint32_t shift)
    {
        return (value >> shift) | (value << (32 - shift));
    }

    void transform(const uint8_t block[64])
    {
        static constexpr uint32_t k[64] = {
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
            0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
            0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
            0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
            0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
            0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
            0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
            0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
            0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
            0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
            0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
            0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
            0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
        };

        uint32_t w[64];
        for (size_t i = 0; i < 16; i++) {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
                | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
                | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
                | static_cast<uint32_t>(block[i * 4 + 3]);
        }

        for (size_t i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];
        uint32_t f = state[5];
        uint32_t g = state[6];
        uint32_t h = state[7];

        for (size_t i = 0; i < 64; i++) {
            uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = h + s1 + ch + k[i] + w[i];
            uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

  public:
    void update(const uint8_t *data, size_t size)
    {
        bit_count += static_cast<uint64_t>(size) * 8;

        while (size > 0) {
            size_t copy_size = std::min(size, sizeof(buffer) - buffer_size);
            std::memcpy(buffer + buffer_size, data, copy_size);
            buffer_size += copy_size;
            data += copy_size;
            size -= copy_size;

            if (buffer_size == sizeof(buffer)) {
                transform(buffer);
                buffer_size = 0;
            }
        }
    }

    std::array<uint8_t, k_hash_size> finish()
    {
        buffer[buffer_size++] = 0x80;

        if (buffer_size > 56) {
            while (buffer_size < 64) {
                buffer[buffer_size++] = 0;
            }
            transform(buffer);
            buffer_size = 0;
        }

        while (buffer_size < 56) {
            buffer[buffer_size++] = 0;
        }

        for (int i = 7; i >= 0; i--) {
            buffer[buffer_size++] = static_cast<uint8_t>((bit_count >> (i * 8)) & 0xff);
        }

        transform(buffer);

        std::array<uint8_t, k_hash_size> digest = {};
        for (size_t i = 0; i < 8; i++) {
            digest[i * 4] = static_cast<uint8_t>((state[i] >> 24) & 0xff);
            digest[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 16) & 0xff);
            digest[i * 4 + 2] = static_cast<uint8_t>((state[i] >> 8) & 0xff);
            digest[i * 4 + 3] = static_cast<uint8_t>(state[i] & 0xff);
        }
        return digest;
    }
};

class BinaryWriter {
  private:
    std::vector<uint8_t> *buffer = nullptr;
    std::ostream *stream = nullptr;
    Sha256 *hasher = nullptr;

    void write_byte(uint8_t value)
    {
        if (buffer) {
            buffer->push_back(value);
        }
        if (stream) {
            char ch = static_cast<char>(value);
            stream->write(&ch, 1);
        }
        if (hasher) {
            hasher->update(&value, 1);
        }
    }

  public:
    explicit BinaryWriter(std::vector<uint8_t> &target) : buffer(&target) {}
    explicit BinaryWriter(std::ostream &target) : stream(&target) {}
    explicit BinaryWriter(Sha256 &target) : hasher(&target) {}

    void u8(uint8_t value)
    {
        write_byte(value);
    }

    void u32(uint32_t value)
    {
        for (size_t i = 0; i < 4; i++) {
            write_byte(static_cast<uint8_t>((value >> (i * 8)) & 0xff));
        }
    }

    void u64(uint64_t value)
    {
        for (size_t i = 0; i < 8; i++) {
            write_byte(static_cast<uint8_t>((value >> (i * 8)) & 0xff));
        }
    }

    void bytes(const uint8_t *data, size_t size)
    {
        if (buffer) {
            buffer->insert(buffer->end(), data, data + size);
        }
        if (stream) {
            stream->write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
        }
        if (hasher) {
            hasher->update(data, size);
        }
    }

    void string(std::string_view value)
    {
        if (value.size() > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("SIF string is too large");
        }
        u32(static_cast<uint32_t>(value.size()));
        bytes(reinterpret_cast<const uint8_t *>(value.data()), value.size());

        size_t padding = (4 - (value.size() % 4)) % 4;
        for (size_t i = 0; i < padding; i++) {
            write_byte(0);
        }
    }

    template <typename Container>
    void count(const Container &container)
    {
        if (container.size() > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("SIF container is too large");
        }
        u32(static_cast<uint32_t>(container.size()));
    }
};

class BinaryReader {
  private:
    std::istream &in;

  public:
    explicit BinaryReader(std::istream &source) : in(source) {}

    uint8_t u8()
    {
        char ch;
        if (!in.read(&ch, 1)) {
            throw std::runtime_error("unexpected end of SIF file");
        }
        return static_cast<uint8_t>(ch);
    }

    uint32_t u32()
    {
        uint32_t value = 0;
        for (size_t i = 0; i < 4; i++) {
            value |= static_cast<uint32_t>(u8()) << (i * 8);
        }
        return value;
    }

    uint64_t u64()
    {
        uint64_t value = 0;
        for (size_t i = 0; i < 8; i++) {
            value |= static_cast<uint64_t>(u8()) << (i * 8);
        }
        return value;
    }

    std::array<uint8_t, k_hash_size> hash()
    {
        std::array<uint8_t, k_hash_size> value = {};
        for (uint8_t &byte : value) {
            byte = u8();
        }
        return value;
    }

    std::array<uint8_t, k_uuid_size> uuid()
    {
        std::array<uint8_t, k_uuid_size> value = {};
        for (uint8_t &byte : value) {
            byte = u8();
        }
        return value;
    }

    std::string string()
    {
        uint32_t size = u32();
        std::string value(size, '\0');
        if (size != 0 && !in.read(value.data(), size)) {
            throw std::runtime_error("unexpected end of SIF string");
        }

        size_t padding = (4 - (static_cast<size_t>(size) % 4)) % 4;
        for (size_t i = 0; i < padding; i++) {
            if (u8() != 0) {
                throw std::runtime_error("non-zero SIF string padding");
            }
        }
        return value;
    }
};

static void write_expression(BinaryWriter &writer, const ExpressionNode &node);
static std::unique_ptr<ExpressionNode> read_expression(BinaryReader &reader, InterfaceNode &owner);
static void write_type(BinaryWriter &writer, const TypeNode &node);
static std::unique_ptr<TypeNode> read_type(BinaryReader &reader, InterfaceNode &owner);
static void decompile_type(std::ostream &out, const TypeNode &node);

static void write_annotations(
    BinaryWriter &writer,
    const std::vector<std::unique_ptr<AnnotationNode>> &annotations
)
{
    writer.count(annotations);
    for (const auto &annotation : annotations) {
        writer.string(annotation->name);
        writer.count(annotation->args);
        for (const auto &arg : annotation->args) {
            write_expression(writer, *arg);
        }
    }
}

static std::vector<std::unique_ptr<AnnotationNode>> read_annotations(
    BinaryReader &reader,
    InterfaceNode &owner
)
{
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    uint32_t count = reader.u32();

    for (uint32_t i = 0; i < count; i++) {
        auto node = std::make_unique<AnnotationNode>();
        node->name = owner.own_string(reader.string());

        uint32_t arg_count = reader.u32();
        for (uint32_t j = 0; j < arg_count; j++) {
            node->args.push_back(read_expression(reader, owner));
        }

        annotations.push_back(std::move(node));
    }

    return annotations;
}

static void write_expression(BinaryWriter &writer, const ExpressionNode &node)
{
    if (const auto *str = dynamic_cast<const StringLiteralExpressionNode *>(&node)) {
        writer.u8(static_cast<uint8_t>(ExprKind::String));
        writer.string(str->value);
        return;
    }

    if (const auto *num = dynamic_cast<const NumberLiteralExpressionNode *>(&node)) {
        writer.u8(static_cast<uint8_t>(ExprKind::Number));
        writer.u64(num->value);
        return;
    }

    if (const auto *ident = dynamic_cast<const IdentifierExpressionNode *>(&node)) {
        writer.u8(static_cast<uint8_t>(ExprKind::Identifier));
        writer.string(ident->name);
        return;
    }

    throw std::runtime_error("unsupported expression node");
}

static std::unique_ptr<ExpressionNode> read_expression(BinaryReader &reader, InterfaceNode &owner)
{
    ExprKind kind = static_cast<ExprKind>(reader.u8());

    switch (kind) {
    case ExprKind::String: {
        auto node = std::make_unique<StringLiteralExpressionNode>();
        node->value = owner.own_string(reader.string());
        return node;
    }
    case ExprKind::Number: {
        auto node = std::make_unique<NumberLiteralExpressionNode>();
        node->value = reader.u64();
        return node;
    }
    case ExprKind::Identifier: {
        auto node = std::make_unique<IdentifierExpressionNode>();
        node->name = owner.own_string(reader.string());
        return node;
    }
    }

    throw std::runtime_error("invalid expression kind in SIF");
}

static void write_type(BinaryWriter &writer, const TypeNode &node)
{
    writer.u8(node.is_const ? 1 : 0);

    if (node.is_ptr) {
        writer.u8(static_cast<uint8_t>(TypeKind::Ptr));
        write_type(writer, *node.inner_type);
        return;
    }

    if (node.is_array) {
        writer.u8(static_cast<uint8_t>(TypeKind::Array));
        write_type(writer, *node.inner_type);
        return;
    }

    writer.u8(static_cast<uint8_t>(TypeKind::Named));
    writer.string(node.name);
}

static std::unique_ptr<TypeNode> read_type(BinaryReader &reader, InterfaceNode &owner)
{
    auto node = std::make_unique<TypeNode>();
    node->is_const = reader.u8() != 0;

    TypeKind kind = static_cast<TypeKind>(reader.u8());
    switch (kind) {
    case TypeKind::Named:
        node->name = owner.own_string(reader.string());
        break;
    case TypeKind::Ptr:
        node->is_ptr = true;
        node->inner_type = read_type(reader, owner);
        break;
    case TypeKind::Array:
        node->is_array = true;
        node->inner_type = read_type(reader, owner);
        break;
    default:
        throw std::runtime_error("invalid type kind in SIF");
    }

    return node;
}

static void write_parameter(BinaryWriter &writer, const ParameterNode &node)
{
    writer.u8(static_cast<uint8_t>(node.direction));
    write_annotations(writer, node.annotations);
    write_type(writer, *node.type);
    writer.string(node.name);
}

static std::unique_ptr<ParameterNode> read_parameter(
    BinaryReader &reader,
    InterfaceNode &owner
)
{
    auto node = std::make_unique<ParameterNode>();
    node->direction = static_cast<ParameterNode::Direction>(reader.u8());
    if (node->direction != ParameterNode::Direction::IN &&
        node->direction != ParameterNode::Direction::OUT &&
        node->direction != ParameterNode::Direction::INOUT) {
        throw std::runtime_error("invalid parameter direction in SIF");
    }
    node->annotations = read_annotations(reader, owner);
    node->type = read_type(reader, owner);
    node->name = owner.own_string(reader.string());
    return node;
}

static void write_function(BinaryWriter &writer, const FunctionNode &node)
{
    writer.u32(node.id);
    writer.string(node.name);
    write_annotations(writer, node.annotations);
    writer.count(node.parameters);
    for (const auto &parameter : node.parameters) {
        write_parameter(writer, *parameter);
    }
}

static std::unique_ptr<FunctionNode> read_function(
    BinaryReader &reader,
    InterfaceNode &owner,
    AbiversionNode &abiversion
)
{
    auto node = std::make_unique<FunctionNode>(abiversion);
    node->id = reader.u32();
    node->name = owner.own_string(reader.string());
    node->annotations = read_annotations(reader, owner);

    uint32_t param_count = reader.u32();
    for (uint32_t i = 0; i < param_count; i++) {
        node->parameters.push_back(read_parameter(reader, owner));
    }

    owner.current_funcid = std::max(owner.current_funcid, node->id + 1);
    return node;
}

static void write_struct(BinaryWriter &writer, const StructNode &node)
{
    writer.string(node.name);
    write_annotations(writer, node.annotations);
    writer.count(node.fields);

    for (const auto &field : node.fields) {
        write_type(writer, *field->type);
        writer.string(field->name);
    }
}

static std::unique_ptr<StructNode> read_struct(
    BinaryReader &reader,
    InterfaceNode &owner,
    AbiversionNode &abiversion
)
{
    auto node = std::make_unique<StructNode>(abiversion);
    node->name = owner.own_string(reader.string());
    node->annotations = read_annotations(reader, owner);

    uint32_t field_count = reader.u32();
    for (uint32_t i = 0; i < field_count; i++) {
        auto field = std::make_unique<StructFieldNode>();
        field->type = read_type(reader, owner);
        field->name = owner.own_string(reader.string());
        node->fields.push_back(std::move(field));
    }

    return node;
}

static void write_bitfield(BinaryWriter &writer, const BitfieldNode &node)
{
    writer.string(node.name);
    write_annotations(writer, node.annotations);
    write_type(writer, *node.base_type);
    writer.count(node.fields);

    for (const auto &field : node.fields) {
        writer.string(field->name);
        writer.u64(field->bits);
    }
}

static std::unique_ptr<BitfieldNode> read_bitfield(
    BinaryReader &reader,
    InterfaceNode &owner,
    AbiversionNode &abiversion
)
{
    auto node = std::make_unique<BitfieldNode>(abiversion);
    node->name = owner.own_string(reader.string());
    node->annotations = read_annotations(reader, owner);
    node->base_type = read_type(reader, owner);

    uint32_t field_count = reader.u32();
    for (uint32_t i = 0; i < field_count; i++) {
        auto field = std::make_unique<BitfieldFieldNode>();
        field->name = owner.own_string(reader.string());
        field->bits = reader.u64();
        node->fields.push_back(std::move(field));
    }

    return node;
}

static void write_enum(BinaryWriter &writer, const EnumNode &node)
{
    writer.string(node.name);
    write_annotations(writer, node.annotations);
    write_type(writer, *node.base_type);
    writer.count(node.members);

    for (const auto &member : node.members) {
        writer.string(member->name);
        writer.u64(member->value);
        write_annotations(writer, member->annotations);
    }
}

static std::unique_ptr<EnumNode> read_enum(
    BinaryReader &reader,
    InterfaceNode &owner,
    AbiversionNode &abiversion
)
{
    auto node = std::make_unique<EnumNode>(abiversion);
    node->name = owner.own_string(reader.string());
    node->annotations = read_annotations(reader, owner);
    node->base_type = read_type(reader, owner);

    uint32_t member_count = reader.u32();
    for (uint32_t i = 0; i < member_count; i++) {
        auto member = std::make_unique<EnumMemberNode>();
        member->name = owner.own_string(reader.string());
        member->value = reader.u64();
        member->annotations = read_annotations(reader, owner);
        node->members.push_back(std::move(member));
    }

    return node;
}

static void write_revision_tree(
    BinaryWriter &writer,
    const AbiversionNode &node,
    const std::array<uint8_t, k_hash_size> &previous_hash
)
{
    writer.u64(node.version);
    writer.bytes(previous_hash.data(), previous_hash.size());
    write_annotations(writer, node.annotations);

    writer.count(node.bitfields);
    for (const auto &bitfield : node.bitfields) {
        write_bitfield(writer, *bitfield);
    }

    writer.count(node.enums);
    for (const auto &enum_node : node.enums) {
        write_enum(writer, *enum_node);
    }

    writer.count(node.structs);
    for (const auto &struct_node : node.structs) {
        write_struct(writer, *struct_node);
    }

    writer.count(node.functions);
    for (const auto &function : node.functions) {
        write_function(writer, *function);
    }
}

static void write_interface_identity(BinaryWriter &writer, const InterfaceNode &interface)
{
    if (!interface.has_uuid) {
        throw std::runtime_error("SIF interface requires a UUID");
    }
    if (!interface.has_prefix) {
        throw std::runtime_error("SIF interface requires a prefix");
    }

    writer.string(interface.name);
    writer.bytes(interface.uuid_namespace.data(), interface.uuid_namespace.size());
    writer.string(interface.uuid_name);
    writer.string(interface.prefix);
    write_annotations(writer, interface.annotations);
}

static std::array<uint8_t, k_uuid_size> compute_interface_uuid(const InterfaceNode &interface)
{
    std::array<uint8_t, k_uuid_size> value = {};

    if (!interface.has_uuid) {
        throw std::runtime_error("SIF interface requires a UUID");
    }

    uuids::uuid_name_generator gen(uuids::uuid(interface.uuid_namespace));
    auto uuid = gen(interface.uuid_name);
    auto bytes = uuid.as_bytes();
    for (size_t i = 0; i < value.size(); i++) {
        value[i] = static_cast<uint8_t>(bytes[i]);
    }

    return value;
}

static std::array<uint8_t, k_hash_size> compute_revision_hash(
    const InterfaceNode &interface,
    const AbiversionNode &node,
    const std::array<uint8_t, k_hash_size> &previous_hash
)
{
    Sha256 sha;
    BinaryWriter writer(sha);
    std::array<uint8_t, k_uuid_size> interface_uuid = compute_interface_uuid(interface);
    std::vector<uint8_t> revision_tree;
    BinaryWriter tree_writer(revision_tree);

    write_revision_tree(tree_writer, node, previous_hash);

    writer.string("strata.sif.revision.v0");
    writer.bytes(interface_uuid.data(), interface_uuid.size());
    writer.bytes(revision_tree.data(), revision_tree.size());
    return sha.finish();
}

static std::unique_ptr<AbiversionNode> read_revision_tree(
    BinaryReader &reader,
    InterfaceNode &owner,
    const std::array<uint8_t, k_hash_size> &expected_previous_hash,
    const std::array<uint8_t, k_hash_size> &expected_revision_hash,
    uint64_t expected_revision
)
{
    auto node = std::make_unique<AbiversionNode>(owner);
    node->version = reader.u64();
    node->previous_hash = reader.hash();

    if (node->version != expected_revision) {
        throw std::runtime_error("SIF ABI revisions are not contiguous");
    }

    if (node->previous_hash != expected_previous_hash) {
        throw std::runtime_error("SIF revision tree previous hash mismatch");
    }

    node->annotations = read_annotations(reader, owner);

    uint32_t bitfield_count = reader.u32();
    for (uint32_t i = 0; i < bitfield_count; i++) {
        node->bitfields.push_back(read_bitfield(reader, owner, *node));
    }

    uint32_t enum_count = reader.u32();
    for (uint32_t i = 0; i < enum_count; i++) {
        node->enums.push_back(read_enum(reader, owner, *node));
    }

    uint32_t struct_count = reader.u32();
    for (uint32_t i = 0; i < struct_count; i++) {
        node->structs.push_back(read_struct(reader, owner, *node));
    }

    uint32_t function_count = reader.u32();
    for (uint32_t i = 0; i < function_count; i++) {
        node->functions.push_back(read_function(reader, owner, *node));
    }

    node->abi_hash = compute_revision_hash(owner, *node, expected_previous_hash);
    if (node->abi_hash != expected_revision_hash) {
        throw std::runtime_error("SIF revision hash mismatch");
    }

    return node;
}

static void write_indent(std::ostream &out, unsigned int indent)
{
    for (unsigned int i = 0; i < indent; i++) {
        out << "    ";
    }
}

static std::string uuid_to_string(const std::array<uint8_t, k_uuid_size> &bytes)
{
    return uuids::to_string(uuids::uuid(bytes));
}

static void decompile_expression(std::ostream &out, const ExpressionNode &node)
{
    if (const auto *str = dynamic_cast<const StringLiteralExpressionNode *>(&node)) {
        out << str->value;
        return;
    }

    if (const auto *num = dynamic_cast<const NumberLiteralExpressionNode *>(&node)) {
        out << num->value;
        return;
    }

    if (const auto *ident = dynamic_cast<const IdentifierExpressionNode *>(&node)) {
        out << ident->name;
        return;
    }

    throw std::runtime_error("unsupported expression node");
}

static void decompile_annotations(
    std::ostream &out,
    const std::vector<std::unique_ptr<AnnotationNode>> &annotations,
    unsigned int indent
)
{
    for (const auto &annotation : annotations) {
        write_indent(out, indent);
        out << "@" << annotation->name << "(";

        for (size_t i = 0; i < annotation->args.size(); i++) {
            decompile_expression(out, *annotation->args[i]);
            if (i + 1 < annotation->args.size()) {
                out << ", ";
            }
        }

        out << ")\n";
    }
}

static void decompile_type(std::ostream &out, const TypeNode &node)
{
    if (node.is_const) {
        out << "const ";
    }

    if (node.is_ptr) {
        out << "ptr<";
        decompile_type(out, *node.inner_type);
        out << ">";
        return;
    }

    if (node.is_array) {
        out << "array<";
        decompile_type(out, *node.inner_type);
        out << ">";
        return;
    }

    out << node.name;
}

static void decompile_struct(std::ostream &out, const StructNode &node)
{
    decompile_annotations(out, node.annotations, 2);
    write_indent(out, 2);
    out << "struct " << node.name << " {\n";

    for (const auto &field : node.fields) {
        write_indent(out, 3);
        decompile_type(out, *field->type);
        out << " " << field->name << ";\n";
    }

    write_indent(out, 2);
    out << "};\n";
}

static void decompile_bitfield(std::ostream &out, const BitfieldNode &node)
{
    decompile_annotations(out, node.annotations, 2);
    write_indent(out, 2);
    out << "bitfield<";
    decompile_type(out, *node.base_type);
    out << "> " << node.name << " {\n";

    for (const auto &field : node.fields) {
        write_indent(out, 3);
        out << field->name << " : " << field->bits << ";\n";
    }

    write_indent(out, 2);
    out << "};\n";
}

static void decompile_enum(std::ostream &out, const EnumNode &node)
{
    decompile_annotations(out, node.annotations, 2);
    write_indent(out, 2);
    out << "enum<";
    decompile_type(out, *node.base_type);
    out << "> " << node.name << " {\n";

    for (size_t i = 0; i < node.members.size(); i++) {
        const auto &member = node.members[i];

        decompile_annotations(out, member->annotations, 3);
        write_indent(out, 3);
        out << member->name << " = " << member->value;
        if (i + 1 < node.members.size()) {
            out << ",";
        }
        out << "\n";
    }

    write_indent(out, 2);
    out << "};\n";
}

static void decompile_parameter(std::ostream &out, const ParameterNode &node)
{
    for (const auto &annotation : node.annotations) {
        out << "@" << annotation->name << "(";
        for (size_t i = 0; i < annotation->args.size(); i++) {
            decompile_expression(out, *annotation->args[i]);
            if (i + 1 < annotation->args.size()) {
                out << ", ";
            }
        }
        out << ") ";
    }

    switch (node.direction) {
    case ParameterNode::Direction::IN:
        out << "in ";
        break;
    case ParameterNode::Direction::OUT:
        out << "out ";
        break;
    case ParameterNode::Direction::INOUT:
        out << "inout ";
        break;
    }

    decompile_type(out, *node.type);
    out << " " << node.name;
}

static void decompile_function(std::ostream &out, const FunctionNode &node)
{
    decompile_annotations(out, node.annotations, 2);
    write_indent(out, 2);
    out << "function " << node.name << "(";

    for (size_t i = 0; i < node.parameters.size(); i++) {
        decompile_parameter(out, *node.parameters[i]);
        if (i + 1 < node.parameters.size()) {
            out << ", ";
        }
    }

    out << ");\n";
}

static void decompile_abirevision(std::ostream &out, const AbiversionNode &node)
{
    decompile_annotations(out, node.annotations, 1);
    write_indent(out, 1);
    out << "abirevision " << node.version << " {\n";

    bool needs_blank = false;

    for (const auto &bitfield : node.bitfields) {
        if (needs_blank) out << "\n";
        decompile_bitfield(out, *bitfield);
        needs_blank = true;
    }

    for (const auto &enum_node : node.enums) {
        if (needs_blank) out << "\n";
        decompile_enum(out, *enum_node);
        needs_blank = true;
    }

    for (const auto &struct_node : node.structs) {
        if (needs_blank) out << "\n";
        decompile_struct(out, *struct_node);
        needs_blank = true;
    }

    for (const auto &function : node.functions) {
        if (needs_blank) out << "\n";
        decompile_function(out, *function);
        needs_blank = true;
    }

    write_indent(out, 1);
    out << "}\n";
}

}  // namespace

void sif_write(
    std::ostream &out,
    const InterfaceNode &interface,
    const std::vector<const InterfaceNode *> &extension_interfaces
)
{
    std::vector<const AbiversionNode *> revisions;
    for (const auto &revision : interface.abiversions) {
        revisions.push_back(revision.get());
    }

    for (const InterfaceNode *extension : extension_interfaces) {
        for (const auto &revision : extension->abiversions) {
            revisions.push_back(revision.get());
        }
    }

    if (revisions.empty()) {
        throw std::runtime_error("SIF artifact requires at least one ABI revision");
    }

    std::vector<std::array<uint8_t, k_hash_size>> revision_hashes;
    std::array<uint8_t, k_hash_size> previous_hash = {};

    for (const AbiversionNode *revision : revisions) {
        std::array<uint8_t, k_hash_size> revision_hash =
            compute_revision_hash(interface, *revision, previous_hash);
        revision_hashes.push_back(revision_hash);
        previous_hash = revision_hash;
    }

    BinaryWriter writer(out);
    writer.bytes(k_sif_magic, sizeof(k_sif_magic));
    writer.u32(k_sif_format_version);
    write_interface_identity(writer, interface);
    writer.count(revision_hashes);
    writer.bytes(revision_hashes.front().data(), revision_hashes.front().size());
    for (const auto &revision_hash : revision_hashes) {
        writer.bytes(revision_hash.data(), revision_hash.size());
    }

    previous_hash = {};
    for (size_t i = 0; i < revisions.size(); i++) {
        write_revision_tree(writer, *revisions[i], previous_hash);
        previous_hash = revision_hashes[i];
    }
}

std::unique_ptr<InterfaceNode> sif_read(std::istream &in)
{
    BinaryReader reader(in);

    for (uint8_t expected : k_sif_magic) {
        if (reader.u8() != expected) {
            throw std::runtime_error("invalid SIF magic");
        }
    }

    if (reader.u32() != k_sif_format_version) {
        throw std::runtime_error("unsupported SIF format version");
    }

    auto interface = std::make_unique<InterfaceNode>();
    interface->name = interface->own_string(reader.string());
    interface->uuid_namespace = reader.uuid();
    interface->uuid_name = interface->own_string(reader.string());
    interface->has_uuid = true;
    interface->prefix = interface->own_string(reader.string());
    interface->has_prefix = true;
    interface->annotations = read_annotations(reader, *interface);

    uint32_t revision_count = reader.u32();
    if (revision_count == 0) {
        throw std::runtime_error("SIF artifact has no ABI revisions");
    }

    std::array<uint8_t, k_hash_size> root_revision_hash = reader.hash();
    std::vector<std::array<uint8_t, k_hash_size>> revision_hashes;
    revision_hashes.reserve(revision_count);
    for (uint32_t i = 0; i < revision_count; i++) {
        revision_hashes.push_back(reader.hash());
    }

    if (root_revision_hash != revision_hashes.front()) {
        throw std::runtime_error("SIF root revision hash mismatch");
    }

    std::array<uint8_t, k_hash_size> previous_hash = {};

    for (uint32_t i = 0; i < revision_count; i++) {
        auto revision = read_revision_tree(reader, *interface, previous_hash, revision_hashes[i], i);
        previous_hash = revision_hashes[i];
        interface->abiversions.push_back(std::move(revision));
    }

    if (in.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("trailing data in SIF file");
    }

    return interface;
}

void sif_decompile(std::ostream &out, const InterfaceNode &interface)
{
    if (!interface.has_uuid) {
        throw std::runtime_error("SIF interface requires a UUID");
    }
    if (!interface.has_prefix) {
        throw std::runtime_error("SIF interface requires a prefix");
    }

    out << "@uuid(\"" << uuid_to_string(interface.uuid_namespace) << "\", \""
        << interface.uuid_name << "\")\n";
    out << "@prefix(\"" << interface.prefix << "\")\n";
    decompile_annotations(out, interface.annotations, 0);
    out << "interface " << interface.name << " {\n";

    for (size_t i = 0; i < interface.abiversions.size(); i++) {
        if (i != 0) {
            out << "\n";
        }
        decompile_abirevision(out, *interface.abiversions[i]);
    }

    out << "}\n";
}
