#include "Type.hxx"
#include "../Interp/Interpreter.hxx"

#include <ranges>



namespace type {

    std::string ExprType::text(const size_t indent) const {
        // return t.empty() ? "Any" : t;
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
        // const auto& type = stringify(*cls, indent);
        // return type.empty() ? "Any" : type;

        return stringify(*cls, indent);
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


    std::string SpaceType::text(const size_t) const { return "Space"; }
    // a namespace is only not greater than any other type...
    bool SpaceType::operator>(const Type&) const { return false; }
    // ...so >= only needs to check for equality!
    bool SpaceType::operator>=(const Type& other) const { return *this == other; }



    std::string FuncType::text(const size_t) const {
        std::string t = params.empty() ? "" : params[0]->text();
        for (size_t i = 1uz; i < params.size(); ++i)
            t += ", " + params[i]->text();

        return '(' + t + "): " + ret->text();
    }

    bool FuncType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        if (not dynamic_cast<const FuncType*>(&other)) return false;

        // this might throw, but it technically shouldn't
        const auto& that = dynamic_cast<const FuncType&>(other);

        for (const auto& [type1, type2] : std::views::zip(params, that.params))
            if (*type1 >= *type2) return false; // args have an inverse relation ship


        return *ret > *that.ret;
    }

    bool FuncType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        if (not dynamic_cast<const FuncType*>(&other)) return false;

        // this might throw, but it technically shouldn't
        const auto& that = dynamic_cast<const FuncType&>(other);

        for (const auto& [type1, type2] : std::views::zip(params, that.params))
            if (*type1 > *type2) return false; // args have an inverse relation ship


        return *ret >= *that.ret;
    }



    std::string VariadicType::text(const size_t indent) const { return "..." + type->text(indent); }

    bool VariadicType::operator>(const Type& other) const {
        if (auto that = dynamic_cast<const VariadicType*>(&other)) return *type >= *that->type;

        // variadic type of a type is a superset of that type
        return *type == other or *type > other;
    }

    bool VariadicType::operator>=(const Type& other) const {
        if (auto that = dynamic_cast<const VariadicType*>(&other)) return *type >= *that->type;

        return *type >= other;
    }

} // namespace type