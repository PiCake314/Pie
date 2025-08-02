#pragma once

#include "Token.hxx"

#include <cctype>
#include <string>
#include <vector>
#include <tuple>
#include <unordered_map>


enum class OperatorType {
    PREFIX,
    INFIX,
    POSTFIX,
};

struct Operator {
    const OperatorType type;
    const size_t precedence;
};

using Tokens = std::vector<Token>;
using TokenLines = std::vector<Tokens>;
using OPs = std::unordered_map<std::string, Operator>;


std::tuple<TokenLines, OPs> lex(const std::string& src) {
    TokenLines lines = {{}};
    Tokens line;

    for (size_t index{}; index < src.length(); ++index) {
        // if (isspace(src[index])) continue;


        switch (src[index]) {
            case '0' ... '9': {
                size_t end = index;
                while (isdigit(src[++end]));
                // allow doubles in the future
                if (src[end] == '.') while (isdigit(src[++end]));

                lines.back().emplace_back(TokenKind::NUM, src.substr(index, end - index));
                index = end -1;
            } break;


            case 'a' ... 'z':
            case 'A' ... 'Z':{
                size_t end = index;
                while (isalnum(src[++end]) || src[end] == '_');

                const auto name = src.substr(index, end - index);
                // check for keywords here :)
                // if (name == "infix") {

                // } 
                lines.back().emplace_back(TokenKind::NAME, name);
                index = end -1;
            } break;


            case '=':
                if (src[index + 1] == '>')
                    lines.back().push_back({TokenKind::FAT_ARROW, src.substr(index++, 2)});
                else
                    lines.back().push_back({TokenKind::ASSIGN, {src[index]}});

                break;

            case ',': lines.back().push_back({TokenKind::COMMA, {src[index]}}); break;
            case ';':
                lines.back().push_back({TokenKind::SEMI, {src[index]}});
                lines.push_back({});

                break;

            case '(': lines.back().push_back({TokenKind::L_PAREN, {src[index]}}); break;
            case ')': lines.back().push_back({TokenKind::R_PAREN, {src[index]}}); break;



            case '_':
            case '!':
            case '@':
            case '#':
            case '$':
            case '%':
            case '^':
            case '&':
                // if (src[index + 1] == '&') {
                //     tokens.push_back({TokenKind::NAME, src.substr(index++, 2)}); // plusplused here
                //     // ++index;
                //     break;
                // }

            case '*':
            case '|':
            case '-':
            case '+':
                lines.back().push_back({TokenKind::NAME, {src[index]}}); break;


            default: break;
        }
    }

    lines.pop_back();
    lines.back().emplace_back(TokenKind::END, "EOF");

    return {lines, {}};
}

