#pragma once

#include <iostream>
#include <print>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <numeric>
#include <memory>
#include <variant>
#include <cmath>

#include "../Token/Token.hxx"
#include "../Declarations.hxx"
#include "../Type/Type.hxx"
#include "../Utils/utils.hxx"



using ASTNode     = expr::ExprPtr;
using Object      = std::pair<ClassValue, std::shared_ptr<Dict>>;
using Value       = std::variant<int, double, bool, std::string, expr::Closure, ClassValue, Object, expr::Node>;
using Environment = std::unordered_map<std::string, std::pair<Value, type::TypePtr>>;



namespace expr {

struct Num : Expr {
    std::string num;

    explicit Num(std::string n) noexcept : num{std::move(n)} {}

    std::string stringify(const size_t = 0) const override { return num; }

    Node variant() const override { return this; }
};


struct String : Expr {
    std::string str;

    explicit String(std::string s) noexcept : str{std::move(s)} {}

    std::string stringify(const size_t = 0) const override { return '"' + str + '"'; }

    Node variant() const override { return this; }
};


struct Name : Expr {
    std::string name;
    type::TypePtr type;


    Name(std::string n, type::TypePtr t = type::builtins::Any()) noexcept : name{std::move(n)}, type{std::move(t)} {}

    std::string stringify(const size_t = 0) const override {
        return name;
        // return name + ": " + type->text();
    }

    Node variant() const override { return this; }
};


struct Assignment : Expr {
    // std::string name;
    ExprPtr lhs;
    ExprPtr rhs;


    Assignment(ExprPtr l, ExprPtr r) noexcept
    : lhs{std::move(l)}, rhs{std::move(r)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        if (auto name = dynamic_cast<const Name*>(lhs.get()); name and name->type->text() != "_") {
            return name->stringify() + ": " + name->type->text() + " = " + rhs->stringify(indent);
        }

        return lhs->stringify(indent) + " = " + rhs->stringify(indent);
    }

    Node variant() const override { return this; }
};


struct Class : Expr {
    std::vector<std::pair<Name, ExprPtr>> fields;

    explicit Class(std::vector<std::pair<Name, ExprPtr>> f) noexcept
    : fields{std::move(f)} {}


    std::string stringify(const size_t indent = 0) const override {

        std::string s = "class {\n";

        const std::string space(indent + 4, ' ');
        for (const auto& field : fields) {
            s += space + field.first.name + ": " + field.first.type->text(indent + 4)
            + " = " + field.second->stringify(indent + 4) + ";\n";
        }


        return s + std::string(indent, ' ') + "}";
    }

    Node variant() const override { return this; }
};


struct Access : Expr {
    ExprPtr var;
    std::string name;

    Access(ExprPtr v, std::string n) noexcept
    : var{std::move(v)}, name{std::move(n)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return var->stringify(indent) + '.' + name;
    }

    Node variant() const override { return this; }
};

struct Grouping : Expr {
    ExprPtr expr;
    Grouping(ExprPtr e) noexcept : expr{std::move(e)} {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + expr->stringify(indent) + ')';
    }

    Node variant() const override { return this; }
};


struct UnaryOp : Expr {
    // Token token; // also always name??
    std::string op;
    ExprPtr expr;


    UnaryOp(std::string o, ExprPtr e) noexcept
    : op{std::move(o)}, expr{std::move(e)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + op + ' ' + expr->stringify(indent) + ')';
    }

    Node variant() const override { return this; }
};

struct BinOp : Expr {
    ExprPtr lhs;
    // const TokenKind token; // always name..I think
    std::string op;
    ExprPtr rhs;


    BinOp(ExprPtr e1, std::string o, ExprPtr e2) noexcept
    : lhs{std::move(e1)}, op{std::move(o)}, rhs{std::move(e2)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + lhs->stringify(indent) + ' ' + op + ' ' + rhs->stringify(indent) + ')';
    }

    Node variant() const override { return this; }
};

struct PostOp : Expr {
    // Token token; // also always name??
    std::string op;
    ExprPtr expr;


    PostOp(std::string o, ExprPtr e) noexcept
    : op{std::move(o)}, expr{std::move(e)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + expr->stringify(indent) + ' ' + op + ')';
    }

    Node variant() const override { return this; }
};

struct CircumOp : Expr {
    std::string op1;
    std::string op2;
    ExprPtr expr;

    CircumOp(std::string o1, std::string o2, ExprPtr e) noexcept
    : op1{std::move(o1)}, op2{std::move(o2)}, expr{std::move(e)} {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + op1 + ' ' + expr->stringify(indent) + ' ' + op2 + ')';
    }

