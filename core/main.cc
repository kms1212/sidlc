#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <arch_abi.hh>
#include <ast.hh>
#include <c_handler.hh>
#include <lang_info.hh>
#include <parser.hh>
#include <sif.hh>
#include <uuid.h>

#include "config.h"

static const std::map<std::string, LangInfo> lang_infos = {
    {
        "c",
        {
            "c",
            {
                { "opaque", { "void", 0, 0 } },
                { "u8", { "uint8_t", 1, 1 } },
                { "u16", { "uint16_t", 2, 2 } },
                { "u32", { "uint32_t", 4, 4 } },
                { "u64", { "uint64_t", 8, 8 } },
                { "s8", { "int8_t", 1, 1 } },
                { "s16", { "int16_t", 2, 2 } },
                { "s32", { "int32_t", 4, 4 } },
                { "s64", { "int64_t", 8, 8 } },
                { "handle", { "StHandle", 4, 4 } },
                { "status", { "StStatus", 4, 4 } },
            },
        },
    },
};

static const std::map<std::string, ArchAbi> arch_abis = {
    { "x86_64", { "x86_64", 8, 6 } },
};

const ArchAbi *g_current_arch_abi = nullptr;
const LangInfo *g_current_lang_info = nullptr;

struct ParsedInterface {
    std::string source;
    std::unique_ptr<InterfaceNode> interface;
};

static void print_usage(const char *argv0)
{
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " compile [-b <base.sif>] -o <out.sif> <source.sidl>\n"
        << "  " << argv0 << " decompile -o <out.sidl> <input.sif>\n"
        << "  " << argv0
        << " generate -a <arch> -l <lang> -m <mode> -h <header-dir> -s <source-path> "
           "[-i <include-dir>] [--weak] <input.sif>\n\n"
        << "Subcommands:\n"
           "  compile     Compile SIDL source plus an optional base SIF into a new SIF\n"
           "  decompile   Convert a SIF artifact back into canonical SIDL source\n"
           "  generate    Generate source/header files from a SIF artifact\n\n"
        << "Generate modes:\n"
           "  client, server, server-client\n";
}

static bool read_file(const std::string &path, std::string *out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file " << path << '\n';
        return false;
    }

    *out = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

static bool get_string_literal_arg(
    const AnnotationNode &annotation,
    size_t index,
    std::string *out
)
{
    auto arg = dynamic_cast<StringLiteralExpressionNode *>(annotation.args[index].get());
    if (!arg) {
        std::cerr << "Error: @" << annotation.name << " argument " << index
                  << " must be a string literal\n";
        return false;
    }

    if (arg->value.size() < 2 || arg->value.front() != '"' || arg->value.back() != '"') {
        std::cerr << "Error: @" << annotation.name << " argument " << index
                  << " is not a valid string literal\n";
        return false;
    }

    *out = std::string(arg->value.substr(1, arg->value.size() - 2));
    return true;
}

