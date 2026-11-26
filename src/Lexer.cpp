#include "Lexer.h"

#include <algorithm>
#include <utility>

namespace stobx {

namespace {

constexpr std::size_t kMaxSourceSize = 64u * 1024u * 1024u; // 64 MiB
constexpr std::size_t kMaxReserve = 1u << 20; // 1M tokens
constexpr std::size_t kMaxDiagnostics = 200u;

constexpr Token makeToken(
    TokenType type,
    uint32_t startOffset,
    uint32_t length,
    uint32_t startLine,
    uint32_t startColumn
) noexcept {
    return Token{type, startOffset, length, startLine, startColumn};
}

} // namespace

Lexer::Lexer(std::string_view src) noexcept : source(src) {}

char Lexer::advance() noexcept {
    if (isAtEnd()) {
        return '\0';
    }
    const char c = source[cursor++];
    if (c == '\n') {
        ++line;
        column = 1;
    } else {
        ++column;
    }
    return c;
}

void Lexer::advanceBy(std::size_t count) noexcept {
    for (std::size_t i = 0; i < count && !isAtEnd(); ++i) {
        advance();
    }
}

bool Lexer::match(char expected) noexcept {
    if (isAtEnd() || source[cursor] != expected) {
        return false;
    }
    advance();
    return true;
}

bool Lexer::isBidiControl(std::size_t at) const noexcept {
    if (at + 2 >= source.size()) {
        return false;
    }
    const auto b0 = static_cast<unsigned char>(source[at]);
    const auto b1 = static_cast<unsigned char>(source[at + 1]);
    const auto b2 = static_cast<unsigned char>(source[at + 2]);
    if (b0 != 0xE2) {
        return false;
    }
    if (b1 == 0x80 && b2 >= 0xAA && b2 <= 0xAE) {
        return true; // U+202A..U+202E
    }
    if (b1 == 0x81 && b2 >= 0xA6 && b2 <= 0xA9) {
        return true; // U+2066..U+2069
    }
    return false;
}

std::size_t Lexer::validUtf8Length(std::size_t at) const noexcept {
    const auto byte = [&](std::size_t i) noexcept -> unsigned char {
        return i < source.size() ? static_cast<unsigned char>(source[i]) : 0u;
    };
    const unsigned char b0 = byte(at);
    if (b0 < 0x80) {
        return 1u;
    }
    if (b0 >= 0xC2 && b0 <= 0xDF) {
        return isUtf8Continuation(byte(at + 1)) ? 2u : 0u;
    }
    if (b0 >= 0xE0 && b0 <= 0xEF) {
        const unsigned char b1 = byte(at + 1);
        const unsigned char lo = (b0 == 0xE0) ? 0xA0 : 0x80;
        const unsigned char hi = (b0 == 0xED) ? 0x9F : 0xBF;
        if (b1 < lo || b1 > hi) {
            return 0u;
        }
        return isUtf8Continuation(byte(at + 2)) ? 3u : 0u;
    }
    if (b0 >= 0xF0 && b0 <= 0xF4) {
        const unsigned char b1 = byte(at + 1);
        const unsigned char lo = (b0 == 0xF0) ? 0x90 : 0x80;
        const unsigned char hi = (b0 == 0xF4) ? 0x8F : 0xBF;
        if (b1 < lo || b1 > hi) {
            return 0u;
        }
        return (isUtf8Continuation(byte(at + 2)) &&
                isUtf8Continuation(byte(at + 3))) ? 4u : 0u;
    }
    return 0u;
}

bool Lexer::isInvisibleCodepoint(std::size_t at, std::size_t n) const noexcept {
    if (at + n > source.size()) {
        return false;
    }
    const auto b0 = static_cast<unsigned char>(source[at]);
    const auto b1 = static_cast<unsigned char>(source[at + 1]);
    if (n == 2) {
        return b0 == 0xC2 && b1 == 0xA0; // U+00A0 NBSP
    }
    if (n == 3) {
        const auto b2 = static_cast<unsigned char>(source[at + 2]);
        if (b0 == 0xE2 && b1 == 0x80 && (b2 <= 0x8F || (b2 >= 0xA8 && b2 <= 0xAF))) {
            return true; // U+2000..U+200F, U+2028..U+202F
        }
        if (b0 == 0xE2 && b1 == 0x81 && b2 >= 0xA0 && b2 <= 0xAF) {
            return true; // U+2060..U+206F
        }
        if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
            return true; // U+FEFF BOM
        }
    }
    return false;
}

