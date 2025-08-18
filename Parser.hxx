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
#include <cassert>


// [[noreturn]] void error(const std::string& msg) noexcept {
//     puts(msg.c_str());
//     exit(1);
// }


class Parser {
    // TokenLines __lines; // owner of memory for iterator below
    // typename TokenLines::iterator lines;

    Tokens tokens;         // this is the owner of the memory now
    typename Tokens::iterator token_iterator;

    Operators ops;


    std::deque<Token> red;

public:
    Parser(Tokens t) : tokens{std::move(t)}, token_iterator{tokens.begin()} {
        // if (__lines.empty()) [[unlikely]] error("Empty File!");
        // token_iterator = lines->begin();
    }

    [[nodiscard]] bool atEnd() const noexcept { return token_iterator == tokens.end() or token_iterator->kind == TokenKind::END; }

    // Parser(Tokens l) : tokens{std::move(l)}, token{tokens.begin()} { }
    // [[nodiscard]] bool atEnd() const noexcept { return token == tokens.end(); }


    std::pair<std::vector<ExprPtr>, Operators> parse() {
        std::vector<ExprPtr> expressions;

        while (not atEnd()) {
            expressions.push_back(parseExpr());
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

            case INT:
            case FLOAT: return std::make_unique<Num>(token.text);

            case STRING: return std::make_unique<String>(token.text);

            case NAME:
                if (ops.contains(token.text)) {
                    switch (const auto op = ops[token.text]; op->type()) {
                        case TokenKind::PREFIX:{
                            const auto prec = precedence::calculate(op->high, op->low, ops);
                            return std::make_unique<UnaryOp>(token.text, parseExpr(prec));
                        }
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
                {
                type::TypePtr type = match(COLON) ? parseType() : type::builtins::Any();

                return std::make_unique<Name>(token.text, std::move(type));
                }
            // case DASH: return std::make_unique<UnaryOp>(token.kind, parse(precedence::SUM));

            case CLASS:{
                consume(L_BRACE);

                std::vector<Assignment> fields;

                while (not match(R_BRACE)) {
                    auto expr = parseExpr();
                    consume(SEMI);

                    auto ass = dynamic_cast<const Assignment*>(expr.get());
                    if (not ass) error("Can only have assignments in class definition!");

                    fields.push_back(*ass);
                }

                return std::make_unique<Class>(std::move(fields));
            }


            case PREFIX:
            case INFIX :
            case SUFFIX: {
                consume(L_PAREN);
                Token low = consume();

                int shift{};
                if (check(NAME)) {
                    const Token& shift_token = consume();
                    assert(shift_token.text.length() <= 1);

                    if (shift_token.text.length() == 1){
                        if (shift_token.text[0] != '+' and shift_token.text[0] != '-')
                            error("can only have '+' or '-' after precedene!");
                        // if (shift_token.text.find_first_not_of(shift_token.text.front()) != std::string::npos) error("can't have a mix of + and - or any other symbol after precedene!");

                        shift = shift_token.text[0] == '+' ? 1 : -1;
                    }
                }

                const Token high = [shift, &low, this] {
                    if (shift > 0) return precedence::higher(low, ops);
                    if (shift < 0) return std::exchange(low, precedence::lower(low, ops));
                    return low;
                }();

                consume(R_PAREN);

                const Token name = consume(NAME);

                // technically I can report this error 2 lines earlier, but printing out the operator name could be very handy!
                if (high.kind == low.kind and (precedence::fromToken(high, ops) == precedence::HIGH or precedence::fromToken(low, ops) == precedence::LOW))
                    error("Can't have set operator precedence to only LOW/HIGH: " + name.text);

                consume(ASSIGN);

                ExprPtr func = parseExpr();
                Closure *c = dynamic_cast<Closure*>(func.get());
                if (not c) error("[pre/in/suf] fix operator has to be equal to a function!");


                // gotta dry out this part
                // plus, I don't like that I made Fix : Expr take a ExprPtr rather than closure but I'll leave it for now
                if (token.kind == PREFIX){
                    auto p = std::make_unique<Prefix>(std::move(name.text), std::move(high), std::move(low), shift, std::move(func));
                    ops[name.text] = p.get();
                    return p;
                }
                else if (token.kind == INFIX) {
                    auto p = std::make_unique<Infix> (std::move(name.text), std::move(high), std::move(low), shift, std::move(func));
                    ops[name.text] = p.get();
                    return p;

                }
                // if (token.kind == SUFFIX)
                else {
                    auto p = std::make_unique<Suffix>(std::move(name.text), std::move(high), std::move(low), shift, std::move(func));
                    ops[name.text] = p.get();
                    return p;
                }
            }
            break;


            case L_BRACE: {
                std::vector<ExprPtr> lines;
                do {
                    lines.emplace_back(parseExpr());
                    consume(SEMI);
                }
                while(not match(R_BRACE));

                return std::make_unique<Block>(std::move(lines));
            }


            case L_PAREN: {
                if (match(R_PAREN)) { // nullary closure

                    type::TypePtr return_type = match(COLON) ? parseType() : type::builtins::Any();

                    consume(FAT_ARROW);
                    // It's a closure
                    auto body = parseExpr();
                    return std::make_unique<Closure>(std::vector<std::string>{}, type::FuncType{{}, std::move(return_type)}, std::move(body));
                }

                // could still be closure or groupings
                // Token t = consume(); // NAME or NUM

                const bool is_closure = check(NAME) and (check(COMMA, 1) or check(COLON, 1) or (check(R_PAREN, 1) and check(FAT_ARROW, 2)));
                // bool is_closure = t.kind == NAME and (check(COMMA) or check(R_PAREN)); // if there is a ','/')', closure, otherwise,

                if (is_closure) {

                    Token name = consume(NAME);
                    // type::TypePtr type = std::make_shared<type::VarType>(match(COLON) ? consume(NAME).text : "Any");
                    type::TypePtr type = match(COLON) ? parseType() : type::builtins::Any();

                    std::vector<std::string> params = {std::move(name).text};
                    std::vector<type::TypePtr> params_types = {std::move(type)};

                    while(match(COMMA)) {
                        name = consume(NAME);
                        type = match(COLON) ? std::make_shared<type::VarType>(consume(NAME).text) : type::builtins::Any();

                        params.push_back(std::move(name).text);
                        params_types.push_back(std::move(type));
                    }

                    consume(R_PAREN);

                    type = std::make_shared<type::VarType>(match(COLON) ? consume(NAME).text : "Any"); // return type

                    consume(FAT_ARROW);

                    auto body = parseExpr();
                    return std::make_unique<Closure>(
                        std::move(params), type::FuncType{std::move(params_types), std::move(type)}, std::move(body)
                    );
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

            case R_BRACE: error("Can't have empty block!");

            default:
                log();
                error("Couldn't parse \"" + token.text + "\"!");
        }
    }


    ExprPtr infix(const Token& token, ExprPtr left) {
        switch (token.kind) {
            using enum TokenKind;

            case NAME:
                if (ops.contains(token.text)) {
                    switch (const auto op = ops[token.text]; op->type()) {
                        // case TokenKind::PREFIX:
                        //     return std::make_unique<UnaryOp>(token, parseExpr(precFromToken(op->prec)));
                        case TokenKind::INFIX :{
                            const auto prec = precedence::calculate(op->high, op->low, ops);
                            return std::make_unique<BinOp>(std::move(left), token.text, parseExpr(prec));
                        }
                        case TokenKind::SUFFIX:
                            return std::make_unique<PostOp>(token.text, std::move(left));

                        default: error("prefix operator used as [inf/suf]fix");
                    }
                }

                return std::make_unique<Name>(token.text);

            case DOT:{
                const auto& accessee = parseExpr(precedence::HIGH);
                auto accessee_ptr = dynamic_cast<Name*>(accessee.get());
                if (accessee_ptr == nullptr) error("Can only follow a '.' with a name: " + accessee->stringify(0));

                return std::make_unique<Access>(left, std::move(accessee_ptr->name));
            }

            case ASSIGN: 
                return std::make_unique<Assignment>(left, parseExpr(precedence::ASSIGNMENT));


            case L_PAREN: {
                std::vector<ExprPtr> args;
                if (not match(R_PAREN)) { // while not closing the paren for the call
                    do args.emplace_back(parseExpr()); while(match(COMMA));

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


    type::TypePtr parseType() {
        using enum TokenKind;

        if (check(NAME) or check(INT)) return std::make_shared<type::VarType>(consume().text);

        if (match(L_PAREN)) {
            // type::TypePtr type = std::make_shared<type::FuncType>();
            type::FuncType type;

            if (not check(R_PAREN))
                do {
                    // type += parseType() + ", "; 
                    type.params.push_back(parseType());
                }
                while(match(COMMA));

            consume(R_PAREN);

            consume(COLON);

            // return '(' + type + "): " + parseType(); // maybe??

            type.ret = parseType();
            return std::make_shared<type::FuncType>(std::move(type));
        }

        log();
        error("Invalid type!");
    }

    Token consume() {
        lookAhead(0);

        const auto token = red.front();
        red.pop_front();
        return token;
    }

    Token consume(const TokenKind exp, const std::source_location& loc = std::source_location::current()) {
        using std::operator""s;

		if (const Token token = lookAhead(0); token.kind != exp) [[unlikely]] {
            log();
            expected(exp, token, loc);
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
            case DOT   : return precedence::HIGH;

            // case TokenKind::NUM: return precedence::SUM;

            // case PLUS:
            // case DASH:    return precedence::SUM;

            // case STAR:
            // case SLASH:   return precedence::PRODUCT;


            case NAME: {
                // can't have a name as an infix if it's not an operator
                if (not ops.contains(token.text)) {
                    log();
                    error("Operator " + token.text + " not found!");
                }

                const auto op = ops[token.text];
                return precedence::calculate(op->high, op->low, ops);
            }

            case L_PAREN: return precedence::CALL;


            default: return 0;
        }
    }


    /**
     * @attention only call right before calling error/expected
     */
    void log(bool begin = false) {
        puts("");
        std::copy(begin ? tokens.begin() : (token_iterator - 5), tokens.end(), std::ostream_iterator<Token>{std::cout, " "});
        puts("");
    }


    const auto& operators() const noexcept { return ops; };
};

