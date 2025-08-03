#pragma once

#include "Token.hxx"
#include <cmath>
#include <string>
#include <iostream>
#include <utility>
#include <memory>

struct Expr;
using ExprPtr = std::unique_ptr< Expr>;

struct Expr {
    virtual ~Expr() = default;
    virtual void print() const = 0;
};

struct Num : Expr {
    std::string num;


    Num(std::string n) noexcept : num{std::move(n)} {}

    void print() const override { std::cout << num; }
};

struct Name : Expr {
    std::string name;


    Name(std::string n) noexcept : name{std::move(n)} {}

    void print() const override { std::cout << name; }
};


struct Assignment : Expr {
    std::string name;
    ExprPtr expr;


    Assignment(std::string n, ExprPtr e) noexcept
    : name{std::move(n)}, expr{std::move(e)}
    {}

    void print() const override {
        std::cout << name << " = ";
        expr->print();
    }
};

struct BinOp : Expr {
    ExprPtr lhs;
    // const TokenKind token; // always name..I think
    std::string text;
    ExprPtr rhs;


    BinOp(ExprPtr e1, std::string txt, ExprPtr e2) noexcept
    : lhs{std::move(e1)}, text{std::move(txt)}, rhs{std::move(e2)}
    {}

    void print() const override {
        std::cout << '(';

        lhs->print();
        std::cout << ' ' << text << ' ';
        rhs->print();

        std::cout << ')';
    }
};

struct UnaryOp : Expr {
    UnaryOp(const TokenKind t, ExprPtr e) noexcept
    : token{t}, expr{std::move(e)}
    {}

    void print() const override {
        std::cout << '(' << stringify(token);
        expr->print();
        std::cout << ')';
    }

    TokenKind token;
    ExprPtr expr;
};


struct Call : Expr {
    Call(ExprPtr function, std::vector<ExprPtr> arguments)
    : func{std::move(function)}, args{std::move(arguments)}
    {}


    void print() const noexcept {
        func->print();
        std::cout << '(';

        std::string_view comma = "";
        for (const auto& arg : args) {
            std::cout << comma;
            arg->print();
            comma = ", ";
        }

        std::cout << ')';
    }

    ExprPtr func;
    std::vector<ExprPtr> args;
};

struct Closure : Expr {
    std::vector<std::string> params;
    ExprPtr body;

    Closure(std::vector<std::string> ps, ExprPtr b)
    : params{std::move(ps)}, body{std::move(b)} {};

    void print() const override {
        std::cout << '(';

        if (not params.empty()) std::cout << params[0];
        for(size_t i{1}; i < params.size(); ++i)
            std::cout << ", " << params[i];

        std::cout << ") => ";
        body->print();
    }
};



struct Fix : Expr {
    Fix(std::string n, const TokenKind p, const int s, Closure c)
    : name{std::move(n)}, prec{p}, shift{s}, func{std::move(c)} {}

    std::string name;
    TokenKind prec;
    int shift;
    Closure func;
};

struct Prefix : Fix {
    Prefix(std::string n, const TokenKind p, const int s, Closure c)
    : Fix{std::move(n), p, s, std::move(c)} {}

    void print() const override {
        const char c = shift < 0 ? '-' : '+';
        const std::string shifts(size_t(std::abs(shift)), c);

        std::cout << "prefix(" << stringify(prec) << shifts << ") "  << name << ' ';
        func.print();
    }
};

struct Infix : Fix {
    Infix(std::string n, const TokenKind p, const int s, Closure c)
    : Fix{std::move(n), p, s, std::move(c)} {}

    void print() const override {
        const char c = shift < 0 ? '-' : '+';
        const std::string shifts(size_t(std::abs(shift)), c);

        std::cout << "infix(" << stringify(prec) << shifts << ") "  << name << ' ';
        func.print();
    }
};

struct Suffix : Fix {
    Suffix(std::string n, const TokenKind p, const int s, Closure c)
    : Fix{std::move(n), p, s, std::move(c)} {}

    void print() const override {
        const char c = shift < 0 ? '-' : '+';
        const std::string shifts(size_t(std::abs(shift)), c);

        std::cout << "suffix(" << stringify(prec) << shifts << ") "  << name << ' ';
        func.print();
    }
};
