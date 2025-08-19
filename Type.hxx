#pragma once

#include <string>
#include <vector>
#include <ranges>
#include <memory>


#ifdef  TYPE_STRING

using Type = std::string;

#else

namespace type {
    struct Type_t {
        virtual std::string text() const = 0;

        virtual bool operator==(const Type_t& other) const { return text() == other.text(); }
        virtual bool operator> (const Type_t& other) const = 0;
        virtual bool operator>=(const Type_t& other) const { return *this == other or *this > other; }

        virtual ~Type_t() = default;
    };

    using TypePtr = std::shared_ptr<Type_t>;

    struct VarType final : Type_t {
        std::string t;

        VarType(std::string s) noexcept : t{std::move(s)} {}

        std::string text() const override { return t.empty() ? "Any" : t; }


        bool operator>(const Type_t& other) const override {
            const auto& type = text();
            return type == "Any" and other.text() != "Any";
        }


        bool operator>=(const Type_t& other) const override {
            const auto& type = text();
            return type == "Any" or type == other.text();
        }
    };

    struct FuncType final : Type_t {
        std::vector<TypePtr> params;
        TypePtr ret;


        FuncType() = default;
        explicit FuncType(std::vector<TypePtr> ps, TypePtr r) noexcept : params{std::move(ps)}, ret{std::move(r)} {}

        std::string text() const override {
            std::string t = params.empty() ? "Any" : params[0]->text();

            for (size_t i = 1uz; i < params.size(); ++i)
                t += ", " + params[i]->text();

            return '(' + t + "): " + ret->text();
        }


        bool operator>(const Type_t& other) const override {
            if (dynamic_cast<const VarType*>(&other)) return false;

            // this might throw, but it technically shouldn't
            const auto& that = dynamic_cast<const FuncType&>(other);

            for (const auto& [type1, type2] : std::ranges::zip_view(params, that.params))
                if (*type1 >= *type2) return false; // args have an inverse relation ship


            return *ret > *that.ret;
        }

        bool operator>=(const Type_t& other) const override {
            if (dynamic_cast<const VarType*>(&other)) return false;

            // this might throw, but it technically shouldn't
            const auto& that = dynamic_cast<const FuncType&>(other);

            for (const auto& [type1, type2] : std::ranges::zip_view(params, that.params))
                if (*type1 > *type2) return false; // args have an inverse relation ship


            return *ret >= *that.ret;
        }

    };



    namespace builtins{
        TypePtr Int()    { return std::make_shared<VarType>("Int"   ); }
        TypePtr Double() { return std::make_shared<VarType>("Double"); };
        TypePtr Bool()   { return std::make_shared<VarType>("Bool"  ); };
        TypePtr String() { return std::make_shared<VarType>("String"); };
        TypePtr Any()    { return std::make_shared<VarType>("Any"   ); };
        TypePtr Type()   { return std::make_shared<VarType>("Type"  ); };
    }
}

#endif
