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


#include <stdx/tuple.hpp>



using Value = std::variant<int, double, bool, std::string, Closure>;
using Environment = std::unordered_map<std::string, Value>;

struct Any;

struct Visitor {
    // struct Builtin { std::string name; }; // for later

    std::vector<Environment> env;
    const Operators ops;


    // inline static const std::unordered_map<const char*, 


    Visitor(Operators ops) noexcept : env(1), ops{std::move(ops)} { }




    Value operator()(const Num *n) {
        if (const auto& var = getVar(n->num); var) return *var;

        if (n->num.find('.') == std::string::npos) return std::stoi(n->num);

        return std::stod(n->num);
    }

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


        if (const auto name = dynamic_cast<const Name*>(ass->lhs.get()))
            return addVar(name->name, std::visit(*this, ass->rhs->variant()));

        if (const auto num = dynamic_cast<const Num*>(ass->lhs.get())){
            // if (*num == )
            return addVar(num->num, std::visit(*this, ass->rhs->variant()));
        }

        if (const auto closure = dynamic_cast<const Closure*>(ass->lhs.get()))
            return addVar("Closure", std::visit(*this, ass->rhs->variant()));


        error();
        // return addVar(ass->name, std::visit(*this, ass->expr->variant()));

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

            return var;
        }


        if (std::holds_alternative<Closure>(var)) {
            const auto& func = std::get<Closure>(var);

            // assert(call->args.size() == func.params.size());
            if (call->args.size() > func.params.size()) error("Wrong arity call!");

            if (const auto args_size = call->args.size(); args_size < func.params.size()) {
                Closure closure{std::vector<std::string>{func.params.begin() + args_size, func.params.end()}, func.body};

                Environment argument_capture_list = func.env;
                for(const auto& [name, value] : std::views::zip(func.params, call->args))
                    argument_capture_list[name] = std::visit(*this, value->variant());

                closure.capture(argument_capture_list);

                return closure;
            }


            for (const auto& [param, arg] : std::views::zip(func.params, call->args))
                addVar(param, std::visit(*this, arg->variant()));
                // env.back()[param] = std::visit(*this, arg->variant());

            ScopeGuard sg{this, func.env};

            return std::visit(*this, func.body->variant());
        }

        // error("operator() applied on not-a-function");
        return var;
    }

    Value operator()(const Closure *c) {
        // take a snapshot of the current env (capture by value). comment this line if you want capture by reference..
        c->capture(envStackToEnvMap());
        return *c;
    }


    Value operator()(const Block *block) {
        ScopeGuard sg{this};

        Value ret;
        for (const auto& line : block->lines) 
            ret = std::visit(*this, line->variant()); // a scope's value is the last expression

        return ret;
    }

    Value operator()(const Fix *fix) { return std::visit(*this, fix->func->variant()); }

    // Value operator()(const Prefix *fix) { }

    // Value operator()(const Infix *fix) { }

    // Value operator()(const Suffix *fix) { }


    [[nodiscard]] bool isBuiltIn(const std::string_view func) const noexcept {
        const auto make_builtin = [] (const std::string& n) { return "__builtin_" + n; };

        for(const auto& builtin : {
            "true", "false",                                                            //* nullary
            "print", "reset",                                                           //* unary
            "neg", "not", "add", "sub", "mul", "div", "gt", "geq", "eq", "leq", "lt", "and", "or",   //* binary
            "conditional"                                                               //* trinary
        })
            if (func == make_builtin(builtin)) return true;

        return false;
    }


    // the gate into the META operators!
    Value evaluateBuiltin(const Call* call, std::string name) {

        //* ============================ FUNCTIONS ============================
        const auto functions = stdx::make_indexed_tuple<KeyFor>(
            //* NULLARY FUNCTIONS
            MapEntry<
                S<"true">,
                Func<
                    decltype([](const auto&) { return true; }),
                    void
                >
            >{},
            MapEntry<
                S<"false">,
                Func<
                    decltype([](const auto&) { return false; }),
                    void
                >
            >{},

            //* UNARY FUNCTIONS
            MapEntry<
                S<"neg">,
                Func<
                    decltype([](auto&& x, const auto&) { return -x; }),
                    TypeList<int>,
                    TypeList<double>
                >
            >{},

            MapEntry<
                S<"not">,
                Func<
                    decltype([](auto&& x, const auto&) { return not x; }),
                    TypeList<int>,
                    TypeList<double>,
                    TypeList<bool>
                >
            >{},

            MapEntry<
                S<"print">,
                Func<
                    decltype([](auto&& x, const auto& that) {
                        std::visit(
                            [&that](const auto& v) { that->print(v); },
                            x
                        );
                        return x;
                    }),
                    TypeList<Any>
                    // TypeList<int>,
                    // TypeList<double>,
                    // TypeList<std::string>,
                    // TypeList<bool>
                >
            >{},

            //* BINARY FUNCTIONS
            MapEntry<
                S<"add">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a + b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"sub">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a - b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"mul">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a * b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"div">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a / b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"gt">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a > b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"geq">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a >= b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"eq">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) {
                        if (std::holds_alternative<std::string>(a) and std::holds_alternative<std::string>(b)) return get<std::string>(a) == get<std::string>(b); // ADL

                        // will be caught down
                        // if (std::holds_alternative<std::string>(a)  or std::holds_alternative<std::string>(b)) return false;

                        if (std::holds_alternative<int>(a) and std::holds_alternative<int>(b)) return get<int>(a) == get<int>(b);
                        if (std::holds_alternative<int>(a) and std::holds_alternative<double>(b)) return get<int>(a) == get<int>(b);
                        if (std::holds_alternative<double>(a) and std::holds_alternative<int>(b)) return get<int>(a) == get<int>(b);
                        if (std::holds_alternative<double>(a) and std::holds_alternative<double>(b)) return get<int>(a) == get<int>(b);

                        return false;
                    }),
                    TypeList<Any, Any>
                    // TypeList<int, int>,
                    // TypeList<int, double>,
                    // TypeList<double, int>,
                    // TypeList<double, double>,
                    // TypeList<std::string, std::string>
                >
            >{},

            MapEntry<
                S<"leq">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a >= b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"lt">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a < b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            // * BOOLEAN FUNCTIONS
            MapEntry<
                S<"or">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a or b; }),
                    TypeList<bool, bool>
                >
            >{},

            MapEntry<
                S<"and">,
                Func<
                    decltype([](auto&& a, auto&& b, const auto&) { return a and b; }),
                    TypeList<bool, bool>
                >
            >{},

            //* TRUNARY FUNCTIONS

            MapEntry<
                S<"conditional">,
                Func<
                    decltype([](auto&& a, auto&& b, auto&& c, const auto&) -> Value {
                        return std::visit(
                            [] (const Value& aa, const Value& bb, const Value& cc) -> Value {
                                if (std::holds_alternative<bool>(aa)) {
                                    if (std::get<bool>(aa)) return bb;
                                    else return cc;
                                }

                                 // if it's not a bool, always return the else branch
                                return cc;
                            },
                            a, b, c
                        );
                    }),
                    TypeList<Any, Any, Any>
                >
            >{}
        );



        name = name.substr(10); // cutout the "__builtin_"

        const auto arity_check = [&name, call] (const size_t arity) {
            if (call->args.size() != arity) error("Wrong arity with call to \"__builtin_" + name + "\"");
        };

        // const auto int_check = [&name] (const Value& val) {
        //     if (not std::holds_alternative<int>(val)) error("Wrong argument passed to to \"__builtin_" + name + "\"");
        // };


        // Since this is a meta function that operates on AST nodes rather than values
        // it gets its special treatment here..
        if (name == "reset") {
            arity_check(1);
            const auto num = dynamic_cast<const Num*>(call->args.front().get());

            if (not num) error("Can only reset numbers!");

            if (const auto& v = getVar(num->num); not v) error("Reseting an unset number!");
            else removeVar(num->num);

            return std::stoi(num->num);
        }

        if (name == "true" or name == "false") arity_check(0);
        if (name == "true")  return execute<0>(stdx::get<S<"true">>(functions).value, {}, this);
        if (name == "false") return execute<0>(stdx::get<S<"false">>(functions).value, {}, this);



        if (name == "print" or name == "neg" or name == "not") arity_check(1); // just for now..


        const auto& value1 = std::visit(*this, call->args[0]->variant());
        if (name == "print") return execute<1>(stdx::get<S<"print">>(functions).value, {value1}, this);

        if (name == "neg") return execute<1>(stdx::get<S<"neg">>(functions).value, {value1}, this);

        if (name == "not") return execute<1>(stdx::get<S<"not">>(functions).value, {value1}, this);


        // all the rest of those funcs expect 2 arguments

        using std::operator""sv;
        const auto names_2 = {"add"sv, "sub"sv, "mul"sv, "div"sv, "gt"sv, "geq"sv, "eq"sv, "leq"sv, "lt"sv, "and"sv, "or"sv};

        if (std::ranges::find(names_2, name) != names_2.end()) arity_check(2); 

        const auto& value2 = std::visit(*this, call->args[1]->variant());

        if (name == "add") return execute<2>(stdx::get<S<"add">>(functions).value, {value1, value2}, this);
        if (name == "sub") return execute<2>(stdx::get<S<"sub">>(functions).value, {value1, value2}, this);
        if (name == "mul") return execute<2>(stdx::get<S<"mul">>(functions).value, {value1, value2}, this);
        if (name == "div") return execute<2>(stdx::get<S<"div">>(functions).value, {value1, value2}, this);

        if (name == "gt" ) return execute<2>(stdx::get<S<"gt" >>(functions).value, {value1, value2}, this);
        if (name == "geq") return execute<2>(stdx::get<S<"geq">>(functions).value, {value1, value2}, this);
        if (name == "eq" ) return execute<2>(stdx::get<S<"eq" >>(functions).value, {value1, value2}, this);
        if (name == "leq") return execute<2>(stdx::get<S<"leq">>(functions).value, {value1, value2}, this);
        if (name == "lt" ) return execute<2>(stdx::get<S<"lt" >>(functions).value, {value1, value2}, this);

        if (name == "and") return execute<2>(stdx::get<S<"and">>(functions).value, {value1, value2}, this);
        if (name == "or" ) return execute<2>(stdx::get<S<"or" >>(functions).value, {value1, value2}, this);


        arity_check(3);
        const auto& value3 = std::visit(*this, call->args[2]->variant());
        if (name == "conditional")  return execute<3>(stdx::get<S<"conditional">>(functions).value, {value1, value2, value3}, this);


        error("Calling a builtin fuction that doesn't exist!");
    }



    void print(const Value& value) const noexcept {
        if (std::holds_alternative<bool>(value)) {
            const auto& v = std::get<bool>(value);

            if (const auto var = getVar(std::to_string(v)); var) print(*var);
            else std::println("{}", v);
        }

        if (std::holds_alternative<int>(value)) {
            const auto& v = std::get<int>(value);

            if (const auto var = getVar(std::to_string(v)); var) print(*var);
            else std::println("{}", v);
        }

        if (std::holds_alternative<double>(value)) {
            const auto& v = std::get<double>(value);

            if (const auto var = getVar(std::to_string(v)); var)  print(*var);
            else std::println("{}", v);
        }

        if (std::holds_alternative<std::string>(value)) {
            const auto& v = std::get<std::string>(value);

            // if (const auto var = getVar(""); var) print(*var);
            // else  std::println("{}", v);
            std::println("{}", v);
        }

        if (std::holds_alternative<Closure>(value)) {
            const auto& v = std::get<Closure>(value);
            
            // ! this is new
            if (const auto var = getVar("Closure"); var) print(*var);
            else {
                std::cout << v.print(0);
                puts(""); // .print() doesn't add a \n so we add it manually
            }

        }
    }

private:

    struct ScopeGuard {
        Visitor* v;
        ScopeGuard(Visitor* t, const Environment& e = {}) noexcept : v{t} {
            v->scope();

            if (not e.empty())
                for (const auto& [name, value] : e)
                    v->addVar(name, value);
        }

        ~ScopeGuard() { v->unscope(); }
    };


    void scope() { env.emplace_back(); }
    void unscope() { env.pop_back(); }

    const Value& addVar(const std::string& name, const Value& v) { env.back()[name] = v; return v; }

    std::optional<Value> getVar(const std::string& name) const {
        for (auto rev_it = env.crbegin(); rev_it != env.crend(); ++rev_it)
            if (rev_it->contains(name)) return rev_it->at(name);


        return {};
    }

    void removeVar(const std::string& name) {
        for(auto& curr_env : env) curr_env.erase(name);
    }


    Environment envStackToEnvMap() const {
        Environment e;

        for(const auto& curr_env : env)
            for(const auto& [key, value] : curr_env)
                e[key] = value; // I want the recent values (higher in the stack) to be the ones captured


        return e;
    }

};

