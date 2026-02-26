#include <parser.hh>

#include <iostream>
#include <memory>

#include <ast.hh>

void Parser::advance()
{
    current_token = lexer.next_token();
}

void Parser::expect(Token::Type token_type)
{
    if (current_token.type != token_type) {
        throw std::runtime_error("Expected token type " + std::to_string(token_type));
    }
}

void Parser::consume(Token::Type token_type)
{
    expect(token_type);
    advance();
}

std::unique_ptr<InterfaceNode> Parser::parse()
{
    try {
        auto node = std::make_unique<InterfaceNode>();
        auto default_group = std::make_unique<GroupNode>();
        default_group->name = "Default";

        advance();

        while (current_token.type == '@') {
            node->annotations.push_back(parse_annotation());
        }

        consume(Token::TYPE_KWD_INTERFACE);

        expect(Token::TYPE_IDENTIFIER);
        node->name = current_token.text;
        advance();

        consume(Token::Type('{'));

        while (current_token.type != Token::Type('}')) {
            switch (current_token.type) {
            case Token::TYPE_KWD_ABIREVISION:
                default_group->abiversions.push_back(parse_abiversion());
                break;
            default:
                throw std::runtime_error(
                    "Unexpected token type " + std::to_string(current_token.type)
                );
            }
        }

        node->groups.push_back(std::move(default_group));

        consume(Token::Type('}'));

        return node;
    } catch (const std::runtime_error &e) {
        std::cerr << "Error: " << e.what() << " at " << current_token.line << ":"
                  << current_token.start_column << std::endl;
        return nullptr;
    }
}

std::unique_ptr<IdentifierExpressionNode> Parser::parse_identifier_expression()
{
    auto node = std::make_unique<IdentifierExpressionNode>();

    expect(Token::TYPE_IDENTIFIER);
    node->name = current_token.text;
    advance();

    return node;
}

std::unique_ptr<LiteralExpressionNode> Parser::parse_literal_expression()
{
    switch (current_token.type) {
    case Token::TYPE_STRING: {
        auto str_arg = std::make_unique<StringLiteralExpressionNode>();
        str_arg->value = current_token.text;
        advance();
        return std::move(str_arg);
    }
    case Token::TYPE_NUMBER: {
        auto num_arg = std::make_unique<NumberLiteralExpressionNode>();
        num_arg->value = std::stoull(std::string(current_token.text));
        advance();
        return std::move(num_arg);
    }
    default:
        throw std::runtime_error("Unexpected token type " + std::to_string(current_token.type));
    }
}

std::unique_ptr<AnnotationNode> Parser::parse_annotation()
{
    auto node = std::make_unique<AnnotationNode>();

    consume(Token::Type('@'));

    expect(Token::TYPE_IDENTIFIER);
    node->name = current_token.text;
    advance();

    consume(Token::Type('('));

    while (current_token.type != Token::Type(')')) {
        if (current_token.type == Token::TYPE_IDENTIFIER) {
            node->args.push_back(parse_identifier_expression());
        } else {
            node->args.push_back(parse_literal_expression());
        }

        if (current_token.type != Token::Type(')')) {
            consume(Token::Type(','));
        }
    }

    consume(Token::Type(')'));

    return node;
}

std::unique_ptr<TypeNode> Parser::parse_type()
{
    auto node = std::make_unique<TypeNode>();

    if (current_token.type == Token::TYPE_KWD_CONST) {
        node->is_const = true;
        advance();
    }

    if (current_token.type == Token::TYPE_KWD_PTR) {
        node->is_ptr = true;
        advance();

        consume(Token::Type('<'));

        node->ptr_type = parse_type();

        consume(Token::Type('>'));
    } else {
        expect(Token::TYPE_IDENTIFIER);
        node->name = current_token.text;
        advance();
    }

    return node;
}

