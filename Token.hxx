#pragma once

#include <print>
#include <ostream>
#include <string>


enum class TokenKind {
    // UNKNOWN = -1,
    // NONE = 0,
    NAME,
    ASSIGN,
    COMMA,
    NUM,
    // PLUS,
    // DASH,
    // STAR,
    // SLASH,
    L_PAREN,
    R_PAREN,
    FAT_ARROW,

    // INFIX,
    // IF,

    SEMI,

    END,
};

const char* stringify(const TokenKind token) noexcept {
    switch (token) {
        // using enum TokenKind;
        case TokenKind::NAME:      return "NAME";
        case TokenKind::ASSIGN:    return "ASSIGN";
        case TokenKind::NUM:       return "NUM";
        case TokenKind::END:       return "END";
        case TokenKind::L_PAREN:   return "L_PAREN";
        case TokenKind::R_PAREN:   return "R_PAREN";
        case TokenKind::COMMA:     return "COMMA";
        case TokenKind::SEMI:      return "SEMI";
        case TokenKind::FAT_ARROW: return "FAT_ARROW";
    }

    return "<UNNAMED>";
}



struct Token {
    // Token(const TokenKind k, const char c)
    // : kind{k}, text{c} {}

    // Token(const TokenKind k, std::string t)
    // : kind{k}, text{std::move(t)} {}

    TokenKind kind;
    std::string text;
};

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
