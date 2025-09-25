#pragma once

#include <print>
#include <ostream>
#include <string>


enum class TokenKind {
    // UNKNOWN = -1,
    // NONE = 0,
    ASSIGN,
    FAT_ARROW,
    COMMA,
    DOT,
    CASCADE, // for future support
    ELLIPSIS,

    NAME,
    INT,
    FLOAT,
    BOOL,
    STRING,

    L_BRACE,
    R_BRACE,
    L_PAREN,
    R_PAREN,

// Keywords
    MIXFIX,
    PREFIX,
    INFIX,
    SUFFIX,
    EXFIX,
    CLASS,

// PRECEDENCE
    PR_LOW,
    PR_ASSIGNMENT,
    PR_SUM,
    PR_PROD,
    PR_INFIX,
    PR_PREFIX,
    PR_POSTFIX,
    // PR_CIRCUMFIX,
    PR_CALL,
    PR_HIGH,

    SEMI,
    COLON,

    END,
};


inline const char* stringify(const TokenKind token) noexcept {
    switch (token) {
        using enum TokenKind;
        case NAME:          return "NAME";
        case ASSIGN:        return "ASSIGN";
        case INT:           return "INT";
        case FLOAT:         return "FLOAT";
        case BOOL:          return "BOOL";
        case STRING:        return "STRING";
        case END:           return "END";

        // punctuation
        case L_BRACE:       return "L_BRACE";
        case R_BRACE:       return "R_BRACE";
        case L_PAREN:       return "L_PAREN";
        case R_PAREN:       return "R_PAREN";
        case COMMA:         return "COMMA";
        case CASCADE:       return "CASCADE";
        case ELLIPSIS:      return "ELLIPSIS";
        case DOT:           return "DOT";
        case SEMI:          return "SEMI";
        case COLON:         return "COLON";
        case FAT_ARROW:     return "FAT_ARROW";

        // keywords
        case MIXFIX:        return "MIXFIX";
        case PREFIX:        return "PREFIX";
        case INFIX:         return "INFIX";
        case SUFFIX:        return "SUFFIX";
        case EXFIX:         return "EXFIX";
        case CLASS:         return "CLASS";

        // should probs prefix the strings with PR_ but maybe it's fine for now
        case PR_LOW:        return "LOW"; 
        case PR_ASSIGNMENT: return "ASSIGNMENT"; 
        case PR_INFIX:      return "PR_INFIX"; 
        case PR_SUM:        return "SUM"; 
        case PR_PROD:       return "PROD"; 
        case PR_PREFIX:     return "PR_PREFIX"; 
        case PR_POSTFIX:    return "PR_POSTFIX"; 
        case PR_CALL:       return "CALL"; 
        case PR_HIGH:       return "HIGH"; 
    }

    return "<UNNAMED>";
}



struct Token {
    TokenKind kind;
    std::string text;

    bool operator==(const Token&) const = default;
};


inline std::ostream& operator<<(std::ostream& os, const Token& token) {
    return os << "Token{" << stringify(token.kind) << ", '" << token.text << "'}";
}

template <>
struct std::formatter<Token> : std::formatter<std::string> {
    auto format(const Token& token, auto& ctx) const {
        return std::format_to(ctx.out(), "Token{{{}, '{}'}}", stringify(token.kind), token.text);
    }
};


template <>
struct std::formatter<std::vector<Token>> : std::formatter<std::string_view> {
    auto format(const std::vector<Token>& vec, auto& ctx) const {
        std::string result = "[";
        bool first = true;
        for (const auto& token : vec) {
            if (!first) result += ", ";
            result += std::format("{}", token);
            first = false;
        }
        result += "]";
        return std::format_to(ctx.out(), "{}", result);
    }
};