bool Lexer::warnIfInvisibleCodepoint(std::size_t at, const char* contextMessage) {
    if (at >= source.size()) {
        return false;
    }
    const auto uc = static_cast<unsigned char>(source[at]);
    if (uc < 0x80) {
        return false;
    }
    const std::size_t n = validUtf8Length(at);
    if (n > 1 && isInvisibleCodepoint(at, n)) {
        reportError(
            contextMessage,
            static_cast<uint32_t>(at),
            line,
            column,
            LexSeverity::Warning
        );
        advanceBy(n);
        return true;
    }
    return false;
}

bool Lexer::scanEscapeBody() noexcept {
    if (isAtEnd() || peek() == '\n' || peek() == '\r') {
        return false;
    }
    const char e = peek();
    advance();
    switch (e) {
        case 'n':
        case 't':
        case 'r':
        case '0':
        case '\\':
        case '\'':
        case '"':
            return true;
        case 'x':
            for (int i = 0; i < 2; ++i) {
                if (!isHexDigit(peek())) {
                    return false;
                }
                advance();
            }
            return true;
        case 'u': {
            if (peek() != '{') {
                return false;
            }
            advance(); // '{'
            uint32_t codePoint = 0u;
            int digits = 0;
            while (isHexDigit(peek())) {
                const char h = peek();
                uint32_t val = 0u;
                if (h >= '0' && h <= '9') {
                    val = static_cast<uint32_t>(h - '0');
                } else if (h >= 'a' && h <= 'f') {
                    val = static_cast<uint32_t>(h - 'a' + 10);
                } else {
                    val = static_cast<uint32_t>(h - 'A' + 10);
                }
                codePoint = (codePoint << 4) | val;
                advance();
                ++digits;
            }
            if (digits < 1 || digits > 6 || peek() != '}') {
                return false;
            }
            advance(); // '}'
            if (codePoint > 0x10FFFFu || (codePoint >= 0xD800u && codePoint <= 0xDFFFu)) {
                return false;
            }
            return true;
        }
        default:
            return false;
    }
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!isAtEnd() && peek() != '\n') {
                if (isBidiControl(cursor)) {
                    reportError(
                        "Bidirectional control character in comment",
                        static_cast<uint32_t>(cursor),
                        line,
                        column
                    );
                }
                advance();
            }
        } else if (c == '/' && peek(1) == '*') {
            const uint32_t cOff = static_cast<uint32_t>(cursor);
            const uint32_t cLine = line;
            const uint32_t cCol = column;
            advance(); // '/'
            advance(); // '*'
            int depth = 1;
            while (!isAtEnd() && depth > 0) {
                if (peek() == '/' && peek(1) == '*') {
                    advance(); // '/'
                    advance(); // '*'
                    ++depth;
                    continue;
                }
                if (peek() == '*' && peek(1) == '/') {
                    advance(); // '*'
                    advance(); // '/'
                    --depth;
                    continue;
                }
                if (isBidiControl(cursor)) {
                    reportError(
                        "Bidirectional control character in comment",
                        static_cast<uint32_t>(cursor),
                        line,
                        column
                    );
                }
                advance();
            }
            if (depth > 0) {
                reportError("Unterminated block comment", cOff, cLine, cCol);
            }
        } else {
            break;
        }
    }
}

