#pragma once

#include <print>
#include <variant>
#include <optional>
#include <memory>
#include <utility>
#include <span>
#include <vector>
#include <unordered_map>
#include <deque>
#include <algorithm>
#include <numeric>
#include <ranges>
#include <cassert>


#include "../Lexer/Lexer.hxx"
#include "../Token/Token.hxx"
#include "../Expr/Expr.hxx"
#include "../Parser/Precedence.hxx"
#include "../Utils/utils.hxx"



class Parser {
    Tokens tokens;         // owner of memory for iterator below
    typename Tokens::iterator token_iterator;

    Operators ops;

    std::deque<Token> red; // past tense of read lol

public:
    Parser(Tokens t) : tokens{std::move(t)}, token_iterator{tokens.begin()} { }

    [[nodiscard]] bool atEnd() const noexcept { return token_iterator == tokens.end() or token_iterator->kind == TokenKind::END; }


    std::pair<std::vector<expr::ExprPtr>, Operators> parse() {
        std::vector<expr::ExprPtr> expressions;

        while (not atEnd()) {
            expressions.push_back(parseExpr());

            // consume(TokenKind::SEMI);
            if (not match(TokenKind::SEMI)) {
                const auto t = lookAhead(0);
                if (t.kind == TokenKind::NAME)
                    error("Operator '" + t.text + "' not found!");
                expected(TokenKind::SEMI, t);
            }
        }

        return {expressions, std::move(ops)};
    }

    expr::ExprPtr parseExpr(const size_t precedence = {}) {
        Token token = consume();

        expr::ExprPtr left = prefix(std::move(token));

        // infix
        while (precedence < getPrecedence()) {
            token = consume();

            left = infix(token, std::move(left));
        }

        return left;
    }

    expr::ExprPtr prefix(Token token) {
        switch (token.kind) {
            using enum TokenKind;

            case INT:
            case FLOAT : return std::make_shared<expr::Num>(std::move(token).text);
            case BOOL  : return std::make_shared<expr::Bool>(token.text == "true" ? true : false);
            case STRING: return std::make_shared<expr::String>(std::move(token).text);

            case NAME  : return name(std::move(token));

            case CLASS : return klass();
            case NAMESPACE : return nameSpace();

            case MIXFIX:
            case PREFIX:
            case INFIX :
            case SUFFIX:
            case EXFIX :
                return fixOperator(std::move(token));


            case MATCH: return match();

            // block / scope
            case L_BRACE: {
                std::vector<expr::ExprPtr> lines;
                do {
                    lines.emplace_back(parseExpr());
                    consume(SEMI);
                }
                while(not match(R_BRACE));

                return std::make_shared<expr::Block>(std::move(lines));
            }

            // either a grouping or a closure
            case L_PAREN: {
                if (match(R_PAREN)) { // nullary closure
                    type::TypePtr return_type = match(COLON) ? parseType() : type::builtins::Any();

                    consume(FAT_ARROW);
                    // It's a closure
                    auto body = parseExpr();
                    return std::make_shared<expr::Closure>(std::vector<std::string>{}, type::FuncType{{}, std::move(return_type)}, std::move(body));
                }

                // could still be closure or groupings
                const bool is_closure =
                    check(NAME) and (
                        check(COMMA, 1) or  // (a, 
                        check(COLON, 1) or  // (a: 
                        (check(R_PAREN, 1) and (check(FAT_ARROW, 2) or check(COLON, 2)))  // (a) =>
                    );

                if (is_closure) return closure();

                // tt's just grouping,
                // can have (a). Fine. But (a, b) is not fine for now. maybe it should be...
                auto expr = parseExpr();
                consume(R_PAREN);
                return expr;
            }

            case R_BRACE: error("Can't have empty block!");

            default:
                log();
                error("Couldn't parse \"" + token.text + "\"!");
        }
    }


