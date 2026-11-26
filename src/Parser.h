#ifndef STOBX_PARSER_H
#define STOBX_PARSER_H

#include "Lexer.h"
#include "Diagnostic.h"
#include "DiagnosticEngine.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stobx {

struct SourceLocation {
    uint32_t line = 1;
    uint32_t column = 1;
    uint32_t offset = 0;
};

struct Span {
    SourceLocation start;
    SourceLocation end;

    static Span fromToken(const Token& tok, std::string_view tokenText = {}) noexcept {
        Span s;
        s.start = SourceLocation{tok.line, tok.column, tok.offset};
        uint32_t endLine = tok.line;
        uint32_t endCol = tok.column + tok.length;

        if (!tokenText.empty()) {
            const std::size_t scanLen = tokenText.size() < static_cast<std::size_t>(tok.length)
                ? tokenText.size()
                : static_cast<std::size_t>(tok.length);
            uint32_t colAfterNewline = 1u;
            bool sawNewline = false;
            for (std::size_t i = 0; i < scanLen; ++i) {
                if (tokenText[i] == '\n') {
                    ++endLine;
                    colAfterNewline = 1u;
                    sawNewline = true;
                } else if (sawNewline) {
                    ++colAfterNewline;
                }
            }
            if (sawNewline) {
                endCol = colAfterNewline;
            }
        }

        s.end = SourceLocation{
            endLine,
            endCol,
            tok.offset + tok.length
        };
        return s;
    }

    static Span merge(const Span& first, const Span& last) noexcept {
        return Span{first.start, last.end};
    }

    constexpr bool isValid() const noexcept {
        return start.offset <= end.offset;
    }

    constexpr bool contains(const Span& child) const noexcept {
        return start.offset <= child.start.offset && child.end.offset <= end.offset;
    }
};

struct TypeSpec {
    std::string name;
    bool isMutable = false;
    uint32_t pointerDepth = 0;
    bool hasArraySize = false;
    uint64_t arraySize = 0;
    std::unique_ptr<TypeSpec> elementType;
    std::vector<TypeSpec> genericArgs;
    std::vector<TypeSpec> fnParams;
    std::unique_ptr<TypeSpec> fnReturn;
    bool isFunctionType = false;

    TypeSpec() = default;
    explicit TypeSpec(std::string n) : name(std::move(n)) {}

    TypeSpec(const TypeSpec& other)
        : name(other.name),
          isMutable(other.isMutable),
          pointerDepth(other.pointerDepth),
          hasArraySize(other.hasArraySize),
          arraySize(other.arraySize),
          elementType(other.elementType ? std::make_unique<TypeSpec>(*other.elementType) : nullptr),
          genericArgs(other.genericArgs),
          fnParams(other.fnParams),
          fnReturn(other.fnReturn ? std::make_unique<TypeSpec>(*other.fnReturn) : nullptr),
          isFunctionType(other.isFunctionType) {}

    TypeSpec& operator=(const TypeSpec& other) {
        if (this != &other) {
            name = other.name;
            isMutable = other.isMutable;
            pointerDepth = other.pointerDepth;
            hasArraySize = other.hasArraySize;
            arraySize = other.arraySize;
            elementType = other.elementType ? std::make_unique<TypeSpec>(*other.elementType) : nullptr;
            genericArgs = other.genericArgs;
            fnParams = other.fnParams;
            fnReturn = other.fnReturn ? std::make_unique<TypeSpec>(*other.fnReturn) : nullptr;
            isFunctionType = other.isFunctionType;
        }
        return *this;
    }

    TypeSpec(TypeSpec&&) noexcept = default;
    TypeSpec& operator=(TypeSpec&&) noexcept = default;
};

class ASTVisitor;

class ASTNode {
public:
    Span span;

    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

class NumberASTNode : public ASTNode {
public:
    std::string value;
    TokenType literalKind = TokenType::NUMBER;

    explicit NumberASTNode(std::string value, TokenType kind = TokenType::NUMBER)
        : value(std::move(value)),
          literalKind(kind) {}

    void accept(ASTVisitor& visitor) override;
};

class StringASTNode : public ASTNode {
public:
    std::string value;

    explicit StringASTNode(std::string value)
        : value(std::move(value)) {}

