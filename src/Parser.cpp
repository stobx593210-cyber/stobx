#include "Parser.h"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace stobx {

namespace {

bool extractAssignableTargetName(const ASTNode* node, std::string& outName) noexcept {
    if (!node) {
        return false;
    }
    if (const auto* v = dynamic_cast<const VariableASTNode*>(node)) {
        outName = v->name;
        return true;
    }
    if (const auto* f = dynamic_cast<const FieldAccessASTNode*>(node)) {
        outName = f->field;
        return true;
    }
    if (dynamic_cast<const IndexASTNode*>(node)) {
        outName.clear();
        return true;
    }
    if (const auto* s = dynamic_cast<const ScopeResolutionASTNode*>(node)) {
        if (!s->path.empty()) {
            outName = s->path.back();
        }
        return true;
    }
    if (const auto* un = dynamic_cast<const UnaryASTNode*>(node)) {
        if (un->op == "*") {
            outName.clear();
            return true;
        }
    }
    return false;
}

} // namespace

void NumberASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void StringASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CharLiteralASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BoolLiteralASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void NullLiteralASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VariableASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ScopeResolutionASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FieldAccessASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IndexASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void TryASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FunctionCallASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void UnaryASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CastASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void SizeOfASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BinaryASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void PrintASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void AssignmentASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ProgramASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ModuleDeclASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ImportDeclASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void StructDeclASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void UnionDeclASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void EnumDeclASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void WildcardPatternASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void LiteralPatternASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IdentifierPatternASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void PathPatternASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void MatchArmASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void MatchASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IfASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void WhileASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ForASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BreakASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ContinueASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ParameterASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FunctionASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ImplBlockASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VariableDeclarationASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ReturnASTNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }

void SpanValidatorVisitor::checkNode(const ASTNode& node, const char* kind) {
    if (!node.span.isValid()) {
        issues.push_back(
            std::string("Invalid span in ") + kind +
            " at line " + std::to_string(node.span.start.line)
        );
    }
}

void SpanValidatorVisitor::checkChild(const ASTNode& parent, const ASTNode* child, const char* relation) {
    if (!child) {
        return;
    }
    if (parent.span.isValid() && child->span.isValid() && !parent.span.contains(child->span)) {
        issues.push_back(
            std::string("Child span out of parent bounds (") + relation +
            ") at line " + std::to_string(child->span.start.line)
        );
    }
}

bool SpanValidatorVisitor::validate(ASTNode& root) {
    issues.clear();
    root.accept(*this);
    return issues.empty();
}

void SpanValidatorVisitor::visit(NumberASTNode& node) { checkNode(node, "NumberASTNode"); }
void SpanValidatorVisitor::visit(StringASTNode& node) { checkNode(node, "StringASTNode"); }
void SpanValidatorVisitor::visit(CharLiteralASTNode& node) { checkNode(node, "CharLiteralASTNode"); }
void SpanValidatorVisitor::visit(BoolLiteralASTNode& node) { checkNode(node, "BoolLiteralASTNode"); }
void SpanValidatorVisitor::visit(NullLiteralASTNode& node) { checkNode(node, "NullLiteralASTNode"); }
void SpanValidatorVisitor::visit(VariableASTNode& node) { checkNode(node, "VariableASTNode"); }
void SpanValidatorVisitor::visit(ScopeResolutionASTNode& node) { checkNode(node, "ScopeResolutionASTNode"); }
void SpanValidatorVisitor::visit(ModuleDeclASTNode& node) { checkNode(node, "ModuleDeclASTNode"); }
void SpanValidatorVisitor::visit(ImportDeclASTNode& node) { checkNode(node, "ImportDeclASTNode"); }
void SpanValidatorVisitor::visit(StructDeclASTNode& node) { checkNode(node, "StructDeclASTNode"); }
void SpanValidatorVisitor::visit(UnionDeclASTNode& node) { checkNode(node, "UnionDeclASTNode"); }
void SpanValidatorVisitor::visit(EnumDeclASTNode& node) { checkNode(node, "EnumDeclASTNode"); }
void SpanValidatorVisitor::visit(WildcardPatternASTNode& node) { checkNode(node, "WildcardPatternASTNode"); }
void SpanValidatorVisitor::visit(IdentifierPatternASTNode& node) { checkNode(node, "IdentifierPatternASTNode"); }
void SpanValidatorVisitor::visit(PathPatternASTNode& node) { checkNode(node, "PathPatternASTNode"); }
void SpanValidatorVisitor::visit(BreakASTNode& node) { checkNode(node, "BreakASTNode"); }
void SpanValidatorVisitor::visit(ContinueASTNode& node) { checkNode(node, "ContinueASTNode"); }
void SpanValidatorVisitor::visit(ParameterASTNode& node) { checkNode(node, "ParameterASTNode"); }

void SpanValidatorVisitor::visit(LiteralPatternASTNode& node) {
    checkNode(node, "LiteralPatternASTNode");
    if (node.literal) {
        checkChild(node, node.literal.get(), "LiteralPattern.literal");
        node.literal->accept(*this);
    }
}

void SpanValidatorVisitor::visit(FieldAccessASTNode& node) {
    checkNode(node, "FieldAccessASTNode");
    if (node.object) {
        checkChild(node, node.object.get(), "FieldAccess.object");
        node.object->accept(*this);
    }
}

void SpanValidatorVisitor::visit(IndexASTNode& node) {
    checkNode(node, "IndexASTNode");
    if (node.target) {
        checkChild(node, node.target.get(), "Index.target");
        node.target->accept(*this);
    }
    if (node.index) {
        checkChild(node, node.index.get(), "Index.index");
        node.index->accept(*this);
    }
}

void SpanValidatorVisitor::visit(TryASTNode& node) {
    checkNode(node, "TryASTNode");
    if (node.operand) {
        checkChild(node, node.operand.get(), "Try.operand");
        node.operand->accept(*this);
    }
}

void SpanValidatorVisitor::visit(FunctionCallASTNode& node) {
    checkNode(node, "FunctionCallASTNode");
    if (node.callee) {
        checkChild(node, node.callee.get(), "FunctionCall.callee");
        node.callee->accept(*this);
    }
    for (auto& arg : node.arguments) {
        if (arg) {
            checkChild(node, arg.get(), "FunctionCall.argument");
            arg->accept(*this);
        }
    }
}

void SpanValidatorVisitor::visit(UnaryASTNode& node) {
    checkNode(node, "UnaryASTNode");
    if (node.operand) {
        checkChild(node, node.operand.get(), "Unary.operand");
        node.operand->accept(*this);
    }
}

void SpanValidatorVisitor::visit(CastASTNode& node) {
    checkNode(node, "CastASTNode");
    if (node.expression) {
        checkChild(node, node.expression.get(), "Cast.expression");
        node.expression->accept(*this);
    }
}

void SpanValidatorVisitor::visit(SizeOfASTNode& node) {
    checkNode(node, "SizeOfASTNode");
    if (!node.isTypeTarget && node.targetExpr) {
        checkChild(node, node.targetExpr.get(), "SizeOf.targetExpr");
        node.targetExpr->accept(*this);
    }
}

void SpanValidatorVisitor::visit(BinaryASTNode& node) {
    checkNode(node, "BinaryASTNode");
    if (node.left) {
        checkChild(node, node.left.get(), "Binary.left");
        node.left->accept(*this);
    }
    if (node.right) {
        checkChild(node, node.right.get(), "Binary.right");
        node.right->accept(*this);
    }
}

void SpanValidatorVisitor::visit(PrintASTNode& node) {
    checkNode(node, "PrintASTNode");
    if (node.expression) {
        checkChild(node, node.expression.get(), "Print.expression");
        node.expression->accept(*this);
    }
}

void SpanValidatorVisitor::visit(AssignmentASTNode& node) {
    checkNode(node, "AssignmentASTNode");
    if (node.target) {
        checkChild(node, node.target.get(), "Assignment.target");
        node.target->accept(*this);
    }
    if (node.value) {
        checkChild(node, node.value.get(), "Assignment.value");
        node.value->accept(*this);
    }
}

