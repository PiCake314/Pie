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

struct UnaryOp : Expr {
    UnaryOp(std::string t, ExprPtr e) noexcept
    : text{std::move(t)}, expr{std::move(e)}
    {}

    void print() const override {
        // puts("\nUNARY");

        std::cout << '(' << text << ' ';
        expr->print();
        std::cout << ')';
    }

    // Token token; // also always name??
    std::string text;
    ExprPtr expr;
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
        // puts("\nBINARY");
        std::cout << ' ' << text << ' ';
        rhs->print();

        std::cout << ')';
    }
};

struct PostOp : Expr {
    PostOp(std::string t, ExprPtr e) noexcept
    : text{std::move(t)}, expr{std::move(e)}
    {}

    void print() const override {
        // puts("\nPOST");

        std::cout << '(';
        expr->print();
        std::cout << ' ' << text << ')';
    }

    // Token token; // also always name??
    std::string text;
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




// defintions of operators. Usage is BinOp or UnaryOp
struct Fix : Expr {
    // these two are literally what a token is...
    // std::string name;
    // TokenKind prec;

    Token token;
    int shift;
    ExprPtr func;

    Fix(Token t, const int s, ExprPtr c)
    : token{std::move(t)}, shift{s}, func{std::move(c)} {}


    virtual TokenKind type() const = 0;
};

struct Prefix : Fix {
    Prefix(Token t, const int s, ExprPtr c)
    : Fix{std::move(t), s, std::move(c)} {}

    void print() const override {
        const char c = shift < 0 ? '-' : '+';
        const std::string shifts(size_t(std::abs(shift)), c);

        std::cout << "prefix(" << stringify(token.kind) << shifts << ") "  << token.text << ' ';
        func->print();
    }


    TokenKind type() const override { return TokenKind::PREFIX; }
};

struct Infix : Fix {
    Infix(Token t, const int s, ExprPtr c)
    : Fix{std::move(t), s, std::move(c)} {}

    void print() const override {
        const char c = shift < 0 ? '-' : '+';
        const std::string shifts(size_t(std::abs(shift)), c);

        std::cout << "infix(" << stringify(token.kind) << shifts << ") "  << token.text << ' ';
        func->print();
    }

    TokenKind type() const override { return TokenKind::INFIX; }
};

struct Suffix : Fix {
    Suffix(Token t, const int s, ExprPtr c)
    : Fix{std::move(t), s, std::move(c)} {}

    void print() const override {
        const char c = shift < 0 ? '-' : '+';
        const std::string shifts(size_t(std::abs(shift)), c);

        std::cout << "suffix(" << stringify(token.kind) << shifts << ") "  << token.text << ' ';
        func->print();
    }

    TokenKind type() const override { return TokenKind::SUFFIX; }
};