void Lexer::reportError(
    std::string message,
    uint32_t errOffset,
    uint32_t errLine,
    uint32_t errColumn,
    LexSeverity severity
) {
    if (diagnostics_.size() > kMaxDiagnostics) {
        return;
    }
    if (diagnostics_.size() == kMaxDiagnostics) {
        diagnostics_.push_back(LexDiagnostic{
            "Too many errors; further diagnostics suppressed",
            errOffset,
            errLine,
            errColumn,
            LexSeverity::Error
        });
        return;
    }
    diagnostics_.push_back(LexDiagnostic{
        std::move(message),
        errOffset,
        errLine,
        errColumn,
        severity
    });
}
Token Lexer::scanNumber() {
    const uint32_t startOffset = static_cast<uint32_t>(cursor);
    const uint32_t startLine = line;
    const uint32_t startColumn = column;

    bool invalid = false;
    bool underscoreError = false;
    TokenType kind = TokenType::NUMBER;

    auto consumeRadixDigits = [&](auto&& isValidDigit) noexcept -> bool {
        bool hasDigits = false;
        bool lastWasUnderscore = false;
        while (!isAtEnd()) {
            const char ch = peek();
            if (isValidDigit(ch)) {
                hasDigits = true;
                lastWasUnderscore = false;
                advance();
            } else if (ch == '_') {
                if (!hasDigits || lastWasUnderscore) {
                    invalid = true;
                    underscoreError = true;
                }
                lastWasUnderscore = true;
                advance();
            } else {
                break;
            }
        }
        if (lastWasUnderscore) {
            invalid = true;
            underscoreError = true;
        }
        return hasDigits;
    };

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        kind = TokenType::HEX_LITERAL;
        advance(); // '0'
        advance(); // 'x' or 'X'
        if (!consumeRadixDigits([](char ch) noexcept { return isHexDigit(ch); })) {
            invalid = true;
        }
    } else if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
        kind = TokenType::BINARY_LITERAL;
        advance(); // '0'
        advance(); // 'b' or 'B'
        if (!consumeRadixDigits([](char ch) noexcept { return isBinaryDigit(ch); })) {
            invalid = true;
        }
    } else {
        consumeRadixDigits([](char ch) noexcept { return isAsciiDigit(ch); });

        if (!isAtEnd() && peek() == '.' && isAsciiDigit(peek(1))) {
            kind = TokenType::FLOAT_LITERAL;
            advance(); // '.'
            if (!consumeRadixDigits([](char ch) noexcept { return isAsciiDigit(ch); })) {
                invalid = true;
            }
        }

        if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
            kind = TokenType::FLOAT_LITERAL;
            advance(); // 'e' or 'E'
            if (!isAtEnd() && (peek() == '+' || peek() == '-')) {
                advance();
            }
            if (!consumeRadixDigits([](char ch) noexcept { return isAsciiDigit(ch); })) {
                invalid = true;
            }
        }
    }

    // Consume trailing malformed characters, while leaving a standalone '.' (e.g. 123.method) for DOT token.
    while (!isAtEnd()) {
        const char c = peek();
        const unsigned char uc = static_cast<unsigned char>(c);
        if (c == '.') {
            if (isAsciiDigit(peek(1))) {
                invalid = true;
                advance();
            } else {
                break;
            }
        } else if (isAsciiDigit(c) || isAsciiAlpha(c) || isUtf8Start(uc)) {
            invalid = true;
            advance();
        } else {
            break;
        }
    }

    const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;
    if (invalid) {
        const char* message = underscoreError
            ? "Invalid underscore placement in numeric literal"
            : "Invalid numeric literal";
        reportError(message, startOffset, startLine, startColumn);
        return makeToken(TokenType::INVALID_NUMBER, startOffset, length, startLine, startColumn);
    }

    return makeToken(kind, startOffset, length, startLine, startColumn);
}