void SpanValidatorVisitor::visit(ProgramASTNode& node) {
    checkNode(node, "ProgramASTNode");
    for (auto& stmt : node.statements) {
        if (stmt) {
            checkChild(node, stmt.get(), "Program.statement");
            stmt->accept(*this);
        }
    }
}

void SpanValidatorVisitor::visit(MatchArmASTNode& node) {
    checkNode(node, "MatchArmASTNode");
    if (node.pattern) {
        checkChild(node, node.pattern.get(), "MatchArm.pattern");
        node.pattern->accept(*this);
    }
    if (node.hasBlockBody) {
        for (auto& stmt : node.bodyStatements) {
            if (stmt) {
                checkChild(node, stmt.get(), "MatchArm.statement");
                stmt->accept(*this);
            }
        }
    } else if (node.bodyExpr) {
        checkChild(node, node.bodyExpr.get(), "MatchArm.bodyExpr");
        node.bodyExpr->accept(*this);
    }
}

void SpanValidatorVisitor::visit(MatchASTNode& node) {
    checkNode(node, "MatchASTNode");
    if (node.scrutinee) {
        checkChild(node, node.scrutinee.get(), "Match.scrutinee");
        node.scrutinee->accept(*this);
    }
    for (auto& arm : node.arms) {
        if (arm) {
            checkChild(node, arm.get(), "Match.arm");
            arm->accept(*this);
        }
    }
}

void SpanValidatorVisitor::visit(IfASTNode& node) {
    checkNode(node, "IfASTNode");
    if (node.condition) {
        checkChild(node, node.condition.get(), "If.condition");
        node.condition->accept(*this);
    }
    for (auto& stmt : node.statements) {
        if (stmt) {
            checkChild(node, stmt.get(), "If.statement");
            stmt->accept(*this);
        }
    }
    for (auto& stmt : node.elseStatements) {
        if (stmt) {
            checkChild(node, stmt.get(), "If.elseStatement");
            stmt->accept(*this);
        }
    }
}

void SpanValidatorVisitor::visit(WhileASTNode& node) {
    checkNode(node, "WhileASTNode");
    if (node.condition) {
        checkChild(node, node.condition.get(), "While.condition");
        node.condition->accept(*this);
    }
    for (auto& stmt : node.statements) {
        if (stmt) {
            checkChild(node, stmt.get(), "While.statement");
            stmt->accept(*this);
        }
    }
}

void SpanValidatorVisitor::visit(ForASTNode& node) {
    checkNode(node, "ForASTNode");
    if (node.initialization) {
        checkChild(node, node.initialization.get(), "For.initialization");
        node.initialization->accept(*this);
    }
    if (node.condition) {
        checkChild(node, node.condition.get(), "For.condition");
        node.condition->accept(*this);
    }
    if (node.update) {
        checkChild(node, node.update.get(), "For.update");
        node.update->accept(*this);
    }
    for (auto& stmt : node.statements) {
        if (stmt) {
            checkChild(node, stmt.get(), "For.statement");
            stmt->accept(*this);
        }
    }
}

void SpanValidatorVisitor::visit(FunctionASTNode& node) {
    checkNode(node, "FunctionASTNode");
    for (auto& param : node.parameters) {
        if (param) {
            checkChild(node, param.get(), "Function.parameter");
            param->accept(*this);
        }
    }
    for (auto& stmt : node.statements) {
        if (stmt) {
            checkChild(node, stmt.get(), "Function.statement");
            stmt->accept(*this);
        }
    }
}

void SpanValidatorVisitor::visit(ImplBlockASTNode& node) {
    checkNode(node, "ImplBlockASTNode");
    for (auto& method : node.methods) {
        if (method) {
            checkChild(node, method.get(), "ImplBlock.method");
            method->accept(*this);
        }
    }
}

void SpanValidatorVisitor::visit(VariableDeclarationASTNode& node) {
    checkNode(node, "VariableDeclarationASTNode");
    if (node.value) {
        checkChild(node, node.value.get(), "VariableDeclaration.value");
        node.value->accept(*this);
    }
}

void SpanValidatorVisitor::visit(ReturnASTNode& node) {
    checkNode(node, "ReturnASTNode");
    if (node.value) {
        checkChild(node, node.value.get(), "Return.value");
        node.value->accept(*this);
    }
}
Parser::Parser(std::vector<Token> tokens, std::string_view source)
    : tokens(std::move(tokens)),
      source(source) {}

Token Parser::peek(std::size_t lookahead) const noexcept {
    if (cursor + lookahead >= tokens.size()) {
        const uint32_t line = tokens.empty() ? 1u : tokens.back().line;
        const uint32_t column = tokens.empty() ? 1u : tokens.back().column;
        const uint32_t offset = tokens.empty()
            ? 0u
            : (tokens.back().offset + tokens.back().length);
        return Token{TokenType::END_OF_FILE, offset, 0u, line, column};
    }
    return tokens[cursor + lookahead];
}

Token Parser::previousToken() const noexcept {
    if (cursor == 0 || tokens.empty()) {
        return peek();
    }
    const std::size_t idx = cursor <= tokens.size() ? cursor - 1 : tokens.size() - 1;
    return tokens[idx];
}

Token Parser::advance() noexcept {
    if (cursor >= tokens.size()) {
        return peek();
    }
    return tokens[cursor++];
}

bool Parser::match(TokenType type) noexcept {
    if (peek().type == type) {
        advance();
        return true;
    }
    return false;
}

void Parser::addError(uint32_t line, const std::string& message) {
    errors.push_back(
        "Error: Line " +
        std::to_string(line) +
        ": " +
        message
    );
}

void Parser::addDiagnostic(int id, uint32_t line, uint32_t column, uint32_t offset) {
    diagnostics.push_back(
        diagnosticEngine.analyze(
            id,
            static_cast<int>(line),
            static_cast<int>(column),
            static_cast<std::size_t>(offset)
        )
    );
}

Token Parser::consume(TokenType type, const std::string& errorMessage) {
    const Token current = peek();
    if (current.type == type) {
        return advance();
    }

    addError(current.line, errorMessage);
    return Token{type, current.offset, 0u, current.line, current.column};
}

void Parser::consumeExpectedSemicolon(const char* contextMessage, const Span* anchorSpan) {
    if (match(TokenType::SEMICOLON)) {
        return;
    }
    const Token current = peek();
    const uint32_t errLine = (anchorSpan && current.line > anchorSpan->end.line)
        ? anchorSpan->end.line
        : current.line;
    addError(errLine, contextMessage);
}

bool Parser::consumeGenericClose() {
    if (match(TokenType::GREATER)) {
        return true;
    }
    if (cursor < tokens.size() && tokens[cursor].type == TokenType::SHIFT_RIGHT) {
        Token& cur = tokens[cursor];
        cur.type = TokenType::GREATER;
        cur.length = 1u;

        Token second = cur;
        second.offset += 1u;
        if (second.column > 0) {
            second.column += 1u;
        }
        tokens.insert(tokens.begin() + static_cast<std::ptrdiff_t>(cursor) + 1, second);
        advance();
        return true;
    }
    addError(peek().line, "Expected '>' after generic type arguments");
    return false;
}

bool Parser::canStartGenericArgs() const noexcept {
    if (peek().type != TokenType::LESS) {
        return false;
    }
    int depth = 0;
    std::size_t i = 0;
    while (true) {
        const Token tok = peek(i++);
        if (tok.type == TokenType::END_OF_FILE || !isTypeArgAllowedToken(tok.type)) {
            return false;
        }
        if (tok.type == TokenType::LESS) {
            ++depth;
        } else if (tok.type == TokenType::GREATER) {
            --depth;
            if (depth == 0) {
                const TokenType after = peek(i).type;
                return after == TokenType::LPAREN || after == TokenType::SCOPE_RESOLUTION;
            }
        } else if (tok.type == TokenType::SHIFT_RIGHT) {
            depth -= 2;
            if (depth <= 0) {
                const TokenType after = peek(i).type;
                return after == TokenType::LPAREN || after == TokenType::SCOPE_RESOLUTION;
            }
        }
    }
}

void Parser::consumeClosingParenthesis(const char* contextMessage) {
    const Token current = peek();
    if (current.type == TokenType::RPAREN) {
        advance();
    } else {
        addDiagnostic(1, current.line, current.column, current.offset);
        if (contextMessage) {
            addError(current.line, contextMessage);
        }
    }
}