    Node variant() const override { return this; }
};


struct OpCall : Expr {
    std::string first;
    std::vector<std::string> rest;
    std::vector<ExprPtr> exprs;
    std::vector<bool> op_pos;
    // bool begin_expr;
    // bool end_expr;

    OpCall(
        std::string f, std::vector<std::string> ops, std::vector<ExprPtr> ex,
        std::vector<bool> pos
        // const bool begin, const bool end
    ) noexcept
    :
    first{std::move(f)}, rest{std::move(ops)}, exprs{std::move(ex)}, op_pos{std::move(pos)}
    {}


    std::string stringify(const size_t indent = 0) const override {
        // std::string s = op_pos[0] ? first : exprs[0]->stringify(indent) + ' ' + first;

        // std::println(std::cerr, "{}, {}", first, rest);

        std::string s;
        for (ssize_t op = -1, i{}; const auto& field : op_pos) {
            if (field) {
                if (op == -1) s += first;
                else s += ' ' + rest[op];
                ++op;
            }
            else if (op == -1) s += exprs[i++]->stringify(indent) + ' ';
            else s += ' ' + exprs[i++]->stringify(indent);
        }


        // // if it begins with an expr, we already exhausted the first op
        // for (size_t i = not op_pos[0]; i < rest.size(); ++i) {
        //     // s += ':' + ops[i];
        //     s += exprs[i]->stringify(indent) + ' ' + rest[i - op_pos[0]];
        // }

        // if (not op_pos.back()) s += ' ' + exprs.back()->stringify(indent);

        return '(' + s + ')';
        // return '(' + op1 + ' ' + expr->stringify(indent) + ' ' + op2 + ')';
    }

    Node variant() const override { return this; }
};


struct Call : Expr {
    ExprPtr func;
    std::vector<ExprPtr> args;


    Call(ExprPtr function, std::vector<ExprPtr> arguments)
    : func{std::move(function)}, args{std::move(arguments)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        std::string s;
        s = func->stringify(indent) + '(';

        std::string_view comma = "";
        for (const auto& arg : args) {
            s += comma;
            s += arg->stringify(indent);
            comma = ", ";
        }

        return s + ')';
    }

    Node variant() const override { return this; }
};


// redeclared in Interpreter.hxx
// using Value = std::variant<int, double, bool, std::string, Closure>;


struct Closure : Expr {
    std::vector<std::string> params;

    // type::TypePtr return_type;
    type::FuncType type;
    ExprPtr body;

    mutable Environment env{};

    Closure(std::vector<std::string> ps, type::FuncType t, ExprPtr b)
    : params{std::move(ps)}, type{std::move(t)}, body{std::move(b)} {
        if(ps.size() != t.params.size()) {
            puts("ERROR!! This should never happen..,");
            exit(1);
        }
    }


    void capture(const Environment& e) const { // const as in doesn't change params or body.
        for(const auto& [key, value] : e)
        env[key] = value;
    }

    std::string stringify(const size_t indent = 0) const override {
        std::string s = "(";

        if (not params.empty()) s += params[0] + ": " + type.params[0]->text();
        for(size_t i{1}; i < params.size(); ++i)
            s += ", " + params[i] + ": " + type.params[i]->text();

        return s + "): " + type.ret->text() + " => " + body->stringify(indent);
    }

    Node variant() const override { return this; }
};


struct Block : Expr {
    std::vector<ExprPtr> lines;

    explicit Block(std::vector<ExprPtr> l) noexcept : lines{std::move(l)} {};


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "{\n";

        // std::ranges::for_each(lines, &Expr::stringify);
        for(const auto& line : lines) {
            s += std::string(indent + 4, ' ');
            s += line->stringify(indent + 4);
            s += ";\n";
        }

        s += std::string(indent, ' ') + "}";

        return s;
    }


    Node variant() const override { return this; }
};

// defintions of operators. Usage is BinOp or UnaryOp
struct Fix : Expr {
    // these two are literally what a token is...
    // std::string name;
    // TokenKind prec;

    std::string name;
    Token high;
    Token low;
    int shift; // needed for printing

    std::vector<ExprPtr> funcs;
    // ExprPtr func;


    Fix(std::string n, Token up, Token down, const int s, std::vector<ExprPtr> cs)
    : name{std::move(n)}, high{std::move(up)}, low{std::move(down)}, shift{s}, funcs{std::move(cs)} {}


