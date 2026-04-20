#include <c_module_source_generator.hh>

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <uuid.h>

#include <arch_abi.hh>
#include <ast.hh>
#include <lang_info.hh>

#include "config.h"

void CModuleSourceGenerator::register_builtin_type_infos()
{
    type_value_infos.clear();

    for (const auto &[name, info] : g_current_lang_info->type_infos) {
        TypeValueInfo value_info;

        value_info.c_type = info.lang_name;
        value_info.type_size = info.size;

        if (name == "handle") {
            value_info.is_pointer_like = true;
        } else if (name == "status") {
            value_info.is_integer = true;
        } else if (name.size() >= 2 && name[0] == 'u' && std::isdigit(name[1])) {
            value_info.is_integer = true;
            value_info.is_unsigned = true;
        } else if (name.size() >= 2 && name[0] == 's' && std::isdigit(name[1])) {
            value_info.is_integer = true;
        }

        type_value_infos[name] = value_info;
    }
}

CModuleSourceGenerator::TypeValueInfo CModuleSourceGenerator::get_type_value_info(TypeNode &node)
{
    if (node.inner_type && node.is_ptr) {
        TypeValueInfo info;

        info.c_type = to_c_type(prefix, node);
        info.type_size = sizeof(void *);
        info.is_pointer_like = true;
        return info;
    }

    auto builtin_it = type_value_infos.find(std::string(node.name));
    if (builtin_it != type_value_infos.end()) {
        TypeValueInfo info = builtin_it->second;
        info.c_type = to_c_type(prefix, node);
        return info;
    }

    TypeValueInfo info;

    info.c_type = to_c_type(prefix, node);
    info.type_size = node.type_size;
    return info;
}

std::string CModuleSourceGenerator::get_parameter_c_type(const ParameterNode &node)
{
    bool add_pointer = node.direction != ParameterNode::Direction::IN;

    if (node.type->is_ptr) {
        return to_c_type(prefix, *node.type) + (add_pointer ? "*" : "");
    }

    return to_c_type(prefix, *node.type) + (add_pointer ? " *" : "");
}

bool CModuleSourceGenerator::parameter_uses_pointer_argument(const ParameterNode &node)
{
    return node.direction != ParameterNode::Direction::IN || node.type->is_ptr ||
        node.type->is_array;
}

bool CModuleSourceGenerator::is_scalar_in_register_param(const ParameterNode &node)
{
    TypeValueInfo type_info;

    if (node.direction != ParameterNode::Direction::IN) return false;
    if (node.type->is_ptr || node.type->is_array) return false;

    type_info = get_type_value_info(*node.type);
    return type_info.type_size > 0 && type_info.type_size <= g_current_arch_abi->pointer_size;
}

