#pragma once

#include "Lexer.hxx"
#include "Token.hxx"
#include "Expr.hxx"
#include "Precedence.hxx"

#include <print>
#include <variant>
#include <memory>
#include <utility>
#include <span>
#include <vector>
#include <deque>
#include <algorithm>
#include <ranges>
// #include <list>


// [[noreturn]] void error(const std::string& msg) noexcept {
//     puts(msg.c_str());
//     exit(1);
// }

[[noreturn]] void expected(const TokenKind exp, const TokenKind got) noexcept {
    using std::operator""s;
    error("Expected token "s + stringify(exp) + " and found "s + stringify(got));
}



class Parser {
    // TokenLines __lines; // owner of memory for iterator below
    // typename TokenLines::iterator lines;
    Tokens tokens; 
    typename Tokens::iterator token;


    std::deque<Token> red;

public:
    // Parser(TokenLines l) : __lines{std::move(l)}, lines{__lines.begin()} {
    //     if (__lines.empty()) [[unlikely]] error("Empty File!");
    //     token = lines->begin();
    // }

    Parser(Tokens l) : tokens{std::move(l)}, token{tokens.begin()} { }

    // [[nodiscard]] bool atEnd() const noexcept { return token == __lines.back().end(); }
    [[nodiscard]] bool atEnd() const noexcept { return token == tokens.end(); }


    std::vector<ExprPtr> parse() {
        // std::vector<ExprPtr> expressions;
        // for (; not atEnd(); ++lines, token = lines->begin())
        //     expressions.push_back(parseExpr());

        // return expressions;

        std::vector<ExprPtr> expressions;
        expressions.push_back(parseExpr());
        return expressions;
    }

    ExprPtr parseExpr(const size_t precedence = 0) {
        Token token = consume();

        ExprPtr left = prefix(token);

        // infix
        while (precedence < getPrecedence()) {
            token = consume();

            left = infix(token, std::move(left));

            // if (not atEnd()) consume(TokenKind::SEMI);
        }

        return left;
    }

    ExprPtr prefix(const Token& token) {
        switch (token.kind) {
            using enum TokenKind;

            default: error("Couldn't parse \"" + token.text + "\"!");

            case NUM: return std::make_unique<Num>(token.text);

            case NAME: return std::make_unique<Name>(token.text);
            // case DASH: return std::make_unique<UnaryOp>(token.kind, parse(precedence::SUM));

            case L_PAREN: {
                // auto expr = parse(); // can't be const bc it needs to be moved from
                // consume(R_PAREN);
                // return expr;
                std::vector<std::string> params;

                bool all_names = true;
                if (not match(R_PAREN)) {
                    do {
                        if (not peek(NAME)) all_names = false;

                        params.push_back(consume().text);
                    } while (match(COMMA));
                    consume(R_PAREN);
                }

                if (all_names and match(FAT_ARROW)) {
                    // It's a closure
                    auto body = parseExpr();
                    return std::make_unique<Closure>(std::move(params), std::move(body));
                } else { // tt's just grouping,

                    // can have (a). Fine. But (a, b) is not fine for now. maybe it should be...
                    if (params.size() > 1) error("Unexpected parameter-like list in grouping context");
                    auto expr = parseExpr();
                    consume(R_PAREN);
                    return expr;
                }
            }
        }
    }


    ExprPtr infix(const Token& token, ExprPtr left) {
        switch (token.kind) {
            using enum TokenKind;

            // case PLUS:
            // case DASH:
            //     left = std::make_unique<BinOp>(std::move(left), token.kind, parse(precedence::SUM));
            //     break;

            // case STAR:
            // case SLASH:
            //     left = std::make_unique<BinOp>(std::move(left), token.kind, parse(precedence::PRODUCT));
            //     break;


            case NAME: return std::make_unique<BinOp>(std::move(left), token.text, parseExpr(precedence::CALL - 1));

            case ASSIGN:
                if (const auto name = dynamic_cast<const Name*>(left.get()))
                    return std::make_unique<Assignment>(name->name, parseExpr(precedence::ASSIGNMENT));
                else expected(ASSIGN, token.kind);

            case L_PAREN: {
                std::vector<ExprPtr> args;
                if (not match(R_PAREN)) { // while not closing the paren for the call
                    do
                        args.emplace_back(parseExpr());
                    while(match(COMMA));

                    consume(R_PAREN);
                }

                return std::make_unique<Call>(std::move(left), std::move(args));
            }


            case SEMI:
                puts("SEMI!!!");
                return nullptr;

            default: error("Couldn't parse \"" + token.text + "\"!!");
        }
    }

    Token consume() {
        lookAhead(0);

        const auto token = red.front();
        red.pop_front();
        return token;
    }

    Token consume(const TokenKind exp) {
        using std::operator""s;

		if (const Token token = lookAhead(0); token.kind != exp)
            [[unlikely]]
                // error("Expected token "s + stringify(exp) + " and found "s + stringify(token.kind));
                expected(exp, token.kind);

		return consume();
	}

    [[nodiscard]] bool match(const TokenKind exp) {
		const Token token = lookAhead(0);

		if (token.kind != exp) return false;

		consume();
		return true;
	}

    Token lookAhead(const size_t distance) {
        while (distance >= red.size()) {
            if (atEnd()) error("out of token!");

            red.push_back(*token++);
        }

        // Get the queued token.
        return red[distance];
    }

    [[nodiscard]] bool peek(const TokenKind exp) { return lookAhead(0).kind == exp; }

    [[nodiscard]] size_t getPrecedence() {
        // assuming only for infix (duh, if it's a prefix, then no precedence is needed)
        // if (atEnd()) {
        //     puts("ENDING!");
        //     return 0; 
        // }

        switch (lookAhead(0).kind) {
            using enum TokenKind;

            case ASSIGN: return precedence::ASSIGNMENT;

            // case TokenKind::NUM: return precedence::SUM;

            // case PLUS:
            // case DASH:    return precedence::SUM;

            // case STAR:
            // case SLASH:   return precedence::PRODUCT;

            case NAME: // an infix name is a call to a binary operator
            case L_PAREN: return precedence::CALL;


            default: return 0;
        }
    }
};