    expr::ExprPtr infix(Token token, expr::ExprPtr left) {
        switch (token.kind) {
            using enum TokenKind;

            case NAME:
                if (ops.contains(token.text)) {
                    switch (const auto& op = ops[token.text]; op->type()) {
                        // case TokenKind::PREFIX:
                        //     return std::make_shared<UnaryOp>(token, parseExpr(precFromToken(op->prec)));
                        case TokenKind::INFIX :{
                            const auto prec = prec::calculate(op->high, op->low, ops);
                            return std::make_shared<expr::BinOp>(std::move(left), std::move(token).text, parseExpr(prec));
                        }
                        case TokenKind::SUFFIX:
                            return std::make_shared<expr::PostOp>(std::move(token).text, std::move(left));


                        //* I can fix this. Check if the name is the first or not and error accordingly!
                        case TokenKind::EXFIX: {
                            const auto& op = dynamic_cast<const expr::Exfix*>(ops[token.text].get());
                            if (token.text != op->name2) error("Open exfix operator found where closing one was expected!");

                            return left;
                        }


                        // some other part of Operator. 
                        case TokenKind::MIXFIX:
                        {
                            const auto& op = dynamic_cast<const expr::Operator*>(ops[token.text].get());

                            // error("Beginning operator '" + token.text  + "' found where it shouldn't be!");
                            // in the middle of parsing a OpCall. Do nothing.
                            if (token.text != op->name)  return left;
                            if (op->op_pos[0]) error("Operator '" + op->name + "' has to come before an expression!");


                            // if (op->begin_expr) error("Operator '" + op->name + " ...' has to come after a name!");


                            const int prec = prec::calculate(op->high, op->low, ops);

                            std::vector<expr::ExprPtr> exprs = {std::move(left)};


                            for (size_t i{}; const auto& is_op : op->op_pos | std::views::drop(2)) {
                                // match will consume the op
                                if (is_op) {
                                    if (not match(op->rest[i++]))
                                        error("Expected '" + op->rest[i-1] + "', got '" + lookAhead().text + "'!");
                                }
                                else exprs.push_back(parseExpr(prec));
                            }


                            return std::make_shared<expr::OpCall>(op->name, op->rest, std::move(exprs), op->op_pos);
                        }

                        default: error("prefix operator used as [inf/suf]fix");
                    }
                }

                return std::make_shared<expr::Name>(std::move(token).text);

            case DOT:{
                const auto& accessee = parseExpr(prec::HIGH);

                //* maybe this could change and i can allow object.1 + 2. :). Just a thought
                auto accessee_ptr = dynamic_cast<expr::Name*>(accessee.get());
                if (accessee_ptr == nullptr) error("Can only follow a '.' with a name: " + accessee->stringify(0));

                return std::make_shared<expr::Access>(std::move(left), std::move(accessee_ptr->name));
            }

            case ASSIGN: 
                return std::make_shared<expr::Assignment>(std::move(left), parseExpr(prec::ASSIGNMENT +1)); // right associative


            case L_PAREN: {
                std::unordered_map<std::string, expr::ExprPtr> named_args;
                std::vector<expr::ExprPtr> args;

                if (not match(R_PAREN)) { // while not closing the paren for the call

                    do if (check(NAME)) {
                        std::string name = consume().text;
                        if (match(ASSIGN)) {
                            if (std::ranges::find_if(named_args, [&name] (auto&& a) { return a.first == name; }) != named_args.end())
                                error("Named parameter '" + name + "' passed more than once!");


                            named_args[std::move(name)] = parseExpr();

                            if (match(ELLIPSIS)) error("Cannot expand pack in named argument!");
                        }
                        else {
                            ----token_iterator; //? back track the consumption of the name..?
                            red.pop_front();
                            auto expr = parseExpr();
                            if (match(ELLIPSIS))
                                args.emplace_back(std::make_shared<expr::Expansion>(std::move(expr)));
                            else args.emplace_back(std::move(expr));
                        }
                    }
                    else args.emplace_back(parseExpr()); 
                    while (match(COMMA));

                    consume(R_PAREN);
                }

                return std::make_shared<expr::Call>(std::move(left), std::move(named_args), std::move(args));
            }


            default: error("Couldn't parse \"" + token.text + "\"!!");
        }
    }


    type::TypePtr parseType(const bool allow_variadic = true) {
        using enum TokenKind;

        if (match(ELLIPSIS)) {
            if (not allow_variadic) error("Can't have a variadic of a variadic type!");

            return std::make_shared<type::VariadicType>(parseType(false));
        }

        // either a function type
        if (match(L_PAREN)) {
            // type::TypePtr type = std::make_shared<type::FuncType>();
            type::FuncType type{{}, {}};

            if (not check(R_PAREN)) do
                type.params.push_back(parseType());
            while(match(COMMA));

            consume(R_PAREN);

            consume(COLON);

            // return '(' + type + "): " + parseType(); // maybe??

            type.ret = parseType();
            return std::make_shared<type::FuncType>(std::move(type));
        }
        else if (check(NAME)) { // checking for builtin types..this is a quick hack I think
            if (match("Int"   )) return type::builtins::Int();
            if (match("Double")) return type::builtins::Double();
            if (match("Bool"  )) return type::builtins::Bool();
            if (match("String")) return type::builtins::String();
            if (match("Any"   )) return type::builtins::Any();
            if (match("Syntax")) return type::builtins::Syntax();
            if (match("Type"  )) return type::builtins::Type();
        }
        // or an expression
        return std::make_shared<type::ExprType>(parseExpr(prec::ASSIGNMENT));
        // if (check(NAME) or check(INT)) return std::make_shared<type::VarType>(consume().text);

        // log();
        error("Invalid type!");
    }

