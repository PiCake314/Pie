#pragma once

#include "Token.hxx"
#include "Precedence.hxx"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <unordered_map>



// enum class OperatorType {
//     PREFIX,
//     INFIX,
//     POSTFIX,
// };

// struct Operator {
//     const OperatorType type;
//     const size_t precedence;
// };

using Tokens = std::vector<Token>;
using TokenLines = std::vector<Tokens>;
// using OPs = std::unordered_map<std::string, Operator>;



TokenKind keyword(const std::string_view word) noexcept {
         if (word == "prefix"    ) return TokenKind::PREFIX;
    else if (word == "infix"     ) return TokenKind::INFIX;
    else if (word == "suffix"    ) return TokenKind::SUFFIX;

    // PRIORITIES
    else if (word == "LOW"       ) return TokenKind::PR_LOW;
    else if (word == "ASSIGNMENT") return TokenKind::PR_ASSIGNMENT;
    else if (word == "SUM"       ) return TokenKind::PR_SUM;
    else if (word == "PROD"      ) return TokenKind::PR_PROD;
    else if (word == "OP_CALL"   ) return TokenKind::PR_OP_CALL;
    else if (word == "PREFIX"    ) return TokenKind::PR_PREFIX;
    else if (word == "POSTFIX"   ) return TokenKind::PR_POSTFIX;
    else if (word == "CALL"      ) return TokenKind::PR_CALL;
    else if (word == "HIGH"      ) return TokenKind::PR_HIGH;


    return TokenKind::NAME;
}

TokenLines lex(const std::string& src) {
    TokenLines lines = {{}};
    Tokens line;

    for (size_t index{}; index < src.length(); ++index) {
        // if (isspace(src[index])) continue;

        try{
        switch (src[index]) {
            case '0' ... '9': {
                size_t end = index;
                while (isdigit(src.at(++end)));
                // allow doubles in the future
                if (src[end] == '.') while (isdigit(src.at(++end)));

                lines.back().emplace_back(TokenKind::NUM, src.substr(index, end - index));
                index = end -1;
            } break;


            case 'a' ... 'z':
            case 'A' ... 'Z':{
                size_t end = index;
                while (isalnum(src.at(++end)) || src[end] == '_');

                const auto word = src.substr(index, end - index);
                // check for keywords here :)
                const TokenKind token = keyword(word);

                lines.back().emplace_back(token, word);

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
        catch(...) {
            error("Lexing Error!");
        }

    }


    lines.pop_back();
    lines.back().emplace_back(TokenKind::END, "EOF");

    return lines;
}

