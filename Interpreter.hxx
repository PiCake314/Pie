#pragma once


#include "utils.hxx"
#include "Expr.hxx"
#include "Parser.hxx"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <ranges>
#include <optional>
#include <stdexcept>

#include <cassert>



struct Visitor {
    // struct Builtin { std::string name; }; // for late

    using Value = std::variant<int, std::string, Closure>;

    std::vector<std::unordered_map<std::string, Value>> env;
    const Operators ops;

    struct ScopeGuard {
        Visitor* v;
        ScopeGuard(Visitor* t) noexcept : v{t} { v->scope(); }
        ~ScopeGuard() { v->unscope(); }
    };


    Visitor(Operators ops) noexcept : env(1), ops{std::move(ops)} {};

    void scope() { env.emplace_back(); }
    void unscope() { env.pop_back(); }

    const Value& addVar(const std::string& name, const Value& v) { env.back()[name] = v; return v; }

    std::optional<Value> getVar(const std::string& name) {
        for (const auto& curr : env)
            if (curr.contains(name)) return curr.at(name);

        return {};
    }


    Value operator()(const Num *n) { return std::stoi(n->num); }

    Value operator()(const String *s) { return s->str; }

    Value operator()(const Name *n) {
        // what should builtins evaluate to?
        // If I return the string back, then expressions like `"__builtin_neg"(1)` are valid now :)))))
        // interesting!
        // how about a special value?
        if (isBuiltIn(n->name)) return n->name;

        if (const auto opt = getVar(n->name); opt) return *opt;

        error("Name \"" + n->name + "\" is not defined");
    }

    Value operator()(const Assignment *ass) {
        return addVar(ass->name, std::visit(*this, ass->expr->variant()));

        // const auto& value = std::visit(*this, ass->expr->variant());
        // addVar(ass->name, value);
        // return value;

        // return env.back()[ass->name] = std::visit(*this, ass->expr->variant());
    }

    Value operator()(const UnaryOp *up) {
        ScopeGuard sg{this}; // RAII is so cool
        // scope();

        const Fix* op = ops.at(up->text);
        Closure* func = dynamic_cast<Closure*>(op->func.get());

        if (not func) error("This shouldn't happen, but also Unary Operator not equal to a closure!");

        assert(func->params.size() == 1);
        // env.back()[func->params.front()] = std::visit(*this, up->expr->variant());
        addVar(func->params.front(), std::visit(*this, up->expr->variant()));

        return std::visit(*this, func->body.get()->variant()); // RAII takes care of unscoping

        // unscope();
    }

    Value operator()(const BinOp *bp) {
        ScopeGuard sg{this};

        const Fix* op = ops.at(bp->text);
        Closure* func = dynamic_cast<Closure*>(op->func.get());

        if (not func) error("This shouldn't happen, but also Unary Operator not equal to a closure!");

        assert(func->params.size() == 2);
        addVar(func->params[0], std::visit(*this, bp->lhs->variant()));
        addVar(func->params[1], std::visit(*this, bp->rhs->variant()));
        // env.back()[func->params[0]] = std::visit(*this, bp->lhs->variant());
        // env.back()[func->params[1]] = std::visit(*this, bp->rhs->variant());

        return std::visit(*this, func->body.get()->variant());
    }

    Value operator()(const PostOp *pp) {
        ScopeGuard sg{this};

        const Fix* op = ops.at(pp->text);
        Closure* func = dynamic_cast<Closure*>(op->func.get());

        if (not func) error("This shouldn't happen, but also Unary Operator not equal to a closure!");

        assert(func->params.size() == 1);
        addVar(func->params.front(), std::visit(*this, pp->expr->variant()));
        // env.back()[func->params.front()] = std::visit(*this, pp->expr->variant());

        return std::visit(*this, func->body.get()->variant());
    }

    Value operator()(const Call* call) {
        ScopeGuard sg{this};

        auto var = std::visit(*this, call->func->variant());

        if (std::holds_alternative<std::string>(var)) { // that dumb lol. but now it works
            const auto& name = std::get<std::string>(var);
            if (isBuiltIn(name)) return evaluateBuiltin(call, name);

            // otherwise, access the function from the env
            const auto& opt = getVar(name);
            if (not opt) error("Function not defined!");

            var = *opt;
        }


        if (std::holds_alternative<Closure>(var)) {
            const auto& func = std::get<Closure>(var);

            for (const auto& [param, arg] : std::views::zip(func.params, call->args))
                addVar(param, std::visit(*this, arg->variant()));
                // env.back()[param] = std::visit(*this, arg->variant());

            return std::visit(*this, func.body->variant());
        }

        error("operator() applied on not-a-function");
    }

    Value operator()(const Closure *c) { return *c; }

    Value operator()(const Fix *fix) { return std::visit(*this, fix->func->variant()); }

    // Value operator()(const Prefix *fix) { }

    // Value operator()(const Infix *fix) { }

    // Value operator()(const Suffix *fix) { }


    [[nodiscard]] bool isBuiltIn(const std::string_view func) const noexcept {
        const auto make_builtin = [] (const std::string& n) { return "__builtin_" + n; };

        for(const auto& builtin : {"neg", "not", "add", "sub", "mul", "div", "print"})
            if (func == make_builtin(builtin)) return true;

        return false;
    }


    // the gate into the META operators!
    Value evaluateBuiltin(const Call* call, std::string name) {


        name = name.substr(10); // cutout the "__builtin_"

        const auto arity_check = [&name, call] (const size_t arity) {
            if (call->args.size() != arity) error("Wrong arity with call to \"__builtin_" + name + "\"");
        };

        const auto int_check = [&name] (const Value val) {
            if (not std::holds_alternative<int>(val)) error("Wrong argument passed to to \"__builtin_" + name + "\"");
        };


        const auto& value1 = std::visit(*this, call->args.front()->variant());

        if (name == "print") {
            arity_check(1);

            if (std::holds_alternative<int>(value1)){
                const auto& v = std::get<int>(value1);
                std::cout << v << '\n';
                return value1; // I have to return a value. Maybe return what I printed?
            }

            if (std::holds_alternative<std::string>(value1)){
                const auto& v = std::get<std::string>(value1);
                std::cout << v << '\n';
                return value1; // I have to return a value. Maybe return what I printed?
            }

            if (std::holds_alternative<Closure>(value1)){
                const auto& v = std::get<Closure>(value1);
                v.print();
                puts(""); // .print() doesn't add a \n so we add it manually
                return value1; // I have to return a value. Maybe return what I printed?
            }

        }

        int_check(value1);
        const int num1 = std::get<int>(value1);

        if (name == "neg") {
            arity_check(1);
            return -num1;
        }

        if (name == "not") {
            arity_check(1);
            return not num1;
        }


        arity_check(2); // all the rest of those funcs expect 2 arguments


        const auto& value2 = std::visit(*this, call->args[1]->variant());
        int_check(value2);
        const int num2 = std::get<int>(value2);


        if (name == "add") return num1 + num2;
        if (name == "sub") return num1 - num2;
        if (name == "mul") return num1 * num2;
        if (name == "div") return num1 / num2;

        error("something went wrong..;-; sorry!");
    }


};

