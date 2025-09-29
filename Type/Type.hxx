#pragma once

#include <string>
#include <vector>
#include <ranges>
#include <algorithm>
#include <memory>

// #include "../Declarations.hxx"


namespace expr {
    struct Expr;
    using ExprPtr = std::shared_ptr<expr::Expr>;
}

struct ClassValue;
// using NameSpace = ClassValue;

namespace type {
    struct Type {
        virtual std::string text(const size_t indent = 0) const = 0;

        virtual bool operator==(const Type& other) const { return text() == other.text(); }
        virtual bool operator> (const Type& other) const = 0;
        virtual bool operator>=(const Type& other) const = 0;

        virtual ~Type() = default;
    };

    using TypePtr = std::shared_ptr<Type>;


    struct ExprType : Type {
        expr::ExprPtr t;

        ExprType(expr::ExprPtr s) noexcept : t{std::move(s)} {}

        std::string text(const size_t indent = 0) const override;

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct BuiltinType final : ExprType {
        std::string t;

        BuiltinType(std::string s) noexcept : ExprType{nullptr}, t{std::move(s)} {}
        std::string text(const size_t = 0) const;
    };


    struct LiteralType : Type {
        std::shared_ptr<ClassValue> cls;
        // std::string t;

        LiteralType(std::shared_ptr<ClassValue> c) noexcept : cls{std::move(c)} {}

        std::string text(const size_t indent = 0) const override;

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct SpaceType : Type {

        // SpaceType() = default;

        std::string text(const size_t indent = 0) const override;

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct FuncType final : Type {
        std::vector<TypePtr> params;
        TypePtr ret;


        FuncType(std::vector<TypePtr> ps, TypePtr r) noexcept : params{std::move(ps)}, ret{std::move(r)} {}

        std::string text(const size_t = 0) const override;

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };

    struct VariadicType final : Type {
        TypePtr type;


        VariadicType(TypePtr t) : type{std::move(t)} {}

        std::string text(const size_t indent = 0) const override;

        bool operator>(const Type& other) const override;

        bool operator>=(const Type& other) const override;
    };


    struct TryReassign : Type {
        std::string text(const size_t = 0) const override { return ""; }

        bool operator> (const Type&) const override { return false; };
        bool operator>=(const Type&) const override { return false; };
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
        inline TypePtr _      () { return std::make_shared<TryReassign>(); };
    }

    inline bool isFunction(const TypePtr& t) noexcept {
        return dynamic_cast<const FuncType*>(t.get());
    }

    inline bool isVariadic(const TypePtr& t) noexcept {
        return dynamic_cast<const VariadicType*>(t.get());
    }

    inline bool isBuiltin(const TypePtr& t) noexcept {
        return dynamic_cast<const BuiltinType*>(t.get());
    }

    inline bool shouldReassign(const TypePtr& t) {
        return dynamic_cast<const TryReassign*>(t.get());
    }
}

