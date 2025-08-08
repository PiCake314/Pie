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
#include <unordered_map>
#include <deque>
#include <algorithm>
#include <ranges>
// #include <list>


// [[noreturn]] void error(const std::string& msg) noexcept {
//     puts(msg.c_str());
//     exit(1);
// }


using Operators = std::unordered_map<std::string, Fix*>;


class Parser {
    TokenLines __lines; // owner of memory for iterator below
    typename TokenLines::iterator lines;
    // Tokens tokens; 
    typename Tokens::iterator token_iterator;

    Operators ops;


    std::deque<Token> red;

public:
    Parser(TokenLines l) : __lines{std::move(l)}, lines{__lines.begin()} {
        if (__lines.empty()) [[unlikely]] error("Empty File!");
        token_iterator = lines->begin();
    }

    [[nodiscard]] bool atEnd() const noexcept { return lines == __lines.end() or token_iterator == __lines.back().end(); }

    // Parser(Tokens l) : tokens{std::move(l)}, token{tokens.begin()} { }
    // [[nodiscard]] bool atEnd() const noexcept { return token == tokens.end(); }


    std::pair<std::vector<ExprPtr>, Operators> parse() {
        std::vector<ExprPtr> expressions;

        for (; not atEnd(); ++lines, token_iterator = lines->begin()) {
            expressions.push_back(parseExpr());

            if (not atEnd())
                consume(TokenKind::SEMI);
        }


        return {expressions, ops};

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

            default:
                log();
                error("Couldn't parse \"" + token.text + "\"!");

            case NUM: return std::make_unique<Num>(token.text);

            case STRING: return std::make_unique<String>(token.text);

            case NAME:
                if (ops.contains(token.text)) {
                    switch (const auto op = ops[token.text]; op->type()) {
                        using namespace precedence;
                        case TokenKind::PREFIX:
                            return std::make_unique<UnaryOp>(token.text, parseExpr(precFromToken(op->token.kind) + op->shift));
                            // return std::make_unique<Prefix>(token, op->shift, parseExpr(precFromToken(op->token.kind)));
                        // case TokenKind::INFIX :
                        //     return std::make_unique<BinOp>(token, parseExpr(precFromToken(op->prec)));
                        // case TokenKind::SUFFIX:
                        //     return std::make_unique<UnaryOp>(token, parseExpr(precFromToken(op->prec)));

                        default: log(); error("[in/suf]fix operator used as prefix");
                    }
                }
                // if (lookAhead(0).kind == NAME || lookAhead(0).kind == NUM)
                //     return std::make_unique<UnaryOp>(token, parseExpr(precedence::PREFIX));
                // else
                return std::make_unique<Name>(token.text);
            // case DASH: return std::make_unique<UnaryOp>(token.kind, parse(precedence::SUM));

            case PREFIX: 
            case INFIX : 
            case SUFFIX: {
                consume(L_PAREN);
                const auto prec = consume();

                int shift{};
                if (check(NAME)) {
                    const Token& shift_token = consume();
                    if (shift_token.text.find_first_not_of(shift_token.text.front()) != std::string::npos) error("can't have a mix of + and - or any other symbol after precedene!");

                    switch (shift_token.text.front()) {
                        case '+': shift += shift_token.text.length(); break;
                        case '-': shift -= shift_token.text.length(); break;
                        default: error("Can only have '+' or '-' when specifying precedence");
                    }
                }

                consume(R_PAREN);

                const Token name = consume(NAME);

                consume(ASSIGN);

                ExprPtr func = parseExpr();
                Closure *c = dynamic_cast<Closure*>(func.get());
                if (not c) error("[pre/in/suf] fix operator has to be equal to a function!");


                // gotta dry out this part
                // plus, I don't like that I made Fix : Expr take a ExprPtr rather than closure but I'll leave it for now
                if (token.kind == PREFIX){
                    auto p = std::make_unique<Prefix>(Token{prec.kind, name.text}, shift, std::move(func));
                    ops[name.text] = p.get();
                    return p;
                }
                else if (token.kind == INFIX) {
                    auto p = std::make_unique<Infix> (Token{prec.kind, name.text}, shift, std::move(func));
                    ops[name.text] = p.get();
                    return p;

                }
                // if (token.kind == SUFFIX)
                else {
                    auto p = std::make_unique<Suffix>(Token{prec.kind, name.text}, shift, std::move(func));
                    ops[name.text] = p.get();
                    return p;
                }
            }
            break;

            case L_PAREN: {
                if (match(R_PAREN)) { // closure
                    consume(FAT_ARROW);
                    // It's a closure
                    auto body = parseExpr();
                    return std::make_unique<Closure>(std::vector<std::string>{}, std::move(body));
                }

                // could still be closure or groupings
                // Token t = consume(); // NAME or NUM

                const bool is_closure = check(NAME) and (check(COMMA, 1) or (check(R_PAREN, 1) and check(FAT_ARROW, 2)));
                // bool is_closure = t.kind == NAME and (check(COMMA) or check(R_PAREN)); // if there is a ','/')', closure, otherwise,

                if (is_closure) {
                    Token t = consume(NAME);
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
                    // log(); 
                    // error("");
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


            case NAME:
                if (ops.contains(token.text)) {
                    switch (const auto op = ops[token.text]; op->type()) {
                        using namespace precedence;
                        // case TokenKind::PREFIX:
                        //     return std::make_unique<UnaryOp>(token, parseExpr(precFromToken(op->prec)));
                        case TokenKind::INFIX :
                            return std::make_unique<BinOp>(std::move(left), token.text, parseExpr(precFromToken(op->token.kind) + op->shift));
                        case TokenKind::SUFFIX:
                            return std::make_unique<PostOp>(token.text, std::move(left));

                        default: error("prefix operator used as [inf/suf]fix");
                    }
                }

                return std::make_unique<Name>(token.text);

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


            // case SEMI:
            //     puts("SEMI!!!");
            //     return nullptr;

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
            [[unlikely]] {
                log();
                expected(exp, token.kind, loc);
            }

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

            red.push_back(*token_iterator++);
        }

        // Get the queued token.
        return red[distance];
    }

    [[nodiscard]] bool check(const TokenKind exp, const size_t i = {}) { return lookAhead(i).kind == exp; }

    [[nodiscard]] size_t getPrecedence() {
        // assuming only for infix (duh, if it's a prefix, then no precedence is needed)
        // if (atEnd()) {
        //     puts("ENDING!");
        //     return 0; 
        // }

        const Token& token = lookAhead(0);
        switch (token.kind) {
            using enum TokenKind;

            case ASSIGN: return precedence::ASSIGNMENT;

            // case TokenKind::NUM: return precedence::SUM;

            // case PLUS:
            // case DASH:    return precedence::SUM;

            // case STAR:
            // case SLASH:   return precedence::PRODUCT;


            case NAME: {
                // can't have a name as an infix if it's not an operator
                if (not ops.contains(token.text)) error("Operator " + token.text + " not found!");

                const auto op = ops[token.text];
                return precedence::precFromToken(op->token.kind) + op->shift;
            }

            case L_PAREN: return precedence::CALL;


            default: return 0;
        }
    }


    /**
     * @attention only call right before calling error/expected
     */
    void log() const {
        puts("");
        std::copy(token_iterator, lines->end(), std::ostream_iterator<Token>{std::cout, " "});
        puts("");
    }
};