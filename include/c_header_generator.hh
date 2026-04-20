#ifndef __C_HEADER_GENERATOR_HH__
#define __C_HEADER_GENERATOR_HH__

#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include <ast.hh>

class CHeaderGenerator : public AstVisitor {
  public:
    enum class Mode {
        TYPE,
        USER,
        MODULE,
    };

  private:
    std::ostream &out;
    Mode mode;
    std::string dependency_header_name;
    std::string prefix;
    std::string macro_prefix;
    std::string macro_interface_name;
    std::stringstream buf_macros;
    std::stringstream buf_types;
    std::stringstream buf_functions;
    std::stringstream buf_module_vtable_fields;
    uint32_t total_funcid_span;
    size_t abi_revision_count;

    std::string get_parameter_c_type(const ParameterNode &node);
    void write_parameter_declaration(std::ostream &stream, const ParameterNode &node);

  public:
    CHeaderGenerator(std::ostream &out, Mode mode, std::string dependency_header_name = {})
        : out(out), mode(mode), dependency_header_name(std::move(dependency_header_name))
    {
    }

    void visit(InterfaceNode &node) override;
    void visit(AbiversionNode &node) override;
    void visit(StructNode &node) override;
    void visit(BitfieldNode &node) override;
    void visit(EnumNode &node) override;
    void visit(FunctionNode &node) override;
};

#endif  // __C_HEADER_GENERATOR_HH__
