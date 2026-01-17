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

struct Members  { std::vector<std::tuple<expr::Name, type::TypePtr, Value>> members; };
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




// inline namespace pie {
// inline namespace value {


// struct Members;
// // struct ClassValue;
// // using Object = std::pair<ClassValue, std::shared_ptr<Members>>;
// using Object = std::pair<type::TypePtr, std::shared_ptr<Members>>;

// // * Struct defined in Declarations
// // using Value = std::variant<
// //     ssize_t,
// //     double,
// //     bool,
// //     std::string,
// //     expr::Closure,
// //     type::TypePtr,
// //     NameSpace,
// //     Object,
// //     expr::Node,
// //     PackList,
// //     ListValue,
// //     MapValue
// // >;


// // struct ValuePtr { std::shared_ptr<Value> value; };


// inline std::string stringify(const Value& value, const size_t indent = {});
// }
// }


// // needed for maps
// template<>
// struct std::hash<Value> { size_t operator()(const pie::value::Value& value) const { return std::hash<std::string>{}(pie::value::stringify(value)); } };


// inline namespace pie {

// inline namespace value {


// struct Members  { std::vector<std::tuple<expr::Name, type::TypePtr, Value>> members; };
// struct Elements { std::vector<Value> values;                                         };
// struct Items    { std::unordered_map<Value, Value> map;                              };



// template <typename ...Ts>
// [[nodiscard]] inline PackList makePack(Ts&&... args) {
//     return std::make_shared<Elements>(std::forward<Ts>(args)...);
// }

// [[nodiscard]] inline ListValue makeList(std::vector<Value> values = {}) {
//     return {std::make_shared<Elements>(std::move(values))};
// }

// [[nodiscard]] inline MapValue makeMap(std::unordered_map<Value, Value> items = {}) {
//     return {std::make_shared<Items>(std::move(items))};
// }



// // using Environment = std::unordered_map<std::string, std::pair<Value, type::TypePtr>>;
// using Environment = std::unordered_map<
//     std::string,
//     std::pair<
//         std::shared_ptr<Value>,
//         type::TypePtr
//     >
// >;


// inline std::string stringify(const Value& value, const size_t indent) {
//     std::string s;

//     if (std::holds_alternative<bool>(value)) {
//         const auto& v = std::get<bool>(value);
//         // s = std::to_string(v); // reason I did this in the first place is to allow for copy-ellision to happen
//         s = v ? "true" : "false";
//     }

//     else if (std::holds_alternative<ssize_t>(value)) {
//         const auto& v = std::get<ssize_t>(value);

//         s = std::to_string(v);
//     }

//     else if (std::holds_alternative<double>(value)) {
//         const auto& v = std::get<double>(value);
//         s = std::to_string(v);
//     }

//     else if (std::holds_alternative<std::string>(value)) {
//         s = std::get<std::string>(value);
//     }

//     else if (std::holds_alternative<expr::Closure>(value)) {
//         const auto& v = std::get<expr::Closure>(value);

//         s = v.stringify(indent);
//     }

//     // else if (std::holds_alternative<ClassValue>(value)) {
//     //     const auto& v = std::get<ClassValue>(value);

//     //     if (v.blueprint->members.empty())
//     //         s = "class { }";
//     //     else {
//     //         s = "class {\n";

//     //         const std::string space(indent + 4, ' ');
//     //         for (const auto& [name, type, value] : v.blueprint->members) {
//     //             s += space + name.stringify() + ": " + type->text(indent + 4) + " = ";

//     //             const bool is_string = std::holds_alternative<std::string>(value);
//     //             if (is_string) s += '\"';

//     //             s += stringify(value, indent + 4);

//     //             if (is_string) s += '\"';

//     //             s += ";\n";
//     //         }

//     //         s += std::string(indent, ' ') + '}';
//     //     }
//     // }

//     // else if (std::holds_alternative<expr::Union>(value)) {
//     //     const auto& v = std::get<expr::Union>(value);

//     //     s = v.stringify(indent);
//     // }


//     else if (std::holds_alternative<type::TypePtr>(value)) s = get<type::TypePtr>(value)->text();

//     else if (std::holds_alternative<NameSpace>(value)) {
//         const auto& v = get<NameSpace>(value);

//         if (v.members->members.empty())
//             s = "space { }";
//         else {
//             s = "space {\n";

//             const std::string space(indent + 4, ' ');
//             for (const auto& [name, type, value] : v.members->members) {
//                 s += space + name.stringify() + ": " + type->text(indent + 4) + " = ";

//                 const bool is_string = std::holds_alternative<std::string>(value);
//                 if (is_string) s += '\"';

//                 s += stringify(value, indent + 4);

//                 if (is_string) s += '\"';

//                 s += ";\n";
//             }

//             s += std::string(indent, ' ') + '}';
//         }
//     }

//     else if (std::holds_alternative<Object>(value)) {
//         const auto& v = std::get<Object>(value);

//         if (v.second->members.empty()) {
//             s = "Object { }";
//         }
//         else {
//             s = "Object {\n";

//             const std::string space(indent + 4, ' ');
//             for (const auto& [name, _, value] : v.second->members) {
//                 s += space + name.stringify() + " = ";