void CModuleSourceGenerator::write_dispatch_case(FunctionNode &node)
{
    bool can_use_call_reg = node.parameters.size() <= module_arg_slot_count;
    std::vector<ParameterNode *> peeled_params;
    std::vector<ParameterNode *> packed_in_params;
    std::vector<ParameterNode *> packed_out_params;
    std::map<ParameterNode *, size_t> peeled_indexes;

    for (const auto &param : node.parameters) {
        bool is_scalar_in_reg = is_scalar_in_register_param(*param);

        if (!is_scalar_in_reg) {
            can_use_call_reg = false;
        }

        if (param->direction == ParameterNode::Direction::OUT) {
            packed_out_params.push_back(param.get());
            continue;
        }

        if (is_scalar_in_reg && peeled_params.size() < module_peel_slot_count) {
            peeled_params.push_back(param.get());
            continue;
        }

        packed_in_params.push_back(param.get());
    }

    for (size_t i = 0; i < peeled_params.size(); ++i) {
        peeled_indexes[peeled_params[i]] = i;
    }

    out << "    case " << node.id << ": {\n";
    if (node.parameters.empty()) out << "        (void)args;\n";
    out << "\n";

    if (!can_use_call_reg) {
        if (packed_in_params.size() > 1) {
            out << "        struct StSidlP_InPack {\n";
            for (const auto *param : packed_in_params) {
                out << "            " << get_parameter_c_type(*param) << " " << param->name
                    << ";\n";
            }
            out << "        };\n";
            out << "        const struct StSidlP_InPack *packed_in = "
                   "(const struct StSidlP_InPack *)(uintptr_t)args[0];\n";
        }
        if (packed_out_params.size() > 1) {
            out << "        struct StSidlP_OutPack {\n";
            for (const auto *param : packed_out_params) {
                out << "            " << to_c_type(prefix, *param->type) << " " << param->name
                    << ";\n";
            }
            out << "        };\n";
            out << "        struct StSidlP_OutPack *packed_out = "
                   "(struct StSidlP_OutPack *)(uintptr_t)args[1];\n";
        }

        if (packed_in_params.size() > 1 ||
            (packed_in_params.size() == 1 && !packed_in_params.front()->type->is_ptr)) {
            out << "        if ((const void *)(uintptr_t)args[0] == NULL) {\n";
            out << "            return STATUS_INVALID_VALUE;\n";
            out << "        }\n";
        }
        if (packed_out_params.size() > 1) {
            out << "        if ((void *)(uintptr_t)args[1] == NULL) {\n";
            out << "            return STATUS_INVALID_VALUE;\n";
            out << "        }\n";
        }

        if (!packed_in_params.empty() || !packed_out_params.empty()) {
            out << "\n";
        }
    }

    for (size_t i = 0; i < node.parameters.size(); ++i) {
        const auto &param = node.parameters[i];
        std::string exact_type = get_parameter_c_type(*param);
        std::string arg_expr;
        auto peeled_it = peeled_indexes.find(param.get());

        if (can_use_call_reg) {
            arg_expr = parameter_uses_pointer_argument(*param) ? "(uintptr_t)args[" : "args[";
            arg_expr += std::to_string(i);
            arg_expr += "]";
        } else if (param->direction == ParameterNode::Direction::OUT) {
            if (packed_out_params.size() == 1) {
                arg_expr = "(uintptr_t)args[1]";
            } else {
                arg_expr = "&packed_out->" + std::string(param->name);
            }
        } else if (peeled_it != peeled_indexes.end()) {
            arg_expr = "args[" + std::to_string(2 + peeled_it->second) + "]";
        } else if (packed_in_params.size() == 1) {
            if (packed_in_params.front()->type->is_ptr) {
                arg_expr = "(uintptr_t)args[0]";
            } else {
                arg_expr = "*(const " + exact_type + " *)(uintptr_t)args[0]";
            }
        } else {
            arg_expr = "packed_in->" + std::string(param->name);
        }

        out << "        " << exact_type << " " << param->name << " = (" << exact_type << ")"
            << arg_expr << ";\n";
    }

    out << "\n        return typed_vtable->" << node.name << "(context, handle";
    for (const auto &param : node.parameters) {
        out << ", " << param->name;
    }
    out << ");\n";
    out << "    }\n";
}

