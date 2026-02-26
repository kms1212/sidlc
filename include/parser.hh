#ifndef __PARSER_HH__
#define __PARSER_HH__

#include <ast.hh>
#include <lexer.hh>

class Parser {
  private:
    Lexer lexer;
    Token current_token;

    void advance();
    void expect(Token::Type token_type);
    void consume(Token::Type token_type);

    std::unique_ptr<IdentifierExpressionNode> parse_identifier_expression();
    std::unique_ptr<LiteralExpressionNode> parse_literal_expression();
    std::unique_ptr<AnnotationNode> parse_annotation();
    std::unique_ptr<AbiversionNode> parse_abiversion();
    std::unique_ptr<FunctionNode> parse_function();
    std::unique_ptr<StructNode> parse_struct();
    std::unique_ptr<BitfieldNode> parse_bitfield();
    std::unique_ptr<TypeNode> parse_type();
    std::unique_ptr<ParameterNode> parse_parameter();

  public:
    Parser(std::string_view source) : lexer(source) {}

    std::unique_ptr<InterfaceNode> parse();
};

#endif  // __PARSER_HH__
