#pragma once

#include "Token.hxx"
#include <iostream>
#include <cmath>
#include <string>
#include <utility>
#include <algorithm>
#include <memory>
#include <variant>

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;


struct Num;
struct String;
struct Name;
struct Assignment;
struct UnaryOp;
struct BinOp;
struct PostOp;
struct Call;
struct Closure;
struct Block;
struct Fix;
// struct Prefix;
// struct Infix;
// struct Suffix;

// has to be pointers kuz we're forward declareing
// has to be forward declared bc we're using in in the class bellow
using Node = std::variant<
    const Num*,
    const String*,
    const Name*,
    const Assignment*,
    const UnaryOp*,
    const BinOp*,
    const PostOp*,
    const Call*,
    const Closure*,
    const Fix*,
    const Block*
    // const Prefix*,
    // const Infix*,
    // const Suffix*
>;


struct Expr {
    virtual ~Expr() = default;
    virtual std::string stringify(const size_t indent) const = 0;

    virtual Node variant() const = 0;
};

struct Num : Expr {
    std::string num;

    Num(std::string n) noexcept : num{std::move(n)} {}

    std::string stringify(const size_t) const override { return num; }

    Node variant() const override { return this; }
};


struct String : Expr {
    std::string str;

    String(std::string s) noexcept : str{std::move(s)} {}

    std::string stringify(const size_t) const override { return '"' + str + '"'; }

    Node variant() const override { return this; }
};


struct Name : Expr {
    std::string name;


    Name(std::string n) noexcept : name{std::move(n)} {}

    std::string stringify(const size_t) const override { return name; }

    Node variant() const override { return this; }
};


struct Assignment : Expr {
    // std::string name;
    ExprPtr lhs;
    ExprPtr rhs;


    Assignment(ExprPtr l, ExprPtr r) noexcept
    : lhs{std::move(l)}, rhs{std::move(r)}
    {}

    std::string stringify(const size_t indent) const override {
        return lhs->stringify(indent) + " = " + rhs->stringify(indent);
    }

    Node variant() const override { return this; }
};

struct UnaryOp : Expr {
    // Token token; // also always name??
    std::string text;
    ExprPtr expr;


    UnaryOp(std::string t, ExprPtr e) noexcept
    : text{std::move(t)}, expr{std::move(e)}
    {}

    std::string stringify(const size_t indent) const override {
        return '(' + text + ' ' + expr->stringify(indent) + ')';
    }

    Node variant() const override { return this; }
};

struct BinOp : Expr {
    ExprPtr lhs;
    // const TokenKind token; // always name..I think
    std::string text;
    ExprPtr rhs;


    BinOp(ExprPtr e1, std::string txt, ExprPtr e2) noexcept
    : lhs{std::move(e1)}, text{std::move(txt)}, rhs{std::move(e2)}
    {}

    std::string stringify(const size_t indent) const override {
        return '(' + lhs->stringify(indent) + ' ' + text + ' ' + rhs->stringify(indent) + ')';
    }

    Node variant() const override { return this; }
};

struct PostOp : Expr {
    // Token token; // also always name??
    std::string text;
    ExprPtr expr;


    PostOp(std::string t, ExprPtr e) noexcept
    : text{std::move(t)}, expr{std::move(e)}
    {}

    std::string stringify(const size_t indent) const override {
        return '(' + expr->stringify(indent) + ' ' + text + ')';
    }

    Node variant() const override { return this; }
};


struct Call : Expr {
    ExprPtr func;
    std::vector<ExprPtr> args;


    Call(ExprPtr function, std::vector<ExprPtr> arguments)
    : func{std::move(function)}, args{std::move(arguments)}
    {}

    std::string stringify(const size_t indent) const override {
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
using Value = std::variant<int, double, bool, std::string, Closure>;
using Environment = std::unordered_map<std::string, Value>;

struct Closure : Expr {
    std::vector<std::string> params;
    ExprPtr body;

    mutable Environment env{};

    Closure(std::vector<std::string> ps, ExprPtr b)
    : params{std::move(ps)}, body{std::move(b)} {};


    void capture(const Environment& e) const { // const as in doesn't change params or body.
        for(const auto& [key, value] : e)
        env[key] = value;
    }

    std::string stringify(const size_t indent) const override {
        std::string s = "(";

        if (not params.empty()) s += params[0];
        for(size_t i{1}; i < params.size(); ++i)
            s += ", " + params[i];

        s += ") => " + body->stringify(indent);

        return s;
    }

    Node variant() const override { return this; }
};


struct Block : Expr {
    std::vector<ExprPtr> lines;

    Block(std::vector<ExprPtr> l) noexcept : lines{std::move(l)} {};


    std::string stringify(const size_t indent) const override {
        std::string s = "{\n";

        // std::ranges::for_each(lines, &Expr::stringify);
        for(const auto& line : lines) {
            s += std::string(indent + 4, ' ');
            s += line->stringify(indent + 4);
            s += ";\n";
        }

        s += std::string(indent, ' ') + "};";

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
    ExprPtr func;

    Fix(std::string n, Token up, Token down, const int s, ExprPtr c)
    : name{std::move(n)}, high{std::move(up)}, low{std::move(down)}, shift{s}, func{std::move(c)} {}


    virtual TokenKind type() const = 0;
};


struct Prefix : Fix {
    // Prefix(Token t, const int s, ExprPtr c)
    // : Fix{std::move(t), s, std::move(c)} {}
    using Fix::Fix;

    std::string stringify(const size_t indent) const override {
        const auto [c, token] = [this] -> std::pair<char, Token> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        const std::string shifts(size_t(std::abs(shift)), c);


        return "prefix(" + token.text + shifts + ") " + name + ' ' +func->stringify(indent);
    }


    TokenKind type() const override { return TokenKind::PREFIX; }
    Node variant() const override { return this; }
};

struct Infix : Fix {
    using Fix::Fix;

    std::string stringify(const size_t indent) const override {
        const auto [c, token] = [this] -> std::pair<char, Token> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        const std::string shifts(size_t(std::abs(shift)), c);


        return "infix(" + token.text + shifts + ") " + name + ' ' + func->stringify(indent);
    }

    TokenKind type() const override { return TokenKind::INFIX; }
    Node variant() const override { return this; }
};

struct Suffix : Fix {
    using Fix::Fix;

    std::string stringify(const size_t indent) const override {
        const auto [c, token] = [this] -> std::pair<char, Token> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        const std::string shifts(size_t(std::abs(shift)), c);

        //! FIX THIS
        return "suffix(" + token.text + shifts + ") " + name + ' ' + func->stringify(indent);
    }

    TokenKind type() const override { return TokenKind::SUFFIX; }
    Node variant() const override { return this; }
};
