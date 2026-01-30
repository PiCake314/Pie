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

inline namespace value {
    struct ClassValue;
    struct Value;
    using ValuePtr = std::shared_ptr<Value>;
}

namespace interp {
    struct Visitor;
}

namespace type {

    using TypePtr = std::shared_ptr<struct Type>;

    struct Type {
        virtual std::string text(const size_t indent = 0) const = 0;

        virtual bool involvesT(const Type&) const = 0;
        virtual bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const = 0;

        virtual bool operator==(const Type& other) const { return text() == other.text(); }
        virtual bool operator> (const Type&) const = 0;
        virtual bool operator>=(const Type&) const = 0;

        virtual ~Type() = default;
    };



    struct ExprType : Type {
        expr::ExprPtr t;

        explicit ExprType(expr::ExprPtr s) noexcept : t{std::move(s)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override;
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct BuiltinType : Type {
        std::string t;

        explicit BuiltinType(std::string s) noexcept : t{std::move(s)} {}

        std::string text(const size_t = 0) const override { return t; };
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct ValueType : Type {
        std::shared_ptr<pie::value::Value> val;

        explicit ValueType(std::shared_ptr<pie::value::Value> v) noexcept : val{std::move(v)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override;

        // // no value is greater than another value
        bool operator>(const Type&) const override { return false; }
        // // since no value is greater than another, >= checks for equality only
        bool operator>=(const Type& other) const override {
            return *this == other; // I think that's fine
        }
    };


    struct ConceptType : Type {
        value::ValuePtr func; // should this be std::shared_ptr<expr::Closure>?? or 

        explicit ConceptType(value::ValuePtr v) noexcept : func{std::move(v)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override;

        // // no value is greater than another value
        bool operator>(const Type&) const override { return false; }
        // // since no value is greater than another, >= checks for equality only
        bool operator>=(const Type& other) const override { return *this == other; }
    };


    struct LiteralType : Type {
        std::shared_ptr<pie::value::ClassValue> cls;
        // std::string t;

        explicit LiteralType(std::shared_ptr<pie::value::ClassValue> c) noexcept : cls{std::move(c)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override {
            return *this >= *other; // not checking value...for now at least to see how things go
        }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct UnionType : Type {
        std::vector<TypePtr> types;

        explicit UnionType(std::vector<TypePtr> ts) noexcept : types{std::move(ts)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override;
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override;

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct SpaceType : Type {

        std::string text(const size_t = 0) const override { return "Space"; }
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, [[maybe_unused]] const value::Value& v, const TypePtr& other) const override { return *this >= *other; }

        // a namespace is only not greater than any other type...
        bool operator>(const Type&) const override { return false; }
        // ...so >= only needs to check for equality!
        bool operator>=(const Type& other) const override { return *this == other; }
    };


    struct FuncType final : Type {
        std::vector<TypePtr> params;
        TypePtr ret;

        FuncType(std::vector<TypePtr> ps, TypePtr r) noexcept : params{std::move(ps)}, ret{std::move(r)} {}

        std::string text(const size_t = 0) const override;
        bool involvesT(const Type& T) const override;
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };

    struct VariadicType final : Type {
        TypePtr type;

        explicit VariadicType(TypePtr t) : type{std::move(t)} {}

        std::string text(const size_t = 0) const override;
        bool involvesT(const Type& T) const override { return type->involvesT(T); }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };

    struct ListType final : Type {
        TypePtr type;

        explicit ListType(TypePtr t) : type{std::move(t)} {}

        std::string text(const size_t = 0) const override;
        bool involvesT(const Type& T) const override { return type->involvesT(T); }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override {
            // if (std::holds_alternative<value::ListValue>(v)) {
            //     const auto& l = get<value::ListValue>(v);
            //     if (l.elts->values.size() == 1 and )
            // }

            return *this >= *other;
        }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };

    struct MapType final : Type {
        TypePtr key_type;
        TypePtr val_type;

        MapType(TypePtr t1, TypePtr t2) : key_type{std::move(t1)}, val_type{std::move(t2)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return key_type->involvesT(T) or val_type->involvesT(T); }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;
    };


    struct TryReassign : Type {
        std::string text(const size_t = 0) const override { return ""; }
        bool involvesT(const Type&) const override { return false; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override { return true; };

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



    inline const ExprType* isExpr(const TypePtr& t) noexcept {
        return dynamic_cast<const ExprType*>(t.get());
    }

    inline const FuncType* isFunction(const TypePtr& t) noexcept {
        return dynamic_cast<const FuncType*>(t.get());
    }

    inline const LiteralType* isClass(const TypePtr& t) noexcept {
        return dynamic_cast<const LiteralType*>(t.get());
    }

    inline const ValueType* isValue(const TypePtr& t) noexcept {
        return dynamic_cast<const ValueType*>(t.get());
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

    inline bool isAny(const TypePtr& t) noexcept {
        return isBuiltin(t) && t->text() == "Any";
    }

    inline bool isSyntax(const TypePtr& t) noexcept {
        return isBuiltin(t) && t->text() == "Syntax";
    }

    inline bool isType(const TypePtr& t) noexcept {
        return isBuiltin(t) && t->text() == "Type";
    }

    inline bool shouldReassign(const TypePtr& t) {
        return dynamic_cast<const TryReassign*>(t.get());
    }
}


} // namespace pie