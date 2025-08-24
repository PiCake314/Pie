#pragma once

#include <memory>

struct Dict;
struct ClassValue { std::shared_ptr<Dict> blueprint; };

namespace expr {

struct Num;
struct String;
struct Name;
struct Assignment;
struct Class;
struct Access;
struct Grouping;
struct UnaryOp;
struct BinOp;
struct PostOp;
struct CircumOp;
struct Call;
struct Closure;
struct Block;
// struct Fix;
struct Prefix;
struct Infix;
struct Suffix;
struct Exfix;

// has to be pointers kuz we're forward declareing
// has to be forward declared bc we're using in in the class bellow
using Node = std::variant<
    const Num*,
    const String*,
    const Name*,
    const Assignment*,
    const Class*,
    const Access*,
    const Grouping*,
    const UnaryOp*,
    const BinOp*,
    const PostOp*,
    const CircumOp*,
    const Call*,
    const Closure*,
    const Block*,
    // const Fix*,
    const Prefix*,
    const Infix*,
    const Suffix*,
    const Exfix*
>;


struct Expr {
    virtual ~Expr() = default;
    virtual std::string stringify(const size_t indent = 0) const = 0;

    virtual Node variant() const = 0;
};

using ExprPtr = std::shared_ptr<Expr>;


} // namespace expr




