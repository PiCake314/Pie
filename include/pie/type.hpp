
#pragma once

#include <string>
#include <vector>
#include <ranges>
#include <algorithm>
#include <memory>

inline namespace pie {

namespace expr {
    struct Expr;
    using ExprPtr = std::shared_ptr<expr::Expr>;
}

inline namespace value { struct ClassValue; }

namespace type {
    struct Type {
        virtual std::string text(const size_t indent = 0) const = 0;

        virtual bool involvesT(const Type& T) const = 0;

        virtual bool operator==(const Type& other) const { return text() == other.text(); }
        virtual bool operator> (const Type& other) const = 0;
        virtual bool operator>=(const Type& other) const = 0;

        virtual ~Type() = default;
    };

    using TypePtr = std::shared_ptr<Type>;


    struct ExprType : Type {
        expr::ExprPtr t;

        explicit ExprType(expr::ExprPtr s) noexcept : t{std::move(s)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct BuiltinType final : ExprType {
        std::string t;

        explicit BuiltinType(std::string s) noexcept : ExprType{nullptr}, t{std::move(s)} {}
        std::string text(const size_t = 0) const;
    };


    struct LiteralType : Type {
        std::shared_ptr<ClassValue> cls;
        // std::string t;

        explicit LiteralType(std::shared_ptr<ClassValue> c) noexcept : cls{std::move(c)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }


        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct UnionType : Type {
        std::vector<TypePtr> types;

        explicit UnionType(std::vector<TypePtr> ts) noexcept : types{std::move(ts)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override;

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct SpaceType : Type {

        // SpaceType() = default;

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct FuncType final : Type {
        std::vector<TypePtr> params;
        TypePtr ret;


        FuncType(std::vector<TypePtr> ps, TypePtr r) noexcept : params{std::move(ps)}, ret{std::move(r)} {}

        std::string text(const size_t = 0) const override;
        bool involvesT(const Type& T) const override;

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };

    struct VariadicType final : Type {
        TypePtr type;

        explicit VariadicType(TypePtr t) : type{std::move(t)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return type->involvesT(T); }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };

    struct ListType final : Type {
        TypePtr type;

        explicit ListType(TypePtr t) : type{std::move(t)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return type->involvesT(T); }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };

    struct MapType final : Type {
        TypePtr key_type;
        TypePtr val_type;

        MapType(TypePtr t1, TypePtr t2) : key_type{std::move(t1)}, val_type{std::move(t2)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return key_type->involvesT(T) or val_type->involvesT(T); }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct TryReassign : Type {
        std::string text(const size_t = 0) const override { return ""; }
        bool involvesT(const Type&) const override { return false; }

        bool operator> (const Type&) const override { return false; };
        bool operator>=(const Type&) const override { return false; };
    };


    //* these builtins could be their own types!
    namespace builtins {
        inline TypePtr Int    () { return std::make_shared<BuiltinType>("Int"   ); }
        inline TypePtr Double () { return std::make_shared<BuiltinType>("Double"); }
        inline TypePtr Bool   () { return std::make_shared<BuiltinType>("Bool"  ); }
        inline TypePtr String () { return std::make_shared<BuiltinType>("String"); }
        inline TypePtr Any    () { return std::make_shared<BuiltinType>("Any"   ); }
        inline TypePtr Syntax () { return std::make_shared<BuiltinType>("Syntax"); }
        inline TypePtr Type   () { return std::make_shared<BuiltinType>("Type"  ); }

        inline TypePtr _      () { return std::make_shared<TryReassign>(); };
    }


    inline TypePtr VariadicOf(TypePtr type) {
        return std::make_shared<VariadicType>(std::move(type));
    }

    inline TypePtr ListOf(TypePtr type) {
        return std::make_shared<ListType>(std::move(type));
    }

    inline TypePtr MapOf(TypePtr type1, TypePtr type2) {
        return std::make_shared<MapType>(std::move(type1), std::move(type2));
    }





    inline const FuncType* isFunction(const TypePtr& t) noexcept {
        return dynamic_cast<const FuncType*>(t.get());
    }

    inline const LiteralType* isClass(const TypePtr& t) noexcept {
        return dynamic_cast<const LiteralType*>(t.get());
    }

    inline const UnionType* isUnion(const TypePtr& t) noexcept {
        return dynamic_cast<const UnionType*>(t.get());
    }

    inline const VariadicType* isVariadic(const TypePtr& t) noexcept {
        return dynamic_cast<const VariadicType*>(t.get());
    }

    inline const ListType* isList(const TypePtr& t) noexcept {
        return dynamic_cast<const ListType*>(t.get());
    }

    inline const MapType* isMap(const TypePtr& t) noexcept {
        return dynamic_cast<const MapType*>(t.get());
    }

    inline const BuiltinType* isBuiltin(const TypePtr& t) noexcept {
        return dynamic_cast<const BuiltinType*>(t.get());
    }

    inline bool shouldReassign(const TypePtr& t) {
        return dynamic_cast<const TryReassign*>(t.get());
    }
}


} // namespace pie