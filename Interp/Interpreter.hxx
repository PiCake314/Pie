#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <ranges>
#include <optional>
#include <utility>
#include <stdexcept>
#include <cassert>
#include <stdx/tuple.hpp>


#include "../Utils/utils.hxx"
#include "../Expr/Expr.hxx"
#include "../Parser/Parser.hxx"
#include "../Declarations.hxx"



struct Dict;

// carrys the default values to be copied into an object. essentially const
// carrys objects' values. could be changed

using Object = std::shared_ptr<Dict>; 

// for lazy eval type
using Value = std::variant<int, double, bool, std::string, expr::Closure, ClassValue, Object, expr::Node>;

struct Dict { std::vector<std::pair<expr::Name, Value>> members; };

using Environment = std::unordered_map<std::string, std::pair<Value, type::TypePtr>>;



struct Visitor {
    // struct Builtin { std::string name; }; // for later

    std::vector<Environment> env;
    const Operators ops;


    // inline static const std::unordered_map<const char*, 


    Visitor(Operators ops) noexcept : env(1), ops{std::move(ops)} { }


    Value operator()(const expr::Num *n) {
        if (auto&& var = getVar(n->stringify()); var) return var->first;


        // have to do an if rather than ternary so the return value isn't always coerced into doubles
        if (n->num.find('.') != std::string::npos) return std::stod(n->num);
        else return std::stoi(n->num);
    }

    Value operator()(const expr::String *s) {
        if (auto&& var = getVar(s->stringify()); var) return var->first;

        return s->str; 
    }

    Value operator()(const expr::Name *n) {
        // what should builtins evaluate to?
        // If I return the string back, then expressions like `"__builtin_neg"(1)` are valid now :)))))
        // interesting!
        // how about a special value?
        if (isBuiltIn(n->name)) return n->name;

        if (auto&& var = getVar(n->stringify()); var) return var->first;


        error("Name \"" + n->stringify() + "\" is not defined");
    }


    Value operator()(const expr::Assignment *ass) {


        // assigning to x.y should never create a variable "x.y" bu access x and change y;
        if (const auto acc = dynamic_cast<expr::Access*>(ass->lhs.get())) {
            const auto& value = std::visit(*this, ass->rhs->variant());
            const auto& left = std::visit(*this, acc->var->variant());

            if (not std::holds_alternative<Object>(left)) error("Can't access a non-class type!");

            const auto& obj = get<Object>(left);

            const auto& found = std::ranges::find_if(obj->members, [name = acc->name](auto&& member) { return member.first.stringify() == name; });
            if (found == obj->members.end()) error("Name '" + acc->name + "' doesn't exist in object!");


            //TODO: Dry this out!
            if (auto&& type_of_value = typeOf(value); not (*found->first.type >= *type_of_value)) { // if not a super type..
                std::cerr << "In assignment: " << ass->stringify() << '\n';
                error("Type mis-match! Expected: " + found->first.type->text() + ", got: " + type_of_value->text());
            }

            found->second = value;

            return value;
        }

        if (const auto name = dynamic_cast<expr::Name*>(ass->lhs.get())){
            type::TypePtr type = type::builtins::Any();

            // variable already exists. Check that type matches the rhs type
            if (auto&& var = getVar(name->name); var) {
                // no need to check if it's a valid type since that already was checked when it was creeated
                type = var->second;
            }
            else { // New var
                validateType(name->type);
                // if(not validate(name->type)) error("Invalid Type: " + name->type->text());

                type = name->type;

                if (auto&& c = getVar(type->text()); c and std::holds_alternative<ClassValue>(c->first))
                    // type = std::make_shared<type::VarType>(stringify(c->first)); // check if the type is a user-defined-class
                    type = std::make_shared<type::VarType>(std::make_shared<expr::Name>(typeStringify(get<ClassValue>(c->first))));
            }



            if (type->text() == "Syntax")
                return addVar(name->stringify(), ass->rhs->variant(), type);



            auto value = std::visit(*this, ass->rhs->variant());
            // if (type->text() != "Any") // only enforce type checking when
            if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value)) { // if not a super type..
                std::cerr << "In assignment: " << ass->stringify() << '\n';
                error("Type mis-match! Expected: " + type->text() + ", got: " + type_of_value->text());
            }


            // casting the function type in case assigning a function to our variable
            if (std::holds_alternative<expr::Closure>(value)) {
                auto& closure = get<expr::Closure>(value);
                // we verified types are compatible so this is fine..should be...I hope
                if (const auto* t = dynamic_cast<type::FuncType*>(type.get()))
                    closure.type = *t;
                else if (const auto* t = dynamic_cast<type::VarType*>(type.get()); t and t->text() != "Any")
                    error("Incompatible types. This should never happen. File a bug report!");
                // else 
            }