    // Fix(std::string n, Token up, Token down, const int s, ExprPtr c)
    // : name{std::move(n)}, high{std::move(up)}, low{std::move(down)}, shift{s}, func{std::move(c)} {}


    virtual TokenKind type() const = 0;
};


struct Prefix : Fix {
    // Prefix(Token t, const int s, ExprPtr c)
    // : Fix{std::move(t), s, std::move(c)} {}
    using Fix::Fix;

    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, Token> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        const std::string shifts(size_t(std::abs(shift)), c);


        std::string s;

        for (const auto& func : funcs)
            s += "prefix(" + token.text + shifts + ") " + name + " = " + func->stringify(indent) + '\n';

        // return "prefix(" + token.text + shifts + ") " + name + " = " + func->stringify(indent);
        return s;
    }


    TokenKind type() const override { return TokenKind::PREFIX; }
    Node variant() const override { return this; }
};

struct Infix : Fix {
    using Fix::Fix;

    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, Token> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        const std::string shifts(size_t(std::abs(shift)), c);


        std::string s;

        for (const auto& func : funcs)
            s += "infix(" + token.text + shifts + ") " + name + " = " + func->stringify(indent) + '\n';

        // return "infix(" + token.text + shifts + ") " + name + " = " + func->stringify(indent);

        return s;
    }

    TokenKind type() const override { return TokenKind::INFIX; }
    Node variant() const override { return this; }
};

struct Suffix : Fix {
    using Fix::Fix;

    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, Token> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        const std::string shifts(size_t(std::abs(shift)), c);

        //! FIX THIS

        std::string s;

        for (const auto& func : funcs)
            s += "suffix(" + token.text + shifts + ") " + name + " = " + func->stringify(indent) + '\n';

        // return "suffix(" + token.text + shifts + ") " + name + " = " + func->stringify(indent);

        return s;
    }

    TokenKind type() const override { return TokenKind::SUFFIX; }
    Node variant() const override { return this; }
};

struct Exfix : Fix {
    std::string name2;
    Exfix(std::string n1, std::string n2, Token up, Token down, const int s, std::vector<ExprPtr> cs)
    : Fix{std::move(n1), std::move(up), std::move(down), s, std::move(cs)}, name2{std::move(n2)} {}

    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, Token> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        const std::string shifts(size_t(std::abs(shift)), c);

        std::string s;

        for (const auto& func : funcs)
            s += "exfix(" + token.text + shifts + ") " + name + " = " + func->stringify(indent) + '\n';


        // //! FIX THIS
        // return "exfix(" + token.text + shifts + ") " + name + ':' + name2 + " = " + func->stringify(indent);

        return s;

    }

    TokenKind type() const override { return TokenKind::EXFIX; }
    Node variant() const override { return this; }
};


struct Operator : Fix {
    std::vector<std::string> rest;
    std::vector<bool> op_pos;
    // bool begin_expr;
    // bool end_expr;

    Operator(
        std::string first, std::vector<std::string> rst, std::vector<bool> pos,
        Token up, Token down,
        const int s,
        // const bool begin, const bool end,
        std::vector<ExprPtr> cs
    )
    : Fix{
        std::move(first),
        std::move(up), std::move(down),
        s,
        std::move(cs)
    },
    rest{std::move(rst)}, op_pos{std::move(pos)}
    // begin_expr{begin}, end_expr{end}
    {
        if (size_t(std::ranges::count(op_pos, true)) != rest.size() + 1) { // + 1 for first
            // std::println("rest: {}", rest);
            puts("ERROR!! This should never happen..,");
            exit(1);
        }
    }


    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, Token> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        const std::string shifts(size_t(std::abs(shift)), c);



        // maybe I should cache that
        std::string op_name;
        for (ssize_t i = -1; const auto& field : op_pos) {
            if (field) {
                op_name += i == -1 ? name : rest[i];
                ++i;
            }
            else op_name += ':';
        }

        // const std::string& op_name = (begin_expr ? ':' : '\0')
        //     + name
        //     + std::accumulate(rest.cbegin(), rest.cend(), std::string{}, [](auto&& acc, auto&& e) { return acc + ':' + e; })
        //     + (end_expr ? ':' : '\0');


        std::string s;

        for (const auto& func : funcs)
            s += "operator(" + token.text + shifts + ") " + op_name + " = " + func->stringify(indent) + '\n';

        // return "operator(" + token.text + shifts + ") " + op_name + " = " + func->stringify(indent);

        return s;
    }


    TokenKind type() const override { return TokenKind::OPERATOR; }
    Node variant() const override { return this; }
};


} // namespace expr