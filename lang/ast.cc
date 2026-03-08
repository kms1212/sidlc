#include <ast.hh>
#include <lang_info.hh>

std::string AstVisitor::to_c_type(std::string &prefix, TypeNode &node)
{
    std::string str;

    if (node.is_const) {
        str += "const ";
    }

    if (node.inner_type) {
        if (node.is_ptr) {
            std::string inner_type = to_c_type(prefix, *node.inner_type);

            if (inner_type == "void") {
                str += "void *";
            } else {
                str += inner_type + " *";
            }

            return str;
        } else if (node.is_array) {
            std::string inner_type = to_c_type(prefix, *node.inner_type);

            str += inner_type;

            return str;
        }
    }

    std::string_view name = node.name;
    auto it = g_current_lang_info->type_infos.find(std::string(name));
    if (it != g_current_lang_info->type_infos.end()) {
        str += it->second.lang_name;
    } else {
        str += prefix + std::string(name);
    }

    return str;
}