            // std::clog << "Here: " << name->stringify() << '\n';
            return addVar(name->stringify(), value, type);
        }


        // assing to the serialization (stringification) of the AST node
        return addVar(ass->lhs->stringify(), std::visit(*this, ass->rhs->variant()));
    }


    Value operator()(const expr::Class *cls) {
        if (auto&& var = getVar(cls->stringify()); var) return var->first;


        std::vector<std::pair<expr::Name, Value>> members;

        ScopeGuard sg{this};
        for (const auto& field : cls->fields) {
            // const auto& name = std::visit(*this, field.lhs->variant());

            // this performs type checking...i think
            // it doesn't add vars to the environment bc we added a scope guard
            std::visit(*this, field.variant());

            // I think we don't need to check the type against the name.type
            // since the line above already did type checking
            const auto& v = std::visit(*this, field.rhs->variant());
            members.push_back({{field.lhs->stringify(), typeOf(v)}, v});
        }


        // for (auto&& [name, value] : members) {
        //     if (std::holds_alternative<Closure>(value)) {
        //         const auto& c = get<Closure>(value);
        //         c.capture(members);

        //         members[name] = c;
        //     }
        // }

        return ClassValue{std::make_shared<Dict>(std::move(members))};
    }


    Value operator()(const expr::Access *acc) {

        const auto& left = std::visit(*this, acc->var->variant());

        if (not std::holds_alternative<Object>(left)) error("Can't access a non-class type!");

        const auto& obj = std::get<Object>(left);

        const auto& found = std::ranges::find_if(obj->members, [&name = acc->name] (auto&& member) { return member.first.stringify() == name; });
        if (found == obj->members.end()) error("Name '" + acc->name + "' doesn't exist in object!");


        if (std::holds_alternative<expr::Closure>(found->second)) {
            const auto& closure = get<expr::Closure>(found->second);

            Environment capture_list;
            for (const auto& [name, value] : obj->members)
                capture_list[name.stringify()] = {value, typeOf(value)}; //* maybe name.name?

            closure.capture(capture_list);

            return closure;
        }

        return found->second;
    }

    // only added to differentiate between expressions such as: 1 + 2 and (1 + 2)
    Value operator()(const expr::Grouping *g) {
        if (auto&& var = getVar(g->stringify()); var) return var->first;

        return std::visit(*this, g->expr->variant());
    }

    Value operator()(const expr::UnaryOp *up) {
        if (auto&& var = getVar(up->stringify()); var) return var->first;


        // ScopeGuard sg{this}; // RAII is so cool
        // scope();

        const expr::Fix* op = ops.at(up->op);
        expr::Closure* func = dynamic_cast<expr::Closure*>(op->func.get());

        // don't need to check this
        // if (not func) error("This shouldn't happen, but also Unary Operator not equal to a closure!");
        // no need to assert I guess
        // assert(func->params.size() == 1);

        //* this could be dried out between all the OPs and function calls in general
        Environment args_env;
        if (func->type.params[0]->text() == "Syntax") {
            // addVar(func->params.front(), up->expr->variant());
            //* maybe should use Syntax() instead of Any();
            args_env[func->params[0]] = {up->expr->variant(), type::builtins::Any()};
        }
        else {
            validateType(func->type.params[0]);

            const auto& arg = std::visit(*this, up->expr->variant());
            if (auto&& type_of_arg = typeOf(arg); not (*func->type.params[0] >= *type_of_arg))
                error(
                    "Type mis-match! Prefix operator '" + up->op + 
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                    // ", got: " + type_of_arg->text()
                );

            // addVar(func->params.front(), arg);
            //* maybe should use Syntax() instead of Any();
            args_env[func->params[0]] = {arg, type::builtins::Any()};
        }

        ScopeGuard sg{this, args_env};

        return std::visit(*this, func->body->variant()); // RAII takes care of unscoping

        // unscope();
    }

    Value operator()(const expr::BinOp *bp) {
        if (auto&& var = getVar(bp->stringify()); var) return var->first;


        // ScopeGuard sg{this};

        const expr::Fix* op = ops.at(bp->op);
        expr::Closure* func = dynamic_cast<expr::Closure*>(op->func.get());


        Environment args_env;
        // LHS
        if (func->type.params[0]->text() == "Syntax") {
            // addVar(func->params[0], bp->lhs->variant());
            args_env[func->params[0]] = {bp->lhs->variant(), type::builtins::Any()};
        }
        else {
            validateType(func->type.params[0]);

            const auto& arg1 = std::visit(*this, bp->lhs->variant());
            if (auto&& type_of_arg = typeOf(arg1); not (*func->type.params[0] >= *type_of_arg))
                error(
                    "Type mis-match! Infix operator '" + bp->op + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg1) + " which is " + type_of_arg->text()
                    // ", got: " + type_of_arg->text()
                );

            // addVar(func->params[0], arg1);
            args_env[func->params[0]] = {arg1, type::builtins::Any()};
        }

        // RHS
        if (func->type.params[1]->text() == "Syntax") {
            // addVar(func->params[1], bp->rhs->variant());
            args_env[func->params[1]] = {bp->rhs->variant(), type::builtins::Any()};
        }
        else {
            validateType(func->type.params[1]);

            const auto& arg2 = std::visit(*this, bp->rhs->variant());
            if (auto&& type_of_arg = typeOf(arg2); not (*func->type.params[1] >= *type_of_arg))
                error(
                    "Type mis-match! Infix operator '" + bp->op + 
                    "', parameter '" + func->params[1] +
                    "' expected: " + func->type.params[1]->text() +
                    ", got: " + stringify(arg2) + " which is " + type_of_arg->text()
                    // ", got: " + type_of_arg->text()
                );

            // addVar(func->params[1], arg2);
            args_env[func->params[1]] = {arg2, type::builtins::Any()};
        }

        ScopeGuard sg{this, args_env};

        return std::visit(*this, func->body->variant());
    }

    Value operator()(const expr::PostOp *pp) {
        if (auto&& var = getVar(pp->stringify()); var) return var->first;


        // ScopeGuard sg{this};

        const expr::Fix* op = ops.at(pp->op);
        expr::Closure* func = dynamic_cast<expr::Closure*>(op->func.get());


        Environment args_env;
        if (func->type.params[0]->text() == "Syntax") {
            // addVar(func->params[0], pp->expr->variant());
            args_env[func->params[0]] = {pp->expr->variant(), type::builtins::Any()};
        }
        else {
            validateType(func->type.params[0]);

            const auto& arg = std::visit(*this, pp->expr->variant());
            if (auto&& type_of_arg = typeOf(arg); not (*func->type.params[0] >= *type_of_arg))
                error(
                    "Type mis-match! Suffix operator '" + pp->op + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                    // ", got: " + type_of_arg->text()
                );

            // addVar(func->params[0], std::visit(*this, pp->expr->variant()));
            args_env[func->params[0]] = {arg, type::builtins::Any()};
        }

        ScopeGuard sg{this, args_env};
        return std::visit(*this, func->body->variant());
    }



    Value operator()(const expr::CircumOp *cp) {
        if (auto&& var = getVar(cp->stringify()); var) return var->first;

        // ScopeGuard sg{this};

        const expr::Fix* op = ops.at(cp->op1);
        expr::Closure* func = dynamic_cast<expr::Closure*>(op->func.get());


        Environment args_env;
        if (func->type.params[0]->text() == "Syntax") {
            // addVar(func->params[0], co->expr->variant());
            args_env[func->params[0]] = {cp->expr->variant(), type::builtins::Any()};
        }
        else {
            validateType(func->type.params[0]);

            const auto& arg = std::visit(*this, cp->expr->variant());
            if (auto&& type_of_arg = typeOf(arg); not (*func->type.params[0] >= *type_of_arg))
                error(
                    "Type mis-match! Suffix operator '" + cp->op1 + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                    // ", got: " + type_of_arg->text()
                );

            // addVar(func->params[0], std::visit(*this, co->expr->variant()));
            args_env[func->params[0]] = {arg, type::builtins::Any()};
        }

        ScopeGuard sg{this, args_env};
        return std::visit(*this, func->body->variant());
    };

    Value operator()(const expr::Call *call) {
        if (auto&& var = getVar(call->stringify()); var) return var->first;


        ScopeGuard sg{this};

        auto var = std::visit(*this, call->func->variant());

        if (std::holds_alternative<std::string>(var)) { // that dumb lol. but now it works
            const auto& name = std::get<std::string>(var);
            if (isBuiltIn(name)) return evaluateBuiltin(call, name);
        }


        if (std::holds_alternative<expr::Closure>(var)) {
            const auto& func = std::get<expr::Closure>(var);

            // assert(call->args.size() == func.params.size());
            if (call->args.size() > func.params.size()) error("Wrong arity call!");

            if (const auto args_size = call->args.size(); args_size < func.params.size()) {
                expr::Closure closure{std::vector<std::string>{func.params.begin() + args_size, func.params.end()}, func.type, func.body};

                Environment argument_capture_list = func.env;
                for(const auto& [name, type, expr] : std::views::zip(func.params, func.type.params, call->args)) {

                    if (type->text() == "Syntax") {
                        argument_capture_list[name] = {expr->variant(), type};
                        continue;
                    }


                    const auto& value = std::visit(*this, expr->variant());
                    if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value)) // if the param type not a super type of arg type
                        error("Type mis-match! Expected: " + type->text() + ", got: " + type_of_value->text());


                    // if (type == "Lazy") value = 
                    argument_capture_list[name] = {value, type};
                }

                closure.capture(argument_capture_list);

                return closure;
            }


            Environment args_env;
            // full call. Don't curry!
            for (const auto& [name, type, arg] : std::views::zip(func.params, func.type.params, call->args)){
                validateType(type);
                // if (not isValidType(type)) error("Type '" + type->text() + "' is not a valid type!");

                if (type->text() == "Syntax") {
                    // addVar(name, arg->variant());
                    args_env[name] = {arg->variant(), type::builtins::Any()};
                    continue;
                }


                const auto& value = std::visit(*this, arg->variant());
                if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                    error("Type mis-match! Expected: " + type->text() + ", got: " + type_of_value->text());

                // addVar(name, std::visit(*this, arg->variant()));
                args_env[name] = {std::visit(*this, arg->variant()), type::builtins::Any()};
            }
                // env.back()[param] = std::visit(*this, arg->variant());


            //* should I capture the env and bundle it with the function before returning it?
            if (func.type.ret->text() == "Syntax") return func.body->variant();

            ScopeGuard sg1{this, func.env}; // in case the lambda had captures
            ScopeGuard sg2{this, args_env};

            const auto& ret = std::visit(*this, func.body->variant());

            const auto& type_of_return_value = typeOf(ret);
            validateType(func.type.ret); //* maybe this should be moved up?
            // if(not isValidType(func.type.ret)) error("Invalid Type: " + func.type.ret->text());

            auto return_type = func.type.ret;
            if (const auto& c = getVar(return_type->text()); c and std::holds_alternative<ClassValue>(c->first))
                // return_type = std::make_shared<type::VarType>(stringify(c->first));
                return_type = std::make_shared<type::VarType>(std::make_shared<expr::Name>(stringify(c->first)));

            if (not (*return_type >= *type_of_return_value))
                error(
                    "Type mis-match! Function return type Expected: " +
                    return_type->text() + ", got: " + type_of_return_value->text()
                );

            return ret;
        }

        if (std::holds_alternative<ClassValue>(var)) {
            // if (not call->args.empty()) error("Can't pass arguments to classes!");
            const auto& cls = std::get<ClassValue>(var);

            if (call->args.size() > cls.blueprint->members.size())
                error("Too many arguments passed to constructor of class: " + typeStringify(cls));

            // copying defaults field values from class definition
            Object obj{ std::make_shared<Dict>(cls.blueprint->members) };

            // I woulda used a range for-loop but I need `arg` to be a reference and `value` to be a const ref
            // const auto& [arg, value] : std::views::zip(call->args, obj->members)
            for (size_t i{}; i < call->args.size(); ++i) {
                const auto& v = std::visit(*this, call->args[i]->variant());

                if (typeOf(v)->text() != typeOf(obj->members[i].second)->text())
                    error(
                        "Type mis-match in constructor of:\n" + typeStringify(cls) + "Member `" +
                        obj->members[i].first.stringify() + "` expected: " + typeOf(obj->members[i].second)->text() +
                        "\nbut got: " + call->args[i]->stringify() + " which is " + typeOf(v)->text()
                    );

                obj->members[i].second = v;
            }

            return obj;
        }

        // error("operator() applied on not-a-function");
        // applied on a value. Just return the value for now
        // maybe I should make it so that x(1) == { t = x; x = 1; t; }; ...?

        if (call->args.size() != 0) error("Can't pass arguments to values!");
        return var;
    }

    Value operator()(const expr::Closure *c) {
        if (auto&& var = getVar(c->stringify()); var) return var->first;

        // take a snapshot of the current env (capture by value). comment this line if you want capture by reference..
        c->capture(envStackToEnvMap());
        return *c;
    }


    Value operator()(const expr::Block *block) {
        if (auto&& var = getVar(block->stringify()); var) return var->first;


        ScopeGuard sg{this};

        Value ret;
        for (const auto& line : block->lines) 
            ret = std::visit(*this, line->variant()); // a scope's value is the last expression

        return ret;
    }

    // Value operator()(const expr::Fix *fix) {
    //     if (auto&& var = getVar(fix->stringify()); var) return var->first;
    //     return std::visit(*this, fix->func->variant());
    // }

    // split so I can param check at definition time instead of call time..
    Value operator()(const expr::Prefix *fix) {
        if (auto&& var = getVar(fix->stringify()); var) return var->first;

        // I know it's a function, otherwise parser woulda been mad
        if (dynamic_cast<expr::Closure*>(fix->func.get())->params.size() != 1)
            error("Prefix operator '" + fix->name + "' was assigned to a function with the wrong arity!");

        return std::visit(*this, fix->func->variant());
    }

    Value operator()(const expr::Infix *fix) {
        if (auto&& var = getVar(fix->stringify()); var) return var->first;

        if (dynamic_cast<expr::Closure*>(fix->func.get())->params.size() != 2)
            error("Prefix operator '" + fix->name + "' was assigned to a function with the wrong arity!");

        return std::visit(*this, fix->func->variant());
    }

    Value operator()(const expr::Suffix *fix) {
        if (auto&& var = getVar(fix->stringify()); var) return var->first;

        if (dynamic_cast<expr::Closure*>(fix->func.get())->params.size() != 1)
            error("Prefix operator '" + fix->name + "' was assigned to a function with the wrong arity!");

        return std::visit(*this, fix->func->variant());
    }

    Value operator()(const expr::Exfix *fix) {
        if (auto&& var = getVar(fix->stringify()); var) return var->first;

        if (dynamic_cast<expr::Closure*>(fix->func.get())->params.size() != 1)
            error("Exfix operator '" + fix->name + "' was assigned to a function with the wrong arity!");

        return std::visit(*this, fix->func->variant());
    }


    Value operator()(const expr::Operator *) { return 1; }


    [[nodiscard]] bool isBuiltIn(const std::string_view func) const noexcept {
        const auto make_builtin = [] (const std::string& n) { return "__builtin_" + n; };

        for(const auto& builtin : {
            "true", "false",                                                          //* nullary
            "print", "reset", "eval","neg", "not",                                    //* unary
            "add", "sub", "mul", "div", "mod", "pow", "gt", "geq", "eq", "leq", "lt", "and", "or",  //* binary
            "conditional"                                                             //* trinary
        })
            if (func == make_builtin(builtin)) return true;

        return false;
    }


    // the gate into the META operators!
    Value evaluateBuiltin(const expr::Call* call, std::string name) {
        //* ============================ FUNCTIONS ============================
        const auto functions = stdx::make_indexed_tuple<KeyFor>(
            //* NULLARY FUNCTIONS
            MapEntry<
                S<"true">,
                Func<"true",
                    decltype([](const auto&) { return true; }),
                    void
                >
            >{},
            MapEntry<
                S<"false">,
                Func<"false",
                    decltype([](const auto&) { return false; }),
                    void
                >
            >{},

            //* UNARY FUNCTIONS
            MapEntry<
                S<"neg">,
                Func<"neg",
                    decltype([](auto&& x, const auto&) { return -x; }),
                    TypeList<int>,
                    TypeList<double>
                >
            >{},

            MapEntry<
                S<"not">,
                Func<"not",
                    decltype([](auto&& x, const auto&) { return not x; }),
                    TypeList<int>,
                    TypeList<double>,
                    TypeList<bool>
                >
            >{},

            // MapEntry<
            //     S<"print">,
            //     Func<"print",
            //         decltype([](auto&& x, const auto& that) {
            //             std::visit(
            //                 [&that](const auto& v) { that->print(v); },
            //                 x
            //             );
            //             return x;
            //         }),
            //         TypeList<Any>
            //     >
            // >{},

            MapEntry<
                S<"eval">,
                Func<"eval",
                    decltype([](auto&& x, const auto& that) {
                        return std::visit(*that, x);
                    }),
                    TypeList<expr::Node>
                >
            >{},



            // MapEntry<
            //     S<"reset">,
            //     Func<
            //         decltype([](auto&& v, const auto& that)  {
            //             // const auto num = dynamic_cast<const Num*>(call->args.front().get());

            //             // if (not num) error("Can only reset numbers!");
            //             std::visit(
            //                 [that](const auto& value) {
            //                     const std::string& s = value->stringify();
            //                     if (const auto& v = that->getVar(s); not v) error("Reseting an unset value: " + s);
            //                     else that->removeVar(s);

            //                     return value;
            //                 },
            //                 v
            //             );
            //         }),
            //         TypeList<Any>
            //     >
            // >{},

            //* BINARY FUNCTIONS
            MapEntry<
                S<"add">,
                Func<"add",
                    decltype([](auto&& a, auto&& b, const auto&) { return a + b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"sub">,
                Func<"sub",
                    decltype([](auto&& a, auto&& b, const auto&) { return a - b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"mul">,
                Func<"mul",
                    decltype([](auto&& a, auto&& b, const auto&) { return a * b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"div">,
                Func<"div",
                    decltype([](auto&& a, auto&& b, const auto&) { return a / b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"mod">,
                Func<"mod",
                    decltype([](auto&& a, auto&& b, const auto&) { return a % b; }),
                    TypeList<int, int>
                >
            >{},

            MapEntry<
                S<"pow">,
                Func<"pow",
                    decltype(
                        [](auto&& a, auto&& b, const auto&) -> std::common_type_t<decltype(a), decltype(b)> { return std::pow(a, b); }
                    ),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"gt">,
                Func<"gt",
                    decltype([](auto&& a, auto&& b, const auto&) { return a > b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"geq">,
                Func<"geq",
                    decltype([](auto&& a, auto&& b, const auto&) { return a >= b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"eq">,
                Func<"eq",
                    decltype([](auto a, auto b, const auto& that) {
                        if (std::holds_alternative<expr::Node>(a)) a = std::visit(*that, get<expr::Node>(a));

                        if (std::holds_alternative<expr::Node>(b)) b = std::visit(*that, get<expr::Node>(b));

                        if (std::holds_alternative<std::string>(a) and std::holds_alternative<std::string>(b)) return get<std::string>(a) == get<std::string>(b); // ADL

                        // will be caught down
                        // if (std::holds_alternative<std::string>(a)  or std::holds_alternative<std::string>(b)) return false;

                        if (std::holds_alternative<int>(a) and std::holds_alternative<int>(b))       return get<int>(a) == get<int>(b);
                        if (std::holds_alternative<int>(a) and std::holds_alternative<double>(b))    return get<int>(a) == get<double>(b);
                        if (std::holds_alternative<double>(a) and std::holds_alternative<int>(b))    return get<double>(a) == get<int>(b);
                        if (std::holds_alternative<double>(a) and std::holds_alternative<double>(b)) return get<double>(a) == get<double>(b);

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
                Func<"leq",
                    decltype([](auto&& a, auto&& b, const auto&) { return a >= b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"lt">,
                Func<"lt",
                    decltype([](auto&& a, auto&& b, const auto&) { return a < b; }),
                    TypeList<int, int>,
                    TypeList<int, double>,
                    TypeList<double, int>,
                    TypeList<double, double>
                >
            >{}

            // // * BOOLEAN FUNCTIONS
            // MapEntry<
            //     S<"or">,
            //     Func<
            //         decltype([](auto&& a, auto&& b, const auto&) { return a or b; }),
            //         TypeList<bool, bool>
            //     >
            // >{},

            // MapEntry<
            //     S<"and">,
            //     Func<
            //         decltype([](auto&& a, auto&& b, const auto&) { return a and b; }),
            //         TypeList<bool, bool>
            //     >
            // >{}

            // // * TRINARY FUNCTIONS
            // MapEntry<
            //     S<"conditional">,
            //     Func<
            //         decltype([](auto&& cond, auto&& then, auto&& otherwise, const auto&) -> Value {
            //             if (std::holds_alternative<bool>(cond) and get<bool>(cond)) return then;

            //             return otherwise;
            //         }),
            //         TypeList<Node, Node, Node>
            //     >
            // >{}
        );



        name = name.substr(10); // cutout the "__builtin_"

        const auto arity_check = [&name, call] (const size_t arity) {
            if (call->args.size() != arity) error("Wrong arity with call to \"__builtin_" + name + "\"");
        };

        // const auto int_check = [&name] (const Value& val) {
        //     if (not std::holds_alternative<int>(val)) error("Wrong argument passed to to \"__builtin_" + name + "\"");
        // };

        if (name == "true" or name == "false") arity_check(0);
        if (name == "true")  return execute<0>(stdx::get<S<"true">>(functions).value, {}, this);
        if (name == "false") return execute<0>(stdx::get<S<"false">>(functions).value, {}, this);

        if (name == "print") {
            if (call->args.empty()) error("'print' requires at least 1 argument passed!");

            Value ret;
            for(const auto& arg : call->args) {
                constexpr bool no_newline = false;
                ret = std::visit(*this, arg->variant());
                print(ret, no_newline);
            }
            puts(""); // print the new line in the end.
            return ret;
        }


        if (name == "neg" or name == "not" or name == "reset") arity_check(1); // just for now..


        // evaluating arguments from left to right as needed
        // first argument is always evaluated
        const auto& value1 = std::visit(*this, call->args[0]->variant());

        // Since this is a meta function that operates on AST nodes rather than values
        // it gets its special treatment here..
        if (name == "reset") {
            const std::string& s = call->args[0]->stringify();
            if (const auto& v = getVar(s); not v) error("Reseting an unset value: " + s);
            else removeVar(s);

            // return std::stoi(num->num);
            return value1;
        }


        // if (name == "print") return execute<1>(stdx::get<S<"print">>(functions).value, {value1}, this);

        if (name == "eval" ) return execute<1>(stdx::get<S<"eval" >>(functions).value, {value1}, this);
        if (name == "neg"  ) return execute<1>(stdx::get<S<"neg"  >>(functions).value, {value1}, this);
        if (name == "not"  ) return execute<1>(stdx::get<S<"not"  >>(functions).value, {value1}, this);


        // all the rest of those funcs expect 2 arguments

        using std::operator""sv;

        const auto eager = {"add"sv, "sub"sv, "mul"sv, "div"sv, "mod"sv, "pow"sv, "gt"sv, "geq"sv, "eq"sv, "leq"sv, "lt"sv};
        if (std::ranges::find(eager, name) != eager.end()) {
            arity_check(2);
            const auto& value2 = std::visit(*this, call->args[1]->variant());

            // this is disgusting..I know
            if (name == "add") return execute<2>(stdx::get<S<"add">>(functions).value, {value1, value2}, this);
            if (name == "sub") return execute<2>(stdx::get<S<"sub">>(functions).value, {value1, value2}, this);
            if (name == "mul") return execute<2>(stdx::get<S<"mul">>(functions).value, {value1, value2}, this);
            if (name == "div") return execute<2>(stdx::get<S<"div">>(functions).value, {value1, value2}, this);
            if (name == "mod") return execute<2>(stdx::get<S<"mod">>(functions).value, {value1, value2}, this);
            if (name == "pow") return execute<2>(stdx::get<S<"pow">>(functions).value, {value1, value2}, this);
            if (name == "gt" ) return execute<2>(stdx::get<S<"gt" >>(functions).value, {value1, value2}, this);
            if (name == "geq") return execute<2>(stdx::get<S<"geq">>(functions).value, {value1, value2}, this);
            if (name == "eq" ) return execute<2>(stdx::get<S<"eq" >>(functions).value, {value1, value2}, this);
            if (name == "leq") return execute<2>(stdx::get<S<"leq">>(functions).value, {value1, value2}, this);
            if (name == "lt" ) return execute<2>(stdx::get<S<"lt" >>(functions).value, {value1, value2}, this);

            error("This shouldn't happen. File a bug report!");

        }


        if (name == "and") {
            arity_check(2);

            if (not std::holds_alternative<bool>(value1)) return value1; // return first falsy value
            if (not get<bool>(value1)) return value1; // first falsey value


            return std::visit(*this, call->args[1]->variant()); // last truthy value
        }

        if (name == "or" ) {
            arity_check(2);
            if (not std::holds_alternative<bool>(value1)) return std::visit(*this, call->args[1]->variant()); // last falsey value


            if(get<bool>(value1)) return value1; // first truthy value
            return std::visit(*this, call->args[1]->variant()); // last falsey value
        }


        if (name == "conditional") {
            arity_check(3);
            const auto& then      = call->args[1]->variant();
            const auto& otherwise = call->args[2]->variant();

            // std::clog << call->args[1]->stringify() << std::endl;
            // std::clog << call->args[2]->stringify() << std::endl;
            // std::clog << stringify(std::visit(*this, otherwise)) << std::endl;

            if (not std::holds_alternative<bool>(value1)) return std::visit(*this, otherwise);

            if(get<bool>(value1)) return std::visit(*this, then);


            return std::visit(*this, otherwise); // Oh ffs! [for cogs on discord!]
        }


        // const auto& value3 = std::visit(*this, call->args[2]->variant());
        // if (name == "conditional") return execute<3>(stdx::get<S<"conditional">>(functions).value, {value1, value2, value3}, this);


        error("Calling a builtin fuction that doesn't exist!");
    }



    void print(const Value& value, bool new_line = true) const {
        std::print("{}{}", stringify(value), new_line? '\n' : '\0');
    }

    static std::string stringify(const Value& value, size_t indent = {}) {
        std::string s;

        if (std::holds_alternative<bool>(value)) {
            const auto& v = std::get<bool>(value);
            // s = std::to_string(v); // reason I did this in the first place is to allow for copy-ellision to happen
            return v ? "true" : "false";
        }

        else if (std::holds_alternative<int>(value)) {
            const auto& v = std::get<int>(value);
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

            s = "class {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, value] : v.blueprint->members) {
                s += space + name.stringify() + " = ";

                const bool is_string = std::holds_alternative<std::string>(value);
                if (is_string) s += '\"';

                s += stringify(value, indent + 4);

                if (is_string) s += '\"';

                s += ";\n";
            }

            s += '}';

        }

        else if (std::holds_alternative<Object>(value)) {
            const auto& v = std::get<Object>(value);

            s = "Object {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, value] : v->members) {
                s += space + name.stringify() + " = ";

                const bool is_string = std::holds_alternative<std::string>(value);
                if (is_string) s += '\"';

                s += stringify(value, indent + 4);

                if (is_string) s += '\"';


                s += ";\n";
            }

            s += indent + '}';

        }
        else if(std::holds_alternative<expr::Node>(value)) {
            s = "ASTNODE: " + std::visit(
                [] (auto&& v)-> std::string { return v->stringify(); },
                get<expr::Node>(value)
            );
        }
        else error("Type not found!");

        return s;
    }

    void validateType(const type::TypePtr& type) noexcept {
        if (const auto var_type = dynamic_cast<type::VarType*>(type.get())) {
            if (
                const auto& t = var_type->text();
                t == "Any" or
                t == "Syntax" or
                t == "Int" or
                t == "Double" or
                t == "Bool" or
                t == "String" or
                t == "Type"
            )
            return;


            if (
                // auto&& type_value = getVar(var_type->text());
                // type_value and std::holds_alternative<ClassValue>(type_value->first)
                auto&& value = std::visit(*this, var_type->t->variant());
                std::holds_alternative<ClassValue>(value)
            )
            return;

        }
        else { // has to be a function type
            const auto func_type = dynamic_cast<type::FuncType*>(type.get());

            for (const auto& t : func_type->params) validateType(t);


            // all param types are valid. Only thing left to check is return type
            return validateType(func_type->ret);
        }


        error("'" + type->text() + "' does not name a type!");
    }


    static std::string typeStringify(const Object& o) {
        std::string s = "class {\n";

        // should I use '\t' or "    "
        for (const auto& [name, value] : o->members)
            s += "    " + name.stringify() + ": " + name.type->text() + ";\n";
        s += "}\n";

        return s;
    }


    static std::string typeStringify(const ClassValue& c) {
        std::string s = "class {\n";

        for (const auto& [name, value] : c.blueprint->members)
            s += "    " + name.stringify() + ": " + name.type->text() + ";\n";
        s += "}\n";

        return s;
    }


    type::TypePtr typeOf(const Value& value) noexcept {
        if (std::holds_alternative<expr::Node>(value))   return type::builtins::Syntax();
        if (std::holds_alternative<int>(value))          return type::builtins::Int();
        if (std::holds_alternative<double>(value))       return type::builtins::Double();
        if (std::holds_alternative<bool>(value))         return type::builtins::Bool();
        if (std::holds_alternative<std::string>(value))  return type::builtins::String();

        if (std::holds_alternative<expr::Closure>(value)) {
            const auto& func = get<expr::Closure>(value);

            type::FuncType type;
            for (const auto& t : func.type.params)
                type.params.push_back(t);


            // std::clog << type.text() << std::endl;
            type.ret = func.type.ret;

            return std::make_shared<type::FuncType>(std::move(type));
        }
        if (std::holds_alternative<ClassValue>(value)) {
            // return Visitor::stringify(value);
            return type::builtins::Type();
        }
        if (std::holds_alternative<Object>(value)) {
            // return std::make_shared<type::VarType>("class" + stringify(value).substr(6)); // skip the "Object " and add "class"

            std::string s = typeStringify(get<Object>(value));

            return std::make_shared<type::VarType>(std::make_shared<expr::Name>(std::move(s)));
            // return std::make_shared<type::VarType>(std::make_shared<expr::Name>("class" + stringify(value).substr(6)));
        }


        error("Unknown Type: " + stringify(value));
    }


private:

    struct ScopeGuard {
        Visitor* v;
        ScopeGuard(Visitor* t, const Environment& e = {}) noexcept : v{t} {
            v->scope();

            if (not e.empty())
                for (const auto& [name, var] : e) {
                    const auto& [value, type] = var;
                    v->addVar(name, value, type);
                }
        }

        ~ScopeGuard() { v->unscope(); }
    };


    void scope() { env.emplace_back(); }
    void unscope() { env.pop_back(); }

    const Value& addVar(const std::string& name, const Value& v, const type::TypePtr& t = type::builtins::Any()) {
        env.back()[name] = {v, t}; //* might cause issues
        return v;
    }

    std::optional<std::pair<Value, type::TypePtr>> getVar(const std::string& name) const {
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


    void printEnv() const {
        const auto& e = envStackToEnvMap();

        for (const auto& [name, v] : e) {
            const auto& [value, type] = v;

            std::println("{}: {} = {}", name, type->text(), stringify(value));
        }
    }

};

