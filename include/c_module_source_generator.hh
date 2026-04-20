#ifndef __C_MODULE_SOURCE_GENERATOR_HH__
#define __C_MODULE_SOURCE_GENERATOR_HH__

#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <ast.hh>

class CModuleSourceGenerator : public AstVisitor {
    struct TypeValueInfo {
        std::string c_type;
        size_t type_size = 0;
        bool is_integer = false;
        bool is_unsigned = false;
        bool is_pointer_like = false;
    };

    std::ostream &out;
    std::string prefix;
    std::string macro_interface_name;
    std::string header_name;
    uint32_t total_funcid_span = 0;
    std::vector<uint32_t> abi_spans;
    std::vector<FunctionNode *> functions_by_id;
    std::map<std::string, TypeValueInfo> type_value_infos;
    size_t module_arg_slot_count = 0;
    size_t module_peel_slot_count = 0;

    void register_builtin_type_infos();
    TypeValueInfo get_type_value_info(TypeNode &node);
    std::string get_parameter_c_type(const ParameterNode &node);
    bool parameter_uses_pointer_argument(const ParameterNode &node);
    bool is_scalar_in_register_param(const ParameterNode &node);
    void write_dispatch_case(FunctionNode &node);

  public:
    CModuleSourceGenerator(std::ostream &out, const std::string &header_name)
        : out(out), header_name(header_name)
    {
    }

    void visit(InterfaceNode &node) override;
    void visit(AbiversionNode &node) override;
    void visit(StructNode &node) override;
    void visit(BitfieldNode &node) override;
    void visit(EnumNode &node) override;
    void visit(FunctionNode &node) override;
};

#endif  // __C_MODULE_SOURCE_GENERATOR_HH__