static bool normalize_interface_metadata(InterfaceNode &node)
{
    bool found_uuid = false;
    bool found_prefix = false;
    std::vector<std::unique_ptr<AnnotationNode>> remaining_annotations;

    for (auto &annotation : node.annotations) {
        if (annotation->name != "uuid" && annotation->name != "prefix") {
            remaining_annotations.push_back(std::move(annotation));
            continue;
        }

        if (annotation->name == "uuid") {
            if (found_uuid) {
                std::cerr << "Error: interface has duplicate @uuid annotations\n";
                return false;
            }

            if (annotation->args.size() != 2) {
                std::cerr << "Error: @uuid requires namespace UUID and name arguments\n";
                return false;
            }

            std::string namespace_str;
            std::string name_str;
            if (!get_string_literal_arg(*annotation, 0, &namespace_str) ||
                !get_string_literal_arg(*annotation, 1, &name_str)) {
                return false;
            }

            if (!uuids::uuid::is_valid_uuid(namespace_str)) {
                std::cerr << "Error: invalid @uuid namespace UUID\n";
                return false;
            }

            auto namespace_uuid = uuids::uuid::from_string(namespace_str);
            if (!namespace_uuid) {
                std::cerr << "Error: invalid @uuid namespace UUID\n";
                return false;
            }

            auto namespace_bytes = namespace_uuid->as_bytes();
            for (size_t i = 0; i < node.uuid_namespace.size(); i++) {
                node.uuid_namespace[i] = static_cast<uint8_t>(namespace_bytes[i]);
            }
            node.uuid_name = node.own_string(std::move(name_str));
            node.has_uuid = true;
            found_uuid = true;
            continue;
        }

        if (found_prefix) {
            std::cerr << "Error: interface has duplicate @prefix annotations\n";
            return false;
        }

        if (annotation->args.size() != 1) {
            std::cerr << "Error: @prefix requires one string argument\n";
            return false;
        }

        std::string prefix;
        if (!get_string_literal_arg(*annotation, 0, &prefix)) {
            return false;
        }

        node.prefix = node.own_string(std::move(prefix));
        node.has_prefix = true;
        found_prefix = true;
    }

    if (!found_uuid) {
        std::cerr << "Error: interface requires a @uuid identity\n";
        return false;
    }

    if (!found_prefix) {
        std::cerr << "Error: interface requires a @prefix\n";
        return false;
    }

    node.annotations = std::move(remaining_annotations);
    return true;
}

static bool parse_interface_file(
    const std::string &path,
    uint64_t first_abirevision,
    uint32_t first_funcid,
    ParsedInterface *out
)
{
    if (!read_file(path, &out->source)) {
        return false;
    }

    Parser parser(out->source, first_abirevision, first_funcid);
    out->interface = parser.parse();
    if (!out->interface) {
        return false;
    }

    if (!normalize_interface_metadata(*out->interface)) {
        return false;
    }

    return true;
}

static bool open_output_file(const std::string &path, std::ofstream *out)
{
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    out->open(path);
    if (!out->is_open()) {
        std::cerr << "Error: could not open output file " << path << '\n';
        return false;
    }

    return true;
}

static bool open_binary_output_file(const std::string &path, std::ofstream *out)
{
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    out->open(path, std::ios::binary);
    if (!out->is_open()) {
        std::cerr << "Error: could not open output file " << path << '\n';
        return false;
    }

    return true;
}

static bool read_sif_file(const std::string &path, std::unique_ptr<InterfaceNode> *out)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: could not open SIF file " << path << '\n';
        return false;
    }

    try {
        *out = sif_read(file);
    } catch (const std::runtime_error &e) {
        std::cerr << "Error: failed to read SIF file " << path << ": " << e.what() << '\n';
        return false;
    }

    return true;
}

static bool take_option_value(int argc, char **argv, int *index, std::string *value)
{
    std::string arg = argv[*index];
    size_t eq = arg.find('=');
    if (eq != std::string::npos) {
        *value = arg.substr(eq + 1);
        return true;
    }

    if (*index + 1 >= argc) {
        std::cerr << "Error: missing value for " << arg << '\n';
        return false;
    }

    *index += 1;
    *value = argv[*index];
    return true;
}

static std::string expression_key(const ExpressionNode &node)
{
    if (const auto *str = dynamic_cast<const StringLiteralExpressionNode *>(&node)) {
        return "s:" + std::string(str->value);
    }

    if (const auto *num = dynamic_cast<const NumberLiteralExpressionNode *>(&node)) {
        return "n:" + std::to_string(num->value);
    }

    if (const auto *ident = dynamic_cast<const IdentifierExpressionNode *>(&node)) {
        return "i:" + std::string(ident->name);
    }

    return "unknown";
}

static std::string annotation_key(const AnnotationNode &node)
{
    std::string key = std::string(node.name) + "(";
    for (size_t i = 0; i < node.args.size(); i++) {
        key += expression_key(*node.args[i]);
        if (i + 1 < node.args.size()) {
            key += ",";
        }
    }
    key += ")";
    return key;
}