void Parser::consumeOpeningParenthesis() {
    const Token current = peek();
    if (current.type == TokenType::LPAREN) {
        advance();
    } else {
        addDiagnostic(4, current.line, current.column, current.offset);
    }
}

void Parser::synchronize() noexcept {
    while (true) {
        const TokenType t = peek().type;
        if (t == TokenType::END_OF_FILE ||
            t == TokenType::PRI ||
            t == TokenType::IF ||
            t == TokenType::WHILE ||
            t == TokenType::FOR ||
            t == TokenType::DO ||
            t == TokenType::LET ||
            t == TokenType::MUT ||
            t == TokenType::CONST ||
            t == TokenType::STATIC ||
            t == TokenType::STRUCT ||
            t == TokenType::UNION ||
            t == TokenType::ENUM ||
            t == TokenType::IMPL ||
            t == TokenType::EXTERN ||
            t == TokenType::MODULE ||
            t == TokenType::IMPORT ||
            t == TokenType::MATCH ||
            t == TokenType::RETURN ||
            t == TokenType::BREAK ||
            t == TokenType::CONTINUE ||
            t == TokenType::IDENTIFIER ||
            t == TokenType::RBRACE) {
            break;
        }
        advance();
    }
}

void Parser::skipUntil(TokenType stop) noexcept {
    skipUntil(stop, stop);
}

void Parser::skipUntil(TokenType stopA, TokenType stopB) noexcept {
    while (peek().type != stopA &&
           peek().type != stopB &&
           peek().type != TokenType::LBRACE &&
           peek().type != TokenType::END_OF_FILE) {
        advance();
    }
}

void Parser::recoverToStatementEnd() noexcept {
    skipUntil(TokenType::SEMICOLON, TokenType::RBRACE);
    match(TokenType::SEMICOLON);
}

