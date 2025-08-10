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
using ExprPtr = std::shared_ptr< Expr>;


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
    virtual void print(const size_t indent) const = 0;

    virtual Node variant() const = 0;
};

struct Num : Expr {
    std::string num;


    Num(std::string n) noexcept : num{std::move(n)} {}

    void print(const size_t) const override { std::cout << num; }

    Node variant() const override { return this; }
};


struct String : Expr {
    std::string str;

    String(std::string s) noexcept : str{std::move(s)} {}

    void print(const size_t) const override { std::cout << '"' << str << '"'; }

    Node variant() const override { return this; }
};


struct Name : Expr {
    std::string name;


    Name(std::string n) noexcept : name{std::move(n)} {}

    void print(const size_t) const override { std::cout << name; }

    Node variant() const override { return this; }
};


struct Assignment : Expr {
    std::string name;
    ExprPtr expr;


    Assignment(std::string n, ExprPtr e) noexcept
    : name{std::move(n)}, expr{std::move(e)}
    {}

    void print(const size_t indent) const override {
        std::cout << name << " = ";
        expr->print(indent);
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

    void print(const size_t indent) const override {
        // puts("\nUNARY");

        std::cout << '(' << text << ' ';
        expr->print(indent);
        std::cout << ')';
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

    void print(const size_t indent) const override {
        std::cout << '(';

        lhs->print(indent);
        // puts("\nBINARY");
        std::cout << ' ' << text << ' ';
        rhs->print(indent);

        std::cout << ')';
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

    void print(const size_t indent) const override {
        // puts("\nPOST");

        std::cout << '(';
        expr->print(indent);
        std::cout << ' ' << text << ')';
    }

    Node variant() const override { return this; }
};


struct Call : Expr {
    ExprPtr func;
    std::vector<ExprPtr> args;


    Call(ExprPtr function, std::vector<ExprPtr> arguments)
    : func{std::move(function)}, args{std::move(arguments)}
    {}

    void print(const size_t indent) const override {
        func->print(indent);
        std::cout << '(';

        std::string_view comma = "";
        for (const auto& arg : args) {
            std::cout << comma;
            arg->print(indent);
            comma = ", ";
        }

        std::cout << ')';
    }

    Node variant() const override { return this; }
};


using Value = std::variant<int, std::string, Closure>;
using Environment = std::unordered_map<std::string, Value>;

struct Closure : Expr {
    std::vector<std::string> params;
    ExprPtr body;

    private:
    mutable std::optional<Environment> env{}; // make this optional<const Environment> would delete move/copy constructros
    public:

    Closure(std::vector<std::string> ps, ExprPtr b)
    : params{std::move(ps)}, body{std::move(b)} {};

    void capture(Environment e) const {
        // std::print("Captured: {{");
        // for(const auto& [key, value] : e){
        //     if (value.index() == 0){
        //         std::print("{}: {}, ", key, std::get<0>(value));
        //     }
        //     else if(value.index() == 1) {
        //         std::print("{}: {}, ", key, std::get<1>(value));
        //     }
        //     else std::print("{}: {}, ", key, "'Closure'");
        // }
        // std::println("}}");

        if (env) error("Can't capture twice. Internal inerpreter error.\nFile a bug report please at:\nhttps://github.com/PiCake314/Pie");
        env.emplace(std::move(e));
    }

    const Environment& environment() const {
        if(not env) error("Use bofore initialization!");
        return *env;
    }

    void print(const size_t indent) const override {
        std::cout << '(';

        if (not params.empty()) std::cout << params[0];
        for(size_t i{1}; i < params.size(); ++i)
            std::cout << ", " << params[i];

        std::cout << ") => ";
        body->print(indent);
    }

    Node variant() const override { return this; }
};


struct Block : Expr {
    std::vector<ExprPtr> lines;

    Block(std::vector<ExprPtr> l) noexcept : lines{std::move(l)} {};


    void print(const size_t indent) const override {
        std::cout << "{\n";

        // std::ranges::for_each(lines, &Expr::print);
        for(const auto& line : lines) {
            std::cout << std::string(indent + 4, ' ');
            line->print(indent + 4);
            puts(";");
        }

        std::cout << std::string(indent, ' ') << "}";
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

using Operators = std::unordered_map<std::string, Fix*>;


struct Prefix : Fix {
    // Prefix(Token t, const int s, ExprPtr c)
    // : Fix{std::move(t), s, std::move(c)} {}
    using Fix::Fix;

    void print(const size_t indent) const override {
        // const auto [c, token] = [this] -> std::pair<char, Token> {
        //     return 
        // }();

        // const std::string shifts(size_t(std::abs(shift)), c);

        //! FIX THIS
        // std::cout << "prefix(" << stringify(token.kind) << shifts << ") "  << token.text << ' ';
        // func->print(indent);
    }


    TokenKind type() const override { return TokenKind::PREFIX; }
    Node variant() const override { return this; }
};

struct Infix : Fix {
    using Fix::Fix;

    void print(const size_t indent) const override {
        const char c = shift < 0 ? '-' : '+';
        const std::string shifts(size_t(std::abs(shift)), c);

        //! FIX THIS
        // std::cout << "infix(" << stringify(token.kind) << shifts << ") "  << token.text << ' ';
        func->print(indent);
    }

    TokenKind type() const override { return TokenKind::INFIX; }
    Node variant() const override { return this; }
};

struct Suffix : Fix {
    using Fix::Fix;

    void print(const size_t indent) const override {
        const char c = shift < 0 ? '-' : '+';
        const std::string shifts(size_t(std::abs(shift)), c);

        //! FIX THIS
        // std::cout << "suffix(" << stringify(token.kind) << shifts << ") "  << token.text << " = ";
        func->print(indent);
    }

    TokenKind type() const override { return TokenKind::SUFFIX; }
    Node variant() const override { return this; }
};