    void accept(ASTVisitor& visitor) override;
};

class CharLiteralASTNode : public ASTNode {
public:
    std::string value;

    explicit CharLiteralASTNode(std::string value)
        : value(std::move(value)) {}

    void accept(ASTVisitor& visitor) override;
};

class BoolLiteralASTNode : public ASTNode {
public:
    bool value;

    explicit BoolLiteralASTNode(bool value)
        : value(value) {}

    void accept(ASTVisitor& visitor) override;
};

class NullLiteralASTNode : public ASTNode {
public:
    NullLiteralASTNode() = default;

    void accept(ASTVisitor& visitor) override;
};

class VariableASTNode : public ASTNode {
public:
    std::string name;

    explicit VariableASTNode(std::string name)
        : name(std::move(name)) {}

    void accept(ASTVisitor& visitor) override;
};

class ScopeResolutionASTNode : public ASTNode {
public:
    std::vector<std::string> path;
    std::vector<TypeSpec> genericArgs;

    explicit ScopeResolutionASTNode(
        std::vector<std::string> path,
        std::vector<TypeSpec> genericArgs = {}
    )
        : path(std::move(path)),
          genericArgs(std::move(genericArgs)) {}

    void accept(ASTVisitor& visitor) override;
};

class FieldAccessASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> object;
    std::string field;

    FieldAccessASTNode(std::unique_ptr<ASTNode> object, std::string field)
        : object(std::move(object)),
          field(std::move(field)) {}

    void accept(ASTVisitor& visitor) override;
};

class IndexASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> target;
    std::unique_ptr<ASTNode> index;

    IndexASTNode(std::unique_ptr<ASTNode> target, std::unique_ptr<ASTNode> index)
        : target(std::move(target)),
          index(std::move(index)) {}

    void accept(ASTVisitor& visitor) override;
};

class TryASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> operand;

    explicit TryASTNode(std::unique_ptr<ASTNode> operand)
        : operand(std::move(operand)) {}

    void accept(ASTVisitor& visitor) override;
};

class FunctionCallASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> callee;
    std::string name;
    std::vector<TypeSpec> genericArgs;
    std::vector<std::unique_ptr<ASTNode>> arguments;

    FunctionCallASTNode(
        std::string name,
        std::vector<std::unique_ptr<ASTNode>> arguments,
        std::vector<TypeSpec> genericArgs = {},
        std::unique_ptr<ASTNode> callee = nullptr
    )
        : callee(std::move(callee)),
          name(std::move(name)),
          genericArgs(std::move(genericArgs)),
          arguments(std::move(arguments)) {}

    void accept(ASTVisitor& visitor) override;
};

class UnaryASTNode : public ASTNode {
public:
    std::string op;
    std::unique_ptr<ASTNode> operand;

    UnaryASTNode(std::string op, std::unique_ptr<ASTNode> operand)
        : op(std::move(op)),
          operand(std::move(operand)) {}

    void accept(ASTVisitor& visitor) override;
};

class CastASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expression;
    TypeSpec targetType;

    CastASTNode(std::unique_ptr<ASTNode> expression, TypeSpec targetType)
        : expression(std::move(expression)),
          targetType(std::move(targetType)) {}

    void accept(ASTVisitor& visitor) override;
};

class SizeOfASTNode : public ASTNode {
public:
    bool isTypeTarget = false;
    TypeSpec targetType;
    std::unique_ptr<ASTNode> targetExpr;

    explicit SizeOfASTNode(TypeSpec type)
        : isTypeTarget(true),
          targetType(std::move(type)) {}

    explicit SizeOfASTNode(std::unique_ptr<ASTNode> expr)
        : isTypeTarget(false),
          targetExpr(std::move(expr)) {}

    void accept(ASTVisitor& visitor) override;
};

class BinaryASTNode : public ASTNode {
public:
    std::string op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;

    BinaryASTNode(
        std::string op,
        std::unique_ptr<ASTNode> left,
        std::unique_ptr<ASTNode> right
    )
        : op(std::move(op)),
          left(std::move(left)),
          right(std::move(right)) {}

    void accept(ASTVisitor& visitor) override;
};

class PrintASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expression;

    explicit PrintASTNode(std::unique_ptr<ASTNode> expression)
        : expression(std::move(expression)) {}

    void accept(ASTVisitor& visitor) override;
};

class AssignmentASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> target;
    std::string name;
    std::string op;
    std::unique_ptr<ASTNode> value;

    AssignmentASTNode(
        std::string name,
        std::unique_ptr<ASTNode> value,
        std::string op = "=",
        std::unique_ptr<ASTNode> target = nullptr
    )
        : target(std::move(target)),
          name(std::move(name)),
          op(std::move(op)),
          value(std::move(value)) {}

    void accept(ASTVisitor& visitor) override;
};

class ProgramASTNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> statements;

    void accept(ASTVisitor& visitor) override;
};

class ModuleDeclASTNode : public ASTNode {
public:
    std::vector<std::string> path;

    explicit ModuleDeclASTNode(std::vector<std::string> path)
        : path(std::move(path)) {}

    void accept(ASTVisitor& visitor) override;
};

class ImportDeclASTNode : public ASTNode {
public:
    std::vector<std::string> path;

    explicit ImportDeclASTNode(std::vector<std::string> path)
        : path(std::move(path)) {}

    void accept(ASTVisitor& visitor) override;
};

struct StructFieldDecl {
    std::string name;
    TypeSpec type;
};

class StructDeclASTNode : public ASTNode {
public:
    std::string name;
    std::vector<std::string> genericParams;
    std::vector<StructFieldDecl> fields;

    StructDeclASTNode(
        std::string name,
        std::vector<std::string> genericParams,
        std::vector<StructFieldDecl> fields
    )
        : name(std::move(name)),
          genericParams(std::move(genericParams)),
          fields(std::move(fields)) {}

    void accept(ASTVisitor& visitor) override;
};

class UnionDeclASTNode : public ASTNode {
public:
    std::string name;
    std::vector<std::string> genericParams;
    std::vector<StructFieldDecl> fields;

    UnionDeclASTNode(
        std::string name,
        std::vector<std::string> genericParams,
        std::vector<StructFieldDecl> fields
    )
        : name(std::move(name)),
          genericParams(std::move(genericParams)),
          fields(std::move(fields)) {}

    void accept(ASTVisitor& visitor) override;
};

class EnumDeclASTNode : public ASTNode {
public:
    std::string name;
    std::vector<std::string> genericParams;
    std::vector<std::string> variants;

    EnumDeclASTNode(
        std::string name,
        std::vector<std::string> genericParams,
        std::vector<std::string> variants
    )
        : name(std::move(name)),
          genericParams(std::move(genericParams)),
          variants(std::move(variants)) {}

    void accept(ASTVisitor& visitor) override;
};

class PatternASTNode : public ASTNode {
public:
    ~PatternASTNode() override = default;
};

class WildcardPatternASTNode : public PatternASTNode {
public:
    WildcardPatternASTNode() = default;

    void accept(ASTVisitor& visitor) override;
};

class LiteralPatternASTNode : public PatternASTNode {
public:
    std::unique_ptr<ASTNode> literal;

    explicit LiteralPatternASTNode(std::unique_ptr<ASTNode> literal)
        : literal(std::move(literal)) {}

    void accept(ASTVisitor& visitor) override;
};

class IdentifierPatternASTNode : public PatternASTNode {
public:
    std::string name;
    bool isMutable = false;

    explicit IdentifierPatternASTNode(std::string name, bool isMutable = false)
        : name(std::move(name)),
          isMutable(isMutable) {}

    void accept(ASTVisitor& visitor) override;
};

class PathPatternASTNode : public PatternASTNode {
public:
    std::vector<std::string> path;

    explicit PathPatternASTNode(std::vector<std::string> path)
        : path(std::move(path)) {}

    void accept(ASTVisitor& visitor) override;
};

class MatchArmASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> pattern;
    bool hasBlockBody = false;
    std::unique_ptr<ASTNode> bodyExpr;
    std::vector<std::unique_ptr<ASTNode>> bodyStatements;

    MatchArmASTNode(
        std::unique_ptr<ASTNode> pattern,
        std::unique_ptr<ASTNode> bodyExpr
    )
        : pattern(std::move(pattern)),
          hasBlockBody(false),
          bodyExpr(std::move(bodyExpr)) {}

    MatchArmASTNode(
        std::unique_ptr<ASTNode> pattern,
        std::vector<std::unique_ptr<ASTNode>> bodyStatements
    )
        : pattern(std::move(pattern)),
          hasBlockBody(true),
          bodyStatements(std::move(bodyStatements)) {}

    void accept(ASTVisitor& visitor) override;
};

class MatchASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> scrutinee;
    std::vector<std::unique_ptr<MatchArmASTNode>> arms;

    MatchASTNode(
        std::unique_ptr<ASTNode> scrutinee,
        std::vector<std::unique_ptr<MatchArmASTNode>> arms
    )
        : scrutinee(std::move(scrutinee)),
          arms(std::move(arms)) {}

    void accept(ASTVisitor& visitor) override;
};

class IfASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::vector<std::unique_ptr<ASTNode>> statements;
    std::vector<std::unique_ptr<ASTNode>> elseStatements;

    IfASTNode(
        std::unique_ptr<ASTNode> condition,
        std::vector<std::unique_ptr<ASTNode>> statements,
        std::vector<std::unique_ptr<ASTNode>> elseStatements
    )
        : condition(std::move(condition)),
          statements(std::move(statements)),
          elseStatements(std::move(elseStatements)) {}

    void accept(ASTVisitor& visitor) override;
};

class WhileASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::vector<std::unique_ptr<ASTNode>> statements;

    WhileASTNode(
        std::unique_ptr<ASTNode> condition,
        std::vector<std::unique_ptr<ASTNode>> statements
    )
        : condition(std::move(condition)),
          statements(std::move(statements)) {}

    void accept(ASTVisitor& visitor) override;
};

class ForASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> initialization;
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> update;
    std::vector<std::unique_ptr<ASTNode>> statements;

    ForASTNode(
        std::unique_ptr<ASTNode> initialization,
        std::unique_ptr<ASTNode> condition,
        std::unique_ptr<ASTNode> update,
        std::vector<std::unique_ptr<ASTNode>> statements
    )
        : initialization(std::move(initialization)),
          condition(std::move(condition)),
          update(std::move(update)),
          statements(std::move(statements)) {}

    void accept(ASTVisitor& visitor) override;
};

class BreakASTNode : public ASTNode {
public:
    BreakASTNode() = default;

    void accept(ASTVisitor& visitor) override;
};

class ContinueASTNode : public ASTNode {
public:
    ContinueASTNode() = default;

    void accept(ASTVisitor& visitor) override;
};

class ParameterASTNode : public ASTNode {
public:
    std::string name;
    TypeSpec typeSpec;
    std::string type;
    bool isSelf = false;
    bool isMutable = false;

    ParameterASTNode(std::string name, TypeSpec typeSpec, bool isSelf = false, bool isMutable = false)
        : name(std::move(name)),
          typeSpec(std::move(typeSpec)),
          type(this->typeSpec.name),
          isSelf(isSelf),
          isMutable(isMutable) {}

    void accept(ASTVisitor& visitor) override;
};

class FunctionASTNode : public ASTNode {
public:
    std::string name;
    std::vector<std::string> genericParams;
    TypeSpec returnTypeSpec;
    std::string returnType;
    bool isExtern = false;

    std::vector<std::unique_ptr<ParameterASTNode>> parameters;
    std::vector<std::unique_ptr<ASTNode>> statements;

    FunctionASTNode(
        std::string name,
        TypeSpec returnTypeSpec,
        std::vector<std::unique_ptr<ParameterASTNode>> parameters,
        std::vector<std::unique_ptr<ASTNode>> statements,
        std::vector<std::string> genericParams = {},
        bool isExtern = false
    )
        : name(std::move(name)),
          genericParams(std::move(genericParams)),
          returnTypeSpec(std::move(returnTypeSpec)),
          returnType(this->returnTypeSpec.name),
          isExtern(isExtern),
          parameters(std::move(parameters)),
          statements(std::move(statements)) {}

    void accept(ASTVisitor& visitor) override;
};