void CModuleSourceGenerator::visit(InterfaceNode &node)
{
    prefix.clear();
    macro_interface_name.clear();
    total_funcid_span = 0;
    abi_spans.clear();
    functions_by_id.clear();
    register_builtin_type_infos();

    if (!g_current_arch_abi || g_current_arch_abi->max_reg_args <= 2) {
        throw std::runtime_error("Invalid architecture ABI");
    }
    module_arg_slot_count = g_current_arch_abi->max_reg_args - 2;
    module_peel_slot_count = module_arg_slot_count > 2 ? module_arg_slot_count - 2 : 0;

    macro_interface_name = node.name;
    std::transform(
        macro_interface_name.begin(),
        macro_interface_name.end(),
        macro_interface_name.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        }
    );

    for (const auto &anno : node.annotations) {
        if (anno->name == "prefix") {
            if (anno->args.size() != 1) {
                throw std::runtime_error("Invalid argument size");
            }

            auto prefix_param = dynamic_cast<StringLiteralExpressionNode *>(anno->args[0].get());
            if (!prefix_param) {
                throw std::runtime_error("Invalid argument type");
            }

            prefix = prefix_param->value.substr(1, prefix_param->value.size() - 2);
        }
    }

    for (const auto &abi : node.abiversions) {
        abi->accept(*this);
    }

    out << "/* =====================================================================\n";
    out << " * Auto-generated by sidlc v" << SIDLC_VERSION << " (" << SIDLC_GIT_HASH << ")\n";
    out << " * Target Interface: " << node.name << "\n";
    out << " * DO NOT EDIT THIS FILE MANUALLY!\n";
    out << " * ===================================================================== */\n\n";

    out << "#include \"" << header_name << "\"\n\n";

    out << "StStatus " << prefix << "ModuleDispatchArgs(\n";
    out << "    const void *vtable __in,\n";
    out << "    void *context __inout,\n";
    out << "    StHandle handle __in,\n";
    out << "    uint32_t funcid __in,\n";
    out << "    const long args[" << module_arg_slot_count << "]\n";
    out << ")\n";
    out << "{\n";
    out << "    const " << prefix << "ModuleVTable *typed_vtable;\n";
    out << "    const uintptr_t *vtable_slots;\n";
    out << "\n";
    out << "    if (vtable == NULL || args == NULL) {\n";
    out << "        return STATUS_INVALID_VALUE;\n";
    out << "    }\n";
    out << "    if (funcid >= " << total_funcid_span << ") {\n";
    out << "        return STATUS_NOT_IMPLEMENTED;\n";
    out << "    }\n";
    out << "    vtable_slots = (const uintptr_t *)vtable;\n";
    out << "    if (vtable_slots[funcid] == 0) {\n";
    out << "        return STATUS_NOT_IMPLEMENTED;\n";
    out << "    }\n";
    out << "    typed_vtable = (const " << prefix << "ModuleVTable *)vtable;\n\n";
    out << "    switch (funcid) {\n";
    for (FunctionNode *function : functions_by_id) {
        if (!function) {
            continue;
        }

        write_dispatch_case(*function);
    }
    out << "    default:\n";
    out << "        return STATUS_NOT_IMPLEMENTED;\n";
    out << "    }\n";
    out << "}\n\n";
}

void CModuleSourceGenerator::visit(AbiversionNode &node)
{
    for (const auto &e : node.enums) {
        e->accept(*this);
    }

    for (const auto &b : node.bitfields) {
        b->accept(*this);
    }

    for (const auto &s : node.structs) {
        s->accept(*this);
    }

    for (const auto &f : node.functions) {
        f->accept(*this);
    }

    if (abi_spans.size() <= node.version) {
        abi_spans.resize(node.version + 1, total_funcid_span);
    }

    if (!node.functions.empty()) {
        total_funcid_span = node.functions.back()->id + 1;
    }

    abi_spans[node.version] = total_funcid_span;
}

void CModuleSourceGenerator::visit(StructNode &node)
{
    TypeValueInfo info;

    info.c_type = prefix + std::string(node.name);
    type_value_infos[std::string(node.name)] = info;
}

void CModuleSourceGenerator::visit(BitfieldNode &node)
{
    TypeValueInfo info = get_type_value_info(*node.base_type);

    info.c_type = prefix + std::string(node.name);
    info.is_pointer_like = false;
    type_value_infos[std::string(node.name)] = info;
}

void CModuleSourceGenerator::visit(EnumNode &node)
{
    TypeValueInfo info = get_type_value_info(*node.base_type);

    info.c_type = prefix + std::string(node.name);
    info.is_pointer_like = false;
    type_value_infos[std::string(node.name)] = info;
}

void CModuleSourceGenerator::visit(FunctionNode &node)
{
    if (functions_by_id.size() <= node.id) {
        functions_by_id.resize(node.id + 1, nullptr);
    }

    functions_by_id[node.id] = &node;
}
