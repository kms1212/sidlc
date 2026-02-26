#ifndef __AST_HH__
#define __AST_HH__

#include <vector>

struct InterfaceNode;
struct AnnotationNode;
struct GroupNode;
struct AbiversionNode;
struct StructNode;
struct BitfieldNode;
struct FunctionNode;
struct ParameterNode;
struct TypeNode;
struct StructFieldNode;
struct BitfieldFieldNode;
struct DeclarationNode;
struct StatementNode;
struct AssignmentStatementNode;
struct ExpressionStatementNode;
struct VariableDeclarationNode;
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
    virtual void visit(GroupNode &node) {};
    virtual void visit(AbiversionNode &node) {};
    virtual void visit(StructNode &node) {};
    virtual void visit(BitfieldNode &node) {};
    virtual void visit(FunctionNode &node) {};
    virtual void visit(ParameterNode &node) {};
    virtual void visit(TypeNode &node) {};
    virtual void visit(StructFieldNode &node) {};
    virtual void visit(BitfieldFieldNode &node) {};
    virtual void visit(DeclarationNode &node) {};
    virtual void visit(VariableDeclarationNode &node) {};
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

struct UnaryExpressionNode : public ExpressionNode {
    enum class Operator {
        NOT,
        NEG,
        REF,
        DEREF,
        SIZEOF,
    };

    Operator op;
    std::unique_ptr<ExpressionNode> expr;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct BinaryExpressionNode : public ExpressionNode {
    enum class Operator {
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
        AND,
        OR,
        XOR,
        SHL,
        SHR,
        EQ,
        NE,
        LT,
        LE,
        GT,
        GE,
        LOGIC_AND,
        LOGIC_OR,
    };

    Operator op;
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct FunctionCallExpressionNode : public ExpressionNode {
    std::unique_ptr<IdentifierExpressionNode> func;
    std::vector<std::unique_ptr<ExpressionNode>> args;

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
    std::unique_ptr<TypeNode> ptr_type;
    bool is_ptr;
    bool is_const;

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

struct DeclarationNode : public AstNode {
    std::unique_ptr<TypeNode> type;
    std::string_view name;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct StatementNode : public AstNode {
    virtual ~StatementNode() = default;
};

struct VariableDeclarationNode : public StatementNode {
    std::unique_ptr<TypeNode> type;
    std::string_view name;
    std::unique_ptr<ExpressionNode> init;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct FunctionNode : public AstNode {
    std::string_view name;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<ParameterNode>> parameters;

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

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct AbiversionNode : public AstNode {
    uint64_t version;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<FunctionNode>> functions;
    std::vector<std::unique_ptr<StructNode>> structs;
    std::vector<std::unique_ptr<BitfieldNode>> bitfields;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct GroupNode : public AstNode {
    std::string_view name;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<AbiversionNode>> abiversions;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

struct InterfaceNode : public AstNode {
    std::string_view name;
    std::vector<std::unique_ptr<AnnotationNode>> annotations;
    std::vector<std::unique_ptr<GroupNode>> groups;

    void accept(AstVisitor &visitor) override
    {
        visitor.visit(*this);
    }
};

#endif  // __AST_HH__
