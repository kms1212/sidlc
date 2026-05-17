#ifndef __AST_HH__
#define __AST_HH__

#include <array>
#include <memory>
#include <string>
#include <vector>

struct InterfaceNode;
struct AnnotationNode;
struct AbiversionNode;
struct StructNode;
struct BitfieldNode;
struct EnumNode;
struct FunctionNode;
struct ParameterNode;
struct TypeNode;
struct StructFieldNode;
struct EnumMemberNode;
struct BitfieldFieldNode;
struct ExpressionNode;
struct LiteralExpressionNode;
struct StringLiteralExpressionNode;
struct NumberLiteralExpressionNode;
struct IdentifierExpressionNode;

class AstVisitor {
protected:
    std::string to_c_type(std::string &prefix, TypeNode &node);

public:
    virtual ~AstVisitor() = default;

    virtual void visit(InterfaceNode &node) {};
    virtual void visit(AnnotationNode &node) {};
    virtual void visit(AbiversionNode &node) {};
    virtual void visit(StructNode &node) {};
    virtual void visit(BitfieldNode &node) {};
    virtual void visit(EnumNode &node) {};
    virtual void visit(FunctionNode &node) {};
    virtual void visit(ParameterNode &node) {};
    virtual void visit(TypeNode &node) {};
    virtual void visit(StructFieldNode &node) {};
    virtual void visit(EnumMemberNode &node) {};
    virtual void visit(BitfieldFieldNode &node) {};
    virtual void visit(ExpressionNode &node) {};
    virtual void visit(LiteralExpressionNode &node) {};
    virtual void visit(StringLiteralExpressionNode &node) {};
    virtual void visit(NumberLiteralExpressionNode &node) {};
    virtual void visit(IdentifierExpressionNode &node) {};
};

struct AstNode {
    virtual ~AstNode() = default;
    virtual void accept(AstVisitor &visitor) = 0;
};

struct ExpressionNode : public AstNode {
    virtual ~ExpressionNode() = default;
};

struct LiteralExpressionNode : public ExpressionNode {
    virtual ~LiteralExpressionNode() = default;
};

struct StringLiteralExpressionNode : public LiteralExpressionNode {
    std::string_view value;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct NumberLiteralExpressionNode : public LiteralExpressionNode {
    uint64_t value;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct IdentifierExpressionNode : public ExpressionNode {
    std::string_view name;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct AnnotationNode : public AstNode {
    std::string_view name;
    std::vector<std::unique_ptr<ExpressionNode>> args;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct TypeNode : public AstNode {
    std::string_view name;
    std::unique_ptr<TypeNode> inner_type;
    bool is_ptr = false;
    bool is_array = false;
    bool is_const = false;
    size_t type_size = 0;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct ParameterNode : public AstNode {
    enum class Direction {
        IN,
        OUT,
        INOUT,
    };

    Direction direction;
    std::unique_ptr<TypeNode> type;
    std::string_view name;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct FunctionNode : public AstNode {
    std::string_view name;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<ParameterNode>> parameters;
    AbiversionNode &abiversion;
    uint32_t id;

    FunctionNode(AbiversionNode &abiversion) : abiversion(abiversion) {}

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct EnumMemberNode : public AstNode {
    std::string_view name;
    uint64_t value;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct EnumNode : public AstNode {
    std::string_view name;
    std::unique_ptr<TypeNode> base_type;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<EnumMemberNode>> members;
    AbiversionNode &abiversion;

    EnumNode(AbiversionNode &abiversion) : abiversion(abiversion) {}

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct StructFieldNode : public AstNode {
    std::unique_ptr<TypeNode> type;
    std::string_view name;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct StructNode : public AstNode {
    std::string_view name;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<StructFieldNode>> fields;
    AbiversionNode &abiversion;

    StructNode(AbiversionNode &abiversion) : abiversion(abiversion) {}

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct BitfieldFieldNode : public AstNode {
    std::string_view name;
    uint64_t bits;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct BitfieldNode : public AstNode {
    std::string_view name;
    std::unique_ptr<TypeNode> base_type;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<BitfieldFieldNode>> fields;
    AbiversionNode &abiversion;

    BitfieldNode(AbiversionNode &abiversion) : abiversion(abiversion) {}

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct AbiversionNode : public AstNode {
    uint64_t version;
    std::array<uint8_t, 32> previous_hash = {};
    std::array<uint8_t, 32> abi_hash = {};
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<FunctionNode>> functions;
    std::vector<std::unique_ptr<StructNode>> structs;
    std::vector<std::unique_ptr<BitfieldNode>> bitfields;
    std::vector<std::unique_ptr<EnumNode>> enums;
    InterfaceNode &interface;

    AbiversionNode(InterfaceNode &interface) : interface(interface) {}

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct InterfaceNode : public AstNode {
    std::string_view name;
    std::vector<std::unique_ptr<std::string>> owned_strings;
    bool has_uuid = false;
    std::array<uint8_t, 16> uuid_namespace = {};
    std::string_view uuid_name;
    bool has_prefix = false;
    std::string_view prefix;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<AbiversionNode>> abiversions;
    uint32_t current_funcid;

    InterfaceNode() : current_funcid(0) {}

    std::string_view own_string(std::string value)
    {
        owned_strings.push_back(std::make_unique<std::string>(std::move(value)));
        return *owned_strings.back();
    }

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

#endif  // __AST_HH__