static std::string interface_annotation_key(const InterfaceNode &node)
{
    std::string key;
    for (const auto &annotation : node.annotations) {
        key += annotation_key(*annotation);
        key += "\n";
    }
    return key;
}

static bool validate_extension_interface(
    const InterfaceNode &base,
    const InterfaceNode &extension
)
{
    if (base.name != extension.name) {
        std::cerr << "Error: SIF base interface " << base.name
                  << " does not match source interface " << extension.name << '\n';
        return false;
    }

    if (!base.has_uuid || !extension.has_uuid ||
        base.uuid_namespace != extension.uuid_namespace ||
        base.uuid_name != extension.uuid_name) {
        std::cerr << "Error: SIF base interface UUID identity does not match source interface\n";
        return false;
    }

    if (!base.has_prefix || !extension.has_prefix || base.prefix != extension.prefix) {
        std::cerr << "Error: SIF base interface prefix does not match source interface\n";
        return false;
    }

    if (interface_annotation_key(base) != interface_annotation_key(extension)) {
        std::cerr << "Error: SIF base interface annotations do not match source interface\n";
        return false;
    }

    return true;
}

static bool set_arch(const std::string &arch)
{
    auto it = arch_abis.find(arch);
    if (it == arch_abis.end()) {
        std::cerr << "Error: unknown architecture " << arch << '\n';
        return false;
    }

    g_current_arch_abi = &it->second;
    return true;
}

static bool set_lang(const std::string &lang)
{
    auto it = lang_infos.find(lang);
    if (it == lang_infos.end()) {
        std::cerr << "Error: unknown language " << lang << '\n';
        return false;
    }

    g_current_lang_info = &it->second;
    return true;
}

static std::string include_path_for(const std::string &include_dir, const std::string &header_name)
{
    if (include_dir.empty()) {
        return header_name;
    }

    return (std::filesystem::path(include_dir) / header_name).generic_string();
}

static int command_compile(int argc, char **argv)
{
    std::string base_path;
    std::string output_path;
    std::string input_path;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-b" || arg == "--base" || arg.rfind("--base=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &base_path)) return 1;
        } else if (arg == "-o" || arg == "--output" || arg.rfind("--output=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &output_path)) return 1;
        } else if (arg.rfind("-", 0) == 0) {
            std::cerr << "Error: unknown compile option " << arg << '\n';
            return 1;
        } else if (input_path.empty()) {
            input_path = arg;
        } else {
            std::cerr << "Error: too many compile inputs\n";
            return 1;
        }
    }

    if (input_path.empty() || output_path.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    std::unique_ptr<InterfaceNode> base;
    ParsedInterface source;
    uint64_t first_abirevision = 0;
    uint32_t first_funcid = 0;
    std::vector<const InterfaceNode *> extensions;

    if (!base_path.empty()) {
        if (!read_sif_file(base_path, &base)) return 1;
        first_abirevision = base->abiversions.size();
        first_funcid = base->current_funcid;
    }

    if (!parse_interface_file(input_path, first_abirevision, first_funcid, &source)) {
        return 1;
    }

    if (!base_path.empty()) {
        if (!validate_extension_interface(*base, *source.interface)) {
            return 1;
        }
        extensions.push_back(source.interface.get());
    }

    std::ofstream output;
    if (!open_binary_output_file(output_path, &output)) return 1;

    try {
        if (base_path.empty()) {
            sif_write(output, *source.interface, {});
        } else {
            sif_write(output, *base, extensions);
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "Error: failed to write SIF file " << output_path << ": " << e.what()
                  << '\n';
        return 1;
    }

    return 0;
}

static int command_decompile(int argc, char **argv)
{
    std::string output_path;
    std::string input_path;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-o" || arg == "--output" || arg.rfind("--output=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &output_path)) return 1;
        } else if (arg.rfind("-", 0) == 0) {
            std::cerr << "Error: unknown decompile option " << arg << '\n';
            return 1;
        } else if (input_path.empty()) {
            input_path = arg;
        } else {
            std::cerr << "Error: too many decompile inputs\n";
            return 1;
        }
    }

    if (input_path.empty() || output_path.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    std::unique_ptr<InterfaceNode> sif;
    if (!read_sif_file(input_path, &sif)) return 1;

    std::ofstream output;
    if (!open_output_file(output_path, &output)) return 1;

    sif_decompile(output, *sif);
    return 0;
}

