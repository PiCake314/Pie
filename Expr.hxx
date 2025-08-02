#pragma once

#include "Token.hxx"
#include <string>
#include <iostream>
#include <utility>
#include <memory>

struct Expr;
using ExprPtr = std::unique_ptr<const Expr>;

struct Expr {
    virtual ~Expr() = default;
    virtual void print() const = 0;
};

struct Num : Expr {
    const std::string name;


    Num(std::string n) noexcept : name{std::move(n)} {}

    void print() const override { std::cout << name; }
};

struct Name : Expr {
    const std::string name;


    Name(std::string n) noexcept : name{std::move(n)} {}

    void print() const override { std::cout << name; }
};


struct Assignment : Expr {
    const std::string name;
    const ExprPtr expr;


    Assignment(std::string n, ExprPtr e) noexcept
    : name{std::move(n)}, expr{std::move(e)}
    {}

    void print() const override {
        std::cout << name << " = ";
        expr->print();
    }
};

struct BinOp : Expr {
    const ExprPtr lhs;
    // const TokenKind token; // always name..I think
    const std::string text;
    const ExprPtr rhs;


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

    const TokenKind token;
    const ExprPtr expr;
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

    const ExprPtr func;
    const std::vector<ExprPtr> args;
};


struct Closure : Expr {
    const std::vector<std::string> params;
    const ExprPtr body;

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