#include "Type.hxx"
#include "../Interp/Interpreter.hxx"

#include <ranges>



namespace type {

    std::string ExprType::text(const size_t indent) const {
        const auto& type = t->stringify(indent);
        return type.empty() ? "Any" : type;
    }

    bool ExprType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;

        const auto& type = text();
        return type == "Syntax" or (type == "Any" and other.text() != "Any");
    }


    bool ExprType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;

        const auto& type = text();
        return type == "Syntax" or type == "Any" or type == other.text();
    }


    std::string BuiltinType::text(const size_t) const { return t; }

    std::string LiteralType::text(const size_t indent) const {
        // return stringify(*cls, indent);
        std::string s;

        if (cls->blueprint->members.empty())
            s = "class { }";
        else {
            s = "class {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, type, value] : cls->blueprint->members) {
                s += space + name.stringify() + ": " + type->text(indent + 4) + " = ";

                const bool is_string = std::holds_alternative<std::string>(value);
                if (is_string) s += '\"';

                s += stringify(value, indent + 4);

                if (is_string) s += '\"';

                s += ";\n";
            }

            s += std::string(indent, ' ') + '}';
        }

        return s;
    }


    bool LiteralType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        if (not dynamic_cast<const LiteralType*>(&other)) return false;

        const auto& type = text();
        return type == "Syntax" or (type == "Any" and other.text() != "Any");
    }


    bool LiteralType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        if (not dynamic_cast<const LiteralType*>(&other)) return false;

        const auto& type = text();
        return type == "Syntax" or type == "Any" or type == other.text();
    }


    std::string UnionType::text(const size_t indent) const {
        std::string s = "union { ";

        for (std::string pipe{}; const auto& t : types) {

            s += t->text(indent + 4) + "; ";
        }


        return s + '}';
    }

    bool UnionType::operator> (const Type& other) const {
        // for (const auto& type : types)
        //     if (*type > other) return true;
        // return false;

        return std::ranges::any_of(types, [&other](const auto& type) { return *type > other; });
    };

    bool UnionType::operator>=(const Type& other) const {
        return std::ranges::any_of(types, [&other](const auto& type) { return *type >= other; });
    };

    std::string SpaceType::text(const size_t) const { return "Space"; }
    // a namespace is only not greater than any other type...
    bool SpaceType::operator>(const Type&) const { return false; }
    // ...so >= only needs to check for equality!
    bool SpaceType::operator>=(const Type& other) const { return *this == other; }



    std::string FuncType::text(const size_t indent) const {
        std::string t = params.empty() ? "" : params[0]->text(indent);
        for (size_t i = 1uz; i < params.size(); ++i)
            t += ", " + params[i]->text(indent);

        return '(' + t + "): " + ret->text(indent);
    }

    bool FuncType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        if (not dynamic_cast<const FuncType*>(&other)) return false;

        // this might throw, but it technically shouldn't
        const auto& that = dynamic_cast<const FuncType&>(other);

        for (const auto& [type1, type2] : std::views::zip(params, that.params)) {
            // if (*type1 >= *type2) return false; // args have an inverse relationship

            if (not (*type2 > *type1)) return false; // args have an inverse relationship
        }


        return *ret > *that.ret;
    }

    bool FuncType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        if (not dynamic_cast<const FuncType*>(&other)) return false;

        // this might throw, but it technically shouldn't
        const auto& that = dynamic_cast<const FuncType&>(other);

        for (const auto& [type1, type2] : std::views::zip(params, that.params)) {
            // std::clog << "type1: " << type1->text() << std::endl;
            // std::clog << "type2: " << type2->text() << std::endl;
            // std::clog << "not (" << type2->text() << " >= " << type1->text() << "): " << not (*type2 >= *type1) << std::endl;

            // if (*type1 > *type2) return false; // args have an inverse relationship

            if (not (*type2 >= *type1)) return false; // args have an inverse relationship
        }


        return *ret >= *that.ret;
    }



    std::string VariadicType::text(const size_t indent) const { return "..." + type->text(indent); }

    bool VariadicType::operator>(const Type& other) const {
        if (auto that = dynamic_cast<const VariadicType*>(&other)) return *type > *that->type;

        // variadic type of a type is a superset of that type
        return *type == other or *type > other;
    }

    bool VariadicType::operator>=(const Type& other) const {
        if (auto that = dynamic_cast<const VariadicType*>(&other)) return *type >= *that->type;

        return *type >= other;
    }



    std::string ListType::text(const size_t indent) const { return '{' + type->text(indent) + '}'; }

    bool ListType::operator>(const Type& other) const {
        if (auto that = dynamic_cast<const ListType*>(&other)) return *type > *that->type;

        return false;
    }

    bool ListType::operator>=(const Type& other) const {
        if (auto that = dynamic_cast<const ListType*>(&other)) return *type >= *that->type;

        return false;
    }



    std::string MapType::text(const size_t indent) const {
        return '{' + key_type->text(indent + 4) + ": " + val_type->text(indent + 4) + '}';
    }

    bool MapType::operator>(const Type& other) const {
        if (auto that = dynamic_cast<const MapType*>(&other))
            return *key_type > *that->key_type and *val_type > *that->val_type;

        return false;
    }

    bool MapType::operator>=(const Type& other) const {
        if (auto that = dynamic_cast<const MapType*>(&other))
            return *key_type >= *that->key_type and *val_type >= *that->val_type;

        return false;
    }

} // namespace type