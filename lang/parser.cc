#include <parser.hh>

#include <iostream>
#include <memory>

#include <arch_abi.hh>
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
        auto default_group = std::make_unique<GroupNode>(*node);
        default_group->name = "Default";
        default_group->id = node->current_groupid++;

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
                default_group->abiversions.push_back(parse_abiversion(*default_group));
                break;
            case Token::TYPE_KWD_GROUP: {
                auto group = parse_group(*node);
                group->id = node->current_groupid++;
                node->groups.push_back(std::move(group));
                break;
            }
            default:
                throw std::runtime_error(
                    "Unexpected token type " + std::to_string(current_token.type)
                );
            }
        }

        node->groups.insert(node->groups.begin(), std::move(default_group));

        consume(Token::Type('}'));

        return node;
    } catch (const std::runtime_error &e) {
        std::cerr << "Error: " << e.what() << " at " << current_token.line << ":"
                  << current_token.start_column << std::endl;
        return nullptr;
    }
}

std::unique_ptr<GroupNode> Parser::parse_group(InterfaceNode &interface)
{
    auto node = std::make_unique<GroupNode>(interface);

    consume(Token::TYPE_KWD_GROUP);

    expect(Token::TYPE_IDENTIFIER);
    node->name = current_token.text;
    advance();

    consume(Token::Type('{'));

    while (current_token.type != Token::Type('}')) {
        switch (current_token.type) {
        case Token::TYPE_KWD_ABIREVISION:
            node->abiversions.push_back(parse_abiversion(*node));
            break;
        default:
            throw std::runtime_error("Unexpected token type " + std::to_string(current_token.type));
        }
    }

    consume(Token::Type('}'));

    return node;
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

        node->inner_type = parse_type();

        consume(Token::Type('>'));

        node->type_size = g_current_arch_abi->pointer_size;
    } else if (current_token.type == Token::TYPE_KWD_ARRAY) {
        node->is_array = true;
        advance();

        consume(Token::Type('<'));

        node->inner_type = parse_type();

        consume(Token::Type('>'));

        node->type_size = 0;
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

std::unique_ptr<StructNode> Parser::parse_struct(AbiversionNode &abiversion)
{
    auto node = std::make_unique<StructNode>(abiversion);

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

std::unique_ptr<BitfieldNode> Parser::parse_bitfield(AbiversionNode &abiversion)
{
    auto node = std::make_unique<BitfieldNode>(abiversion);

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

std::unique_ptr<EnumNode> Parser::parse_enum(AbiversionNode &abiversion)
{
    auto node = std::make_unique<EnumNode>(abiversion);
    auto annotations = std::vector<std::unique_ptr<AnnotationNode>>();

    consume(Token::TYPE_KWD_ENUM);

    consume(Token::Type('<'));

    node->base_type = parse_type();

    consume(Token::Type('>'));

    expect(Token::TYPE_IDENTIFIER);
    node->name = current_token.text;
    advance();

    consume(Token::Type('{'));

    while (current_token.type != Token::Type('}')) {
        auto member = std::make_unique<EnumMemberNode>();

        while (current_token.type == '@') {
            annotations.push_back(parse_annotation());
        }

        expect(Token::TYPE_IDENTIFIER);
        member->name = current_token.text;
        advance();

        consume(Token::Type('='));

        expect(Token::TYPE_NUMBER);
        member->value = std::stoull(std::string(current_token.text));
        advance();

        member->annotations = std::move(annotations);
        node->members.push_back(std::move(member));

        if (current_token.type != Token::Type('}')) {
            consume(Token::Type(','));
        }
    }

    consume(Token::Type('}'));

    consume(Token::Type(';'));

    return node;
}

std::unique_ptr<FunctionNode> Parser::parse_function(AbiversionNode &abiversion)
{
    auto node = std::make_unique<FunctionNode>(abiversion);

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

    node->id = abiversion.group.current_funcid++;

    return node;
}

std::unique_ptr<AbiversionNode> Parser::parse_abiversion(GroupNode &group)
{
    auto node = std::make_unique<AbiversionNode>(group);
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
            auto func = parse_function(*node);
            func->annotations = std::move(annotations);
            node->functions.push_back(std::move(func));
            break;
        }
        case Token::TYPE_KWD_STRUCT: {
            auto strct = parse_struct(*node);
            strct->annotations = std::move(annotations);
            node->structs.push_back(std::move(strct));
            break;
        }
        case Token::TYPE_KWD_BITFIELD: {
            auto bitfield = parse_bitfield(*node);
            bitfield->annotations = std::move(annotations);
            node->bitfields.push_back(std::move(bitfield));
            break;
        }
        case Token::TYPE_KWD_ENUM: {
            auto enum_node = parse_enum(*node);
            enum_node->annotations = std::move(annotations);
            node->enums.push_back(std::move(enum_node));
            break;
        }
        default:
            throw std::runtime_error("Unexpected token type " + std::to_string(current_token.type));
        }
    }

    consume(Token::Type('}'));

    return node;
}
