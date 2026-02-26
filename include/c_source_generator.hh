#ifndef __C_SOURCE_GENERATOR_HH__
#define __C_SOURCE_GENERATOR_HH__

#include <iostream>
#include <sstream>
#include <string>

#include <ast.hh>

class CSourceGenerator : public AstVisitor {
    std::ostream &out;
    std::string prefix;
    std::string header_name;
    std::stringstream buf_macros;
    std::stringstream buf_functions;
    size_t current_funcid;

  public:
    CSourceGenerator(std::ostream &out, const std::string &header_name)
        : out(out), header_name(header_name), current_funcid(0)
    {
    }

    void visit(InterfaceNode &node) override;
    void visit(GroupNode &node) override;
    void visit(AbiversionNode &node) override;
    void visit(FunctionNode &node) override;
};

#endif  // __C_SOURCE_GENERATOR_HH__