Token Lexer::scanString() {
    const uint32_t startOffset = static_cast<uint32_t>(cursor);
    const uint32_t startLine = line;
    const uint32_t startColumn = column;

    bool invalidEscape = false;

    advance(); // consume opening '"'

    while (!isAtEnd() && peek() != '"' && peek() != '\n' && peek() != '\r') {
        if (peek() == '\\') {
            const uint32_t eOff = static_cast<uint32_t>(cursor);
            const uint32_t eLine = line;
            const uint32_t eCol = column;
            advance(); // '\\'
            if (!scanEscapeBody()) {
                reportError("Invalid escape sequence", eOff, eLine, eCol);
                invalidEscape = true;
            }
        } else {
            if (isBidiControl(cursor)) {
                reportError(
                    "Bidirectional control character in string literal",
                    static_cast<uint32_t>(cursor),
                    line,
                    column
                );
            }
            if (warnIfInvisibleCodepoint(cursor, "Invisible Unicode character in string literal")) {
                continue;
            }
            advance();
        }
    }

    if (isAtEnd() || peek() == '\n' || peek() == '\r') {
        const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;
        reportError("Unterminated string literal", startOffset, startLine, startColumn);
        return makeToken(TokenType::UNTERMINATED_STRING, startOffset, length, startLine, startColumn);
    }

    advance(); // consume closing '"'
    const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;
    if (invalidEscape) {
        return makeToken(TokenType::INVALID_CHAR, startOffset, length, startLine, startColumn);
    }
    return makeToken(TokenType::STRING_LITERAL, startOffset, length, startLine, startColumn);
}

Token Lexer::scanChar() {
    const uint32_t startOffset = static_cast<uint32_t>(cursor);
    const uint32_t startLine = line;
    const uint32_t startColumn = column;

    advance(); // consume opening '\''

    if (isAtEnd() || peek() == '\n' || peek() == '\r') {
        const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;
        reportError("Unterminated character literal", startOffset, startLine, startColumn);
        return makeToken(TokenType::UNTERMINATED_CHAR, startOffset, length, startLine, startColumn);
    }

    if (peek() == '\'') {
        advance(); // consume closing '\''
        const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;
        reportError("Empty character literal", startOffset, startLine, startColumn);
        return makeToken(TokenType::INVALID_CHAR, startOffset, length, startLine, startColumn);
    }

    bool invalid = false;

    if (isBidiControl(cursor)) {
        reportError(
            "Bidirectional control character in character literal",
            static_cast<uint32_t>(cursor),
            line,
            column
        );
        invalid = true;
    }

    if (peek() == '\\') {
        advance(); // '\\'
        if (!scanEscapeBody()) {
            invalid = true;
        }
    } else if (static_cast<unsigned char>(peek()) >= 0x80) {
        const std::size_t n = validUtf8Length(cursor);
        if (n == 0) {
            invalid = true;
            advance();
        } else {
            if (!warnIfInvisibleCodepoint(cursor, "Invisible Unicode character in character literal")) {
                advanceBy(n);
            }
        }
    } else {
        advance();
    }

    while (!isAtEnd() && peek() != '\'' && peek() != '\n' && peek() != '\r') {
        if (isBidiControl(cursor)) {
            reportError(
                "Bidirectional control character in character literal",
                static_cast<uint32_t>(cursor),
                line,
                column
            );
        }
        invalid = true;
        advance();
    }

    if (isAtEnd() || peek() == '\n' || peek() == '\r') {
        const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;
        reportError("Unterminated character literal", startOffset, startLine, startColumn);
        return makeToken(TokenType::UNTERMINATED_CHAR, startOffset, length, startLine, startColumn);
    }

    advance(); // consume closing '\''
    const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;

    if (invalid) {
        reportError("Invalid character literal", startOffset, startLine, startColumn);
        return makeToken(TokenType::INVALID_CHAR, startOffset, length, startLine, startColumn);
    }

    return makeToken(TokenType::CHAR_LITERAL, startOffset, length, startLine, startColumn);
}

