#ifndef __C_HEADER_GENERATOR_HH__
#define __C_HEADER_GENERATOR_HH__

#include <iostream>
#include <sstream>
#include <string>

#include <ast.hh>

class CHeaderGenerator : public AstVisitor {
    std::ostream &out;
    std::string prefix;
    std::string macro_prefix;
    std::string macro_interface_name;
    std::stringstream buf_macros;
    std::stringstream buf_types;
    std::stringstream buf_functions;

  public:
    CHeaderGenerator(std::ostream &out) : out(out) {}

    void visit(InterfaceNode &node) override;
    void visit(GroupNode &node) override;
    void visit(AbiversionNode &node) override;
    void visit(StructNode &node) override;
    void visit(BitfieldNode &node) override;
    void visit(FunctionNode &node) override;
};

#endif  // __C_HEADER_GENERATOR_HH__