static int command_generate(int argc, char **argv)
{
    std::string arch;
    std::string lang;
    std::string mode;
    std::string header_dir;
    std::string include_dir;
    std::string source_path;
    std::string input_path;
    bool weak = false;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-a" || arg == "--arch" || arg.rfind("--arch=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &arch)) return 1;
        } else if (arg == "-l" || arg == "--lang" || arg.rfind("--lang=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &lang)) return 1;
        } else if (arg == "-m" || arg == "--mode" || arg.rfind("--mode=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &mode)) return 1;
        } else if (arg == "-h" || arg == "--header-dir" || arg.rfind("--header-dir=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &header_dir)) return 1;
        } else if (arg == "-i" || arg == "--include-dir" || arg.rfind("--include-dir=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &include_dir)) return 1;
        } else if (arg == "-s" || arg == "--source-path" || arg.rfind("--source-path=", 0) == 0) {
            if (!take_option_value(argc, argv, &i, &source_path)) return 1;
        } else if (arg == "--weak") {
            weak = true;
        } else if (arg.rfind("-", 0) == 0) {
            std::cerr << "Error: unknown generate option " << arg << '\n';
            return 1;
        } else if (input_path.empty()) {
            input_path = arg;
        } else {
            std::cerr << "Error: too many generate inputs\n";
            return 1;
        }
    }

    if (arch.empty() || lang.empty() || mode.empty() || header_dir.empty()
        || source_path.empty() || input_path.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    if (!set_arch(arch) || !set_lang(lang)) return 1;
    if (lang != "c") {
        std::cerr << "Error: generate currently supports only --lang=c\n";
        return 1;
    }

    if (mode != "client" && mode != "server" && mode != "server-client") {
        std::cerr << "Error: unknown generate mode " << mode << '\n';
        return 1;
    }

    std::unique_ptr<InterfaceNode> sif;
    if (!read_sif_file(input_path, &sif)) return 1;

    std::filesystem::create_directories(header_dir);
    std::filesystem::path source_parent = std::filesystem::path(source_path).parent_path();
    if (!source_parent.empty()) {
        std::filesystem::create_directories(source_parent);
    }

    std::string base_name = std::filesystem::path(input_path).stem().generic_string();
    std::string type_header_name = base_name + ".types.h";
    std::string type_header_path =
        (std::filesystem::path(header_dir) / type_header_name).generic_string();

    CGenerateOptions options;
    options.type_header_path = type_header_path;
    options.type_header_include_path = type_header_name;
    options.source_path = source_path;
    options.make_weak_symbols = weak;
    std::string header_name;

    if (mode == "client") {
        options.mode = CGenerateMode::Client;
        header_name = base_name + ".h";
    } else if (mode == "server") {
        options.mode = CGenerateMode::Server;
        header_name = base_name + ".server.h";
    } else {
        options.mode = CGenerateMode::ServerClient;
        header_name = base_name + ".server-client.h";
    }

    options.header_path = (std::filesystem::path(header_dir) / header_name).generic_string();
    options.source_header_include_path = include_path_for(include_dir, header_name);

    if (!c_generate(sif.get(), options)) return 1;

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    if (command == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    if (command == "--version" || command == "-v") {
        std::cout << "sidlc version " << SIDLC_VERSION << " (" << SIDLC_GIT_HASH << ")\n";
        return 0;
    }

    if (command == "compile") {
        return command_compile(argc, argv);
    }

    if (command == "decompile") {
        return command_decompile(argc, argv);
    }

    if (command == "generate") {
        return command_generate(argc, argv);
    }

    std::cerr << "Error: unknown subcommand " << command << '\n';
    print_usage(argv[0]);
    return 1;
}
