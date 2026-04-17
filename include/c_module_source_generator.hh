#ifndef __C_MODULE_SOURCE_GENERATOR_HH__
#define __C_MODULE_SOURCE_GENERATOR_HH__

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <ast.hh>

class CModuleSourceGenerator : public AstVisitor {
    std::ostream &out;
    std::string prefix;
    std::string macro_interface_name;
    std::string header_name;
    std::stringstream buf_macros;
    std::stringstream buf_function_decls;
    std::stringstream buf_function_defs;
    std::stringstream buf_function_table;
    uint32_t total_funcid_span = 0;
    std::vector<uint32_t> abi_spans;

  public:
    CModuleSourceGenerator(std::ostream &out, const std::string &header_name)
        : out(out), header_name(header_name)
    {
    }

    void visit(InterfaceNode &node) override;
    void visit(AbiversionNode &node) override;
    void visit(FunctionNode &node) override;
};

#endif  // __C_MODULE_SOURCE_GENERATOR_HH__