    Token consume() {
        lookAhead();

        const auto token = red.front();
        red.pop_front();
        return token;
    }

    Token consume(const TokenKind exp, const std::source_location& loc = std::source_location::current()) {
        using std::operator""s;

		if (const Token token = lookAhead(); token.kind != exp) [[unlikely]] {
            // log();
            expected(exp, token, loc);
        }

		return consume();
	}

    [[nodiscard]] bool match(const TokenKind exp) {
		const Token token = lookAhead();

		if (token.kind != exp) return false;

		consume();
		return true;
	}

    [[nodiscard]] bool match(const std::string_view text) {
		const Token token = lookAhead();

		if (token.text != text) return false;

		consume();
		return true;
    }

    Token lookAhead(const size_t distance = 0) {
        while (distance >= red.size()) {
            if (atEnd()) error("out of token!");
            red.push_back(*token_iterator++);
        }

        // Get the queued token.
        return red[distance];
    }

    [[nodiscard]] bool check(const TokenKind exp, const size_t i = {})        { return lookAhead(i).kind == exp; }
    [[nodiscard]] bool check(const std::string_view exp, const size_t i = {}) { return lookAhead(i).text == exp; }

    [[nodiscard]] size_t getPrecedence() {

        const Token& token = lookAhead();
        switch (token.kind) {
            using enum TokenKind;

            case ASSIGN: return prec::ASSIGNMENT;
            case DOT   : return prec::HIGH;

            case NAME: {
                // Probably in the middle of an operator() with :: or more
                if (not ops.contains(token.text)) {
                    // log();
                    // error("Operator '" + token.text + "' not found!");
                    return 0;
                }

                const auto& op = ops[token.text];
                const int prec = prec::calculate(op->high, op->low, ops);

                // mix fix operators should be parsed right to left.......I think ;-;
                if (token.text == op->name and op->type() == TokenKind::MIXFIX)
                    return prec + 1;

                return  prec;
            }

            case L_PAREN: return prec::CALL;


            default: return 0;
        }
    }


    std::unique_ptr<expr::Match::Case::Pattern> parsePattern() {
        using enum TokenKind;
        using Pattern   = expr::Match::Case::Pattern;
        using Single    = expr::Match::Case::Pattern::Single;
        using Patterns = expr::Match::Case::Pattern::Patterns;

        if (not check(NAME)) return nullptr;


        std::string name = consume(NAME).text;

        // base case
        if (not match(L_PAREN)) {
            // trying to write code that avoids move
            return std::make_unique<Pattern>(
                Single{
                    std::move(name),
                    match(COLON) ? parseType() : type::builtins::Any(),
                    match(ASSIGN) ? parseExpr() : nullptr
                }
            );
        }

        Patterns patterns{};
        if (match(R_PAREN)) return std::make_unique<Pattern>(std::move(name), std::move(patterns));

        do patterns.push_back(parsePattern()); while (match(COMMA));


        consume(R_PAREN);
        return std::make_unique<Pattern>(std::move(name), std::move(patterns));
    }

    expr::ExprPtr match() {
        using enum TokenKind;

        auto expr = parseExpr();

        consume(L_BRACE);

        std::vector<expr::Match::Case> cases;
        size_t so_far{};

        do {
            constexpr auto OR = "|";
            constexpr auto IF = "&";
            constexpr auto EMPTY_COND = nullptr;
            constexpr auto EMPTY_BODY = nullptr;

            std::vector<std::unique_ptr<expr::Match::Case::Pattern>> patterns;
            std::vector<expr::ExprPtr> guards;

            do {
                cases.push_back({
                    parsePattern(),
                    match(IF) ? parseExpr() : EMPTY_COND,
                    EMPTY_BODY
                });

            } while (match(OR));


            consume(FAT_ARROW);

            auto body = parseExpr();

            for (auto& kase : cases | std::views::drop(so_far)) {
                kase.body = body; ++so_far;
            }

            consume(SEMI);
        }
        while (not match(R_BRACE));

        return std::make_shared<expr::Match>(std::move(expr), std::move(cases));
    }