class ImplBlockASTNode : public ASTNode {
public:
    std::string targetType;
    std::vector<std::string> genericParams;
    std::vector<std::unique_ptr<FunctionASTNode>> methods;

    ImplBlockASTNode(
        std::string targetType,
        std::vector<std::string> genericParams,
        std::vector<std::unique_ptr<FunctionASTNode>> methods
    )
        : targetType(std::move(targetType)),
          genericParams(std::move(genericParams)),
          methods(std::move(methods)) {}

    void accept(ASTVisitor& visitor) override;
};

class VariableDeclarationASTNode : public ASTNode {
public:
    std::string name;
    TypeSpec typeSpec;
    std::string type;
    std::unique_ptr<ASTNode> value;
    bool isMutable = false;
    bool isConst = false;
    bool isStatic = false;

    VariableDeclarationASTNode(
        std::string name,
        TypeSpec typeSpec,
        std::unique_ptr<ASTNode> value,
        bool isMutable = false,
        bool isConst = false,
        bool isStatic = false
    )
        : name(std::move(name)),
          typeSpec(std::move(typeSpec)),
          type(this->typeSpec.name),
          value(std::move(value)),
          isMutable(isMutable),
          isConst(isConst),
          isStatic(isStatic) {}

    void accept(ASTVisitor& visitor) override;
};

class ReturnASTNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> value;

    explicit ReturnASTNode(std::unique_ptr<ASTNode> value)
        : value(std::move(value)) {}

    void accept(ASTVisitor& visitor) override;
};

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(NumberASTNode& node) = 0;
    virtual void visit(StringASTNode& node) = 0;
    virtual void visit(CharLiteralASTNode& node) = 0;
    virtual void visit(BoolLiteralASTNode& node) = 0;
    virtual void visit(NullLiteralASTNode& node) = 0;
    virtual void visit(VariableASTNode& node) = 0;
    virtual void visit(ScopeResolutionASTNode& node) = 0;
    virtual void visit(FieldAccessASTNode& node) = 0;
    virtual void visit(IndexASTNode& node) = 0;
    virtual void visit(TryASTNode& node) = 0;
    virtual void visit(FunctionCallASTNode& node) = 0;
    virtual void visit(UnaryASTNode& node) = 0;
    virtual void visit(CastASTNode& node) = 0;
    virtual void visit(SizeOfASTNode& node) = 0;
    virtual void visit(BinaryASTNode& node) = 0;
    virtual void visit(PrintASTNode& node) = 0;
    virtual void visit(AssignmentASTNode& node) = 0;
    virtual void visit(ProgramASTNode& node) = 0;
    virtual void visit(ModuleDeclASTNode& node) = 0;
    virtual void visit(ImportDeclASTNode& node) = 0;
    virtual void visit(StructDeclASTNode& node) = 0;
    virtual void visit(UnionDeclASTNode& node) = 0;
    virtual void visit(EnumDeclASTNode& node) = 0;
    virtual void visit(WildcardPatternASTNode& node) = 0;
    virtual void visit(LiteralPatternASTNode& node) = 0;
    virtual void visit(IdentifierPatternASTNode& node) = 0;
    virtual void visit(PathPatternASTNode& node) = 0;
    virtual void visit(MatchArmASTNode& node) = 0;
    virtual void visit(MatchASTNode& node) = 0;
    virtual void visit(IfASTNode& node) = 0;
    virtual void visit(WhileASTNode& node) = 0;
    virtual void visit(ForASTNode& node) = 0;
    virtual void visit(BreakASTNode& node) = 0;
    virtual void visit(ContinueASTNode& node) = 0;
    virtual void visit(ParameterASTNode& node) = 0;
    virtual void visit(FunctionASTNode& node) = 0;
    virtual void visit(ImplBlockASTNode& node) = 0;
    virtual void visit(VariableDeclarationASTNode& node) = 0;
    virtual void visit(ReturnASTNode& node) = 0;
};

class SpanValidatorVisitor : public ASTVisitor {
public:
    std::vector<std::string> issues;

    bool validate(ASTNode& root);