Token Lexer::scanIdentifierOrKeyword() {
    const uint32_t startOffset = static_cast<uint32_t>(cursor);
    const uint32_t startLine = line;
    const uint32_t startColumn = column;
    bool bad = false;

    while (!isAtEnd()) {
        const unsigned char uc = static_cast<unsigned char>(peek());
        if (isAsciiAlpha(static_cast<char>(uc)) || isAsciiDigit(static_cast<char>(uc))) {
            advance();
        } else if (uc >= 0x80) {
            const std::size_t n = validUtf8Length(cursor);
            if (n == 0) {
                reportError(
                    "Invalid UTF-8 byte sequence in identifier",
                    static_cast<uint32_t>(cursor),
                    line,
                    column
                );
                advance();
                bad = true;
                break;
            }
            if (isInvisibleCodepoint(cursor, n)) {
                reportError(
                    "Invisible or whitespace Unicode character in identifier",
                    static_cast<uint32_t>(cursor),
                    line,
                    column
                );
                advanceBy(n);
                bad = true;
                break;
            }
            advanceBy(n);
        } else {
            break;
        }
    }

    const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;
    if (bad) {
        return makeToken(TokenType::INVALID_CHAR, startOffset, length, startLine, startColumn);
    }

    const TokenType type = resolveKeyword(source.substr(startOffset, length));
    return makeToken(type, startOffset, length, startLine, startColumn);
}

Token Lexer::scanInvalidUtf8Sequence(
    uint32_t startOffset,
    uint32_t startLine,
    uint32_t startColumn
) {
    while (!isAtEnd() && isUtf8Continuation(static_cast<unsigned char>(peek()))) {
        advance();
    }
    const uint32_t length = static_cast<uint32_t>(cursor) - startOffset;
    reportError("Invalid UTF-8 byte sequence", startOffset, startLine, startColumn);
    return makeToken(TokenType::INVALID_CHAR, startOffset, length, startLine, startColumn);
}

