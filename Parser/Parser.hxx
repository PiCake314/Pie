#pragma once

#include <print>
#include <filesystem>
#include <variant>
#include <optional>
#include <memory>
#include <utility>
#include <span>
#include <vector>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <deque>
#include <algorithm>
#include <functional>
#include <numeric>
#include <iterator>
#include <ranges>
#include <cassert>


#include "../Lexer/Lexer.hxx"
#include "../Token/Token.hxx"
#include "../Expr/Expr.hxx"
#include "../Parser/Precedence.hxx"
#include "../Utils/utils.hxx"


inline namespace pie {

inline namespace parse {

class Parser {
    Tokens tokens;
    typename Tokens::iterator token_iterator;

    Operators ops;

    // deque instead of vector for pop_front
    std::deque<Token> red; // past tense of rea d lol

    std::filesystem::path root;


    enum class Context {
        NONE = 0,
        MATCH,
        MAP,
        CALL,
    };

public:

    Parser(Tokens t, std::filesystem::path r = ".") noexcept
    : tokens{std::move(t)}, token_iterator{tokens.begin()}, root{r.remove_filename()}
    {}


    explicit Parser(std::filesystem::path r) noexcept : root{r.remove_filename()} {}


    [[nodiscard]] bool atEnd(size_t offset = 0) const noexcept { return std::next(token_iterator, offset) == tokens.end() or std::next(token_iterator, offset)->kind == TokenKind::END; }


    std::pair<std::vector<expr::ExprPtr>, Operators> parse(Tokens t = {}) {
        if (not t.empty()) {
            tokens = std::move(t);
            token_iterator = tokens.begin();
            red.clear();
        }

        std::vector<expr::ExprPtr> expressions;

        while (not atEnd()) {
            expressions.push_back(parseExpr());

            // consume(TokenKind::SEMI);
            if (not match(TokenKind::SEMI)) {
                const auto t = lookAhead();
                if (t.kind == TokenKind::NAME) {
                    std::string msg = "Operator '" + t.text + "' not found!";
                    if (t.text.length() > 1) msg += "Did you, perhaps, forget a ';' on the previous line?";
                    error(msg + '\n' + expressions.back()->stringify());
                }
                expected(TokenKind::SEMI, t);
            }
        }


        Operators os;
        for (const auto& [name, op] : ops) os[name] = op->clone();

        return {expressions, std::move(os)};
    }

    template <bool PARSE_TYPE = true, Context CTX = Context::NONE>
    expr::ExprPtr parseExpr(const int precedence = 0) {

        expr::ExprPtr left = prefix<PARSE_TYPE, CTX>(consume());

        while (precedence < getPrecedence()) {
            if constexpr (not PARSE_TYPE or CTX == Context::MAP) if (check(TokenKind::COLON)) break;
            left = infix<CTX>(std::move(left), consume());
        }

        return left;
    }

    template <bool PARSE_TYPE = true, Context CTX = Context::NONE>
    expr::ExprPtr prefix(Token token) {
        switch (token.kind) {
            using enum TokenKind;

            case FLOAT :
            case INT   : return std::make_shared<expr::Num   >(std::move(token).text);
            case BOOL  : return std::make_shared<expr::Bool  >(token.text == "true" ? true : false);
            case STRING: return std::make_shared<expr::String>(std::move(token).text);

            case NAME:
                if constexpr (not PARSE_TYPE) return std::make_shared<expr::Name>(std::move(token).text);
                return prefixName(std::move(token));

            case CLASS: return klass();
            case UNION: return onion();
            case MATCH: return match();
            case LOOP:  return loop ();

            case BREAK: 
                // if (check(SEMI)) return std::make_shared<expr::Break>();
                return std::make_shared<expr::Break>(parseExpr());
                // if (check(SEMI)) return std::make_shared<expr::Break>(           );
                // else             return std::make_shared<expr::Break>(parseExpr());

            case CONTINUE:
                // if (check(SEMI))
                return std::make_shared<expr::Continue>();
                // return std::make_shared<expr::Continue>(parseExpr());

                // if (check(SEMI)) return std::make_shared<expr::Continue>(           );
                // else             return std::make_shared<expr::Continue>(parseExpr());

            case NAMESPACE: return nameSpace();
            case USE:       return std::make_shared<expr::Use>(parseExpr());


            // case IMPORT: {
            //     std::filesystem::path path = root;

            //     do {
            //         std::string name = consume(NAME).text;
            //         if (ops.contains(name)) break;

            //         path.append(std::move(name));
            //     }
            //     while(match(COLON));

            //     // path.replace_extension(".pie");
            //     return std::make_shared<expr::Import>(std::move(path));
            // }

            // global namespace
            case SCOPE_RESOLVE: {
                // consume(COLON);

                auto right = parseExpr(prec::HIGH_VALUE);
                auto right_name_ptr = dynamic_cast<const expr::Name*>(right.get());
                if (not right_name_ptr) error("Can only follow a '::' with a name/namespace access: " + right->stringify());

                return std::make_shared<expr::SpaceAccess>(nullptr, std::move(right_name_ptr)->name);
            }

            case COLON: return std::make_shared<expr::Type>(parseType());

            case MIXFIX:
            case PREFIX:
            case INFIX :
            case SUFFIX:
            case EXFIX :
                return fixOperator(std::move(token));

            // block (scope) or list literal
            case L_BRACE: {
                if (match(R_BRACE)) return std::make_shared<expr::List>();

                if (match(COLON)) {
                    consume(R_BRACE);
                    return std::make_shared<expr::Map>();
                }

                const bool map_expr = [this] {

                    for (size_t i{}; /* not atEnd(i) */ ; ++i) {
                        // if you find a colon first, then
                        // it could be a map {x: y};
                        // OR it could be a declaration {x: y = 1;};
                        // must find an assignment to make sure...

                        // { (name1: Int = name3): name4 }
                        // { name1: Int = name3 }
                        if (check(COLON, i)) {
                            for (size_t a = i + 1; /* not atEnd(a) */; ++a) {
                                if (check(COMMA  , a)) return true ; // onto next element, it's a map
                                if (check(R_BRACE, a)) return true ; // closed the map, it's a map
                                if (check(COLON  , a)) return true ; // this time the colon indicates a declaration {n1: n2: n3 = 4};
                                if (check(SEMI   , a)) return false; // proly a declaration, it's a scope..i think :c

                                if (check(L_BRACE, a)) while (not check(R_BRACE, a)) ++a;
                                if (check(L_PAREN, a)) while (not check(R_PAREN, a)) ++a;
                            }
                        }

                        if (check(COMMA  , i)) return false; // if you find a comma first, it's a list {1, 2};
                        if (check(R_BRACE, i)) return false; // finding a `}` before finding `:` means it's a list with potentially one element
                        if (check(SEMI   , i)) return false; // finding a `;` means it's a scope, and that was the end of an expression...

                        if (check(L_BRACE, i)) while (not check(R_BRACE, i)) ++i;
                        if (check(L_PAREN, i)) while (not check(R_PAREN, i)) ++i;
                    }

                    return false; // argubaly, should be `error()`
                }();

                if (map_expr) {

                    auto key = parseExpr<false, Context::MAP>();
                    consume(COLON);
                    std::vector<std::pair<expr::ExprPtr, expr::ExprPtr>> exprs = { {std::move(key), parseExpr(), }, };
                    // std::unordered_map<expr::ExprPtr, expr::ExprPtr> exprs = { {parseExpr<false>(), parseExpr(), }, };

                    while (match(COMMA)) {
                        key = parseExpr<false, Context::MAP>();
                        consume(COLON);
                        exprs.push_back({ std::move(key), parseExpr(), });

                        // auto key = parseExpr<false>();
                        // exprs[std::move(key)] = parseExpr();
                        // exprs.insert_or_assign(std::move(key), parseExpr());?
                    }

                    consume(R_BRACE);

                    return std::make_shared<expr::Map>(std::move(exprs));
                }

                std::vector<expr::ExprPtr> exprs = { parseExpr(), };

                if (match(SEMI)) { // scope
                    while(not match(R_BRACE)) {
                        exprs.emplace_back(parseExpr());
                        consume(SEMI);
                    }
                    return std::make_shared<expr::Block>(std::move(exprs));
                }
                else { // list literals
                    while (match(COMMA)) exprs.emplace_back(parseExpr()); 

                    consume(R_BRACE);

                    return std::make_shared<expr::List>(std::move(exprs));
                }

                error();
            }

            // either a grouping or a closure - (or a closure type)
            case L_PAREN: {
                if (match(R_PAREN)) { // nullary closure
                    type::TypePtr return_type = match(COLON) ? parseType() : type::builtins::Any();

                    consume(FAT_ARROW);
                    // It's a closure
                    auto body = parseExpr<PARSE_TYPE>();
                    return std::make_shared<expr::Closure>(std::vector<std::string>{}, type::FuncType{{}, std::move(return_type)}, std::move(body));
                }

                const bool fold_expr = [this] {
                    for (size_t i{}; /* not atEnd(i) */; ++i) {
                        if (check(R_PAREN , i)) return false;
                        if (check(COLON   , i)) return false;
                        if (check(ELLIPSIS, i)) return true ;


                        if (check(L_BRACE, i)) while (not check(R_BRACE, i)) ++i;
                        if (check(L_PAREN, i)) while (not check(R_PAREN, i)) ++i;
                    }
                    return false;
                }();

                if (fold_expr) return parseFoldExpr();

                // auto exprs = parseCommaList();


                const bool closure_expr = [this] {
                    size_t i{};
                    for (; /* not atEnd(i) and */ not check(R_PAREN , i); ++i) {
                        if (check(L_BRACE, i)) while (not check(R_BRACE, i)) ++i;
                        if (check(L_PAREN, i)) while (not check(R_PAREN, i)) ++i;
                    }
                    ++i;

                    return (CTX != Context::MAP and check(COLON, i)) or check(FAT_ARROW, i); // ( ... ): OR ( ... ) =>
                }();


                if (closure_expr) return closure();


                // just a grouping `(x)`
                auto expr = parseExpr();
                consume(R_PAREN);
                return std::make_shared<expr::Grouping>(std::move(expr));
                // return expr;
            }


            default:
                log(true);
                error("Couldn't parse \"" + token.text + "\"!");
        }
    }


    template <Context CTX = Context::NONE>
    expr::ExprPtr infix(expr::ExprPtr left, Token token) {
        switch (token.kind) {
            using enum TokenKind;

            case NAME: return infixName(std::move(left), std::move(token));

            case DOT: {
                auto accessee = parseExpr(prec::HIGH_VALUE);

                //* maybe this could change and i can allow object.1 + 2. :). Just a thought
                auto accessee_ptr = dynamic_cast<expr::Name*>(accessee.get());
                if (not accessee_ptr) error("Can only follow a '.' with a name: " + accessee->stringify());

                return std::make_shared<expr::Access>(std::move(left), std::move(accessee_ptr)->name);
            }


            case SCOPE_RESOLVE: {
                if (not (
                        dynamic_cast<expr::Name*>(left.get()) or
                        dynamic_cast<expr::SpaceAccess*>(left.get()) or
                        dynamic_cast<expr::Namespace*>(left.get())
                    )
                )
                    error("Can only space-access names or namespaces: " + left->stringify());

                auto right = parseExpr(prec::HIGH_VALUE);
                auto right_name_ptr = dynamic_cast<const expr::Name*>(right.get());
                if (not right_name_ptr) error("Can only follow a '::' with a name/namespace access: " + right->stringify());

                return std::make_shared<expr::SpaceAccess>(std::move(left), std::move(right_name_ptr)->name);
            }

            case COLON: {
                auto type = parseType();
                if (not match(ASSIGN)) error();

                return std::make_shared<expr::Assignment>(
                    std::move(left),
                    std::move(type),
                    parseExpr(prec::ASSIGNMENT_VALUE - 1)
                );

            };

            case ASSIGN:
                if constexpr (CTX == Context::MATCH) return left;

                return std::make_shared<expr::Assignment>(
                    std::move(left),
                    type::builtins::_(),
                    parseExpr(prec::ASSIGNMENT_VALUE - 1)
                );


            case L_PAREN: return call(std::move(left));

            case ELLIPSIS:
                if (CTX == Context::CALL) return left; // in expansion
                // [[fallthrough]];

            default: error("Couldn't parse \"" + token.text + "\"!!");
        }
    }


    template <bool allow_variadic = true>
    type::TypePtr parseType() {
        using enum TokenKind;

        if (match(ELLIPSIS)) {
            if (not allow_variadic) error("Can't have a variadic of a variadic type!");

            return std::make_shared<type::VariadicType>(parseType<false>());
        }

        // either a function type
        if (match(L_PAREN)) {
            // type::TypePtr type = std::make_shared<type::FuncType>();
            type::FuncType type{{}, {}};

            if (not check(R_PAREN)) do type.params.push_back(parseType()); while(match(COMMA));

            consume(R_PAREN);
            consume(COLON);

            type.ret = parseType();
            return std::make_shared<type::FuncType>(std::move(type));
        }

        if (match(L_BRACE)) { // list or map type
            constexpr auto NO_VARIADICS = false;


            auto type1 = parseType<NO_VARIADICS>();
            if (match(R_BRACE)) return std::make_shared<type::ListType>(std::move(type1));

            consume(COLON);
            auto type2 = parseType<NO_VARIADICS>();
            consume(R_BRACE);

            return std::make_shared<type::MapType>(std::move(type1), std::move(type2));
        }

        // auto type = parseExpr(prec::ASSIGNMENT_VALUE);

        if (check(NAME)) { // checking for builtin types..this is a quick hack I think
        // if (dynamic_cast<expr::Name*>(type.get())) { // checking for builtin types..this is a quick hack I think
            // auto name = type->stringify();
            // if (name == "Int"    ) return type::builtins::Int   ();
            // if (name == "Double" ) return type::builtins::Double();
            // if (name == "Bool"   ) return type::builtins::Bool  ();
            // if (name == "String" ) return type::builtins::String();
            // if (name == "Any"    ) return type::builtins::Any   ();
            // if (name == "Syntax" ) return type::builtins::Syntax();
            // if (name == "Type"   ) return type::builtins::Type  ();
            if (match("Int"   )) return type::builtins::Int   ();
            if (match("Double")) return type::builtins::Double();
            if (match("Bool"  )) return type::builtins::Bool  ();
            if (match("String")) return type::builtins::String();
            if (match("Any"   )) return type::builtins::Any   ();
            if (match("Syntax")) return type::builtins::Syntax();
            if (match("Type"  )) return type::builtins::Type  ();
        }
        // or an expression
        return std::make_shared<type::ExprType>(parseExpr(prec::ASSIGNMENT_VALUE));
        // return std::make_shared<type::ExprType>(std::move(type));

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
            log();
            expected(exp, token, loc);
        }

		return consume();
	}

    Token consume(const std::string& exp, const std::source_location& loc = std::source_location::current()) {
        using std::operator""s;

		if (const Token token = lookAhead(); token.text != exp) [[unlikely]] {
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

    [[nodiscard]] int getPrecedence() {

        const Token& token = lookAhead();
        switch (token.kind) {
            using enum TokenKind;

            // right associative
            case ASSIGN: return prec::ASSIGNMENT_VALUE;

            // case COLON : return prec::SCOPE_RESOLUTION_VALUE;
            case SCOPE_RESOLVE : return prec::SCOPE_RESOLUTION_VALUE;

            case DOT   : return prec::MEMBER_ACCESS_VALUE;

            case NAME: {
                // Probably in the middle of a mixfix() that takes 2 colons ': :' or more (2 expression arguments back to back)
                // ex: mixfix(LOW +) if : : else : = (cond, thn, els) => ...;
                if (not ops.contains(token.text)) {
                    // log();
                    // error("Operator '" + token.text + "' not found!");
                    return 0;
                }

                const auto& op = ops[token.text];
                const int prec = prec::calculate(op->high, op->low, ops);

                //todo: prefix sould also be right to left
                // mix fix operators should be parsed right to left.......I think ;-;
                if (token.text == op->name and op->type() == TokenKind::MIXFIX)
                    return prec + 1;

                return prec;
            }

            case L_PAREN: return prec::CALL_VALUE;


            default: return 0;
        }
    }


    std::unique_ptr<expr::Match::Case::Pattern> parsePattern() {
        using enum TokenKind;
        using Pattern   = expr::Match::Case::Pattern;
        using Single    = expr::Match::Case::Pattern::Single;
        using Patterns  = expr::Match::Case::Pattern::Patterns;

        bool has_name{}, has_type{}, has_valu{};

        std::string name;

        // const bool name_expr = not check(ASSIGN) and not check(COLON) and [this] {
        //     for (size_t i{}; not atEnd(); ++i) {
        //         if (check(  L_PAREN, i)) return false; // pattern, not a name
        //         if (check(   ASSIGN, i)) return true;
        //         if (check(    COLON, i)) return true;
        //         if (check(FAT_ARROW, i)) return true;
        //     }
        //     return true;
        // }();

        // // if (not check(ASSIGN) and not check(COLON))
        // if (name_expr) {
        //     constexpr auto in_match = true;
        //     name = parseExpr<in_match>(0)->stringify();
        //     has_name = true;
        // }
        // else
        if (check(NAME)) {
            name = consume(NAME).text;
            has_name = true;
        }


        // base case
        if (not match(L_PAREN)) {
            // trying to write code that avoids move

            auto type = type::builtins::Any();
            if (match(COLON)) {
                type = parseType();
                has_type = true;
            }

            expr::ExprPtr value;
            if (match(ASSIGN)) {
                value = parseExpr();
                has_valu = true;
            }

            if (not (has_name or has_type or has_valu))
                error("Match expression case doesn't contain a pattern");

            return std::make_unique<Pattern>(
                Single {
                    std::move(name),
                    std::move(type),
                    std::move(value)
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

        std::vector<std::tuple<expr::Name, type::TypePtr, expr::ExprPtr>> fields;

        while (not match(R_BRACE)) {
            auto expr = parseExpr();
            consume(SEMI);

            auto ass = dynamic_cast<expr::Assignment*>(expr.get());
            if (not ass) error("Can only have assignments in class definition!");

            // const auto& n = dynamic_cast<const expr::Name*>(ass->lhs.get());
            // if (not n) error("Can only assign to names in class definition!");
            // auto name = *n; // copy so I can modify
;
            // Can't reassign variables in a class definition
            // This just means the type was not annotated. Default to "Any"
            // if (type::shouldReassign(type)) type = type::builtins::Any();


            if (type::shouldReassign(ass->type))
                fields.push_back({expr::Name{ass->lhs->stringify()}, type::builtins::Any(), std::move(ass)->rhs});

            else
                fields.push_back({expr::Name{ass->lhs->stringify()}, std::move(ass)->type , std::move(ass)->rhs});
        }

        return std::make_shared<expr::Class>(std::move(fields));
    }


    expr::ExprPtr onion() {
        using enum TokenKind;

        consume(L_BRACE);

        std::vector<type::TypePtr> types;

        // empty union
        if (match(R_BRACE)) [[unlikely]] return std::make_shared<expr::Union>(std::move(types));

        types.push_back(parseType());
        consume(SEMI);

        while (not match(R_BRACE)) {
            types.push_back(parseType());
            consume(SEMI);
        }

        return std::make_shared<expr::Union>(std::move(types));
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


    // '(' already consumed
    std::vector<expr::ExprPtr> parseCommaList() {
        std::vector<expr::ExprPtr> exprs;

        if (match(TokenKind::R_PAREN)) return exprs;

        do exprs.push_back(parseExpr()); while(match(TokenKind::COMMA));

        consume(TokenKind::R_PAREN);

        return exprs;
    }

// case 1: (... + args)                 right  unary
// case 2: (... + args + init)          right binary

// case 3: (args + ...)                 left   unary
// case 4: (args + ... +  sep )         left   unary separated
// case 5: (sep  + ... +  args)         right  unary separated
// case 6: (sep  + ... +  args + init)  right binary separated

// case 7: (init + args + ...)          left binary
// case 8: (init + args + ... + sep)    left binary separated

    // left paren already consumed
    expr::ExprPtr parseFoldExpr() {
        using enum TokenKind;

        // right folds (unseparated)
        if (match(ELLIPSIS)) return foldCase1And2();

        constexpr auto l2r = true;
        constexpr auto r2l = false;

        auto pack = parseExpr(prec::HIGH_VALUE);


        std::string op = consume().text;
        if (not ops.contains(op))                error("Folding over unknown operator: " + op);
        if (ops[op]->type() != TokenKind::INFIX) error("Folding over non-infix operator: " + op);


        if (match(ELLIPSIS)) {
            // left unary fold (unseparated) | case 3
            if (match(R_PAREN))  return std::make_shared<expr::UnaryFold>(std::move(pack), std::move(op), l2r);

            consume(op);

            auto rhs = parseExpr(prec::HIGH_VALUE);

            // separated unary  | cases 4 and 5
            if (match(R_PAREN)) return std::make_shared<expr::SeparatedUnaryFold>(std::move(pack), std::move(rhs), std::move(op));

            consume(op);

            // right binary separated fold | case 6
            auto separator = std::move(pack);
            pack = std::move(rhs);
            auto init = parseExpr(prec::HIGH_VALUE);

            consume(R_PAREN);
            return std::make_shared<expr::BinaryFold>(std::move(pack), std::move(init), std::move(op), r2l, std::move(separator));
        }


        // binary fold
        // pack was in fact not a pack, but init
        auto init = std::move(pack);
        pack = parseExpr(prec::HIGH_VALUE);

        consume(op);

        consume(ELLIPSIS);

        expr::ExprPtr seperator{};

        // populating the sep for case 8
        if (match(op)) seperator = parseExpr(prec::HIGH_VALUE);

        consume(R_PAREN);

        // case 7 and 8
        return std::make_shared<expr::BinaryFold>(std::move(pack), std::move(init), std::move(op), l2r, std::move(seperator));
    }


    expr::ExprPtr foldCase1And2() {
        using enum TokenKind;

        constexpr auto is_left_to_right = false;

        std::string op = consume().text;
        if (not ops.contains(op))                error("Folding over unknown operator: " + op);
        if (ops[op]->type() != TokenKind::INFIX) error("Folding over non-infix operator: " + op);

        auto pack = parseExpr(prec::HIGH_VALUE);

        // unary fold | case 1
        if (match(R_PAREN)) return std::make_shared<expr::UnaryFold>(std::move(pack), std::move(op), is_left_to_right);

        // binary fold | case 2
        consume(op);
        auto init = parseExpr(prec::HIGH_VALUE);
        consume(R_PAREN);

        return std::make_shared<expr::BinaryFold>(std::move(pack), std::move(init), std::move(op), is_left_to_right);
    }


    expr::ExprPtr loop() {
        using enum TokenKind;

        auto kind = match(FAT_ARROW) ? nullptr : parseExpr();

        if (kind) consume(FAT_ARROW);

        auto var_or_body = parseExpr();

        if (match(FAT_ARROW))
            return std::make_shared<expr::Loop>(std::move(var_or_body), nullptr, std::move(kind), parseExpr());

        if (check(SEMI))
            return std::make_shared<expr::Loop>(std::move(var_or_body), nullptr, std::move(kind));


        auto body = parseExpr();

        if (match(FAT_ARROW))
            return std::make_shared<expr::Loop>(std::move(body), std::move(var_or_body), std::move(kind), parseExpr());

        return std::make_shared<expr::Loop>(std::move(body), std::move(var_or_body), std::move(kind));
    }


        // std::vector<expr::ExprPtr> exprs;
        // if (match(TokenKind::R_PAREN)) return exprs;
        // do exprs.push_back(parseExpr()); while(match(TokenKind::COMMA));
        // consume(TokenKind::R_PAREN);
        // return exprs;

    expr::ExprPtr closure() {
        using enum TokenKind;

        // std::vector<std::pair<expr::ExprPtr, type::TypePtr>> exprs;

        std::vector<std::string> params;
        std::vector<type::TypePtr> params_types;

        if (not match(R_PAREN)) {
            do {
                params.push_back(parseExpr<false>()->stringify());

                if (match(COLON))
                    params_types.push_back(parseType());
                else 
                    params_types.push_back(type::builtins::Any());
            }
            while (match(COMMA));

            consume(R_PAREN);
        }



        // constexpr auto fix_type = [] (const auto& e) -> type::TypePtr {
        //     if (auto name = dynamic_cast<expr::Name*>(e.get()); name and not type::shouldReassign(name->type))
        //         return name->type;

        //     return type::builtins::Any();
        // };


        // std::ranges::transform(exprs, std::back_inserter(params), [] (const auto& e) { return e.first->stringify(); });

        // std::ranges::transform(exprs, std::back_inserter(params_types), fix_type);

        // do {
        //     Token name = consume(NAME);

        //     type::TypePtr type = match(COLON) ? parseType() : type::builtins::Any();

        //     params.push_back(std::move(name).text);
        //     params_types.push_back(std::move(type));
        // }
        // while(match(COMMA));

        // consume(R_PAREN);


        for (bool found{}; auto&& type : params_types) {
            if (type::isVariadic(type)) {
                if  (found) error("Variadic parameters can only appear once in parameter list!");
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


    expr::ExprPtr call(expr::ExprPtr left) {
        using enum TokenKind;

        std::unordered_map<std::string, expr::ExprPtr> named_args;
        std::vector<expr::ExprPtr> args;

        if (not match(R_PAREN)) { // while not closing the paren for the call

            do {
                constexpr auto PARSE_TYPE = true;
                auto arg = parseExpr<PARSE_TYPE, Context::CALL>();

                if (auto ass = dynamic_cast<expr::Assignment*>(arg.get())) {
                    if (match(ELLIPSIS)) error("Cannot expand pack in named argument: " + ass->stringify());

                    if (not type::shouldReassign(ass->type)) error("Can't have type annotation for named arguments: " + ass->stringify());

                    const auto name = ass->lhs->stringify();
                    if (std::ranges::find_if(named_args, [&name] (auto&& a) { return a.first == name; }) != named_args.end())
                        error("Named parameter '" + name + "' passed more than once: " + ass->stringify());

                    named_args[std::move(name)] = std::move(ass)->rhs;
                }

                else if (match(ELLIPSIS)) {
                    arg = std::make_shared<expr::Expansion>(std::move(arg));

                    while(match(ELLIPSIS)) // allows back to back expansions (args... ...);
                        arg = std::make_shared<expr::Expansion>(std::move(arg));


                    args.emplace_back(std::move(arg));
                }
                else args.emplace_back(std::move(arg));


                // std::string name = consume().text;
                // if (match(ASSIGN)) {
                //     if (std::ranges::find_if(named_args, [&name] (auto&& a) { return a.first == name; }) != named_args.end())
                //         error("Named parameter '" + name + "' passed more than once!");


                //     named_args[std::move(name)] = parseExpr();

                //     if (match(ELLIPSIS)) error("Cannot expand pack in named argument!");
                // }
                // else {
                //     std::clog << "NAME: " << name << std::endl;
                //     log();
                //     std::clog << *token_iterator << std::endl;
                //     std::clog << *--token_iterator << std::endl;
                //     std::clog << *--token_iterator << std::endl;
                //     // ----token_iterator; //? back track the consumption of the name..?
                //     red.pop_front();
                //     log();
                //     auto expr = parseExpr();
                //     if (match(ELLIPSIS)) {
                //         expr = std::make_shared<expr::Expansion>(std::move(expr));
                //         while(match(ELLIPSIS)) // allows back to back expansions (args... ...);
                //         expr = std::make_shared<expr::Expansion>(std::move(expr));
                //     }
                //     args.emplace_back(std::move(expr));;
                // }
            }
            while (match(COMMA));

            consume(R_PAREN);
        }

        return std::make_shared<expr::Call>(std::move(left), std::move(named_args), std::move(args));
    }


    expr::ExprPtr prefixName(Token token) {
        if (ops.contains(token.text)) return parseOperator(std::move(token));

        // if constexpr (not PARSE_TYPE) return std::make_shared<expr::Name>(std::move(token).text);


        // if (check(TokenKind::COLON) and not check(TokenKind::COLON, 1)){ // making sure it's not a namespace access like "n::x"
        if (match(TokenKind::COLON)){
            // consume(/* COLON */);
            auto type = parseType();
            consume(TokenKind::ASSIGN);

            return std::make_shared<expr::Assignment>(
                std::make_shared<expr::Name>(std::move(token).text),
                std::move(type),
                parseExpr()
            );
            // return std::make_shared<expr::Name>(token.text, std::move(type));
        }

        // return std::make_shared<expr::Name>(token.text, type::builtins::_());
        return std::make_shared<expr::Name>(std::move(token).text);
    }


    expr::ExprPtr infixName(expr::ExprPtr left, Token token) {
        if (ops.contains(token.text)) {
            switch (const auto& op = ops[token.text]; op->type()) {
                // case TokenKind::PREFIX:
                //     return std::make_shared<UnaryOp>(token, parseExpr(precFromToken(op->prec)));
                case TokenKind::INFIX: {
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
                case TokenKind::MIXFIX: {
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
    }

    expr::ExprPtr parseOperator(Token token) {
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

            default:
                // log();
                error("[in/suf]fix operator '" + token.text + "' used as [pre/ex]fix");
        }
    }



    void fixPrecedence(std::string& p) {
        if (p == "(") { // precedence is `()`
            consume(")");
            p += ')';
        }

        else if (p == "[") { // precedence is `[]`
            consume("]");
            p += ']';
        }
    }

    // operator defintion
    // fix(PREC) op = (...) => ...
    expr::ExprPtr fixOperator(Token token) {
        using enum TokenKind;

        if (token.kind == EXFIX ) return exfixOperator();
        if (token.kind == MIXFIX) return arbitraryOperator();


        int shift{};
        std::string name, low, high;

        if (match(L_PAREN)) {
            low = consume().text;
            fixPrecedence(low);

            shift = parseOperatorShift();

            high = [shift, &low, this] {
                if (shift > 0) return prec::higher(low, ops);
                if (shift < 0) return std::exchange(low, prec::lower(low, ops));
                return low;
            }();

            consume(R_PAREN);

            name = consume(NAME).text;
        }
        else name = low = high = consume(NAME).text;




        // technically I can report this error 2 lines earlier, but printing out the operator name could be very handy!
        if (high == low and (prec::precedenceOf(high, ops) == prec::HIGH_VALUE or prec::precedenceOf(low, ops) == prec::LOW_VALUE))
            error("Can't have set operator precedence to only LOW/HIGH: " + name);


        consume(ASSIGN);

        expr::ExprPtr func = parseExpr();
        expr::Closure *c = dynamic_cast<expr::Closure*>(func.get());
        if (not c) error("[pre/in/suf] fix operator has to be equal to a function!");


        // gotta dry out this part
        // plus, I don't like that I made "Fix" take a "ExprPtr" rather than closure but I'll leave it for now

        if (ops.contains(name)) {
            const auto& op = ops[name];
            // op->funcs.push_back(std::move(func));

            if (op->type() != token.kind) {
                std::println(std::cerr, "Overload set for operator '{}' must have the same operator type:", name);
                expected(op->type(), token.kind);
            }

            if (op->high != high or op->low != low)
                error("Overloaded set of operator '" + name + "' must all have the same precedence!");
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

        // pushing back after cloning so that the op table doesn't contain the closure
        p->funcs.push_back(std::move(func));
        return p;
    }

    expr::ExprPtr exfixOperator() {
        using enum TokenKind;

        std::string name1 = consume(NAME).text;
        consume(COLON);
        std::string name2 = consume(NAME).text;

        consume(ASSIGN);

        expr::ExprPtr func = parseExpr();
        expr::Closure *c = dynamic_cast<expr::Closure*>(func.get());
        if (not c) error("Exfix operator has to be equal to a function!");
        if (c->params.size() != 1) error("Exfix operator must be assigned to a unary closure!");


        std::shared_ptr<expr::Fix> p = std::make_shared<expr::Exfix>(
            name1, name2, prec::LOW, prec::LOW, 0, std::vector<expr::ExprPtr>{/* std::move(func) */}
        );


        if (ops.contains(name1)) {
            const auto& op = ops[name1];
            // op->funcs.push_back(std::move(func));

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

        // pushing back after cloning so that the op table doesn't contain the closure
        p->funcs.push_back(std::move(func));

        return p;
    };


    expr::ExprPtr arbitraryOperator() {
        using enum TokenKind;

        consume(L_PAREN);
        std::string low = consume().text;
        fixPrecedence(low);

        const int shift = parseOperatorShift();

        // non-const so it's movable later
        auto high = [shift, &low, this] {
            if (shift > 0) return prec::higher(low, ops);
            if (shift < 0) return std::exchange(low, prec::lower(low, ops));
            return low;
        }();

        consume(R_PAREN);

        // const bool begins_with_expr = match(COLON);

        std::vector<bool> op_pos;
        std::string first;

        if (match(SCOPE_RESOLVE)) error();

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
                 if (match(SCOPE_RESOLVE)) op_pos.push_back(false), op_pos.push_back(false);
            else if (match(COLON))         op_pos.push_back(false);
            else                           op_pos.push_back(true );

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
                std::vector<expr::ExprPtr>{/* std::move(func) */}
            );



        if (ops.contains(first)) {
            const auto& op = ops[first];
            // op->funcs.push_back(std::move(func));

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

        // push_back after clone on purpose. See other note
        p->funcs.push_back(std::move(func));
        return p;
    }


    int parseOperatorShift() {
        if (check(TokenKind::NAME)) {
            const auto shift_token = consume(TokenKind::NAME).text;
            // assert(shift_token.text.length() <= 1);

            if (shift_token.length() == 1){
                if (shift_token[0] != '+' and shift_token[0] != '-')
                    error("Can only have '+' or '-' after precedene!");
                // if (shift_token.text.find_first_not_of(shift_token.text.front()) != std::string::npos) error("can't have a mix of + and - or any other symbol after precedene!");

                return shift_token[0] == '+' ? 1 : -1;
            }
            else if (shift_token.length() > 1) {
                if (shift_token[0] == '+' or shift_token[0] == '-')
                    error("Can only have one +/- after a precedence level");
                else
                    error("Can only have '+' or '-' after precedene!");
            }
        }

        return 0; // no shift
    }

    /**
     * @attention only call right before calling error/expected
     * It exhausts the stream
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


} // parse
} // pie