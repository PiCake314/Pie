#pragma once

#include "Token.hxx"
#include "Precedence.hxx"

#include <cctype>
#include <string>
#include <string_view>
#include <cctype>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <algorithm>



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


bool validNameChar(const char c) noexcept {
    switch (c) {
        case '!':
        case '@':
        case '#':
        case '$':
        case '%':
        case '^':
        case '*':
        case '+':
        case '~':
        case '-':
        case '_':
            return true;
    }

    return isalnum(c);
}

TokenLines lex(const std::string& src) {
    TokenLines lines = {{}};
    Tokens line;

    size_t line_count = 1;
    size_t line_starting_index{};


    for (size_t index{}; index < src.length(); ++index) {
        // if (isspace(src[index])) continue;

        try{
        switch (src[index]) {
            // ! remove pragmas
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpedantic"
            case '0' ... '9':
            #pragma GCC diagnostic pop
            {
                const auto beginning = index;
                while (isdigit(src.at(++index)));

                bool is_name = validNameChar(src[index]);
                if (is_name) {
                    while (validNameChar(src.at(++index)));
                    lines.back().emplace_back(TokenKind::NAME, src.substr(beginning, index - beginning));
                    --index;
                    break;
                }

                bool is_float = src[index] == '.';
                if (is_float) while (isdigit(src.at(++index)));

                lines.back().emplace_back(is_float ? TokenKind::FLOAT : TokenKind::INT, src.substr(beginning, index - beginning));
                --index;
            } break;


            case '!':
            case '@':
            case '#':
            case '$':
            case '%':
            case '^':
            case '*':
            case '+':
            case '~':
            case '-':
            case '_':
            // ! remove pragmas
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpedantic"
            case 'a' ... 'z':
            case 'A' ... 'Z':
            #pragma GCC diagnostic pop
            {
                const auto beginning = index;
                while (validNameChar(src.at(++index)));

                const auto word = src.substr(beginning, index - beginning);
                // check for keywords here :)

                // technically "comments" and friends are keywords..ghost keywords!
                std::string lower = word;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](const char c) { return std::tolower(c); });

                if((lower == "comment" or lower == "note" or lower == "ps" or lower == "todo") and src[index] == ':'){
                    while(++index < src.length() and src[index] != '\n');
                    break;
                }

                if (word == "__TEXT__") [[unlikely]] {
                    std::string line_text;

                    for (size_t ind = line_starting_index; ind < src.size() and src[ind] != '\n'; ++ind)
                        line_text += src[ind];

                    lines.back().push_back({TokenKind::STRING, line_text});
                    --index;
                    break;
                }

                if (word == "__LINE__") [[unlikely]] {
                    lines.back().push_back({TokenKind::INT, std::to_string(line_count)});
                    --index;
                    break;
                }


                const TokenKind token = keyword(word);

                lines.back().emplace_back(token, word);
                --index;
            } break;


            case '=':
                if (src.at(index + 1) == '>')
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
            case '\n':
                ++line_count;
                line_starting_index = index + 1;
                break;

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
            } break;

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

            // case '!':
            // case '@':
            // case '#':
            // case '$':
            // case '%':
            // case '^':
            // case '*':
            // case '-':
            // case '+':
            // case '~':
            // {
            //     const char op = src[index];
            //     const auto beginning = index;
            //     while (src[++index] == op);
            //     lines.back().push_back({TokenKind::NAME, src.substr(beginning, index - beginning)});
            //     --index;
            // } break;



            default: break;
        }
        }
        catch(const std::exception& err) {
            std::cerr << err.what() << ":\n";
            for (auto&& line : lines)
                for (auto&& tok : line)
                    std::cout << tok << '\n';

            error("Lexing Error!");
        }

    }


    lines.pop_back();
    lines.back().emplace_back(TokenKind::END, "EOF");

    return lines;
}

