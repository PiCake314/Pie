#pragma once

#include <string_view>
#include <variant>
#include <memory>

#ifdef WEB_PIE
using BigInt = long long;
#else
using BigInt = ssize_t;
#endif

inline namespace pie {

namespace expr {

// has to be pointers kuz we're forward declareing
// has to be forward declared bc we're using in in the class bellow

using Node = std::variant<
    struct Num               *,
    struct Bool              *,
    struct String            *,
    struct Name              *,
    // struct Pack              *,
    struct List              *,
    struct Map               *,
    struct Expansion         *,
    struct UnaryFold         *,
    struct SeparatedUnaryFold*,
    struct BinaryFold        *,
    struct Assignment        *,
    struct Class             *,
    struct Union             *,
    struct Match             *,
    struct Type              *,
    struct Loop              *,
    struct Break             *,
    struct Continue          *,
    struct Access            *,
    struct Cascade           *,
    struct Namespace         *,
    struct Use               *,
    struct UseSpace          *,
    struct Import            *,
    struct SpaceAccess       *,
    struct Grouping          *,
    struct UnaryOp           *,
    struct BinOp             *,
    struct PostOp            *,
    struct CircumOp          *,
    struct OpCall            *,
    struct Call              *,
    struct Closure           *,
    struct Block             *,
    struct Prefix            *,
    struct Infix             *,
    struct Suffix            *,
    struct Exfix             *,
    struct Operator          *
>;


struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct Expr {
    ssize_t ID{-1};

    virtual ~Expr() = default;
    virtual std::string stringify(const size_t indent = 0) const = 0;
    virtual bool involvesName(const std::string_view sv) const = 0;
    virtual ExprPtr left() const = 0;
    virtual Node variant() = 0;
};



} // namespace expr
} // namespace pie