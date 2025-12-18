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


struct Members;
struct ClassValue;
using Object = std::pair<ClassValue, std::shared_ptr<Members>>;


// using expr::Union instead of a separate class for Union since they'd probably be the same
// it might come and bite me later, but I mean..
// I've been using expr::Closure for a while now and it's been okay
// + expr::Union doesn't have any ExprPtr in it anyway
using Value = std::variant<
    ssize_t,
    double,
    bool,
    std::string,
    expr::Closure,
    ClassValue,
    expr::Union,
    NameSpace,
    Object,
    expr::Node,
    PackList,
    ListValue,
    MapValue
>;

struct Members { std::vector<std::tuple<expr::Name, type::TypePtr, Value>> members; };
struct Elements { std::vector<Value> values; };

inline std::string stringify(const Value& value, const size_t indent = {});

template<>
struct std::hash<Value> {
    size_t operator()(const Value& value) const { return std::hash<std::string>{}(stringify(value)); }
};


struct Items { std::unordered_map<Value, Value> map; };


[[nodiscard]] inline ListValue makeList(std::vector<Value> values) {
    return {std::make_shared<Elements>(std::move(values))};
}

using Environment = std::unordered_map<std::string, std::pair<Value, type::TypePtr>>;


inline std::string stringify(const Value& value, const size_t indent) {
    std::string s;

    if (std::holds_alternative<bool>(value)) {
        const auto& v = std::get<bool>(value);
        // s = std::to_string(v); // reason I did this in the first place is to allow for copy-ellision to happen
        s = v ? "true" : "false";
    }

    else if (std::holds_alternative<ssize_t>(value)) {
        const auto& v = std::get<ssize_t>(value);

        s = std::to_string(v);
    }

    else if (std::holds_alternative<double>(value)) {
        const auto& v = std::get<double>(value);
        s = std::to_string(v);
    }

    else if (std::holds_alternative<std::string>(value)) {
        s = std::get<std::string>(value);
    }

    else if (std::holds_alternative<expr::Closure>(value)) {
        const auto& v = std::get<expr::Closure>(value);

        s = v.stringify(indent);
    }

    else if (std::holds_alternative<ClassValue>(value)) {
        const auto& v = std::get<ClassValue>(value);

        if (v.blueprint->members.empty())
            s = "class { }";
        else {
            s = "class {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, type, value] : v.blueprint->members) {
                s += space + name.stringify() + ": " + type->text(indent + 4) + " = ";

                const bool is_string = std::holds_alternative<std::string>(value);
                if (is_string) s += '\"';

                s += stringify(value, indent + 4);

                if (is_string) s += '\"';

                s += ";\n";
            }

            s += std::string(indent, ' ') + '}';
        }
    }

    else if (std::holds_alternative<expr::Union>(value)) {
        const auto& v = std::get<expr::Union>(value);

        s = v.stringify(indent);
    }

    else if (std::holds_alternative<NameSpace>(value)) {
        const auto& v = get<NameSpace>(value);

        if (v.members->members.empty())
            s = "space { }";
        else {
            s = "space {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, type, value] : v.members->members) {
                s += space + name.stringify() + ": " + type->text(indent + 4) + " = ";

                const bool is_string = std::holds_alternative<std::string>(value);
                if (is_string) s += '\"';

                s += stringify(value, indent + 4);

                if (is_string) s += '\"';

                s += ";\n";
            }

            s += std::string(indent, ' ') + '}';
        }
    }

    else if (std::holds_alternative<Object>(value)) {
        const auto& v = std::get<Object>(value);

        if (v.second->members.empty()) {
            s = "Object { }";
        }
        else {
            s = "Object {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, _, value] : v.second->members) {
                s += space + name.stringify() + " = ";

                const bool is_string = std::holds_alternative<std::string>(value);
                if (is_string) s += '\"';

                s += stringify(value, indent + 4);

                if (is_string) s += '\"';

                s += ";\n";
            }

            s += std::string(indent, ' ') + '}';
        }


    }

    else if(std::holds_alternative<expr::Node>(value)) {
        const std::string space(indent + 4, ' ');

        s = "Syntax {\n" + space + std::visit(
            [indent] (const auto& v) { return v->stringify(indent + 4); },
            get<expr::Node>(value)
        ) + '\n' + std::string(indent, ' ') + '}';
    }

    else if (std::holds_alternative<PackList>(value)) {
        std::string comma = "";
        for (const auto& v : get<PackList>(value)->values) {
            s += comma + stringify(v, indent + 4);
            comma = ", ";
        }
    }
    else if (std::holds_alternative<ListValue>(value)) {
        s += '{';

        std::string comma = "";
        for (const auto& v : get<ListValue>(value).elts->values) {
            s += comma + stringify(v, indent + 4);
            comma = ", ";
        }

        s += '}';
    }
    else if (std::holds_alternative<MapValue>(value)) {
        s += '{';

        std::string comma = "";
        for (const auto& [key, value] : get<MapValue>(value).items->map) {
            s += comma + stringify(key, indent + 4) + ": " + stringify(value, indent + 4);
            comma = ", ";
        }

        s += '}';
    }

    else error("Type not found!");

    return s;
}


