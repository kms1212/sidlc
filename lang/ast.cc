#include <ast.hh>

std::string AstVisitor::to_c_type(std::string &prefix, TypeNode &node)
{
    std::string str;

    if (node.is_const) {
        str += "const ";
    }

    if (node.is_ptr && node.ptr_type) {
        std::string inner_type = to_c_type(prefix, *node.ptr_type);

        if (inner_type == "opaque") {
            str += "void *";
        } else {
            str += inner_type + " *";
        }

        return str;
    }

    std::string_view name = node.name;
    if (name == "opaque") {
        str += "void";
    } else if (name == "u8") {
        str += "uint8_t";
    } else if (name == "u16") {
        str += "uint16_t";
    } else if (name == "u32") {
        str += "uint32_t";
    } else if (name == "u64") {
        str += "uint64_t";
    } else if (name == "s8") {
        str += "int8_t";
    } else if (name == "s16") {
        str += "int16_t";
    } else if (name == "s32") {
        str += "int32_t";
    } else if (name == "s64") {
        str += "int64_t";
    } else {
        str += prefix + std::string(name);
    }

    return str;
}
