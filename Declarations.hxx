#pragma once

#include <memory>


struct Dict;
struct ClassValue { std::shared_ptr<Dict> blueprint; };
struct NameSpace  { std::shared_ptr<Dict> members  ; };

struct List;
using PackList = std::shared_ptr<List>;

template <typename ...Ts>
[[nodiscard]] inline std::shared_ptr<List> makePack(Ts&&... args) {
    return std::make_shared<List>(std::forward<Ts>(args)...);
}

namespace expr {

// has to be pointers kuz we're forward declareing
// has to be forward declared bc we're using in in the class bellow
using Node = std::variant<
    const struct Num         *,
    const struct Bool        *,
    const struct String      *,
    const struct Name        *,
    const struct Pack        *,
    const struct Expansion   *,
    const struct UnaryFold   *,
    const struct BinaryFold  *,
    const struct Assignment  *,
    const struct Class       *,
    const struct Union       *,
    const struct Match       *,
    const struct Access      *,
    const struct Namespace   *,
    const struct Use         *,
//  const struct Import      *,
    const struct SpaceAccess *,
    const struct Grouping    *,
    const struct UnaryOp     *,
    const struct BinOp       *,
    const struct PostOp      *,
    const struct CircumOp    *,
    const struct OpCall      *,
    const struct Call        *,
    const struct Closure     *,
    const struct Block       *,
    const struct Prefix      *,
    const struct Infix       *,
    const struct Suffix      *,
    const struct Exfix       *,
    const struct Operator    *
>;


struct Expr {
    virtual ~Expr() = default;
    virtual std::string stringify(const size_t indent = 0) const = 0;

    virtual Node variant() const = 0;
};

using ExprPtr = std::shared_ptr<Expr>;


} // namespace expr