//                 const bool is_string = std::holds_alternative<std::string>(value);
//                 if (is_string) s += '\"';

//                 s += stringify(value, indent + 4);

//                 if (is_string) s += '\"';

//                 s += ";\n";
//             }

//             s += std::string(indent, ' ') + '}';
//         }


//     }

//     else if(std::holds_alternative<expr::Node>(value)) {
//         const std::string space(indent + 4, ' ');

//         s = "Syntax {\n" + space + std::visit(
//             [indent] (const auto& v) { return v->stringify(indent + 4); },
//             get<expr::Node>(value)
//         ) + '\n' + std::string(indent, ' ') + '}';
//     }

//     else if (std::holds_alternative<PackList>(value)) {
//         std::string comma = "";
//         for (const auto& v : get<PackList>(value)->values) {
//             s += comma + stringify(v, indent + 4);
//             comma = ", ";
//         }
//     }
//     else if (std::holds_alternative<ListValue>(value)) {
//         s += '{';

//         std::string comma = "";
//         for (const auto& v : get<ListValue>(value).elts->values) {
//             s += comma + stringify(v, indent + 4);
//             comma = ", ";
//         }

//         s += '}';
//     }
//     else if (std::holds_alternative<MapValue>(value)) {
//         s += '{';

//         std::string comma = "";
//         for (const auto& [key, value] : get<MapValue>(value).items->map) {
//             s += comma + stringify(key, indent + 4) + ": " + stringify(value, indent + 4);
//             comma = ", ";
//         }

//         s += '}';
//     }

//     else error("Type not found!");

//     return s;
// }


// inline std::ostream& operator<<(std::ostream& os, const Environment& env) {
//     for (const auto& [name, expr] : env){
//         const auto& [value, type] = expr;
//         os << name << ": " << type->text() << " = " << stringify(value) << std::endl;
//     }

//     return os;
// }


// [[nodiscard]] inline bool operator==(const Value& lhs, const Value& rhs) noexcept {
//     if (std::holds_alternative<ssize_t>(lhs) and std::holds_alternative<ssize_t>(rhs))
//         return get<ssize_t>(lhs) == get<ssize_t>(rhs);

//     if (std::holds_alternative<double>(lhs) and std::holds_alternative<double>(rhs))
//         return get<double>(lhs) == get<double>(rhs);

//     if (std::holds_alternative<bool>(lhs) and std::holds_alternative<bool>(rhs))
//         return get<bool>(lhs) == get<bool>(rhs);

//     if (std::holds_alternative<std::string>(lhs) and std::holds_alternative<std::string>(rhs))
//         return get<std::string>(lhs) == get<std::string>(rhs);

//     if (std::holds_alternative<expr::Closure>(lhs) and std::holds_alternative<expr::Closure>(rhs))
//         return get<expr::Closure>(lhs).stringify() == get<expr::Closure>(rhs).stringify();


//     if (std::holds_alternative<type::TypePtr>(lhs) and std::holds_alternative<type::TypePtr>(rhs)) {

//         if (type::isClass(get<type::TypePtr>(lhs)) and type::isClass(get<type::TypePtr>(rhs))) {
//             const auto a = dynamic_cast<const type::LiteralType&>(*get<type::TypePtr>(lhs).get()).cls->blueprint->members,
//                        b = dynamic_cast<const type::LiteralType&>(*get<type::TypePtr>(rhs).get()).cls->blueprint->members;

//             return a.size() == b.size() and
//                 std::ranges::all_of(
//                     std::views::zip(a, b),
//                     [] (const auto& tuple) {
//                         const auto& [member1, member2] = tuple;
//                         const auto& [name1, type1, value1] = member1;
//                         const auto& [name2, type2, value2] = member2;

//                         return name1.stringify() == name2.stringify()
//                                     and *type1 == *type2
//                                     and value1 == value2;
//                     }
//                 );
//         }

//         return *get<type::TypePtr>(lhs) == *get<type::TypePtr>(rhs);
//     }

//     if (std::holds_alternative<Object>(lhs) and std::holds_alternative<Object>(rhs)) {
//         const auto& a = get<Object>(lhs).second->members, b = get<Object>(rhs).second->members;
//         return
//             // a.first == b.first and
//             a.size() == b.size() and
//             std::ranges::all_of(
//                 std::views::zip(a, b),
//                 [] (const auto& tuple) {
//                     const auto& [member1, member2] = tuple;
//                     const auto& [name1, type1, value1] = member1;
//                     const auto& [name2, type2, value2] = member2;

//                     return name1.stringify() == name2.stringify()
//                                   and *type1 == *type2
//                                   and value1 == value2;
//                 }
//             );
//     }

//     if (std::holds_alternative<expr::Node>(lhs) and std::holds_alternative<expr::Node>(rhs))
//         error("Can't check equality of a Syntax!");

//     if (std::holds_alternative<PackList>(lhs) and std::holds_alternative<PackList>(rhs))
//         return get<PackList>(lhs)->values == get<PackList>(rhs)->values;


//     // error();
//     return false;
// }

// } // namespace value
// } // namespace pie