std::vector<std::string> Parser::parseGenericParams() {
    std::vector<std::string> params;
    if (!match(TokenType::LESS)) {
        return params;
    }

    while (peek().type != TokenType::GREATER &&
           peek().type != TokenType::SHIFT_RIGHT &&
           peek().type != TokenType::END_OF_FILE) {
        const Token paramTok = consume(TokenType::IDENTIFIER, "Expected generic type parameter name");
        if (paramTok.length > 0) {
            params.emplace_back(lexeme(paramTok));
        }
        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    consumeGenericClose();
    return params;
}

std::vector<TypeSpec> Parser::parseGenericArgs() {
    std::vector<TypeSpec> args;
    if (!match(TokenType::LESS)) {
        return args;
    }

    while (peek().type != TokenType::GREATER &&
           peek().type != TokenType::SHIFT_RIGHT &&
           peek().type != TokenType::END_OF_FILE) {
        args.push_back(parseTypeSpec());
        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    consumeGenericClose();
    return args;
}

std::vector<std::string> Parser::parseScopedPath() {
    std::vector<std::string> path;
    const Token first = consume(TokenType::IDENTIFIER, "Expected identifier in path");
    if (first.length > 0) {
        path.emplace_back(lexeme(first));
    }
    while (match(TokenType::SCOPE_RESOLUTION)) {
        const Token seg = consume(TokenType::IDENTIFIER, "Expected identifier after '::'");
        if (seg.length > 0) {
            path.emplace_back(lexeme(seg));
        }
    }
    return path;
}

TypeSpec Parser::parseTypeSpec() {
    TypeSpec spec;

    if (match(TokenType::MUT)) {
        spec.isMutable = true;
    }

    while (match(TokenType::MULTIPLY)) {
        ++spec.pointerDepth;
    }

    if (match(TokenType::LBRACKET)) {
        TypeSpec elemType = parseTypeSpec();
        if (match(TokenType::SEMICOLON)) {
            const Token sizeTok = peek();
            if (sizeTok.type == TokenType::NUMBER ||
                sizeTok.type == TokenType::HEX_LITERAL ||
                sizeTok.type == TokenType::BINARY_LITERAL) {
                advance();
                spec.hasArraySize = true;
                spec.arraySize = std::strtoull(std::string(lexeme(sizeTok)).c_str(), nullptr, 0);
            } else {
                addError(sizeTok.line, "Expected array length after ';'");
            }
        }
        consume(TokenType::RBRACKET, "Expected ']' after array type");
        spec.name = "[" + elemType.name + "]";
        spec.elementType = std::make_unique<TypeSpec>(std::move(elemType));
        return spec;
    }

    if (match(TokenType::FN)) {
        spec.isFunctionType = true;
        spec.name = "fn";
        consumeOpeningParenthesis();
        if (peek().type != TokenType::RPAREN) {
            while (true) {
                spec.fnParams.push_back(parseTypeSpec());
                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
        }
        consumeClosingParenthesis("Expected ')' after function type parameters");
        if (match(TokenType::ARROW)) {
            spec.fnReturn = std::make_unique<TypeSpec>(parseTypeSpec());
        }
        return spec;
    }

    const Token tok = peek();
    if (isPrimitiveTypeToken(tok.type) || tok.type == TokenType::IDENTIFIER) {
        advance();
        spec.name = std::string(lexeme(tok));
        if (peek().type == TokenType::LESS) {
            spec.genericArgs = parseGenericArgs();
        }
    } else {
        addError(tok.line, "Expected a valid type");
        if (tok.type != TokenType::END_OF_FILE &&
            tok.type != TokenType::SEMICOLON &&
            tok.type != TokenType::RPAREN &&
            tok.type != TokenType::RBRACE &&
            tok.type != TokenType::COMMA &&
            tok.type != TokenType::EQUAL) {
            advance();
        }
    }

    return spec;
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseBlock(const char* missingCloseMsg, Span* outSpan) {
    const Token openBrace = previousToken();
    std::vector<std::unique_ptr<ASTNode>> statements;

    while (peek().type != TokenType::RBRACE &&
           peek().type != TokenType::END_OF_FILE) {
        const std::size_t previousCursor = cursor;
        auto statement = parseStatement();

        if (statement) {
            statements.push_back(std::move(statement));
        }

        if (cursor == previousCursor) {
            synchronize();
            if (cursor == previousCursor && peek().type != TokenType::END_OF_FILE) {
                advance();
            }
        }
    }

    const Token closeBrace = consume(TokenType::RBRACE, missingCloseMsg);
    if (outSpan) {
        const Span startSpan = tokenSpan(openBrace);
        const Span endSpan = closeBrace.length > 0 ? tokenSpan(closeBrace) : tokenSpan(previousToken());
        *outSpan = Span::merge(startSpan, endSpan);
    }
    return statements;
}

std::unique_ptr<FunctionCallASTNode> Parser::parseFunctionCall(
    std::string name,
    std::vector<TypeSpec> genericArgs,
    std::unique_ptr<ASTNode> callee
) {
    const Span startSpan = callee ? callee->span : tokenSpan(previousToken());
    std::vector<std::unique_ptr<ASTNode>> arguments;

    if (peek().type != TokenType::RPAREN) {
        while (true) {
            auto argument = parseExpression();
            if (!argument) {
                skipUntil(TokenType::COMMA, TokenType::RPAREN);
                if (match(TokenType::COMMA)) {
                    continue;
                }
                break;
            }
            arguments.push_back(std::move(argument));

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
    }

    consumeClosingParenthesis("Expected ')' to close function call arguments");
    const Token afterCall = previousToken();
    auto callNode = std::make_unique<FunctionCallASTNode>(
        std::move(name),
        std::move(arguments),
        std::move(genericArgs),
        std::move(callee)
    );
    callNode->span = Span::merge(startSpan, tokenSpan(afterCall));
    return callNode;
}
std::unique_ptr<PatternASTNode> Parser::parsePattern() {
    const Token current = peek();

    if (current.type == TokenType::IDENTIFIER && lexeme(current) == "_") {
        const Token tok = advance();
        auto node = std::make_unique<WildcardPatternASTNode>();
        node->span = tokenSpan(tok);
        return node;
    }

    if (current.type == TokenType::NUMBER ||
        current.type == TokenType::FLOAT_LITERAL ||
        current.type == TokenType::HEX_LITERAL ||
        current.type == TokenType::BINARY_LITERAL) {
        const Token tok = advance();
        auto lit = std::make_unique<NumberASTNode>(std::string(lexeme(tok)), tok.type);
        lit->span = tokenSpan(tok);
        const Span litSpan = lit->span;
        auto pat = std::make_unique<LiteralPatternASTNode>(std::move(lit));
        pat->span = litSpan;
        return pat;
    }

    if (match(TokenType::MINUS)) {
        const Token minusTok = previousToken();
        const Token numTok = peek();
        if (numTok.type == TokenType::NUMBER ||
            numTok.type == TokenType::FLOAT_LITERAL ||
            numTok.type == TokenType::HEX_LITERAL ||
            numTok.type == TokenType::BINARY_LITERAL) {
            advance();
            auto numNode = std::make_unique<NumberASTNode>(std::string(lexeme(numTok)), numTok.type);
            numNode->span = tokenSpan(numTok);
            auto unNode = std::make_unique<UnaryASTNode>("-", std::move(numNode));
            unNode->span = Span::merge(tokenSpan(minusTok), tokenSpan(numTok));
            const Span fullSpan = unNode->span;
            auto pat = std::make_unique<LiteralPatternASTNode>(std::move(unNode));
            pat->span = fullSpan;
            return pat;
        }
        addError(minusTok.line, "Expected numeric literal after '-' in pattern");
        return nullptr;
    }

    if (match(TokenType::STRING_LITERAL)) {
        const Token tok = previousToken();
        auto lit = std::make_unique<StringASTNode>(std::string(lexeme(tok)));
        lit->span = tokenSpan(tok);
        auto pat = std::make_unique<LiteralPatternASTNode>(std::move(lit));
        pat->span = lit->span;
        return pat;
    }

    if (match(TokenType::CHAR_LITERAL)) {
        const Token tok = previousToken();
        auto lit = std::make_unique<CharLiteralASTNode>(std::string(lexeme(tok)));
        lit->span = tokenSpan(tok);
        auto pat = std::make_unique<LiteralPatternASTNode>(std::move(lit));
        pat->span = lit->span;
        return pat;
    }

    if (match(TokenType::TRUE)) {
        const Token tok = previousToken();
        auto lit = std::make_unique<BoolLiteralASTNode>(true);
        lit->span = tokenSpan(tok);
        auto pat = std::make_unique<LiteralPatternASTNode>(std::move(lit));
        pat->span = lit->span;
        return pat;
    }

    if (match(TokenType::FALSE)) {
        const Token tok = previousToken();
        auto lit = std::make_unique<BoolLiteralASTNode>(false);
        lit->span = tokenSpan(tok);
        auto pat = std::make_unique<LiteralPatternASTNode>(std::move(lit));
        pat->span = lit->span;
        return pat;
    }

    if (match(TokenType::NULL_LITERAL)) {
        const Token tok = previousToken();
        auto lit = std::make_unique<NullLiteralASTNode>();
        lit->span = tokenSpan(tok);
        auto pat = std::make_unique<LiteralPatternASTNode>(std::move(lit));
        pat->span = lit->span;
        return pat;
    }

    bool isMutBinding = match(TokenType::MUT);
    if (match(TokenType::IDENTIFIER)) {
        const Token identTok = previousToken();
        std::string name(lexeme(identTok));

        if (!isMutBinding && peek().type == TokenType::SCOPE_RESOLUTION) {
            std::vector<std::string> path;
            path.push_back(std::move(name));
            Token lastTok = identTok;
            while (match(TokenType::SCOPE_RESOLUTION)) {
                const Token seg = consume(TokenType::IDENTIFIER, "Expected identifier after '::' in pattern");
                if (seg.length > 0) {
                    path.emplace_back(lexeme(seg));
                    lastTok = seg;
                }
            }
            auto pat = std::make_unique<PathPatternASTNode>(std::move(path));
            pat->span = Span::merge(tokenSpan(identTok), tokenSpan(lastTok));
            return pat;
        }

        const Token startTok = isMutBinding ? tokens[cursor - 2] : identTok;
        auto pat = std::make_unique<IdentifierPatternASTNode>(std::move(name), isMutBinding);
        pat->span = Span::merge(tokenSpan(startTok), tokenSpan(identTok));
        return pat;
    }

    addError(peek().line, "Expected pattern in match arm");
    return nullptr;
}

std::unique_ptr<ASTNode> Parser::parsePrimary() {
    if (match(TokenType::LPAREN)) {
        const Token lparen = previousToken();
        auto node = parseExpression();
        consumeClosingParenthesis("Expected ')' to close parenthesized expression");
        const Token rparen = previousToken();
        if (node) {
            node->span = Span::merge(tokenSpan(lparen), tokenSpan(rparen));
        }
        return node;
    }

    const Token current = peek();
    if (current.type == TokenType::NUMBER ||
        current.type == TokenType::FLOAT_LITERAL ||
        current.type == TokenType::HEX_LITERAL ||
        current.type == TokenType::BINARY_LITERAL) {
        const Token tok = advance();
        auto node = std::make_unique<NumberASTNode>(std::string(lexeme(tok)), tok.type);
        node->span = tokenSpan(tok);
        return node;
    }

    if (match(TokenType::STRING_LITERAL)) {
        const Token tok = previousToken();
        auto node = std::make_unique<StringASTNode>(std::string(lexeme(tok)));
        node->span = tokenSpan(tok);
        return node;
    }

    if (match(TokenType::CHAR_LITERAL)) {
        const Token tok = previousToken();
        auto node = std::make_unique<CharLiteralASTNode>(std::string(lexeme(tok)));
        node->span = tokenSpan(tok);
        return node;
    }

    if (match(TokenType::TRUE)) {
        const Token tok = previousToken();
        auto node = std::make_unique<BoolLiteralASTNode>(true);
        node->span = tokenSpan(tok);
        return node;
    }

    if (match(TokenType::FALSE)) {
        const Token tok = previousToken();
        auto node = std::make_unique<BoolLiteralASTNode>(false);
        node->span = tokenSpan(tok);
        return node;
    }

    if (match(TokenType::NULL_LITERAL)) {
        const Token tok = previousToken();
        auto node = std::make_unique<NullLiteralASTNode>();
        node->span = tokenSpan(tok);
        return node;
    }

    if (match(TokenType::SIZEOF)) {
        const Token sizeofTok = previousToken();
        consumeOpeningParenthesis();
        const Token afterLParen = peek();
        std::unique_ptr<SizeOfASTNode> node;
        if (isPrimitiveTypeToken(afterLParen.type) ||
            afterLParen.type == TokenType::MULTIPLY ||
            afterLParen.type == TokenType::LBRACKET ||
            afterLParen.type == TokenType::FN) {
            TypeSpec t = parseTypeSpec();
            node = std::make_unique<SizeOfASTNode>(std::move(t));
        } else {
            auto expr = parseExpression();
            node = std::make_unique<SizeOfASTNode>(std::move(expr));
        }
        consumeClosingParenthesis("Expected ')' after sizeof argument");
        node->span = Span::merge(tokenSpan(sizeofTok), tokenSpan(previousToken()));
        return node;
    }

    if (match(TokenType::SELF)) {
        const Token tok = previousToken();
        auto node = std::make_unique<VariableASTNode>("self");
        node->span = tokenSpan(tok);
        return node;
    }

    if (match(TokenType::IDENTIFIER)) {
        const Token identTok = previousToken();
        std::string name(lexeme(identTok));
        std::vector<TypeSpec> leadingGenerics;

        if (canStartGenericArgs()) {
            leadingGenerics = parseGenericArgs();
        }

        if (peek().type == TokenType::SCOPE_RESOLUTION) {
            std::vector<std::string> path;
            path.push_back(std::move(name));
            Token lastTok = previousToken();
            while (match(TokenType::SCOPE_RESOLUTION)) {
                const Token seg = consume(TokenType::IDENTIFIER, "Expected identifier after '::'");
                if (seg.length > 0) {
                    path.emplace_back(lexeme(seg));
                    lastTok = seg;
                }
            }
            auto node = std::make_unique<ScopeResolutionASTNode>(
                std::move(path),
                std::move(leadingGenerics)
            );
            node->span = Span::merge(tokenSpan(identTok), tokenSpan(lastTok));
            return node;
        }

        if (!leadingGenerics.empty() && match(TokenType::LPAREN)) {
            const Token afterGenTok = previousToken();
            auto calleeNode = std::make_unique<VariableASTNode>(name);
            calleeNode->span = Span::merge(tokenSpan(identTok), tokenSpan(afterGenTok));
            return parseFunctionCall(
                std::move(name),
                std::move(leadingGenerics),
                std::move(calleeNode)
            );
        }

        auto node = std::make_unique<VariableASTNode>(std::move(name));
        node->span = tokenSpan(identTok);
        return node;
    }

    addError(peek().line, "Expected expression");
    return nullptr;
}

std::unique_ptr<ASTNode> Parser::parsePostfix() {
    auto node = parsePrimary();
    if (!node) {
        return nullptr;
    }

    while (true) {
        std::vector<TypeSpec> callGenerics;
        if (canStartGenericArgs()) {
            const std::size_t savedCursor = cursor;
            const std::size_t savedErrors = errors.size();
            auto parsedGens = parseGenericArgs();
            if (errors.size() == savedErrors && peek().type == TokenType::LPAREN) {
                callGenerics = std::move(parsedGens);
                node->span = Span::merge(node->span, tokenSpan(previousToken()));
            } else {
                cursor = savedCursor;
                errors.resize(savedErrors);
            }
        }

        if (match(TokenType::LPAREN)) {
            std::string callName;
            if (auto* varNode = dynamic_cast<VariableASTNode*>(node.get())) {
                callName = varNode->name;
            } else if (auto* scopeNode = dynamic_cast<ScopeResolutionASTNode*>(node.get())) {
                if (!scopeNode->path.empty()) {
                    callName = scopeNode->path.back();
                }
                if (callGenerics.empty() && !scopeNode->genericArgs.empty()) {
                    callGenerics = scopeNode->genericArgs;
                }
            } else if (auto* fieldNode = dynamic_cast<FieldAccessASTNode*>(node.get())) {
                callName = fieldNode->field;
            }
            node = parseFunctionCall(std::move(callName), std::move(callGenerics), std::move(node));
            if (!node) {
                return nullptr;
            }
        } else if (match(TokenType::LBRACKET)) {
            const Span baseSpan = node->span;
            auto idx = parseExpression();
            const Token rbrack = consume(TokenType::RBRACKET, "Expected ']' after index expression");
            if (!idx) {
                return nullptr;
            }
            const Span endSpan = rbrack.length > 0 ? tokenSpan(rbrack) : idx->span;
            auto idxNode = std::make_unique<IndexASTNode>(std::move(node), std::move(idx));
            idxNode->span = Span::merge(baseSpan, endSpan);
            node = std::move(idxNode);
        } else if (match(TokenType::DOT)) {
            const Span objSpan = node->span;
            const Token fieldTok = consume(TokenType::IDENTIFIER, "Expected field name after '.'");
            const Span endSpan = fieldTok.length > 0 ? tokenSpan(fieldTok) : objSpan;
            auto fieldNode = std::make_unique<FieldAccessASTNode>(std::move(node), std::string(lexeme(fieldTok)));
            fieldNode->span = Span::merge(objSpan, endSpan);
            node = std::move(fieldNode);
        } else if (match(TokenType::QUESTION)) {
            const Token qTok = previousToken();
            const Span opSpan = node->span;
            auto tryNode = std::make_unique<TryASTNode>(std::move(node));
            tryNode->span = Span::merge(opSpan, tokenSpan(qTok));
            node = std::move(tryNode);
        } else {
            break;
        }
    }

    return node;
}

std::unique_ptr<ASTNode> Parser::parseUnary() {
    if (match(TokenType::MINUS)) {
        const Token opTok = previousToken();
        auto operand = parseUnary();
        if (!operand) {
            return nullptr;
        }
        const Span opSpan = operand->span;
        auto node = std::make_unique<UnaryASTNode>("-", std::move(operand));
        node->span = Span::merge(tokenSpan(opTok), opSpan);
        return node;
    }

    if (match(TokenType::PLUS)) {
        const Token opTok = previousToken();
        auto operand = parseUnary();
        if (!operand) {
            return nullptr;
        }
        const Span opSpan = operand->span;
        auto node = std::make_unique<UnaryASTNode>("+", std::move(operand));
        node->span = Span::merge(tokenSpan(opTok), opSpan);
        return node;
    }

    if (match(TokenType::TILDE)) {
        const Token opTok = previousToken();
        auto operand = parseUnary();
        if (!operand) {
            return nullptr;
        }
        const Span opSpan = operand->span;
        auto node = std::make_unique<UnaryASTNode>("~", std::move(operand));
        node->span = Span::merge(tokenSpan(opTok), opSpan);
        return node;
    }

    if (match(TokenType::AMPERSAND)) {
        const Token opTok = previousToken();
        auto operand = parseUnary();
        if (!operand) {
            return nullptr;
        }
        const Span opSpan = operand->span;
        auto node = std::make_unique<UnaryASTNode>("&", std::move(operand));
        node->span = Span::merge(tokenSpan(opTok), opSpan);
        return node;
    }

    if (match(TokenType::MULTIPLY)) {
        const Token opTok = previousToken();
        auto operand = parseUnary();
        if (!operand) {
            return nullptr;
        }
        const Span opSpan = operand->span;
        auto node = std::make_unique<UnaryASTNode>("*", std::move(operand));
        node->span = Span::merge(tokenSpan(opTok), opSpan);
        return node;
    }

    return parsePostfix();
}

std::unique_ptr<ASTNode> Parser::parseNot() {
    if (match(TokenType::NOT)) {
        const Token opTok = previousToken();
        auto operand = parseNot();
        if (!operand) {
            return nullptr;
        }
        const Span opSpan = operand->span;
        auto node = std::make_unique<UnaryASTNode>("not", std::move(operand));
        node->span = Span::merge(tokenSpan(opTok), opSpan);
        return node;
    }
    return parseUnary();
}

std::unique_ptr<ASTNode> Parser::parseCast() {
    auto node = parseNot();
    while (match(TokenType::AS)) {
        TypeSpec target = parseTypeSpec();
        if (!node) {
            return nullptr;
        }
        const Span exprSpan = node->span;
        const Span endSpan = tokenSpan(previousToken());
        auto castNode = std::make_unique<CastASTNode>(std::move(node), std::move(target));
        castNode->span = Span::merge(exprSpan, endSpan);
        node = std::move(castNode);
    }
    return node;
}

std::unique_ptr<ASTNode> Parser::parseBinaryExpression(int minPrecedence) {
    auto left = parseCast();
    if (!left) {
        return nullptr;
    }

    while (true) {
        const Token op = peek();
        const int prec = getBinaryPrecedence(op.type);
        if (prec < minPrecedence) {
            break;
        }

        advance();
        std::string opStr = (op.type == TokenType::AND)
            ? std::string("and")
            : (op.type == TokenType::OR ? std::string("or") : std::string(lexeme(op)));

        auto right = parseBinaryExpression(prec + 1);
        if (!right) {
            return nullptr;
        }

        const Span leftSpan = left->span;
        const Span rightSpan = right->span;
        auto bin = std::make_unique<BinaryASTNode>(
            std::move(opStr),
            std::move(left),
            std::move(right)
        );
        bin->span = Span::merge(leftSpan, rightSpan);
        left = std::move(bin);
    }

    return left;
}

std::unique_ptr<ASTNode> Parser::parseAssignment() {
    auto lhs = parseBinaryExpression(1);
    if (!lhs) {
        return nullptr;
    }

    const TokenType t = peek().type;
    if (t == TokenType::EQUAL || isCompoundAssignToken(t)) {
        const Token opTok = advance();
        auto rhs = parseAssignment();
        if (!rhs) {
            return nullptr;
        }

        std::string targetName;
        if (!extractAssignableTargetName(lhs.get(), targetName)) {
            addError(opTok.line, "Invalid assignment target");
            return nullptr;
        }

        const Span lhsSpan = lhs->span;
        const Span rhsSpan = rhs->span;
        auto assignNode = std::make_unique<AssignmentASTNode>(
            std::move(targetName),
            std::move(rhs),
            std::string(lexeme(opTok)),
            std::move(lhs)
        );
        assignNode->span = Span::merge(lhsSpan, rhsSpan);
        return assignNode;
    }

    return lhs;
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseAssignment();
}
std::unique_ptr<ASTNode> Parser::parseModuleDecl() {
    const Token modTok = consume(TokenType::MODULE, "Expected 'module'");
    auto path = parseScopedPath();
    const Span anchor = Span::merge(tokenSpan(modTok), tokenSpan(previousToken()));
    consumeExpectedSemicolon("Expected ';' after module declaration", &anchor);
    auto node = std::make_unique<ModuleDeclASTNode>(std::move(path));
    node->span = Span::merge(tokenSpan(modTok), tokenSpan(previousToken()));
    return node;
}

std::unique_ptr<ASTNode> Parser::parseImportDecl() {
    const Token impTok = consume(TokenType::IMPORT, "Expected 'import'");
    auto path = parseScopedPath();
    const Span anchor = Span::merge(tokenSpan(impTok), tokenSpan(previousToken()));
    consumeExpectedSemicolon("Expected ';' after import declaration", &anchor);
    auto node = std::make_unique<ImportDeclASTNode>(std::move(path));
    node->span = Span::merge(tokenSpan(impTok), tokenSpan(previousToken()));
    return node;
}

std::unique_ptr<ASTNode> Parser::parseStructDecl() {
    const Token structTok = consume(TokenType::STRUCT, "Expected 'struct'");
    const Token nameTok = consume(TokenType::IDENTIFIER, "Expected struct name");
    auto generics = parseGenericParams();
    consume(TokenType::LBRACE, "Expected '{' after struct name");

    std::vector<StructFieldDecl> fields;
    while (peek().type != TokenType::RBRACE && peek().type != TokenType::END_OF_FILE) {
        const Token fieldName = consume(TokenType::IDENTIFIER, "Expected field name in struct");
        consume(TokenType::COLON, "Expected ':' after field name");
        TypeSpec fieldType = parseTypeSpec();
        fields.push_back(StructFieldDecl{std::string(lexeme(fieldName)), std::move(fieldType)});
        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    const Token rbraceTok = consume(TokenType::RBRACE, "Expected '}' after struct body");
    if (nameTok.length == 0) {
        return nullptr;
    }
    auto node = std::make_unique<StructDeclASTNode>(
        std::string(lexeme(nameTok)),
        std::move(generics),
        std::move(fields)
    );
    const Span endSpan = rbraceTok.length > 0 ? tokenSpan(rbraceTok) : tokenSpan(previousToken());
    node->span = Span::merge(tokenSpan(structTok), endSpan);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseUnionDecl() {
    const Token unionTok = consume(TokenType::UNION, "Expected 'union'");
    const Token nameTok = consume(TokenType::IDENTIFIER, "Expected union name");
    auto generics = parseGenericParams();
    consume(TokenType::LBRACE, "Expected '{' after union name");

    std::vector<StructFieldDecl> fields;
    while (peek().type != TokenType::RBRACE && peek().type != TokenType::END_OF_FILE) {
        const Token fieldName = consume(TokenType::IDENTIFIER, "Expected field name in union");
        consume(TokenType::COLON, "Expected ':' after field name");
        TypeSpec fieldType = parseTypeSpec();
        fields.push_back(StructFieldDecl{std::string(lexeme(fieldName)), std::move(fieldType)});
        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    const Token rbraceTok = consume(TokenType::RBRACE, "Expected '}' after union body");
    if (nameTok.length == 0) {
        return nullptr;
    }
    auto node = std::make_unique<UnionDeclASTNode>(
        std::string(lexeme(nameTok)),
        std::move(generics),
        std::move(fields)
    );
    const Span endSpan = rbraceTok.length > 0 ? tokenSpan(rbraceTok) : tokenSpan(previousToken());
    node->span = Span::merge(tokenSpan(unionTok), endSpan);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseEnumDecl() {
    const Token enumTok = consume(TokenType::ENUM, "Expected 'enum'");
    const Token nameTok = consume(TokenType::IDENTIFIER, "Expected enum name");
    auto generics = parseGenericParams();
    consume(TokenType::LBRACE, "Expected '{' after enum name");

    std::vector<std::string> variants;
    while (peek().type != TokenType::RBRACE && peek().type != TokenType::END_OF_FILE) {
        const Token variantTok = consume(TokenType::IDENTIFIER, "Expected enum variant name");
        if (variantTok.length > 0) {
            variants.emplace_back(lexeme(variantTok));
        }
        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    const Token rbraceTok = consume(TokenType::RBRACE, "Expected '}' after enum body");
    if (nameTok.length == 0) {
        return nullptr;
    }
    auto node = std::make_unique<EnumDeclASTNode>(
        std::string(lexeme(nameTok)),
        std::move(generics),
        std::move(variants)
    );
    const Span endSpan = rbraceTok.length > 0 ? tokenSpan(rbraceTok) : tokenSpan(previousToken());
    node->span = Span::merge(tokenSpan(enumTok), endSpan);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseImplBlock() {
    const Token implTok = consume(TokenType::IMPL, "Expected 'impl'");
    auto generics = parseGenericParams();
    const Token targetTok = consume(TokenType::IDENTIFIER, "Expected target type name in impl");
    consume(TokenType::LBRACE, "Expected '{' after impl target type");

    std::vector<std::unique_ptr<FunctionASTNode>> methods;
    while (peek().type != TokenType::RBRACE && peek().type != TokenType::END_OF_FILE) {
        if (peek().type == TokenType::DO) {
            auto fn = parseFunction(true);
            if (fn) {
                methods.push_back(std::move(fn));
            }
        } else {
            const Token bad = advance();
            addError(bad.line, "Expected method ('do') inside impl block");
        }
    }

    const Token rbraceTok = consume(TokenType::RBRACE, "Expected '}' after impl block");
    if (targetTok.length == 0) {
        return nullptr;
    }
    auto node = std::make_unique<ImplBlockASTNode>(
        std::string(lexeme(targetTok)),
        std::move(generics),
        std::move(methods)
    );
    const Span endSpan = rbraceTok.length > 0 ? tokenSpan(rbraceTok) : tokenSpan(previousToken());
    node->span = Span::merge(tokenSpan(implTok), endSpan);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseExternFunction() {
    const Token extTok = consume(TokenType::EXTERN, "Expected 'extern'");
    auto fn = parseFunction(false);
    if (!fn) {
        return nullptr;
    }
    fn->isExtern = true;
    fn->span = Span::merge(tokenSpan(extTok), fn->span);
    return fn;
}

std::unique_ptr<ASTNode> Parser::parseMatchStatement() {
    const Token matchTok = consume(TokenType::MATCH, "Expected 'match'");
    auto scrutinee = parseExpression();
    if (!scrutinee) {
        skipUntil(TokenType::LBRACE);
    }
    consume(TokenType::LBRACE, "Expected '{' after match expression");

    std::vector<std::unique_ptr<MatchArmASTNode>> arms;
    while (peek().type != TokenType::RBRACE && peek().type != TokenType::END_OF_FILE) {
        auto pattern = parsePattern();
        if (!pattern) {
            skipUntil(TokenType::COMMA, TokenType::RBRACE);
            match(TokenType::COMMA);
            continue;
        }
        const Span patSpan = pattern->span;
        consume(TokenType::FAT_ARROW, "Expected '=>' in match arm");

        if (peek().type == TokenType::LBRACE) {
            advance(); // '{'
            Span blockSpan;
            auto blockStmts = parseBlock("Expected '}' after match arm block", &blockSpan);
            match(TokenType::COMMA);
            auto arm = std::make_unique<MatchArmASTNode>(
                std::move(pattern),
                std::move(blockStmts)
            );
            arm->span = Span::merge(patSpan, blockSpan);
            arms.push_back(std::move(arm));
        } else {
            auto armExpr = parseExpression();
            match(TokenType::COMMA);
            if (armExpr) {
                const Span exprSpan = armExpr->span;
                auto arm = std::make_unique<MatchArmASTNode>(
                    std::move(pattern),
                    std::move(armExpr)
                );
                arm->span = Span::merge(patSpan, exprSpan);
                arms.push_back(std::move(arm));
            }
        }
    }

    const Token rbraceTok = consume(TokenType::RBRACE, "Expected '}' after match arms");
    if (!scrutinee) {
        return nullptr;
    }
    auto node = std::make_unique<MatchASTNode>(std::move(scrutinee), std::move(arms));
    const Span endSpan = rbraceTok.length > 0 ? tokenSpan(rbraceTok) : tokenSpan(previousToken());
    node->span = Span::merge(tokenSpan(matchTok), endSpan);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseContinue() {
    const Token contTok = consume(TokenType::CONTINUE, "Expected 'continue'");
    Token endTok = contTok;
    if (peek().type != TokenType::SEMICOLON) {
        addError(contTok.line, "Expected ';' after 'continue'");
        recoverToStatementEnd();
    } else {
        endTok = advance();
    }
    auto node = std::make_unique<ContinueASTNode>();
    node->span = Span::merge(tokenSpan(contTok), tokenSpan(endTok));
    return node;
}

std::unique_ptr<ASTNode> Parser::parseBreak() {
    const Token brkTok = consume(TokenType::BREAK, "Expected 'break'");
    Token endTok = brkTok;
    if (peek().type != TokenType::SEMICOLON) {
        addError(brkTok.line, "Expected ';' after 'break'");
        recoverToStatementEnd();
    } else {
        endTok = advance();
    }
    auto node = std::make_unique<BreakASTNode>();
    node->span = Span::merge(tokenSpan(brkTok), tokenSpan(endTok));
    return node;
}

std::unique_ptr<ASTNode> Parser::parseWhile() {
    const Token whileTok = consume(TokenType::WHILE, "Expected 'while'");
    consumeOpeningParenthesis();
    auto condition = parseExpression();
    if (!condition) {
        skipUntil(TokenType::RPAREN);
        match(TokenType::RPAREN);
    } else {
        consumeClosingParenthesis("Expected ')' after while condition");
    }
    consume(TokenType::LBRACE, "Expected '{' before while body");

    Span blockSpan;
    auto statements = parseBlock("Expected '}' after while body", &blockSpan);

    if (condition) {
        auto node = std::make_unique<WhileASTNode>(
            std::move(condition),
            std::move(statements)
        );
        node->span = Span::merge(tokenSpan(whileTok), blockSpan);
        return node;
    }

    return nullptr;
}

std::unique_ptr<ASTNode> Parser::parseFor() {
    const Token forTok = consume(TokenType::FOR, "Expected 'for'");
    consumeOpeningParenthesis();

    auto initialization = parseVariableDeclaration(false, false);
    if (!initialization) {
        skipUntil(TokenType::SEMICOLON, TokenType::RPAREN);
        if (!match(TokenType::SEMICOLON)) {
            addError(peek().line, "Expected ';' after for initialization");
        }
    }

    auto condition = parseExpression();
    if (!condition) {
        skipUntil(TokenType::SEMICOLON, TokenType::RPAREN);
        if (!match(TokenType::SEMICOLON)) {
            addError(peek().line, "Expected ';' after for condition");
        }
    } else {
        const Span condSpan = condition->span;
        consumeExpectedSemicolon("Expected ';' after for condition", &condSpan);
    }

    const Token nameToken = consume(
        TokenType::IDENTIFIER,
        "Expected variable name in for update"
    );

    std::string assignOp = "=";
    if (peek().type == TokenType::EQUAL || isCompoundAssignToken(peek().type)) {
        assignOp = std::string(lexeme(advance()));
    } else {
        consume(TokenType::EQUAL, "Expected '=' after variable name in for update");
    }

    std::unique_ptr<ASTNode> updateValue;
    const TokenType afterEqual = peek().type;
    if (afterEqual != TokenType::RPAREN &&
        afterEqual != TokenType::LBRACE &&
        afterEqual != TokenType::END_OF_FILE) {
        updateValue = parseExpression();
    }

    std::unique_ptr<ASTNode> update;
    if (nameToken.length > 0 && updateValue) {
        const Span updSpan = Span::merge(tokenSpan(nameToken), updateValue->span);
        auto updNode = std::make_unique<AssignmentASTNode>(
            std::string(lexeme(nameToken)),
            std::move(updateValue),
            std::move(assignOp)
        );
        updNode->span = updSpan;
        update = std::move(updNode);
    } else {
        skipUntil(TokenType::RPAREN);
    }

    consumeClosingParenthesis("Expected ')' after for clauses");
    consume(TokenType::LBRACE, "Expected '{' before for body");

    Span blockSpan;
    auto statements = parseBlock("Expected '}' after for body", &blockSpan);

    if (initialization && condition) {
        auto node = std::make_unique<ForASTNode>(
            std::move(initialization),
            std::move(condition),
            std::move(update),
            std::move(statements)
        );
        node->span = Span::merge(tokenSpan(forTok), blockSpan);
        return node;
    }

    return nullptr;
}

std::unique_ptr<FunctionASTNode> Parser::parseFunction(bool isMethod) {
    const Token doTok = consume(TokenType::DO, "Expected 'do'");
    const Token nameToken = consume(TokenType::IDENTIFIER, "Expected function name");
    auto generics = parseGenericParams();
    consumeOpeningParenthesis();

    std::vector<std::unique_ptr<ParameterASTNode>> parameters;

    if (peek().type != TokenType::RPAREN) {
        while (true) {
            const Token startParamTok = peek();
            bool paramMut = match(TokenType::MUT);

            if (peek().type == TokenType::SELF) {
                const Token selfTok = advance();
                auto param = std::make_unique<ParameterASTNode>("self", TypeSpec("self"), true, paramMut);
                param->span = Span::merge(tokenSpan(startParamTok), tokenSpan(selfTok));
                parameters.push_back(std::move(param));
            } else {
                const Token parameterName = consume(
                    TokenType::IDENTIFIER,
                    "Expected parameter name"
                );
                consume(TokenType::COLON, "Expected ':' after parameter name");
                TypeSpec paramType = parseTypeSpec();
                const Token endParamTok = previousToken();

                auto param = std::make_unique<ParameterASTNode>(
                    std::string(lexeme(parameterName)),
                    std::move(paramType),
                    false,
                    paramMut
                );
                const Token firstTok = parameterName.length > 0 ? parameterName : startParamTok;
                param->span = Span::merge(tokenSpan(firstTok), tokenSpan(endParamTok));
                parameters.push_back(std::move(param));
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
    }

    consumeClosingParenthesis("Expected ')' after function parameters");

    TypeSpec returnTypeSpec("void");
    if (match(TokenType::ARROW)) {
        returnTypeSpec = parseTypeSpec();
    }

    std::vector<std::unique_ptr<ASTNode>> statements;
    Span endSpan = tokenSpan(previousToken());
    if (match(TokenType::SEMICOLON)) {
        const Token semiTok = previousToken();
        endSpan = tokenSpan(semiTok);
        (void)isMethod;
    } else {
        consume(TokenType::LBRACE, "Expected '{' before function body");
        statements = parseBlock("Expected '}' after function body", &endSpan);
    }

    if (nameToken.length == 0) {
        return nullptr;
    }

    auto fn = std::make_unique<FunctionASTNode>(
        std::string(lexeme(nameToken)),
        std::move(returnTypeSpec),
        std::move(parameters),
        std::move(statements),
        std::move(generics),
        false
    );
    fn->span = Span::merge(tokenSpan(doTok), endSpan);
    return fn;
}

std::unique_ptr<ASTNode> Parser::parseVariableDeclaration(bool isConst, bool isStatic) {
    Token startTok = peek();
    bool isMut = false;
    if (!isConst && !isStatic) {
        isMut = match(TokenType::MUT);
        if (!isMut) {
            startTok = consume(TokenType::LET, "Expected 'let' or 'mut'");
        } else {
            startTok = previousToken();
        }
    } else if (isConst) {
        startTok = consume(TokenType::CONST, "Expected 'const'");
    } else if (isStatic) {
        startTok = consume(TokenType::STATIC, "Expected 'static'");
    }

    if (!isMut && match(TokenType::MUT)) {
        isMut = true;
    }

    const Token nameToken = consume(TokenType::IDENTIFIER, "Expected variable name");
    consume(TokenType::COLON, "Expected ':' after variable name");

    TypeSpec typeSpec = parseTypeSpec();

    bool hasInit = false;
    std::unique_ptr<ASTNode> value;
    if (match(TokenType::EQUAL)) {
        hasInit = true;
        value = parseExpression();
    } else if (isConst) {
        addError(peek().line, "Expected '=' initializer for const declaration");
    }

    const Span anchor = Span::merge(
        tokenSpan(startTok),
        value ? value->span : tokenSpan(previousToken())
    );
    if (!hasInit || value != nullptr) {
        consumeExpectedSemicolon("Expected ';' after variable declaration", &anchor);
    } else {
        recoverToStatementEnd();
    }

    if (nameToken.length > 0) {
        auto node = std::make_unique<VariableDeclarationASTNode>(
            std::string(lexeme(nameToken)),
            std::move(typeSpec),
            std::move(value),
            isMut,
            isConst,
            isStatic
        );
        const Span endSpan = previousToken().type == TokenType::SEMICOLON
            ? tokenSpan(previousToken())
            : anchor;
        node->span = Span::merge(tokenSpan(startTok), endSpan);
        return node;
    }

    recoverToStatementEnd();
    return nullptr;
}

std::unique_ptr<ASTNode> Parser::parseReturn() {
    const Token retTok = consume(TokenType::RETURN, "Expected 'return'");
    if (match(TokenType::SEMICOLON)) {
        const Token semiTok = previousToken();
        auto node = std::make_unique<ReturnASTNode>(nullptr);
        node->span = Span::merge(tokenSpan(retTok), tokenSpan(semiTok));
        return node;
    }
    auto value = parseExpression();
    if (!value) {
        recoverToStatementEnd();
        return nullptr;
    }
    const Span valSpan = value->span;
    consumeExpectedSemicolon("Expected ';' after return expression", &valSpan);
    auto node = std::make_unique<ReturnASTNode>(std::move(value));
    const Span endSpan = previousToken().type == TokenType::SEMICOLON
        ? tokenSpan(previousToken())
        : valSpan;
    node->span = Span::merge(tokenSpan(retTok), endSpan);
    return node;
}

std::unique_ptr<ASTNode> Parser::parseIfStatement() {
    const Token ifTok = consume(TokenType::IF, "Expected 'if'");
    consumeOpeningParenthesis();
    auto condition = parseExpression();
    if (!condition) {
        skipUntil(TokenType::RPAREN);
        match(TokenType::RPAREN);
    } else {
        consumeClosingParenthesis("Expected ')' after if condition");
    }
    consume(TokenType::LBRACE, "Expected '{' before if body");

    Span endSpan;
    auto statements = parseBlock("Expected '}' after if body", &endSpan);
    std::vector<std::unique_ptr<ASTNode>> elseStatements;

    if (match(TokenType::ELSE)) {
        if (peek().type == TokenType::IF) {
            auto elseIfNode = parseIfStatement();
            if (elseIfNode) {
                endSpan = elseIfNode->span;
                elseStatements.push_back(std::move(elseIfNode));
            }
        } else {
            consume(TokenType::LBRACE, "Expected '{' before else body");
            elseStatements = parseBlock("Expected '}' after else body", &endSpan);
        }
    }

    if (condition) {
        auto node = std::make_unique<IfASTNode>(
            std::move(condition),
            std::move(statements),
            std::move(elseStatements)
        );
        node->span = Span::merge(tokenSpan(ifTok), endSpan);
        return node;
    }

    return nullptr;
}

std::unique_ptr<ASTNode> Parser::parsePrintStatement() {
    const Token priTok = consume(TokenType::PRI, "Expected 'pri'");
    std::unique_ptr<ASTNode> expr;

    if (match(TokenType::LPAREN)) {
        expr = parseExpression();
        consumeClosingParenthesis("Expected ')' after pri argument");
    } else {
        const uint32_t openingLine = peek().line;
        const uint32_t openingColumn = peek().column;
        const uint32_t openingOffset = peek().offset;
        expr = parseExpression();

        if (!expr ||
            (peek().type != TokenType::SEMICOLON &&
             peek().type != TokenType::RBRACE &&
             peek().type != TokenType::END_OF_FILE)) {
            addDiagnostic(4, openingLine, openingColumn, openingOffset);
            if (peek().type != TokenType::SEMICOLON &&
                peek().type != TokenType::RBRACE &&
                peek().type != TokenType::END_OF_FILE) {
                skipUntil(TokenType::SEMICOLON, TokenType::RBRACE);
            }
        }
    }

    if (expr) {
        const Span exprSpan = expr->span;
        consumeExpectedSemicolon("Expected ';' after pri statement", &exprSpan);
        auto node = std::make_unique<PrintASTNode>(std::move(expr));
        const Span endSpan = previousToken().type == TokenType::SEMICOLON
            ? tokenSpan(previousToken())
            : exprSpan;
        node->span = Span::merge(tokenSpan(priTok), endSpan);
        return node;
    }

    recoverToStatementEnd();
    return nullptr;
}

std::unique_ptr<ASTNode> Parser::parseIdentifierStatement() {
    auto expr = parseExpression();
    if (!expr) {
        recoverToStatementEnd();
        return nullptr;
    }
    const Span exprSpan = expr->span;
    consumeExpectedSemicolon("Expected ';' after expression statement", &exprSpan);
    if (previousToken().type == TokenType::SEMICOLON) {
        expr->span = Span::merge(exprSpan, tokenSpan(previousToken()));
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    const Token current = peek();

    switch (current.type) {
        case TokenType::MODULE:
            return parseModuleDecl();
        case TokenType::IMPORT:
            return parseImportDecl();
        case TokenType::STRUCT:
            return parseStructDecl();
        case TokenType::UNION:
            return parseUnionDecl();
        case TokenType::ENUM:
            return parseEnumDecl();
        case TokenType::IMPL:
            return parseImplBlock();
        case TokenType::EXTERN:
            return parseExternFunction();
        case TokenType::MATCH:
            return parseMatchStatement();
        case TokenType::WHILE:
            return parseWhile();
        case TokenType::FOR:
            return parseFor();
        case TokenType::BREAK:
            return parseBreak();
        case TokenType::CONTINUE:
            return parseContinue();
        case TokenType::DO:
            return parseFunction(false);
        case TokenType::LET:
        case TokenType::MUT:
            return parseVariableDeclaration(false, false);
        case TokenType::CONST:
            return parseVariableDeclaration(true, false);
        case TokenType::STATIC:
            return parseVariableDeclaration(false, true);
        case TokenType::RETURN:
            return parseReturn();
        case TokenType::IF:
            return parseIfStatement();
        case TokenType::PRI:
            return parsePrintStatement();
        case TokenType::IDENTIFIER:
        case TokenType::SELF:
        case TokenType::MULTIPLY:
            return parseIdentifierStatement();
        case TokenType::RBRACE:
            addError(current.line, "Unexpected '}'");
            advance();
            return nullptr;
        default: {
            const Token tok = advance();
            addError(tok.line, "Unknown statement type at: " + std::string(lexeme(tok)));
            synchronize();
            return nullptr;
        }
    }
}

std::unique_ptr<ProgramASTNode> Parser::parse() {
    auto program = std::make_unique<ProgramASTNode>();
    bool hasFirst = false;
    Span firstSpan;
    Span lastSpan;

    while (peek().type != TokenType::END_OF_FILE) {
        const std::size_t previousCursor = cursor;
        auto statement = parseStatement();

        if (statement) {
            if (!hasFirst) {
                firstSpan = statement->span;
                hasFirst = true;
            }
            lastSpan = statement->span;
            program->statements.push_back(std::move(statement));
        }

        if (cursor == previousCursor) {
            synchronize();
            if (cursor == previousCursor && peek().type != TokenType::END_OF_FILE) {
                advance();
            }
        }
    }

    if (hasFirst) {
        program->span = Span::merge(firstSpan, lastSpan);
    }

    return program;
}

const std::vector<std::string>& Parser::getErrors() const noexcept {
    return errors;
}

const std::vector<Diagnostic>& Parser::getDiagnostics() const noexcept {
    return diagnostics;
}

} // namespace stobx
