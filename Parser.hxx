#pragma once

#include "Lexer.hxx"
#include "Token.hxx"
#include "Expr.hxx"
#include "Precedence.hxx"
#include "utils.hxx"

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


// using Operators 


class Parser {
    TokenLines __lines; // owner of memory for iterator below
    typename TokenLines::iterator lines;
    // Tokens tokens; 
    typename Tokens::iterator token;


    std::deque<Token> red;

public:
    Parser(TokenLines l) : __lines{std::move(l)}, lines{__lines.begin()} {
        if (__lines.empty()) [[unlikely]] error("Empty File!");
        token = lines->begin();
    }

    [[nodiscard]] bool atEnd() const noexcept { return lines == __lines.end() or token == __lines.back().end(); }

    // Parser(Tokens l) : tokens{std::move(l)}, token{tokens.begin()} { }
    // [[nodiscard]] bool atEnd() const noexcept { return token == tokens.end(); }


    std::vector<ExprPtr> parse() {
        std::vector<ExprPtr> expressions;
        for (; not atEnd(); ++lines, token = lines->begin()) {
            expressions.push_back(parseExpr());
            if (not atEnd()) consume(TokenKind::SEMI);
        }


        return expressions;

        // std::vector<ExprPtr> expressions;
        // expressions.push_back(parseExpr());
        // return expressions;
    }

    ExprPtr parseExpr(const size_t precedence = 0) {
        Token token = consume();

        ExprPtr left = prefix(token);

        // infix
        while (precedence < getPrecedence()) {
            token = consume();

            left = infix(token, std::move(left));
        }

        return left;
    }

    ExprPtr prefix(const Token& token) {
        switch (token.kind) {
            using enum TokenKind;

            default: error("Couldn't parse \"" + token.text + "\"!");

            case NUM: return std::make_unique<Num>(token.text);

            case NAME:
                if (lookAhead(0).kind == NAME || lookAhead(0).kind == NUM)
                    return std::make_unique<UnaryOp>(token.kind, parseExpr(precedence::PREFIX));
                else
                    return std::make_unique<Name>(token.text);
            // case DASH: return std::make_unique<UnaryOp>(token.kind, parse(precedence::SUM));

            case PREFIX: 
            case INFIX : 
            case SUFFIX: {
                consume(L_PAREN);
                const auto prec = consume();

                int shift;
                if (match("+")) {
                    shift = 1;
                    while (match("+")) ++shift;
                }
                else if (match("-")) {
                    shift = -1;
                    while (match("-")) --shift;
                }

                consume(R_PAREN);

                const Token name = consume(NAME);

                consume(ASSIGN);

                ExprPtr func = parseExpr();
                Closure *c = dynamic_cast<Closure*>(func.get());
                if (not c) error("[Pre/In/Suf] fix operator has to be equal to a function!");


                if (token.kind == PREFIX)
                    return std::make_unique<Prefix>(std::move(name.text), prec.kind, shift, std::move(*c));
                if (token.kind == INFIX)
                    return std::make_unique<Infix> (std::move(name.text), prec.kind, shift, std::move(*c));
                // if (token.kind == SUFFIX)
                    return std::make_unique<Suffix>(std::move(name.text), prec.kind, shift, std::move(*c));
            }
            break;

            case L_PAREN: {

                if (match(R_PAREN)) {
                    consume(FAT_ARROW);
                    // It's a closure
                    auto body = parseExpr();
                    return std::make_unique<Closure>(std::vector<std::string>{}, std::move(body));
                }

                // could still be closure or groupings
                Token t = consume(); // NAME or NUM
                bool is_closure = t.kind == NAME and check(COMMA); // if there is a comma, closure, otherwise,

                if (is_closure) {
                    std::vector<std::string> params = {t.text};

                    while(match(COMMA)) {
                        t = consume(NAME);
                        params.push_back(t.text);
                    }
                    consume(R_PAREN);
                    consume(FAT_ARROW);

                    auto body = parseExpr();
                    return std::make_unique<Closure>(std::move(params), std::move(body));
                }
                else { // tt's just grouping,
                    // can have (a). Fine. But (a, b) is not fine for now. maybe it should be...
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


            case NAME: return std::make_unique<BinOp>(std::move(left), token.text, parseExpr(precedence::OP_CALL));

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

    Token consume(const TokenKind exp, const std::source_location& loc = std::source_location::current()) {
        using std::operator""s;

		if (const Token token = lookAhead(0); token.kind != exp)
            [[unlikely]] expected(exp, token.kind, loc);

		return consume();
	}

    [[nodiscard]] bool match(const TokenKind exp) {
		const Token token = lookAhead(0);

		if (token.kind != exp) return false;

		consume();
		return true;
	}

    [[nodiscard]] bool match(const std::string_view text) {
		const Token token = lookAhead(0);

		if (token.text != text) return false;

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

    [[nodiscard]] bool check(const TokenKind exp) { return lookAhead(0).kind == exp; }

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


            case NAME:    return precedence::OP_CALL; // an infix name is a call to a binary operator

            case L_PAREN: return precedence::CALL;


            default: return 0;
        }
    }
};