std::unique_ptr<ParameterNode> Parser::parse_parameter()
{
    auto node = std::make_unique<ParameterNode>();

    while (current_token.type == '@') {
        node->annotations.push_back(parse_annotation());
    }

    switch (current_token.type) {
    case Token::TYPE_KWD_INOUT:
        node->direction = ParameterNode::Direction::INOUT;
        advance();
        break;
    case Token::TYPE_KWD_IN:
        node->direction = ParameterNode::Direction::IN;
        advance();
        break;
    case Token::TYPE_KWD_OUT:
        node->direction = ParameterNode::Direction::OUT;
        advance();
        break;
    default:
        throw std::runtime_error("Unexpected token type " + std::to_string(current_token.type));
    }

    node->type = parse_type();

    expect(Token::TYPE_IDENTIFIER);
    node->name = current_token.text;
    advance();

    return node;
}

std::unique_ptr<StructNode> Parser::parse_struct()
{
    auto node = std::make_unique<StructNode>();

    consume(Token::TYPE_KWD_STRUCT);

    expect(Token::TYPE_IDENTIFIER);
    node->name = current_token.text;
    advance();

    consume(Token::Type('{'));

    while (current_token.type != Token::Type('}')) {
        auto field = std::make_unique<StructFieldNode>();

        field->type = parse_type();

        expect(Token::TYPE_IDENTIFIER);
        field->name = current_token.text;
        advance();

        node->fields.push_back(std::move(field));

        consume(Token::Type(';'));
    }

    consume(Token::Type('}'));

    consume(Token::Type(';'));

    return node;
}

std::unique_ptr<BitfieldNode> Parser::parse_bitfield()
{
    auto node = std::make_unique<BitfieldNode>();

    consume(Token::TYPE_KWD_BITFIELD);

    consume(Token::Type('<'));

    node->base_type = parse_type();

    consume(Token::Type('>'));

    expect(Token::TYPE_IDENTIFIER);
    node->name = current_token.text;
    advance();

    consume(Token::Type('{'));

    while (current_token.type != Token::Type('}')) {
        auto field = std::make_unique<BitfieldFieldNode>();

        expect(Token::TYPE_IDENTIFIER);
        field->name = current_token.text;
        advance();

        consume(Token::Type(':'));

        expect(Token::TYPE_NUMBER);
        field->bits = std::stoull(std::string(current_token.text));
        advance();

        node->fields.push_back(std::move(field));

        consume(Token::Type(';'));
    }

    consume(Token::Type('}'));

    consume(Token::Type(';'));

    return node;
}

std::unique_ptr<FunctionNode> Parser::parse_function()
{
    auto node = std::make_unique<FunctionNode>();

    consume(Token::TYPE_KWD_FUNCTION);

    expect(Token::TYPE_IDENTIFIER);
    node->name = current_token.text;
    advance();

    consume(Token::Type('('));

    while (current_token.type != Token::Type(')')) {
        node->parameters.push_back(parse_parameter());

        if (current_token.type != Token::Type(')')) {
            consume(Token::Type(','));
        }
    }

    consume(Token::Type(')'));

    consume(Token::Type(';'));

    return node;
}

std::unique_ptr<AbiversionNode> Parser::parse_abiversion()
{
    auto node = std::make_unique<AbiversionNode>();
    auto annotations = std::vector<std::unique_ptr<AnnotationNode>>();

    consume(Token::TYPE_KWD_ABIREVISION);

    expect(Token::TYPE_NUMBER);
    node->version = std::stoull(std::string(current_token.text));
    advance();

    consume(Token::Type('{'));

    while (current_token.type != Token::Type('}')) {
        if (current_token.type == '@') {
            annotations.push_back(parse_annotation());
            continue;
        }

        switch (current_token.type) {
        case Token::TYPE_KWD_FUNCTION: {
            auto func = parse_function();
            func->annotations = std::move(annotations);
            node->functions.push_back(std::move(func));
            break;
        }
        case Token::TYPE_KWD_STRUCT: {
            auto strct = parse_struct();
            strct->annotations = std::move(annotations);
            node->structs.push_back(std::move(strct));
            break;
        }
        case Token::TYPE_KWD_BITFIELD: {
            auto bitfield = parse_bitfield();
            bitfield->annotations = std::move(annotations);
            node->bitfields.push_back(std::move(bitfield));
            break;
        }
        default:
            throw std::runtime_error("Unexpected token type " + std::to_string(current_token.type));
        }
    }

    consume(Token::Type('}'));

    return node;
}