    expr::ExprPtr klass() {
        using enum TokenKind;

        consume(L_BRACE);

        std::vector<std::pair<expr::Name, expr::ExprPtr>> fields;

        while (not match(R_BRACE)) {
            auto expr = parseExpr();
            consume(SEMI);

            const auto& ass = dynamic_cast<const expr::Assignment*>(expr.get());
            if (not ass) error("Can only have assignments in class definition!");

            const auto& n = dynamic_cast<const expr::Name*>(ass->lhs.get());
            if (not n) error("Can only assign to names in class definition!");
            auto name = *n; // copy so I can modify

            // Can't reassign variables in a class definition
            // This just means the type was not annotated. Default to "Any"
            if (type::shouldReassign(name.type)) name.type = type::builtins::Any();


            fields.push_back({name, std::move(ass->rhs)});
        }

        return std::make_shared<expr::Class>(std::move(fields));
    }

    expr::ExprPtr nameSpace() {
        using enum TokenKind;

        consume(L_BRACE);

        std::vector<expr::ExprPtr> members;

        while (not match(R_BRACE)) {
            members.push_back(parseExpr());
            consume(SEMI);
        }

        return std::make_shared<expr::Namespace>(std::move(members));
    }

    expr::ExprPtr closure() {
        using enum TokenKind;


        std::vector<std::string> params;
        std::vector<type::TypePtr> params_types;

        do {
            Token name = consume(NAME);

            type::TypePtr type = match(COLON) ? parseType() : type::builtins::Any();

            params.push_back(std::move(name).text);
            params_types.push_back(std::move(type));
        }
        while(match(COMMA));

        consume(R_PAREN);

        for (bool found{}; auto&& type : params_types) {
            if (type::isVariadic(type)) {
                if (found) error("Variadic parameters can only appear once in parameter list!");
                else found = true;
            }
        }


        type::TypePtr return_type = match(COLON) ? parseType() : type::builtins::Any();

        consume(FAT_ARROW);

        auto body = parseExpr();
        return std::make_shared<expr::Closure>(
            std::move(params), type::FuncType{std::move(params_types), std::move(return_type)}, std::move(body)
        );
    }

    expr::ExprPtr name(Token token) {
        using enum TokenKind;

        if (ops.contains(token.text)) {
            switch (const auto& op = ops[token.text]; op->type()) {
                case TokenKind::PREFIX:{
                    const int prec = prec::calculate(op->high, op->low, ops);
                    return std::make_shared<expr::UnaryOp>(std::move(token).text, parseExpr(prec));
                }

                case TokenKind::EXFIX:{
                    const auto& op = dynamic_cast<const expr::Exfix*>(ops[token.text].get());

                    auto ret = std::make_shared<expr::CircumOp>(op->name, op->name2, parseExpr());

                    if (not match(op->name2)) error("Exfix operator not closed!");

                    return ret;
                }

                case TokenKind::MIXFIX: {
                    const auto& op = dynamic_cast<const expr::Operator*>(ops[token.text].get());
                    if (not op->op_pos[0]) error("Operator '" + token.text + "' has to come after an expression!");


                    const int prec = prec::calculate(op->high, op->low, ops);

                    std::vector<expr::ExprPtr> exprs;
                    for (size_t i{}; const auto& is_op : op->op_pos | std::views::drop(1)) {
                        // match will consume the op
                        if (is_op) {
                            if (not match(op->rest[i++]))
                                error("Expected '" + op->rest[i-1] + "', got '" + lookAhead().text + "'!");
                        }
                        else exprs.push_back(parseExpr(prec));
                    }

                    return std::make_shared<expr::OpCall>(
                        op->name, op->rest, std::move(exprs), op->op_pos // op->begin_expr, op->end_expr
                    );
                }
                //     return std::make_shared<BinOp>(token, parseExpr(precFromToken(op->prec)));

                default:
                    // log();
                    error("[in/suf]fix operator used as [pre/ex]fix");
            }
        }


        type::TypePtr type = match(COLON) ? parseType() : type::builtins::_();
        return std::make_shared<expr::Name>(token.text, std::move(type));
    }

