#pragma once

#include <memory>



struct Dict;
struct ClassValue { std::shared_ptr<Dict> blueprint; };

namespace expr {

// struct Num;
// struct String;
// struct Name;
// struct Assignment;
// struct Class;
// struct Access;
// struct Grouping;
// struct UnaryOp;
// struct BinOp;
// struct PostOp;
// struct CircumOp;
// struct Call;
// struct Closure;
// struct Block;
// // struct Fix;
// struct Prefix;
// struct Infix;
// struct Suffix;
// struct Exfix;
// struct Operator;

// has to be pointers kuz we're forward declareing
// has to be forward declared bc we're using in in the class bellow
using Node = std::variant<
    const struct Num*,
    const struct Bool*,
    const struct String*,
    const struct Name*,
    const struct Assignment*,
    const struct Class*,
    const struct Access*,
    const struct Grouping*,
    const struct UnaryOp*,
    const struct BinOp*,
    const struct PostOp*,
    const struct CircumOp*,
    const struct OpCall*,
    const struct Call*,
    const struct Closure*,
    const struct Block*,
    // const Fix*,
    const struct Prefix*,
    const struct Infix*,
    const struct Suffix*,
    const struct Exfix*,
    const struct Operator*
>;


struct Expr {
    virtual ~Expr() = default;
    virtual std::string stringify(const size_t indent = 0) const = 0;

    virtual Node variant() const = 0;
};

using ExprPtr = std::shared_ptr<Expr>;


} // namespace expr