TokenType Lexer::resolveKeyword(std::string_view lexeme) noexcept {
    switch (lexeme.size()) {
        case 2:
            if (lexeme == "do") return TokenType::DO;
            if (lexeme == "fn") return TokenType::FN;
            if (lexeme == "if") return TokenType::IF;
            if (lexeme == "or") return TokenType::OR;
            if (lexeme == "as") return TokenType::AS;
            if (lexeme == "u8") return TokenType::U8;
            if (lexeme == "i8") return TokenType::I8;
            break;
        case 3:
            if (lexeme == "let") return TokenType::LET;
            if (lexeme == "mut") return TokenType::MUT;
            if (lexeme == "for") return TokenType::FOR;
            if (lexeme == "pri") return TokenType::PRI;
            if (lexeme == "and") return TokenType::AND;
            if (lexeme == "not") return TokenType::NOT;
            if (lexeme == "u16") return TokenType::U16;
            if (lexeme == "u32") return TokenType::U32;
            if (lexeme == "u64") return TokenType::U64;
            if (lexeme == "i16") return TokenType::I16;
            if (lexeme == "i32") return TokenType::I32;
            if (lexeme == "i64") return TokenType::I64;
            if (lexeme == "f32") return TokenType::F32;
            if (lexeme == "f64") return TokenType::F64;
            break;
        case 4:
            if (lexeme == "type") return TokenType::TYPE;
            if (lexeme == "enum") return TokenType::ENUM;
            if (lexeme == "impl") return TokenType::IMPL;
            if (lexeme == "self") return TokenType::SELF;
            if (lexeme == "else") return TokenType::ELSE;
            if (lexeme == "loop") return TokenType::LOOP;
            if (lexeme == "true") return TokenType::TRUE;
            if (lexeme == "null") return TokenType::NULL_LITERAL;
            if (lexeme == "bool") return TokenType::BOOL;
            if (lexeme == "char") return TokenType::CHAR;
            if (lexeme == "void") return TokenType::VOID;
            break;
        case 5:
            if (lexeme == "const") return TokenType::CONST;
            if (lexeme == "union") return TokenType::UNION;
            if (lexeme == "match") return TokenType::MATCH;
            if (lexeme == "while") return TokenType::WHILE;
            if (lexeme == "break") return TokenType::BREAK;
            if (lexeme == "false") return TokenType::FALSE;
            if (lexeme == "usize") return TokenType::USIZE;
            if (lexeme == "isize") return TokenType::ISIZE;
            break;
        case 6:
            if (lexeme == "static") return TokenType::STATIC;
            if (lexeme == "extern") return TokenType::EXTERN;
            if (lexeme == "struct") return TokenType::STRUCT;
            if (lexeme == "module") return TokenType::MODULE;
            if (lexeme == "import") return TokenType::IMPORT;
            if (lexeme == "return") return TokenType::RETURN;
            if (lexeme == "sizeof") return TokenType::SIZEOF;
            break;
        case 8:
            if (lexeme == "continue") return TokenType::CONTINUE;
            break;
        default:
            break;
    }
    return TokenType::IDENTIFIER;
}
std::vector<Token> Lexer::tokenize() {
    cursor = 0;
    line = 1;
    column = 1;
    diagnostics_.clear();

    if (source.size() > kMaxSourceSize) {
        reportError("Source input exceeds maximum supported size", 0u, 1u, 1u);
        return {makeToken(TokenType::END_OF_FILE, 0u, 0u, 1u, 1u)};
    }

    if (source.size() >= 3 &&
        static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        cursor = 3;
        column = 4;
    }

    std::vector<Token> tokens;
    tokens.reserve(std::min(source.size() / 4 + 1, kMaxReserve));

    while (true) {
        skipWhitespaceAndComments();

        const uint32_t startOffset = static_cast<uint32_t>(cursor);
        const uint32_t startLine = line;
        const uint32_t startColumn = column;

        auto emit = [&](TokenType t, uint32_t len) {
            tokens.push_back(makeToken(t, startOffset, len, startLine, startColumn));
        };

        if (isAtEnd()) {
            emit(TokenType::END_OF_FILE, 0u);
            break;
        }

        const char c = peek();
        const unsigned char uc = static_cast<unsigned char>(c);

        if (isAsciiDigit(c)) {
            tokens.push_back(scanNumber());
            continue;
        }

        if (isAsciiAlpha(c) || isUtf8Start(uc)) {
            tokens.push_back(scanIdentifierOrKeyword());
            continue;
        }

        if (uc >= 0x80) {
            advance();
            tokens.push_back(scanInvalidUtf8Sequence(startOffset, startLine, startColumn));
            continue;
        }

        if (c == '"') {
            tokens.push_back(scanString());
            continue;
        }

        if (c == '\'') {
            tokens.push_back(scanChar());
            continue;
        }

        advance();

        switch (c) {
            case '+':
                emit(match('=') ? TokenType::PLUS_EQUAL : TokenType::PLUS,
                     static_cast<uint32_t>(cursor - startOffset));
                break;
            case '-':
                if (match('>')) {
                    emit(TokenType::ARROW, 2u);
                } else if (match('=')) {
                    emit(TokenType::MINUS_EQUAL, 2u);
                } else {
                    emit(TokenType::MINUS, 1u);
                }
                break;
            case '*':
                emit(match('=') ? TokenType::MULTIPLY_EQUAL : TokenType::MULTIPLY,
                     static_cast<uint32_t>(cursor - startOffset));
                break;
            case '/':
                emit(match('=') ? TokenType::DIVIDE_EQUAL : TokenType::DIVIDE,
                     static_cast<uint32_t>(cursor - startOffset));
                break;
            case '%':
                emit(match('=') ? TokenType::MODULO_EQUAL : TokenType::MODULO,
                     static_cast<uint32_t>(cursor - startOffset));
                break;
            case '=':
                if (match('=')) {
                    emit(TokenType::EQUAL_EQUAL, 2u);
                } else if (match('>')) {
                    emit(TokenType::FAT_ARROW, 2u);
                } else {
                    emit(TokenType::EQUAL, 1u);
                }
                break;
            case '!':
                if (match('=')) {
                    emit(TokenType::NOT_EQUAL, 2u);
                } else {
                    reportError("Unexpected character '!' (use 'not' for logical negation)", startOffset, startLine, startColumn);
                    emit(TokenType::INVALID_CHAR, 1u);
                }
                break;
            case '~':
                emit(TokenType::TILDE, 1u);
                break;
            case '>':
                if (match('>')) {
                    emit(match('=') ? TokenType::SHIFT_RIGHT_EQUAL : TokenType::SHIFT_RIGHT,
                         static_cast<uint32_t>(cursor - startOffset));
                } else if (match('=')) {
                    emit(TokenType::GREATER_EQUAL, 2u);
                } else {
                    emit(TokenType::GREATER, 1u);
                }
                break;
            case '<':
                if (match('<')) {
                    emit(match('=') ? TokenType::SHIFT_LEFT_EQUAL : TokenType::SHIFT_LEFT,
                         static_cast<uint32_t>(cursor - startOffset));
                } else if (match('=')) {
                    emit(TokenType::LESS_EQUAL, 2u);
                } else {
                    emit(TokenType::LESS, 1u);
                }
                break;
            case '&':
                emit(match('=') ? TokenType::AMPERSAND_EQUAL : TokenType::AMPERSAND,
                     static_cast<uint32_t>(cursor - startOffset));
                break;
            case '^':
                emit(match('=') ? TokenType::CARET_EQUAL : TokenType::CARET,
                     static_cast<uint32_t>(cursor - startOffset));
                break;
            case '|':
                emit(match('=') ? TokenType::PIPE_EQUAL : TokenType::PIPE,
                     static_cast<uint32_t>(cursor - startOffset));
                break;
            case '?':
                emit(TokenType::QUESTION, 1u);
                break;
            case '@':
                emit(TokenType::AT, 1u);
                break;
            case '.':
                if (peek() == '.' && peek(1) == '.') {
                    advanceBy(2u);
                    emit(TokenType::ELLIPSIS, 3u);
                } else {
                    emit(TokenType::DOT, 1u);
                }
                break;
            case '(':
                emit(TokenType::LPAREN, 1u);
                break;
            case ')':
                emit(TokenType::RPAREN, 1u);
                break;
            case '{':
                emit(TokenType::LBRACE, 1u);
                break;
            case '}':
                emit(TokenType::RBRACE, 1u);
                break;
            case '[':
                emit(TokenType::LBRACKET, 1u);
                break;
            case ']':
                emit(TokenType::RBRACKET, 1u);
                break;
            case ',':
                emit(TokenType::COMMA, 1u);
                break;
            case ':':
                emit(match(':') ? TokenType::SCOPE_RESOLUTION : TokenType::COLON,
                     static_cast<uint32_t>(cursor - startOffset));
                break;
            case ';':
                emit(TokenType::SEMICOLON, 1u);
                break;
            default:
                reportError("Unexpected character in source", startOffset, startLine, startColumn);
                emit(TokenType::INVALID_CHAR, 1u);
                break;
        }
    }

    return tokens;
}

