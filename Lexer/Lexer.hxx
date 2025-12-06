#pragma once

#include "../Token/Token.hxx"
#include "../Parser/Precedence.hxx"

#include <cctype>
#include <string>
#include <sstream>
#include <string_view>
#include <vector>
#include <fstream>
#include <tuple>
#include <unordered_map>
#include <algorithm>



using Tokens = std::vector<Token>;
using TokenLines = std::vector<Tokens>;



inline TokenKind keyword(const std::string_view word) noexcept {
    using enum TokenKind;
         if (word == "mixfix") return MIXFIX;
    else if (word == "prefix") return PREFIX;
    else if (word == "infix" ) return INFIX ;
    else if (word == "suffix") return SUFFIX;
    else if (word == "exfix" ) return EXFIX ;

    else if (word == "class") return CLASS;
    else if (word == "union") return UNION;
    else if (word == "match") return MATCH;

    else if (word == "loop"    ) return LOOP;
    else if (word == "break"   ) return BREAK;
    else if (word == "continue") return CONTINUE;

    // pre-processor handles imports
    // else if (word == "import"   ) return IMPORT;
    else if (word == "namespace") return NAMESPACE;

    // which one is nicer..?
    else if (word == "use") return USE;
    // else if (word == "using") return USE;

    else if (word == "true"  ) return BOOL;
    else if (word == "false" ) return BOOL;

    // PRIORITIES
    // else if (word == "LOW"       ) return PR_LOW;
    // else if (word == "ASSIGNMENT") return PR_ASSIGNMENT;
    // else if (word == "SUM"       ) return PR_SUM;
    // else if (word == "PROD"      ) return PR_PROD;
    // else if (word == "INFIX"     ) return PR_INFIX;
    // else if (word == "PREFIX"    ) return PR_PREFIX;
    // else if (word == "POSTFIX"   ) return PR_POSTFIX;
    // else if (word == "CALL"      ) return PR_CALL;
    // else if (word == "HIGH"      ) return PR_HIGH;


    return NAME;
}


inline bool validNameChar(const char c) noexcept {
    switch (c) {
        case '!':
        case '@':
        case '#':
        case '$':
        case '%':
        case '^':
        case '&':
        case '|':
        case '*':
        case '+':
        case '~':
        case '-':
        case '_':
        case '\\':
        case '\'':
        case '/':
        case '<':
        case '>':
        case '[':
        case ']':
        // case '=': // function would only be used when checking chars that are not the first in the name
            return true;
    }

    return isalnum(c);
}

[[nodiscard]] inline Tokens lex(const std::string& src) {
    TokenLines lines = {{}};
    Tokens line;

    size_t line_count = 1;
    size_t line_starting_index{};


    for (size_t index{}; index < src.length(); ++index) {
        // if (isspace(src[index])) continue;

        try{
        switch (src[index]) {
            using enum TokenKind;

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
                    lines.back().emplace_back(NAME, src.substr(beginning, index - beginning));
                    --index;
                    break;
                }

                bool is_float = src[index] == '.';
                if (is_float) while (isdigit(src.at(++index)));

                lines.back().emplace_back(is_float ? FLOAT : INT, src.substr(beginning, index - beginning));
                --index;
            } break;


            case '!':
            case '@':
            case '#':
            case '$':
            case '%':
            case '^':
            case '&':
            case '|':
            case '*':
            case '+':
            case '~':
            case '-':
            case '_':
            case '\\':
            case '\'':
            case '/':
            case '<':
            case '>':
            case '[':
            case ']':
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


                if (word == "__TEXT__") [[unlikely]] {
                    std::string line_text;

                    for (size_t ind = line_starting_index; ind < src.size() and src[ind] != '\n'; ++ind)
                        line_text += src[ind];

                    lines.back().push_back({STRING, line_text});
                    --index;
                    break;
                }

                if (word == "__LINE__") [[unlikely]] {
                    lines.back().push_back({INT, std::to_string(line_count)});
                    --index;
                    break;
                }


                const TokenKind token = keyword(word);

                lines.back().emplace_back(token, word);
                --index;
            } break;


            case '=':
                if (src.at(index + 1) == '>')
                    lines.back().push_back({FAT_ARROW, {src[index], src[++index]}});
                else if ((src[index + 1] == '='))
                    lines.back().push_back({NAME, {src[index], src[++index]}});
                else
                    lines.back().push_back({ASSIGN, {src[index]}});

                break;

            case ',': lines.back().push_back({COMMA, {src[index]}}); break;
            case '.':
                if (src.at(index + 1) == ':') {
                    if (src.at(index + 2) == ':') {
                        for(index += 2;
                            // src.at(index) != ':' or src.at(index + 1) != ':' or src.at(index + 2) == '.'
                            src.substr(index, 3) != "::.";
                            ++index
                        );
                        index += 2;
                    }
                    else while(++index < src.length() and src[index] != '\n');
                }
                else if (src[index + 1] == '.' and src.at(index + 2) == '.')
                    lines.back().push_back({ELLIPSIS, {src[index], src[++index], src[++index]}});
                // else if (src[index + 1] == '.')
                //     lines.back().push_back({CASCADE, {src[index], src[++index]}});
                else
                    lines.back().push_back({DOT, {src[index]}});

                break;

            case ':': lines.back().push_back({COLON, {src[index]}}); break;

            case ';':
                lines.back().push_back({SEMI, {src[index]}});
                lines.push_back({});
                break;

            // case '\n': lines.back().clear(); break;
            case '\n':
                ++line_count;
                line_starting_index = index + 1;
                break;

            case '(': lines.back().push_back({L_PAREN, {src[index]}}); break;
            case ')': lines.back().push_back({R_PAREN, {src[index]}}); break;

            case '{':
                lines.back().push_back({L_BRACE, {src[index]}});
                // lines.push_back({});
                break;

            case '}': lines.back().push_back({R_BRACE, {src[index]}}); break;

            case '"':{
                const size_t old = index;
                while(src.at(++index) != '"') {
                    // if (src[index] == '\\')
                }
                lines.back().push_back({STRING, src.substr(old + 1, index - old -1)});
            } break;


            default: break;
        }
        }
        catch(const std::exception& err) {
            // std::cerr << err.what() << ":\n";
            // for (auto&& line : lines)
            //     for (auto&& tok : line)
            //         std::cout << tok << '\n';

            error("Lexing Error!");
        }

    }

    if (not lines.empty() and not lines.back().empty() and lines.back().back().kind != TokenKind::SEMI) error("Last line doesn't end with a ';'!");


    if (lines.size() > 1) {
        lines.pop_back();
        lines.back().emplace_back(TokenKind::END, "EOF");
    }


    Tokens tokens;
    for (auto&& line : lines)
        for (auto&& t : line)
            tokens.push_back(std::move(t));

    return tokens;
}




