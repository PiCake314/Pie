#pragma once

#include <string>
#include <vector>
#include <ranges>
#include <memory>

#include "../Declarations.hxx"



struct ClassValue;

namespace type {
    struct Type_t {
        virtual std::string text(const size_t indent = 0) const = 0;

        virtual bool operator==(const Type_t& other) const { return text() == other.text(); }
        virtual bool operator> (const Type_t& other) const = 0;
        virtual bool operator>=(const Type_t& other) const = 0;

        virtual ~Type_t() = default;
    };

    using TypePtr = std::shared_ptr<Type_t>;

    struct VarType : Type_t {
        expr::ExprPtr t;
        // std::string t;

        VarType(expr::ExprPtr s) noexcept : t{std::move(s)} {}

        std::string text(const size_t indent = 0) const override {
            // return t.empty() ? "Any" : t;
            const auto& type = t->stringify(indent);
            return type.empty() ? "Any" : type;
        }


        bool operator>(const Type_t& other) const override {
            // if (not dynamic_cast<const VarType*>(&other)) return false;

            const auto& type = text();
            return type == "Syntax" or (type == "Any" and other.text() != "Any");
        }


        bool operator>=(const Type_t& other) const override {
            // if (not dynamic_cast<const VarType*>(&other)) return false;

            const auto& type = text();
            return type == "Syntax" or type == "Any" or type == other.text();
        }
    };

    struct LiteralType : Type_t {
        std::shared_ptr<ClassValue> cls;
        // std::string t;

        LiteralType(std::shared_ptr<ClassValue> s) noexcept : cls{std::move(s)} {}

        std::string text(const size_t indent = 0) const override;


        bool operator>(const Type_t& other) const override {
            if (not dynamic_cast<const LiteralType*>(&other)) return false;

            const auto& type = text();
            return type == "Syntax" or (type == "Any" and other.text() != "Any");
        }


        bool operator>=(const Type_t& other) const override {
            if (not dynamic_cast<const LiteralType*>(&other)) return false;

            const auto& type = text();
            return type == "Syntax" or type == "Any" or type == other.text();
        }
    };

    struct FuncType final : Type_t {
        std::vector<TypePtr> params;
        TypePtr ret;


        FuncType() = default;
        explicit FuncType(std::vector<TypePtr> ps, TypePtr r) noexcept : params{std::move(ps)}, ret{std::move(r)} {}

        std::string text(const size_t = 0) const override {

            std::string t = params.empty() ? "" : params[0]->text();
            for (size_t i = 1uz; i < params.size(); ++i)
                t += ", " + params[i]->text();

            return '(' + t + "): " + ret->text();
        }


        bool operator>(const Type_t& other) const override {
            if (not dynamic_cast<const FuncType*>(&other)) return false;

            // this might throw, but it technically shouldn't
            const auto& that = dynamic_cast<const FuncType&>(other);

            for (const auto& [type1, type2] : std::ranges::zip_view(params, that.params))
                if (*type1 >= *type2) return false; // args have an inverse relation ship


            return *ret > *that.ret;
        }

        bool operator>=(const Type_t& other) const override {
            if (not dynamic_cast<const FuncType*>(&other)) return false;

            // this might throw, but it technically shouldn't
            const auto& that = dynamic_cast<const FuncType&>(other);

            for (const auto& [type1, type2] : std::ranges::zip_view(params, that.params))
                if (*type1 > *type2) return false; // args have an inverse relation ship


            return *ret >= *that.ret;
        }
    };


    struct BuiltinType final : VarType {
        std::string t;

        BuiltinType(std::string s) noexcept : VarType{nullptr}, t{std::move(s)} {}
        std::string text(const size_t = 0) const override { return t; }
    };


    //* these builtins could be their own types!
    namespace builtins {
        inline TypePtr Int    () { return std::make_shared<BuiltinType>("Int"   ); }
        inline TypePtr Double () { return std::make_shared<BuiltinType>("Double"); };
        inline TypePtr Bool   () { return std::make_shared<BuiltinType>("Bool"  ); };
        inline TypePtr String () { return std::make_shared<BuiltinType>("String"); };
        inline TypePtr Any    () { return std::make_shared<BuiltinType>("Any"   ); };
        inline TypePtr Syntax () { return std::make_shared<BuiltinType>("Syntax"); };
        inline TypePtr Type   () { return std::make_shared<BuiltinType>("Type"  ); };

        inline TypePtr _      () { return std::make_shared<BuiltinType>("_"  ); };
    }

    inline bool isFunction(const TypePtr& t) noexcept {
        return dynamic_cast<const FuncType*>(t.get());
    }

    inline bool isBuiltin(const TypePtr& t) noexcept {
        return dynamic_cast<const BuiltinType*>(t.get());

        // const std::string& text = t->text();

        // return text == "int"
        //     or text == "Double"
        //     or text == "Bool"
        //     or text == "String"
        //     or text == "Any"
        //     or text == "Syntax"
        //     or text == "Type";
    }
}