    void visit(NumberASTNode& node) override;
    void visit(StringASTNode& node) override;
    void visit(CharLiteralASTNode& node) override;
    void visit(BoolLiteralASTNode& node) override;
    void visit(NullLiteralASTNode& node) override;
    void visit(VariableASTNode& node) override;
    void visit(ScopeResolutionASTNode& node) override;
    void visit(FieldAccessASTNode& node) override;
    void visit(IndexASTNode& node) override;
    void visit(TryASTNode& node) override;
    void visit(FunctionCallASTNode& node) override;
    void visit(UnaryASTNode& node) override;
    void visit(CastASTNode& node) override;
    void visit(SizeOfASTNode& node) override;
    void visit(BinaryASTNode& node) override;
    void visit(PrintASTNode& node) override;
    void visit(AssignmentASTNode& node) override;
    void visit(ProgramASTNode& node) override;
    void visit(ModuleDeclASTNode& node) override;
    void visit(ImportDeclASTNode& node) override;
    void visit(StructDeclASTNode& node) override;
    void visit(UnionDeclASTNode& node) override;
    void visit(EnumDeclASTNode& node) override;
    void visit(WildcardPatternASTNode& node) override;
    void visit(LiteralPatternASTNode& node) override;
    void visit(IdentifierPatternASTNode& node) override;
    void visit(PathPatternASTNode& node) override;
    void visit(MatchArmASTNode& node) override;
    void visit(MatchASTNode& node) override;
    void visit(IfASTNode& node) override;
    void visit(WhileASTNode& node) override;
    void visit(ForASTNode& node) override;
    void visit(BreakASTNode& node) override;
    void visit(ContinueASTNode& node) override;
    void visit(ParameterASTNode& node) override;
    void visit(FunctionASTNode& node) override;
    void visit(ImplBlockASTNode& node) override;
    void visit(VariableDeclarationASTNode& node) override;
    void visit(ReturnASTNode& node) override;

private:
    void checkNode(const ASTNode& node, const char* kind);
    void checkChild(const ASTNode& parent, const ASTNode* child, const char* relation);
};

class Parser {
public:
    Parser(std::vector<Token> tokens, std::string_view source);

    std::unique_ptr<ProgramASTNode> parse();

    const std::vector<std::string>& getErrors() const noexcept;
    const std::vector<Diagnostic>& getDiagnostics() const noexcept;

private:
    std::vector<Token> tokens;
    std::string_view source;
    std::size_t cursor = 0;

    DiagnosticEngine diagnosticEngine;
    std::vector<std::string> errors;
    std::vector<Diagnostic> diagnostics;

    static constexpr bool isPrimitiveTypeToken(TokenType type) noexcept {
        return isPrimitiveType(type);
    }

    static constexpr bool isCompoundAssignToken(TokenType type) noexcept {
        return type == TokenType::PLUS_EQUAL ||
               type == TokenType::MINUS_EQUAL ||
               type == TokenType::MULTIPLY_EQUAL ||
               type == TokenType::DIVIDE_EQUAL ||
               type == TokenType::MODULO_EQUAL ||
               type == TokenType::SHIFT_LEFT_EQUAL ||
               type == TokenType::SHIFT_RIGHT_EQUAL ||
               type == TokenType::AMPERSAND_EQUAL ||
               type == TokenType::PIPE_EQUAL ||
               type == TokenType::CARET_EQUAL;
    }

    static constexpr bool isTypeArgAllowedToken(TokenType type) noexcept {
        return isPrimitiveType(type) ||
               type == TokenType::IDENTIFIER ||
               type == TokenType::COMMA ||
               type == TokenType::MUT ||
               type == TokenType::MULTIPLY ||
               type == TokenType::LBRACKET ||
               type == TokenType::RBRACKET ||
               type == TokenType::SEMICOLON ||
               type == TokenType::NUMBER ||
               type == TokenType::HEX_LITERAL ||
               type == TokenType::BINARY_LITERAL ||
               type == TokenType::FN ||
               type == TokenType::LPAREN ||
               type == TokenType::RPAREN ||
               type == TokenType::ARROW ||
               type == TokenType::SCOPE_RESOLUTION ||
               type == TokenType::LESS ||
               type == TokenType::GREATER ||
               type == TokenType::SHIFT_RIGHT;
    }

