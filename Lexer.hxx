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
            // ! remove pragmas
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpedantic"
            case '0' ... '9': {
            #pragma GCC diagnostic pop
                const auto beginning = index;
                while (isdigit(src.at(++index)));
                // allow doubles in the future
                if (src[index] == '.') while (isdigit(src.at(++index)));

                lines.back().emplace_back(TokenKind::NUM, src.substr(beginning, index - beginning));
                --index;
            } break;

            case '_':
            // ! remove pragmas
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpedantic"
            case 'a' ... 'z':
            case 'A' ... 'Z':{
            #pragma GCC diagnostic pop
                const auto beginning = index;
                while (isalnum(src.at(++index)) or src[index] == '_');

                const auto word = src.substr(beginning, index - beginning);
                // check for keywords here :)

                // technically "comments" and friends are keywords..ghost keywords!
                if((word == "comment" or word == "note" or word == "PS") and src[index] == ':'){
                    while(src.at(++index) != '\n');
                    break;
                }

                const TokenKind token = keyword(word);

                lines.back().emplace_back(token, word);
                --index;
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

            // case '\n': lines.back().clear(); break;

            case '(': lines.back().push_back({TokenKind::L_PAREN, {src[index]}}); break;
            case ')': lines.back().push_back({TokenKind::R_PAREN, {src[index]}}); break;

            case '{':
                lines.back().push_back({TokenKind::L_BRACE, {src[index]}});
                // lines.push_back({});
                break;

            case '}': lines.back().push_back({TokenKind::R_BRACE, {src[index]}}); break;

            case '"':{
                const size_t old = index;
                while(src[++index] != '"');
                lines.back().push_back({TokenKind::STRING, src.substr(old + 1, index - old -1)});
            }
            break;

            case '&':
            //     if (src[index + 1] == '&') {
            //         lines.back().push_back({TokenKind::NAME, src.substr(index++, 2)}); // plusplused here
            //         // ++index;
            //         break;
            //     }
            // [[fallthrough]];

            case '|':
            //     if (src[index + 1] == '|') {
            //         lines.back().push_back({TokenKind::NAME, {src[index], src[++index]}});
            //         break;
            //     }
            // [[fallthrough]];

            case '!':
            case '@':
            case '#':
            case '$':
            case '%':
            case '^':
            case '*':
            case '-':
            case '+':
            case '~':{
                const char op = src[index];
                const auto beginning = index;
                while (src[++index] == op);
                lines.back().push_back({TokenKind::NAME, src.substr(beginning, index - beginning)});
                --index;
            } break;



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