const char* tokenTypeName(TokenType type) noexcept {
    switch (type) {
        case TokenType::LET: return "LET";
        case TokenType::MUT: return "MUT";
        case TokenType::CONST: return "CONST";
        case TokenType::STATIC: return "STATIC";
        case TokenType::TYPE: return "TYPE";
        case TokenType::DO: return "DO";
        case TokenType::FN: return "FN";
        case TokenType::EXTERN: return "EXTERN";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::UNION: return "UNION";
        case TokenType::ENUM: return "ENUM";
        case TokenType::IMPL: return "IMPL";
        case TokenType::SELF: return "SELF";
        case TokenType::MODULE: return "MODULE";
        case TokenType::IMPORT: return "IMPORT";
        case TokenType::RETURN: return "RETURN";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::MATCH: return "MATCH";
        case TokenType::WHILE: return "WHILE";
        case TokenType::FOR: return "FOR";
        case TokenType::LOOP: return "LOOP";
        case TokenType::BREAK: return "BREAK";
        case TokenType::CONTINUE: return "CONTINUE";
        case TokenType::PRI: return "PRI";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::AS: return "AS";
        case TokenType::SIZEOF: return "SIZEOF";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::NULL_LITERAL: return "NULL_LITERAL";
        case TokenType::U8: return "U8";
        case TokenType::U16: return "U16";
        case TokenType::U32: return "U32";
        case TokenType::U64: return "U64";
        case TokenType::USIZE: return "USIZE";
        case TokenType::I8: return "I8";
        case TokenType::I16: return "I16";
        case TokenType::I32: return "I32";
        case TokenType::I64: return "I64";
        case TokenType::ISIZE: return "ISIZE";
        case TokenType::F32: return "F32";
        case TokenType::F64: return "F64";
        case TokenType::BOOL: return "BOOL";
        case TokenType::CHAR: return "CHAR";
        case TokenType::VOID: return "VOID";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::HEX_LITERAL: return "HEX_LITERAL";
        case TokenType::BINARY_LITERAL: return "BINARY_LITERAL";
        case TokenType::CHAR_LITERAL: return "CHAR_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::MULTIPLY: return "MULTIPLY";
        case TokenType::DIVIDE: return "DIVIDE";
        case TokenType::MODULO: return "MODULO";
        case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
        case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
        case TokenType::MULTIPLY_EQUAL: return "MULTIPLY_EQUAL";
        case TokenType::DIVIDE_EQUAL: return "DIVIDE_EQUAL";
        case TokenType::MODULO_EQUAL: return "MODULO_EQUAL";
        case TokenType::SHIFT_LEFT_EQUAL: return "SHIFT_LEFT_EQUAL";
        case TokenType::SHIFT_RIGHT_EQUAL: return "SHIFT_RIGHT_EQUAL";
        case TokenType::AMPERSAND_EQUAL: return "AMPERSAND_EQUAL";
        case TokenType::PIPE_EQUAL: return "PIPE_EQUAL";
        case TokenType::CARET_EQUAL: return "CARET_EQUAL";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::TILDE: return "TILDE";
        case TokenType::GREATER: return "GREATER";
        case TokenType::LESS: return "LESS";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::SHIFT_LEFT: return "SHIFT_LEFT";
        case TokenType::SHIFT_RIGHT: return "SHIFT_RIGHT";
        case TokenType::AMPERSAND: return "AMPERSAND";
        case TokenType::CARET: return "CARET";
        case TokenType::PIPE: return "PIPE";
        case TokenType::ARROW: return "ARROW";
        case TokenType::FAT_ARROW: return "FAT_ARROW";
        case TokenType::QUESTION: return "QUESTION";
        case TokenType::AT: return "AT";
        case TokenType::ELLIPSIS: return "ELLIPSIS";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::SCOPE_RESOLUTION: return "SCOPE_RESOLUTION";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::INVALID_CHAR: return "INVALID_CHAR";
        case TokenType::INVALID_NUMBER: return "INVALID_NUMBER";
        case TokenType::UNTERMINATED_STRING: return "UNTERMINATED_STRING";
        case TokenType::UNTERMINATED_CHAR: return "UNTERMINATED_CHAR";
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        default: return "UNKNOWN";
    }
}

} // namespace stobx