    static constexpr int getBinaryPrecedence(TokenType type) noexcept {
        switch (type) {
            case TokenType::OR:
                return 1;
            case TokenType::AND:
                return 2;
            case TokenType::PIPE:
                return 3;
            case TokenType::CARET:
                return 4;
            case TokenType::AMPERSAND:
                return 5;
            case TokenType::EQUAL_EQUAL:
            case TokenType::NOT_EQUAL:
                return 6;
            case TokenType::GREATER:
            case TokenType::LESS:
            case TokenType::GREATER_EQUAL:
            case TokenType::LESS_EQUAL:
                return 7;
            case TokenType::SHIFT_LEFT:
            case TokenType::SHIFT_RIGHT:
                return 8;
            case TokenType::PLUS:
            case TokenType::MINUS:
                return 9;
            case TokenType::MULTIPLY:
            case TokenType::DIVIDE:
            case TokenType::MODULO:
                return 10;
            default:
                return 0;
        }
    }

    std::string_view lexeme(const Token& tok) const noexcept {
        return tok.text(source);
    }

    Span tokenSpan(const Token& tok) const noexcept {
        return Span::fromToken(tok, lexeme(tok));
    }

    Token peek(std::size_t lookahead = 0) const noexcept;
    Token previousToken() const noexcept;
    Token advance() noexcept;
    bool match(TokenType type) noexcept;
    Token consume(TokenType type, const std::string& errorMessage);
    void consumeExpectedSemicolon(const char* contextMessage, const Span* anchorSpan = nullptr);
    bool consumeGenericClose();
    bool canStartGenericArgs() const noexcept;

    void consumeClosingParenthesis(const char* contextMessage = nullptr);
    void consumeOpeningParenthesis();

    void addError(uint32_t line, const std::string& message);
    void addDiagnostic(int id, uint32_t line, uint32_t column, uint32_t offset);

    void synchronize() noexcept;
    void skipUntil(TokenType stop) noexcept;
    void skipUntil(TokenType stopA, TokenType stopB) noexcept;
    void recoverToStatementEnd() noexcept;

    std::vector<std::string> parseGenericParams();
    std::vector<TypeSpec> parseGenericArgs();
    TypeSpec parseTypeSpec();
    std::vector<std::string> parseScopedPath();
    std::unique_ptr<PatternASTNode> parsePattern();

    std::vector<std::unique_ptr<ASTNode>> parseBlock(const char* missingCloseMsg, Span* outSpan = nullptr);
    std::unique_ptr<FunctionCallASTNode> parseFunctionCall(
        std::string name,
        std::vector<TypeSpec> genericArgs = {},
        std::unique_ptr<ASTNode> callee = nullptr
    );

    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseAssignment();
    std::unique_ptr<ASTNode> parseBinaryExpression(int minPrecedence = 1);
    std::unique_ptr<ASTNode> parseCast();
    std::unique_ptr<ASTNode> parseUnary();
    std::unique_ptr<ASTNode> parseNot();
    std::unique_ptr<ASTNode> parsePostfix();
    std::unique_ptr<ASTNode> parsePrimary();

    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseModuleDecl();
    std::unique_ptr<ASTNode> parseImportDecl();
    std::unique_ptr<ASTNode> parseStructDecl();
    std::unique_ptr<ASTNode> parseUnionDecl();
    std::unique_ptr<ASTNode> parseEnumDecl();
    std::unique_ptr<ASTNode> parseImplBlock();
    std::unique_ptr<ASTNode> parseExternFunction();
    std::unique_ptr<ASTNode> parseMatchStatement();
    std::unique_ptr<ASTNode> parseIfStatement();
    std::unique_ptr<ASTNode> parsePrintStatement();
    std::unique_ptr<ASTNode> parseIdentifierStatement();
    std::unique_ptr<FunctionASTNode> parseFunction(bool isMethod = false);
    std::unique_ptr<ASTNode> parseVariableDeclaration(bool isConst = false, bool isStatic = false);
    std::unique_ptr<ASTNode> parseReturn();
    std::unique_ptr<ASTNode> parseWhile();
    std::unique_ptr<ASTNode> parseFor();
    std::unique_ptr<ASTNode> parseBreak();
    std::unique_ptr<ASTNode> parseContinue();
};

} // namespace stobx

#endif