    expr::ExprPtr fixOperator(Token token) {
        using enum TokenKind;

        if (token.kind == EXFIX ) return exfixOperator();
        if (token.kind == MIXFIX) return arbitraryOperator();


        consume(L_PAREN);
        Token low = consume();

        const int shift = parseOperatorShift();

        // non-const so it's movable later
        Token high = [shift, &low, this] {
            if (shift > 0) return prec::higher(low, ops);
            if (shift < 0) return std::exchange(low, prec::lower(low, ops));
            return low;
        }();

        consume(R_PAREN);

        const std::string& name = consume(NAME).text;


        // technically I can report this error 2 lines earlier, but printing out the operator name could be very handy!
        if (high.kind == low.kind and (prec::fromToken(high, ops) == prec::HIGH or prec::fromToken(low, ops) == prec::LOW))
            error("Can't have set operator precedence to only LOW/HIGH: " + name);

        consume(ASSIGN);

        expr::ExprPtr func = parseExpr();
        expr::Closure *c = dynamic_cast<expr::Closure*>(func.get());
        if (not c) error("[pre/in/suf] fix operator has to be equal to a function!");


        // gotta dry out this part
        // plus, I don't like that I made "Fix" take a "ExprPtr" rather than closure but I'll leave it for now

        if (ops.contains(name)) {
            const auto& op = ops[name];
            op->funcs.push_back(std::move(func));

            if (op->type() != token.kind) {
                std::println(std::cerr, "Overload set for operator '{}' must have the same operator type:", name);
                expected(op->type(), token.kind);
            }

            if (op->high != high or op->low != low)
                error("Overloaded set of operator '" + name + "' must all have the same precedence!");


            // this feels like double checking the fix operator
            // but it also seems like I need to get the derived type to create a shared_ptr anyway..so IDK
            // if (auto p = dynamic_cast<const expr::Prefix*>(ops[name]))
            //     return std::make_shared<expr::Prefix>(*p);

            // if (auto p = dynamic_cast<const expr::Infix*>(ops[name]))
            //     return std::make_shared<expr::Infix>(*p);

            // if (auto p = dynamic_cast<const expr::Suffix*>(ops[name]))
            //     return std::make_shared<expr::Suffix>(*p);


            error();
        }


        std::shared_ptr<expr::Fix> p;
        if (token.kind == PREFIX) {
            if (c->params.size() != 1) error("Prefix operator must be assigned to a unary closure!");
            p = std::make_shared<expr::Prefix>(name, std::move(high), std::move(low), shift, std::vector<expr::ExprPtr>{/*std::move(func)*/});
        }
        else if (token.kind == INFIX) {
            if (c->params.size() != 2) error("Infix operator must be assigned to a binary closure!");
            p = std::make_shared<expr::Infix> (name, std::move(high), std::move(low), shift, std::vector<expr::ExprPtr>{/*std::move(func)*/});
        }
        else /* if (token.kind == SUFFIX) */ {
            if (c->params.size() != 1) error("Suffix operator must be assigned to a unary closure!");
            p = std::make_shared<expr::Suffix>(name, std::move(high), std::move(low), shift, std::vector<expr::ExprPtr>{/*std::move(func)*/});
        }


        // ops[name] = p.get();
        if (not ops.contains(name)) ops[name] = p->clone();

        p->funcs.push_back(std::move(func));
        return p;
    }

    expr::ExprPtr exfixOperator() {
        using enum TokenKind;

        const std::string& name1 = consume(NAME).text;
        consume(COLON);
        const std::string& name2 = consume(NAME).text;

        consume(ASSIGN);

        expr::ExprPtr func = parseExpr();
        expr::Closure *c = dynamic_cast<expr::Closure*>(func.get());
        if (not c) error("Exfix operator has to be equal to a function!");
        if (c->params.size() != 1) error("Exfix operator must be assigned to a unary closure!");



        const Token low_prec{PR_LOW, "LOW"}; // grouping doesn't have precedence
        std::shared_ptr<expr::Fix> p = std::make_shared<expr::Exfix>(
            name1, name2, low_prec, low_prec, 0, std::vector<expr::ExprPtr>{std::move(func)}
        );


        if (ops.contains(name1)) {
            const auto& op = ops[name1];
            op->funcs.push_back(std::move(func));

            if (op->type() != TokenKind::EXFIX) {
                std::println(std::cerr, "Overload set for operator '{}:{}' must have the same operator type:", name1, name2);
                expected(op->type(), TokenKind::EXFIX);
            }

            auto ex = dynamic_cast<const expr::Exfix*>(op.get());

            if (ex->name != name1 or ex->name2 != name2) {
                error("Overload set of exfix operator must all have the same operator name '" + ex->name + ':' + ex->name2 + '\'');
            }


            return std::make_shared<expr::Exfix>(*ex);
        }

        ops[name1] = p->clone();
        ops[name2] = p->clone(); //* maybe? //* maybe not...? idk

        return p;
    };