inline std::ostream& operator<<(std::ostream& os, const Environment& env) {
    for (const auto& [name, expr] : env){
        const auto& [value, type] = expr;
        os << name << ": " << type->text() << " = " << stringify(value) << std::endl;
    }

    return os;
}


[[nodiscard]] inline bool operator==(const Value& lhs, const Value& rhs) noexcept {
    if (std::holds_alternative<ssize_t>(lhs) and std::holds_alternative<ssize_t>(rhs))
        return get<ssize_t>(lhs) == get<ssize_t>(rhs);

    if (std::holds_alternative<double>(lhs) and std::holds_alternative<double>(rhs))
        return get<double>(lhs) == get<double>(rhs);

    if (std::holds_alternative<bool>(lhs) and std::holds_alternative<bool>(rhs))
        return get<bool>(lhs) == get<bool>(rhs);

    if (std::holds_alternative<std::string>(lhs) and std::holds_alternative<std::string>(rhs))
        return get<std::string>(lhs) == get<std::string>(rhs);

    if (std::holds_alternative<expr::Closure>(lhs) and std::holds_alternative<expr::Closure>(rhs))
        return get<expr::Closure>(lhs).stringify() == get<expr::Closure>(rhs).stringify();

    if (std::holds_alternative<ClassValue>(lhs) and std::holds_alternative<ClassValue>(rhs)) {
        return std::ranges::all_of(
            std::views::zip(
                get<ClassValue>(lhs).blueprint->members,
                get<ClassValue>(rhs).blueprint->members
            ),

            [] (auto&& tuple) {
                return get<0>(get<0>(tuple)).stringify() == get<0>(get<1>(tuple)).stringify()
                   and get<1>(get<0>(tuple)) == get<1>(get<1>(tuple));
            }
        );
    }

    if (std::holds_alternative<Object>(lhs) and std::holds_alternative<Object>(rhs)) {
        const auto& a = get<Object>(lhs), b = get<Object>(rhs);
        return a.first == b.first and 
            std::ranges::all_of(
                std::views::zip(
                    a.second->members,
                    b.second->members
                ),

                [] (auto&& tuple) {
                    return get<0>(get<0>(tuple)).stringify() == get<0>(get<1>(tuple)).stringify()
                    and get<1>(get<0>(tuple)) == get<1>(get<1>(tuple));
                }
            );
    }

    if (std::holds_alternative<expr::Node>(lhs) and std::holds_alternative<expr::Node>(rhs))
        error("Can't check equality of a Syntax!");

    if (std::holds_alternative<PackList>(lhs) and std::holds_alternative<PackList>(rhs))
        return get<PackList>(lhs)->values == get<PackList>(rhs)->values;


    // error();
    return false;
}
