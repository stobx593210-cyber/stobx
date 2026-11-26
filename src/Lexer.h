#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace stobx {

enum class TokenType : uint8_t {
    // Keywords (contiguous range: LET .. SIZEOF)
    LET,
    MUT,
    CONST,
    STATIC,
    TYPE,
    DO, // Function / block keyword
    FN,
    EXTERN,
    STRUCT,
    UNION,
    ENUM,
    IMPL,
    SELF,
    MODULE,
    IMPORT,
    RETURN,
    IF,
    ELSE,
    MATCH,
    WHILE,
    FOR,
    LOOP,
    BREAK,
    CONTINUE,
    PRI, // Built-in / legacy print keyword
    AND,
    OR,
    NOT,
    AS,
    SIZEOF,

    // Literal Keywords & Primitive Types (contiguous range: TRUE .. VOID)
    TRUE,
    FALSE,
    NULL_LITERAL,
    U8,
    U16,
    U32,
    U64,
    USIZE,
    I8,
    I16,
    I32,
    I64,
    ISIZE,
    F32,
    F64,
    BOOL,
    CHAR,
    VOID,

    // Literals & Identifiers
    IDENTIFIER,
    NUMBER,
    FLOAT_LITERAL,
    HEX_LITERAL,
    BINARY_LITERAL,
    CHAR_LITERAL,
    STRING_LITERAL,

    // Operators
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    MODULO,
    PLUS_EQUAL,
    MINUS_EQUAL,
    MULTIPLY_EQUAL,
    DIVIDE_EQUAL,
    MODULO_EQUAL,
    SHIFT_LEFT_EQUAL,
    SHIFT_RIGHT_EQUAL,
    AMPERSAND_EQUAL,
    PIPE_EQUAL,
    CARET_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    NOT_EQUAL,
    TILDE,
    GREATER,
    LESS,
    GREATER_EQUAL,
    LESS_EQUAL,
    SHIFT_LEFT,
    SHIFT_RIGHT,
    AMPERSAND,
    CARET,
    PIPE,
    ARROW,
    FAT_ARROW,
    QUESTION,
    AT,
    ELLIPSIS,

    // Delimiters
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    DOT,
    COLON,
    SCOPE_RESOLUTION,
    SEMICOLON,

    // Errors & EOF
    INVALID_CHAR,
    INVALID_NUMBER,
    UNTERMINATED_STRING,
    UNTERMINATED_CHAR,
    END_OF_FILE
};

enum class LexSeverity : uint8_t {
    Error,
    Warning
};

constexpr bool isKeyword(TokenType t) noexcept {
    return t >= TokenType::LET && t <= TokenType::SIZEOF;
}

constexpr bool isLiteralKeyword(TokenType t) noexcept {
    return t >= TokenType::TRUE && t <= TokenType::NULL_LITERAL;
}

constexpr bool isPrimitiveType(TokenType t) noexcept {
    return t >= TokenType::U8 && t <= TokenType::VOID;
}

const char* tokenTypeName(TokenType type) noexcept;

struct LexDiagnostic {
    std::string message;
    uint32_t offset = 0;
    uint32_t line = 1;
    uint32_t column = 1;
    LexSeverity severity = LexSeverity::Error;
};

struct Token {
    TokenType type;
    uint32_t offset;
    uint32_t length;
    uint32_t line;
    uint32_t column;

    [[nodiscard]] constexpr std::string_view text(std::string_view source) const noexcept {
        if (offset >= source.size()) {
            return {};
        }
        const std::size_t maxLen = source.size() - static_cast<std::size_t>(offset);
        const std::size_t safeLen = static_cast<std::size_t>(length) < maxLen
            ? static_cast<std::size_t>(length)
            : maxLen;
        return source.substr(static_cast<std::size_t>(offset), safeLen);
    }
};

static_assert(sizeof(Token) <= 20, "Token size exceeded expected limit");

class Lexer {
public:
    explicit Lexer(std::string_view src) noexcept;

    [[nodiscard]] std::vector<Token> tokenize();
    [[nodiscard]] const std::vector<LexDiagnostic>& diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    std::string_view source;
    std::vector<LexDiagnostic> diagnostics_;
    std::size_t cursor = 0;
    uint32_t line = 1;
    uint32_t column = 1;

    constexpr bool isAtEnd() const noexcept {
        return cursor >= source.size();
    }

    constexpr char peek(std::size_t lookahead = 0) const noexcept {
        if (cursor + lookahead >= source.size()) {
            return '\0';
        }
        return source[cursor + lookahead];
    }

    char advance() noexcept;
    void advanceBy(std::size_t count) noexcept;
    bool match(char expected) noexcept;
    void skipWhitespaceAndComments();
    void reportError(
        std::string message,
        uint32_t errOffset,
        uint32_t errLine,
        uint32_t errColumn,
        LexSeverity severity = LexSeverity::Error
    );

    bool scanEscapeBody() noexcept;
    std::size_t validUtf8Length(std::size_t at) const noexcept;
    bool isInvisibleCodepoint(std::size_t at, std::size_t n) const noexcept;
    bool isBidiControl(std::size_t at) const noexcept;
    bool warnIfInvisibleCodepoint(std::size_t at, const char* contextMessage);

    // Returns NUMBER / FLOAT_LITERAL / HEX_LITERAL / BINARY_LITERAL, or INVALID_NUMBER on malformed literal.
    Token scanNumber();

    // Returns STRING_LITERAL / INVALID_CHAR, or UNTERMINATED_STRING if EOF or newline/CR is reached before closing quote.
    Token scanString();

    // Returns CHAR_LITERAL, or UNTERMINATED_CHAR / INVALID_CHAR if malformed or unclosed.
    Token scanChar();

    // Returns a keyword, literal keyword, or IDENTIFIER, or INVALID_CHAR with diagnostic on bad UTF-8/invisible char.
    Token scanIdentifierOrKeyword();

    // Consumes invalid UTF-8 byte(s) and returns INVALID_CHAR.
    Token scanInvalidUtf8Sequence(uint32_t startOffset, uint32_t startLine, uint32_t startColumn);

    static constexpr bool isAsciiDigit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    static constexpr bool isHexDigit(char c) noexcept {
        return (c >= '0' && c <= '9') ||
               (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    }

    static constexpr bool isBinaryDigit(char c) noexcept {
        return c == '0' || c == '1';
    }

    static constexpr bool isAsciiAlpha(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    static constexpr bool isUtf8Start(unsigned char c) noexcept {
        return c >= 0xC2 && c <= 0xF4;
    }

    static constexpr bool isUtf8Continuation(unsigned char c) noexcept {
        return (c & 0xC0) == 0x80;
    }

    static TokenType resolveKeyword(std::string_view lexeme) noexcept;
};

} // namespace stobx

