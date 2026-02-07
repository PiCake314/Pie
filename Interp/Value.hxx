#pragma once

#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <utility>


#include "../Expr/Expr.hxx"
#include "../Type/Type.hxx"
#include "../Declarations.hxx"
#include "../Utils/utils.hxx"


inline namespace pie {
inline namespace value {

struct Members;
struct ClassValue { std::shared_ptr<Members> blueprint; };
struct NameSpace  { std::shared_ptr<Members> members  ; };

struct Elements;
struct ListValue { std::shared_ptr<Elements> elts; };

struct Items;
struct MapValue { std::shared_ptr<Items> items; };

using PackList = std::shared_ptr<Elements>;


using VariantType = std::variant<
    ssize_t,
    double,
    bool,
    std::string,
    expr::Closure,
    type::TypePtr,
    NameSpace,
    Object,
    expr::Node,
    PackList,
    ListValue,
    MapValue
>;

struct Value : VariantType {
    using VariantType::variant;
    using VariantType::operator=;
};

using ValuePtr = std::shared_ptr<Value>;


std::string stringify(const Value& value, const size_t indent = {});
[[nodiscard]] bool operator==(const Value& lhs, const Value& rhs) noexcept;
}
}

// needed for maps
template<>
struct std::hash<Value> { size_t operator()(const pie::value::Value& value) const { return std::hash<std::string>{}(pie::value::stringify(value)); } };


inline namespace pie {
inline namespace value {

struct Members  { std::vector<std::tuple<expr::Name, type::TypePtr, ValuePtr>> members; };
struct Elements { std::vector<Value> values;                                         };
struct Items    { std::unordered_map<Value, Value> map;                              };


template <typename ...Ts>
[[nodiscard]] inline PackList makePack(Ts&&... args) {
    return std::make_shared<Elements>(std::forward<Ts>(args)...);
}

[[nodiscard]] inline ListValue makeList(std::vector<Value> values = {}) {
    return {std::make_shared<Elements>(std::move(values))};
}

[[nodiscard]] inline MapValue makeMap(std::unordered_map<Value, Value> items = {}) {
    return {std::make_shared<Items>(std::move(items))};
}



using Environment = std::unordered_map<
    std::string,
    std::pair<
             ValuePtr,
        type::TypePtr
    >
>;

}
}