    expr::ExprPtr arbitraryOperator() {
        using enum TokenKind;

        consume(L_PAREN);
        Token low = consume();

        const int shift = parseOperatorShift();

        // non-const so it's movable later
        Token high = [shift, &low, this] {
            if (shift > 0) return prec::higher(low, ops);
            if (shift < 0) return std::exchange(low, prec::lower(low, ops));
            return low;
        }();

        consume(R_PAREN);

        // const bool begins_with_expr = match(COLON);

        std::vector<bool> op_pos;
        std::string first;
        if (match(COLON)) {
            op_pos.push_back(false);
            first = consume(NAME).text;
            op_pos.push_back(true);
        }
        else {
            op_pos.push_back(true);
            first = consume(NAME).text;
        }

        std::vector<std::string> rest;

        while (not check(ASSIGN)) {
            op_pos.push_back(not match(COLON));
            if (op_pos.back()) rest.push_back(consume(NAME).text);
        }

        consume(ASSIGN);

        expr::ExprPtr func = parseExpr();
        expr::Closure *c = dynamic_cast<expr::Closure*>(func.get());
        if (not c) error("Operators have to be equal to a function!");


                                    // false == expression parameter
        if (const size_t param_count = std::ranges::count(op_pos, false);
            param_count != c->params.size()
        )
        {
            std::string op_name;
            for (ssize_t i = -1; const auto& field : op_pos) {
                if (field) {
                    op_name += i == -1 ? first : rest[i];
                    ++i;
                }
                else op_name += ':';
            }

            const std::string& n = std::to_string(param_count);
            error("Operator '" + op_name + "' must be assigned to a closure with " + n + " parameters!");
        }

        std::shared_ptr<expr::Fix> p =
            std::make_shared<expr::Operator>(
                std::move(first),
                rest, // how can I move it?
                std::move(op_pos),
                std::move(high), std::move(low),
                shift,
                // begins_with_expr, ends_with_expr,
                std::vector<expr::ExprPtr>{std::move(func)}
            );



        if (ops.contains(first)) {
            const auto& op = ops[first];
            op->funcs.push_back(std::move(func));

            if (op->type() != TokenKind::MIXFIX) error(); // ! ADD ERR MSG

            auto arb = dynamic_cast<const expr::Operator*>(op.get());

            bool same = first == arb->name;
            for (auto&& [n1, n2] : std::views::zip(rest, arb->rest))
                if (n1 != n2) {
                    same = false;
                    break;
                }

            if (not same) error(); // ! ADD ERR MSG

            return std::make_shared<expr::Operator>(*arb);
        }

        ops[p->name] = p->clone();
        for (const auto& name : rest) ops[name] = p->clone();

        return p;
    }


    int parseOperatorShift() {
        if (check(TokenKind::NAME)) {
            const Token& shift_token = consume();
            // assert(shift_token.text.length() <= 1);

            if (shift_token.text.length() == 1){
                if (shift_token.text[0] != '+' and shift_token.text[0] != '-')
                    error("can only have '+' or '-' after precedene!");
                // if (shift_token.text.find_first_not_of(shift_token.text.front()) != std::string::npos) error("can't have a mix of + and - or any other symbol after precedene!");

                return shift_token.text[0] == '+' ? 1 : -1;
            }
            else if (shift_token.text.length() > 1) {
                if (shift_token.text[0] == '+' or shift_token.text[0] == '-')
                    error("Can only have one +/- after a precedence level");
                else
                    error("can only have '+' or '-' after precedene!");
            }
        }

        return 0; // no shift
    }

    /**
     * @attention only call right before calling error/expected
     */
    void log(bool shift = false, bool begin = false) {
        puts("");
        const ptrdiff_t dist = std::distance(tokens.begin(), token_iterator);
        const ptrdiff_t diff = begin ? 0 : dist < 5 ? dist : 5;

        std::copy(token_iterator - (shift ? diff : 0), tokens.end(), std::ostream_iterator<Token>{std::cout, " "});
        puts("");
    }


    const auto& operators() const noexcept { return ops; };
};

