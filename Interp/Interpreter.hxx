#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <ranges>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <optional>
#include <utility>
#include <stdexcept>

#include <cassert>
#include <cctype>

#include <stdx/tuple.hpp>

#include "../Utils/utils.hxx"
#include "../Utils/ConstexprLookup.hxx"
#include "../Expr/Expr.hxx"
#include "../Parser/Parser.hxx"

#include "Value.hxx"


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
        else return std::stoll(n->num);
    }

    Value operator()(const expr::Bool *b) {
        if (auto&& var = getVar(b->stringify()); var) return var->first;

        return b->boo;
    }

    Value operator()(const expr::String *s) {
        if (auto&& var = getVar(s->stringify()); var) return var->first;

        return s->str;
    }

    Value operator()(const expr::Name *n) {
        // what should builtins evaluate to?
        // If I return the string back, then expressions like `"__builtin_neg"(1)` are valid now :))))
        // interesting!
        // how about a special value?
        if (isBuiltIn(n->name)) return n->name;

        if (auto&& var = getVar(n->stringify()); var) return var->first;



        // std::println("Trace: {}", std::stacktrace::current());
        // trace();
        error("Name \"" + n->stringify() + "\" is not defined");
        // std::unreachable();
    }


    Value operator()(const expr::Pack *) {
        error();
    }


    Value operator()(const expr::Expansion *) {
        error("Can only have an expansion inside of a function call!");
    }


    Value operator()(const expr::Assignment *ass) {

        // assigning to x.y should never create a variable "x.y" bu access x and change y;
        if (const auto acc = dynamic_cast<expr::Access*>(ass->lhs.get())) {
            const auto& left = std::visit(*this, acc->var->variant());

            if (not std::holds_alternative<Object>(left)) error("Can't access a non-class type!");

            const auto& obj = get<Object>(left);

            const auto& found = std::ranges::find_if(obj.second->members, [name = acc->name](auto&& member) { return member.first.stringify() == name; });
            if (found == obj.second->members.end()) error("Name '" + acc->name + "' doesn't exist in object!");

            Value value = found->first.type->text() == "Syntax" ? ass->rhs->variant() : std::visit(*this, ass->rhs->variant());

            //TODO: Dry this out!
            if (auto&& type_of_value = typeOf(value); not (*found->first.type >= *type_of_value)) { // if not a super type..
                std::println(std::cerr, "In assignment: {}", ass->stringify());
                error("Type mis-match! Expected: " + found->first.type->text() + ", got: " + type_of_value->text());
            }

            found->second = value;

            return value;
        }

        if (const auto name = dynamic_cast<expr::Name*>(ass->lhs.get())){
            // type::TypePtr type = type::builtins::Any();
            type::TypePtr type = name->type;
            bool change{};

            // variable already exists. Check that type matches the rhs type
            if (auto&& var = getVar(name->name); var) {
                // no need to check if it's a valid type since that already was checked when it was creeated
                if (type::shouldReassign(name->type)) { // condition not conjuncted with above "if" intentionally
                    type = var->second;
                    change = true;
                }
            }
            else { // New var
                type = type::shouldReassign(type) ? type::builtins::Any() : validateType(type);

                if (auto&& c = getVar(type->text()); c and std::holds_alternative<ClassValue>(c->first))
                    type = std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(get<ClassValue>(c->first)));
            }


            if (type->text() == "Syntax")
                return addVar(name->stringify(), ass->rhs->variant(), type);


            auto value = std::visit(*this, ass->rhs->variant());
            // if (type->text() != "Any") // only enforce type checking when
            if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value)) { // if not a super type..
                std::println(std::cerr, "In assignment: {}", ass->stringify());
                error("Type mis-match! Expected: " + type->text() + ", got: " + type_of_value->text());
            }


            // casting the function type in case assigning a function to our variable
            if (std::holds_alternative<expr::Closure>(value)) {
                auto& closure = get<expr::Closure>(value);
                // we verified types are compatible so this is fine..should be...I hope
                if (const auto* t = dynamic_cast<type::FuncType*>(type.get()))
                    closure.type = *t;
                else if (const auto* t = dynamic_cast<type::BuiltinType*>(type.get()); t and t->text() != "Any")
                     error("Incompatible types. This should never happen. File a bug report!");
                // else error("Again, Incompatible types. This should never happen. File a bug report!");
            }


            if (change) changeVar(name->name, value);
            else        addVar(name->stringify(), value, type);

            return value;
        }


        // assing to the serialization (stringification) of the AST node
        return addVar(ass->lhs->stringify(), std::visit(*this, ass->rhs->variant()));
    }


    Value operator()(const expr::Class *cls) {
        if (auto&& var = getVar(cls->stringify()); var) return var->first;


        std::vector<std::pair<expr::Name, Value>> members;

        ScopeGuard sg{this};
        for (const auto& field : cls->fields) {

            type::TypePtr type = field.first.type;
            if (type::shouldReassign(type)) type = type::builtins::Any();

            Value v;

            if (type->text() == "Syntax") {
                // members.push_back({{field.first.stringify(), type::builtins::Syntax()}, field.second->variant()});
                type = type::builtins::Syntax();
                v = field.second->variant();
            }
            else {
                type = validateType(type);


                if (auto&& c = getVar(type->text()); c and std::holds_alternative<ClassValue>(c->first))
                    type = std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(get<ClassValue>(c->first)));


                v = std::visit(*this, field.second->variant());

                if (auto&& type_of_value = typeOf(v); not (*type >= *type_of_value)) { // if not a super type..
                    std::println(std::cerr, "In class member assignment: {}: {} = {}",
                        field.first.name,
                        field.first.type->text(),
                        field.second->stringify());
                    error("Type mis-match! Expected: " + type->text() + ", got: " + type_of_value->text());
                }
            }


            members.push_back({{field.first.name, type}, v});
        }

        return ClassValue{std::make_shared<Dict>(std::move(members))};
    }


    bool match(const Value& value, const expr::Match::Case::Pattern& pattern) {
        if (std::holds_alternative<expr::Match::Case::Pattern::Single>(pattern.pattern)) {
            const auto& [name, type, val_expr] = get<expr::Match::Case::Pattern::Single>(pattern.pattern);

            if (not (*type >= *typeOf(value))) return false;

            if (val_expr) {
                const Value val = std::visit(*this, val_expr->variant());
                if (value != val) return false;
            }

            addVar(name, value, type);
            return true;
        }

        const auto& [type_name, patterns] = get<expr::Match::Case::Pattern::Structure>(pattern.pattern);

        const auto var = getVar(type_name);
        if (not var)
            error("Pattern in match expression does not name a constructor: " + type_name);

        const Value type_value = var->first;
        if (not std::holds_alternative<ClassValue>(type_value))
            error("Name '" + type_name + "' in match expression does not name a constructor: " + type_name);


        const auto& cls = get<ClassValue>(type_value);
        const type::TypePtr type = std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(cls));
        if (not (*type >= *typeOf(value))) return false;

        if (patterns.size() > cls.blueprint->members.size())
            error("Number of singles is greater than number of fields in class " + stringify(cls));


        const auto& obj = get<Object>(value);
        if (obj.second->members.size() != obj.second->members.size()) error("idek what error message this should be..!");

        for (auto&& [member, pat] : std::views::zip(get<Object>(value).second->members, patterns)) {
            if (not match(member.second, *pat)) return false;
        }

        return true;
    }


    Value operator()(const expr::Match *m) {
        const Value value = std::visit(*this, m->expr->variant());

        for (auto&& kase : m->cases) {
            ScopeGuard sg{this};
            if (match(value, *kase.pattern)) {
                bool guard = true;
                if (kase.guard) {
                    const Value cond = std::visit(*this, kase.guard->variant());

                    if (not std::holds_alternative<bool>(cond)) {
                        std::println(std::cerr, "In guard: {}", kase.guard->stringify());
                        error("Case guard must evaluate to a boolean. Got '" + typeOf(cond)->text() + "' instead!");
                    }

                    guard = get<bool>(cond);
                }

                if (guard) return std::visit(*this, kase.body->variant());
            }
        }


        error("Match expression didn't match any pattern:\n" + m->stringify());
    }

    Value operator()(const expr::Access *acc) {

        const auto& left = std::visit(*this, acc->var->variant());

        if (not std::holds_alternative<Object>(left)) error("Can't access a non-class type!");

        const auto& obj = std::get<Object>(left);

        const auto& found = std::ranges::find_if(obj.second->members, [&name = acc->name] (auto&& member) { return member.first.stringify() == name; });
        if (found == obj.second->members.end()) error("Name '" + acc->name + "' doesn't exist in object!");


        if (std::holds_alternative<expr::Closure>(found->second)) {
            const auto& closure = get<expr::Closure>(found->second);

            Environment capture_list;
            for (const auto& [name, value] : obj.second->members)
                capture_list[name.stringify()] = {value, typeOf(value)};

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


    static void checkNoSyntaxType(const std::vector<expr::ExprPtr>& funcs) noexcept {
        for (auto&& func : funcs) {
            const auto& closure = dynamic_cast<const expr::Closure*>(func.get());
            for (auto&& param_type : closure->type.params) {
                if (param_type->text() == "Syntax") error("Cannot have paramater of 'Syntax' in an overload set!");
            }
        }

        return;
    }

    static expr::Closure* resolveOverloadSet(
        const std::string& name,
        const std::vector<expr::ExprPtr>& funcs,
        const std::vector<type::TypePtr>& types
    ) {
        std::vector<expr::Closure*> set;

        for (auto&& func : funcs) {
            auto closure = dynamic_cast<expr::Closure*>(func.get());

            bool found = true;
            for (auto&& [arg_type, param_type] : std::views::zip(types, closure->type.params)) {
                if (not (*param_type >= *arg_type)) {
                    found = false;
                    break;
                }
            }

            if (found) set.push_back(closure);
        }


        if (set.size() > 1 and set[0]->params.size() == 1) {
            // remove the functions that take Any's to favour the concrete functions rather than "templates"
            std::erase_if(set, [] (const expr::Closure *e) { return e->type.params[0]->text() == "Any"; });
        }

        // for now
        if (set.size() != 1) error("Could not resolve overload set for operator: " + name);

        return set[0];
    }


    const Value& checkReturnType(const Value& ret, type::TypePtr return_type, const std::source_location& location = std::source_location::current()) {
        return_type = validateType(return_type);

        const auto& type_of_return_value = typeOf(ret);

        // auto return_type = ret_type;
        if (const auto& c = getVar(return_type->text()); c and std::holds_alternative<ClassValue>(c->first))
            return_type = std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(get<ClassValue>(c->first)));


        if (not (*return_type >= *type_of_return_value))
            error(
                "Type mis-match! Function return type expected: " +
                return_type->text() + ", got: " + type_of_return_value->text(),
                location
            );


        return ret;
    }

    Value operator()(const expr::UnaryOp *up) {
        if (auto&& var = getVar(up->stringify()); var) return var->first;


        const auto& op = ops.at(up->op);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            //* this could be dried out between all the OPs and function calls in general
            if (func->type.params[0]->text() == "Syntax") {
                // addVar(func->params.front(), up->expr->variant());
                //* maybe should use Syntax() instead of Any();
                args_env[func->params[0]] = {up->expr->variant(), func->type.params[0]}; //? fixed
            }
            else {
                func->type.params[0] = validateType(func->type.params[0]);

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
                args_env[func->params[0]] = {arg, func->type.params[0]}; //? fixed
            }
        }
        else { // do selection based on type
            checkNoSyntaxType(op->funcs);

            const auto arg  = std::visit(*this, up->expr->variant());
            auto type = typeOf(arg);
            type = validateType(type);

            func = resolveOverloadSet(op->OpName(), op->funcs, {type});

            args_env[func->params[0]] = {arg, func->type.params[0]}; //? fixed
        }


        ScopeGuard sg{this, args_env};

        return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }

    Value operator()(const expr::BinOp *bp) {
        if (auto&& var = getVar(bp->stringify()); var) return var->first;


        const auto& op = ops.at(bp->op);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            // LHS
            if (func->type.params[0]->text() == "Syntax") {
                // addVar(func->params[0], bp->lhs->variant());
                args_env[func->params[0]] = {bp->lhs->variant(), func->type.params[0]}; //? fixed
            }
            else {
                func->type.params[0] = validateType(func->type.params[0]);

                const auto& arg1 = std::visit(*this, bp->lhs->variant());
                if (auto&& type_of_arg = typeOf(arg1); not (*func->type.params[0] >= *type_of_arg))
                    error(
                        "Type mis-match! Infix operator '" + bp->op + 
                        "', parameter '" + func->params[0] +
                        "' expected: " + func->type.params[0]->text() +
                        ", got: " + stringify(arg1) + " which is " + type_of_arg->text()
                    );

                // addVar(func->params[0], arg1);
                args_env[func->params[0]] = {arg1, func->type.params[0]};
            }

            // RHS
            if (func->type.params[1]->text() == "Syntax") {
                // addVar(func->params[1], bp->rhs->variant());
                args_env[func->params[1]] = {bp->rhs->variant(), func->type.params[1]};
            }
            else {
                func->type.params[1] = validateType(func->type.params[1]);

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
                args_env[func->params[1]] = {arg2, func->type.params[1]};
            }

        }
        else {
            checkNoSyntaxType(op->funcs);

            const auto arg1  = std::visit(*this, bp->lhs->variant());
            auto type1 = typeOf(arg1);
            type1 = validateType(type1);

            const auto arg2  = std::visit(*this, bp->rhs->variant());
            auto type2 = typeOf(arg2);
            type2 = validateType(type2);

            func = resolveOverloadSet(op->OpName(), op->funcs, {type1, type2});

            args_env[func->params[0]] = {arg1, func->type.params[0]};
            args_env[func->params[1]] = {arg2, func->type.params[1]};
        }



        ScopeGuard sg{this, args_env};
        return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }


    Value operator()(const expr::PostOp *pp) {
        if (auto&& var = getVar(pp->stringify()); var) return var->first;


        const auto& op = ops.at(pp->op);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {

            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());


            if (func->type.params[0]->text() == "Syntax") {
                // addVar(func->params[0], pp->expr->variant());
                args_env[func->params[0]] = {pp->expr->variant(), func->type.params[0]}; //? fixed
            }
            else {
                func->type.params[0] = validateType(func->type.params[0]);

                const auto& arg = std::visit(*this, pp->expr->variant());
                if (auto&& type_of_arg = typeOf(arg); not (*func->type.params[0] >= *type_of_arg))
                    error(
                        "Type mis-match! Suffix operator '" + pp->op + 
                        "', parameter '" + func->params[0] +
                        "' expected: " + func->type.params[0]->text() +
                        ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                        // ", got: " + type_of_arg->text()
                    );

                args_env[func->params[0]] = {arg, func->type.params[0]}; //? fixed
            }

        }
        else {
            checkNoSyntaxType(op->funcs);

            const auto arg  = std::visit(*this, pp->expr->variant());
            auto type = typeOf(arg);
            type = validateType(type);

            func = resolveOverloadSet(op->OpName(), op->funcs, {type});

            args_env[func->params[0]] = {arg, func->type.params[0]};
        }

        ScopeGuard sg{this, args_env};
        return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }



    Value operator()(const expr::CircumOp *cp) {
        if (auto&& var = getVar(cp->stringify()); var) return var->first;

        // ScopeGuard sg{this};

        const auto& op = ops.at(cp->op1);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {

            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());


            if (func->type.params[0]->text() == "Syntax") {
                // addVar(func->params[0], co->expr->variant());
                args_env[func->params[0]] = {cp->expr->variant(), func->type.params[0]}; //? fixed
            }
            else {
                func->type.params[0] = validateType(func->type.params[0]);

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
                args_env[func->params[0]] = {arg, func->type.params[0]}; //? fixed
            }
        }
        else {
            checkNoSyntaxType(op->funcs);

            const auto arg  = std::visit(*this, cp->expr->variant());
            auto type = typeOf(arg);
            type = validateType(type);

            func = resolveOverloadSet(op->OpName(), op->funcs, {type});

            args_env[func->params[0]] = {arg, func->type.params[0]};
        }


        ScopeGuard sg{this, args_env};
        return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    };

    Value operator()(const expr::OpCall *oc) {
        if (auto&& var = getVar(oc->stringify()); var) return var->first;


        const auto& op = ops.at(oc->first);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {

            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            // this may be not needed
            // if (oc->exprs.size() != func->params.size()) error();

            for (auto [arg_expr, param, param_type] : std::views::zip(oc->exprs, func->params, func->type.params)) {
                if (param_type->text() == "Syntax") {
                    args_env[param] = {arg_expr->variant(), param_type}; //?
                }
                else {
                    param_type = validateType(param_type);

                    const auto& arg = std::visit(*this, arg_expr->variant());
                    if (auto&& type_of_arg = typeOf(arg); not (*param_type >= *type_of_arg)) {
                        const auto& op_name = oc->first + 
                            std::accumulate(oc->rest.cbegin(), oc->rest.cend(), std::string{}, [](auto&& acc, auto&& e) { return acc + ':' + e; });
                        error(
                            "Type mis-match! Operator '" + op_name + 
                            "', parameter '" + param +
                            "' expected: " + param_type->text() +
                            ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                        );
                    }

                    // addVar(func->params[0], std::visit(*this, co->expr->variant()));
                    args_env[param] = {arg, param_type}; //? fix Any Type!!
                }
            }
        }
        else {
            checkNoSyntaxType(op->funcs);

            std::vector<Value> args;
            std::vector<type::TypePtr> types;
            for (auto&& expr : oc->exprs) {
                args .push_back(std::visit(*this, expr->variant()));
                types.push_back(typeOf(args.back()));
                types.back() = validateType(types.back());
            }

            func = resolveOverloadSet(op->OpName(), op->funcs, std::move(types));

            for (auto&& [param, arg, type] : std::views::zip(func->params, args, func->type.params))
                args_env[param] = {arg, type};
        }


        ScopeGuard sg{this, args_env};
        return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }

    Value operator()(const expr::Call *call) {
        if (auto&& var = getVar(call->stringify()); var) return var->first;

        ScopeGuard sg{this};


        // using Arg = std::variant<expr::ExprPtr, Value>;
        auto args = call->args;
        std::vector<std::pair<size_t, std::vector<Value>>> expand_at;

        for (size_t i{}; i < args.size(); ++i) {
            if (const auto expand = dynamic_cast<const expr::Expansion*>(args[i].get())) {
                const auto pack = std::visit(*this, expand->pack->variant());

                if (not std::holds_alternative<PackList>(pack))
                    error("Expansion applied on a non-pack variable: " + args[i]->stringify());

                expand_at.push_back({i, get<PackList>(pack)->values});
                // args.erase(std::next(args.begin(), i--));
            }
        }

        const size_t args_size =
            args.size() +
            std::ranges::fold_left(expand_at, size_t{}, [] (auto&& acc, auto&& elt) { return acc + elt.second.size(); }) - // plus the expansions
            expand_at.size(); // minus redundant packs (already expanded)


        auto var = std::visit(*this, call->func->variant());

        if (std::holds_alternative<std::string>(var)) { // that dumb lol. but now it works
            const auto& name = std::get<std::string>(var);
            if (isBuiltIn(name)) return evaluateBuiltin(args, expand_at, call->named_args, name);
        }


        if (std::holds_alternative<expr::Closure>(var)) {
            const auto& func = std::get<expr::Closure>(var);

            // check for invalid named arguments
            for (auto&& [name, _] : call->named_args) {
                if (std::ranges::find(func.params, name) == func.params.end())
                    error("Named argument '" + name + "' does not name a parameter name!");
            }


            const bool is_variadic = std::ranges::any_of(func.type.params, type::isVariadic);
            // const size_t arity = func.params.size() - is_variadic; // don't count pack as a concrete param
            // const size_t args_size = call->args.size() + call->named_args.size(); 

            if (not is_variadic and args_size + call->named_args.size() > func.params.size()) error("Wrong arity call!");


            // * curry!
            if (args_size + call->named_args.size() < func.params.size() - is_variadic) {
                Environment argument_capture_list = func.env;

                for (auto&& [name, expr] : call->named_args) {
                    type::TypePtr type;
                    for (auto&& [n, t] : std::views::zip(func.params, func.type.params)) {
                        if (n == name) {
                            type = t;
                            break;
                        }
                    }

                    if (not type) error(); // should never happen anyway

                    Value value;
                    if (type->text() == "Syntax") value = expr->variant();
                    else {
                        value = std::visit(*this, expr->variant());

                        // if the param type not a super type of arg type, error out
                        if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                            error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                    }

                    argument_capture_list[name] = {std::move(value), std::move(type)};
                }


                std::vector<std::pair<std::string, type::TypePtr>> pos_params;
                for (auto&& [param, type] : std::views::zip(func.params, func.type.params)) pos_params.push_back({param, type});

                std::erase_if(pos_params, [&named_args = call->named_args] (auto&& p) {
                    return std::ranges::find_if(named_args, [&p] (auto&& n) { return n.first == p.first; }) != named_args.cend();
                });

                std::vector<std::string> new_params;
                for (auto&& [name, _] : pos_params | std::views::drop(args_size)) new_params.push_back(name);

                std::vector<type::TypePtr> new_types;
                for (auto&& name : new_params) {
                    type::TypePtr type;
                    for (auto&& [n, t] : std::views::zip(func.params, func.type.params)) {
                        if (n == name) {
                            type = t;
                            break;
                        }
                    }
                    new_types.push_back(std::move(type));
                }

                type::FuncType func_type{std::move(new_types), func.type.ret};
                expr::Closure closure{std::move(new_params),  std::move(func_type),  func.body};

                bool normal = true;

                if (is_variadic) {
                    const auto it = std::ranges::find_if(pos_params, [] (auto&& e) { return type::isVariadic(e.second); });
                    const size_t variadic_index = std::distance(pos_params.begin(), it);

                    // call->args.erase(std::next(call->args.begin(), variadic_index));

                    if (args_size > variadic_index) {
                        normal = false;

                        pos_params.erase(std::next(pos_params.begin(), variadic_index));

                        // we ignore the pack, so we consume an extra argument. Remove it from the carried function
                        closure.params.erase(closure.params.begin());
                        closure.type.params.erase(closure.type.params.begin());

                        for(size_t i{}, curr{}; auto&& [param, expr] : std::views::zip(pos_params, args)) {
                            auto&& [name, type] = param;
                            Value value;

                            if (curr < expand_at.size() and i == expand_at[curr].first) {
                                for (auto&& val : expand_at[curr++].second) {
                                    if (auto&& type_of_value = typeOf(val); not (*type >= *type_of_value))
                                        error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                                }
                            }
                            else {
                                if (type->text() == "Syntax") {
                                    value = expr->variant();
                                }
                                else {
                                    value = std::visit(*this, expr->variant());

                                    if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                                        error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                                }
                            }

                            ++i;
                            argument_capture_list[name] = {std::move(value), std::move(type)};
                        }
                    }
                }

                /* else */
                if (normal) for(size_t i{}, curr{}; auto&& [param, expr] : std::views::zip(pos_params, args)) {
                    auto&& [name, type] = param;
                    Value value;

                    if (curr < expand_at.size() and i == expand_at[curr].first) {
                        for (auto&& val : expand_at[curr++].second) {
                            if (auto&& type_of_value = typeOf(val); not (*type >= *type_of_value))
                                error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                        }
                    }
                    else {
                        if (type->text() == "Syntax") {
                            value = expr->variant();
                        }
                        else {
                            value = std::visit(*this, expr->variant());

                            // if the param type not a super type of arg type, error out
                            if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                                error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                        }
                    }

                    ++i;
                    argument_capture_list[name] = {std::move(value), std::move(type)};
                }


                closure.capture(argument_capture_list);
                return closure;
            }
            else { //* full call. Don't curry!

                Environment args_env;
                for (auto&& [name, expr] : call->named_args) {
                    type::TypePtr type;
                    for (auto&& [n, t] : std::views::zip(func.params, func.type.params)) {
                        if (n == name) {
                            type = t;
                            break;
                        }
                    }

                    if (not type) error(); //* should never happen anyway

                    Value value;
                    if (type->text() == "Syntax") value = expr->variant();
                    else {
                        value = std::visit(*this, expr->variant());

                        // if the param type not a super type of arg type, error out
                        if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                            error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                    }

                    args_env[name] = {std::move(value), std::move(type)};
                }


                std::vector<std::pair<std::string, type::TypePtr>> pos_params;
                for (auto&& [param, type] : std::views::zip(func.params, func.type.params)) pos_params.push_back({param, type});

                std::erase_if(pos_params, [&named_args = call->named_args] (auto&& p) {
                    return std::ranges::find_if(named_args, [&p] (auto&& n) { return n.first == p.first; }) != named_args.cend();
                });


                if (is_variadic) {
                    const auto it = std::ranges::find_if(pos_params, [] (auto&& e) { return type::isVariadic(e.second); });
                    const size_t variadic_index = std::distance(pos_params.begin(), it);

                    const auto pre_variadic = std::ranges::subrange(pos_params.begin(), it);
                    const auto post_variadic  = std::ranges::subrange(it + 1, pos_params.end());

                    const size_t pre_variadic_size = std::ranges::size(pre_variadic);
                    const size_t post_variadic_size = std::ranges::size(post_variadic);
                    const size_t variadic_size = args_size - pre_variadic_size - post_variadic_size;

                    // const auto pre_pack = std::ranges::subrange(args, std::ranges::size(pre_variadic));

                    // const auto variadic = std::ranges::subrange{
                    //     std::next(args.begin(), pre_variadic_size),
                    //     std::prev(args.end(), post_variadic_size)
                    // };

                    // if (variadic_size != std::ranges::size(variadic)) {
                    //     std::clog << "variadic_size: " << variadic_size << std::endl;
                    //     std::clog << "size(variadic): " << std::ranges::size(variadic) << std::endl;
                    // }
                    // assert(variadic_size == std::ranges::size(variadic));

                    // const auto post_pack = std::ranges::subrange{
                    //     std::prev(args.end(), post_variadic_size),
                    //     args.end()
                    // };

                    // *
                    auto pack = makePack();
                    for (
                        size_t arg_index{}, param_index{}, pack_index{}, curr_expansion{};
                        arg_index < args.size(); // can't be args_size since arg_index is only used to index into args
                    ) {
                        auto&& [name, type] = pos_params[param_index];
                        Value value;

                        if (param_index == variadic_index){
                            for (size_t i{}; i < variadic_size; ++i) {

                                if (curr_expansion < expand_at.size() and arg_index == expand_at[curr_expansion].first) {
                                    value = std::move(expand_at[curr_expansion].second[pack_index++]);

                                    if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                                        error("Type mis-match! Parameter '" + name
                                            + "' expected type: "+ type->text() 
                                            + ", got: " + type_of_value->text()
                                        );

                                    if (pack_index >= expand_at[curr_expansion].second.size()) {
                                        ++arg_index;
                                        ++curr_expansion;
                                        pack_index = 0;
                                    }
                                }
                                else {
                                    auto&& expr = args[arg_index];

                                    if (type->text() == "Syntax") {
                                        error(); //* remove!
                                        // value = expr->variant();
                                    }
                                    else {
                                        value = std::visit(*this, expr->variant());

                                        // if the param type not a super type of arg type, error out
                                        if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                                            error("Type mis-match! Parameter '" + name
                                                + "' expected type: "+ type->text() 
                                                + ", got: " + type_of_value->text()
                                            );
                                    }

                                    ++arg_index;
                                }

                                pack->values.push_back(std::move(value));
                            }

                            ++param_index;
                            args_env[name] = {std::move(pack), std::move(type)};
                        }
                        else {
                            if (curr_expansion < expand_at.size() and arg_index == expand_at[curr_expansion].first) {
                                value = expand_at[curr_expansion].second[pack_index++];

                                if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                                    error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());

                                if (pack_index >= expand_at[curr_expansion].second.size()) {
                                    ++arg_index;
                                    ++curr_expansion;
                                    pack_index = 0;
                                }
                            }
                            else {
                                auto&& expr = args[arg_index];

                                if (type->text() == "Syntax") {
                                    value = expr->variant();
                                }
                                else {
                                    value = std::visit(*this, expr->variant());

                                    if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                                        error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                                }

                                ++arg_index;
                            }

                            ++param_index;
                            args_env[name] = {std::move(value), std::move(type)};
                        }
                    }
                }
                // else for(size_t i{}, curr{}; auto&& [param, expr] : std::views::zip(pos_params, args)) {
                else for (size_t i{}, p{}, curr{}; p < args_size; ++p, ++i) {

                    if (curr < expand_at.size() and i == expand_at[curr].first) {
                        for (auto&& val : expand_at[curr++].second) {
                            auto&& [name, type] = pos_params[p];

                            if (auto&& type_of_value = typeOf(val); not (*type >= *type_of_value))
                                error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());

                            ++p;
                            args_env[name] = {std::move(val), std::move(type)};
                        }
                        --p;
                    }
                    else {
                        auto&& [name, type] = pos_params[p];
                        auto&& expr = args[i];
                        Value value;

                        if (type->text() == "Syntax") {
                            value = expr->variant();
                        }
                        else {
                            value = std::visit(*this, expr->variant());

                            // if the param type not a super type of arg type, error out
                            if (auto&& type_of_value = typeOf(value); not (*type >= *type_of_value))
                                error("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                        }

                        args_env[name] = {std::move(value), std::move(type)};
                    }
                }



                //* should I capture the env and bundle it with the function before returning it?
                if (func.type.ret->text() == "Syntax") return func.body->variant();

                ScopeGuard sg1{this, func.env}; // in case the lambda had captures
                ScopeGuard sg2{this, args_env};

                const auto& ret = std::visit(*this, func.body->variant());

                checkReturnType(ret, func.type.ret);

                if (std::holds_alternative<expr::Closure>(ret)) {
                    const auto& closure = get<expr::Closure>(ret);

                    closure.capture(args_env);   // to capture the arguments of the enclosing functions
                    closure.capture(func.env);   // copy the environment of the parent function..

                    closure.capture(env.back()); // in case any argument was reassigned
                    // capturing env.back() after args_env ensures the latest binding gets chosen

                }

                return ret;
            }
        }

        else if (std::holds_alternative<ClassValue>(var)) {
            // if (not call->args.empty()) error("Can't pass arguments to classes!");
            const auto& cls = std::get<ClassValue>(var);

            if (call->args.size() > cls.blueprint->members.size())
                error("Too many arguments passed to constructor of class: " + stringify(cls));

            // copying defaults field values from class definition
            Object obj{cls, std::make_shared<Dict>(cls.blueprint->members) };

            // I woulda used a range for-loop but I need `arg` to be a reference and `value` to be a const ref
            // const auto& [arg, value] : std::views::zip(call->args, obj->members)
            for (size_t i{}; i < call->args.size(); ++i) {
                const auto& v = std::visit(*this, call->args[i]->variant());

                if (not (*obj.second->members[i].first.type >= *typeOf(v)))
                    error(
                        "Type mis-match in constructor of:\n" + stringify(cls) + "\nMember `" +
                        obj.second->members[i].first.stringify() + "` expected: " + obj.second->members[i].first.type->text() +
                        "\nbut got: " + call->args[i]->stringify() + " which is " + typeOf(v)->text()
                    );

                obj.second->members[i].second = v;
            }

            return obj;
        }


        if (call->args.size() != 0) error("Can't pass arguments to values!");
        return var;
    }

    Value operator()(const expr::Closure *c) {
        if (auto&& var = getVar(c->stringify()); var) return var->first;

        expr::Closure closure = *c; // copy to use for fix the types

        // take a snapshot of the current env (capture by value). comment this line if you want capture by reference..
        // closure.capture(envStackToEnvMap());


        for (auto& type : closure.type.params) {
            type = validateType(type);
            // // if(not validate(name->type)) error("Invalid Type: " + name->type->text());

            // replacing expressions that represnt types
            if (auto&& cls = getVar(type->text()); cls and std::holds_alternative<ClassValue>(cls->first))
                // type = std::make_shared<type::VarType>(std::make_shared<expr::Name>(stringify(get<ClassValue>(clos->first))));
                type = std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(get<ClassValue>(cls->first)));
        }

        if (auto&& return_type = getVar(closure.type.ret->text());
            return_type and std::holds_alternative<ClassValue>(return_type->first)
        )
        closure.type.ret =
            // std::make_shared<type::VarType>(std::make_shared<expr::Name>(stringify(get<ClassValue>(return_type->first))));
            std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(get<ClassValue>(return_type->first)));


        return closure;
    }


    Value operator()(const expr::Block *block) {
        if (auto&& var = getVar(block->stringify()); var) return var->first;


        ScopeGuard sg{this};

        Value ret;
        for (const auto& line : block->lines)
            ret = std::visit(*this, line->variant()); // a scope's value is the last expression



        if (std::holds_alternative<expr::Closure>(ret)) {
            const auto& func = get<expr::Closure>(ret);
            func.capture(env.back());
        }

        return ret;
    }


    Value operator()(const expr::Fix *fix) {
        if (auto&& var = getVar(fix->stringify()); var) return var->first;
        // return std::visit(*this, fix->func->variant());
        ops.at(fix->name)->funcs.push_back(fix->funcs[0]); // assuming each fix expression has a single func in it

        return std::visit(*this, fix->funcs[0]->variant());
    }


    [[nodiscard]] bool isBuiltIn(const std::string_view func) const noexcept {
        const auto make_builtin = [] (const std::string& n) { return "__builtin_" + n; };

        for(const auto& builtin : {
            "print", "concat", //* variadic
            "true", "false", "input_str", "input_int", //* nullary
            "pack_len", "reset", "eval","neg", "not",     //* unary
            "add", "sub", "mul", "div", "mod", "pow", "gt", "geq", "eq", "leq", "lt", "and", "or",  //* binary
            "conditional" //* trinary
        })
            if (func == make_builtin(builtin)) return true;

        return false;
    }


    // the gate into the META operators!
    Value evaluateBuiltin(
        const std::vector<expr::ExprPtr>& args,
        const std::vector<std::pair<size_t, std::vector<Value>>>& expand_at,
        const std::unordered_map<std::string, expr::ExprPtr>& named_args,
        std::string name
    ) {
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
            MapEntry<
                S<"input_str">,
                Func<"input_str",
                    decltype([](const auto&) {
                        std::string out;
                        std::getline(std::cin, out);
                        return out;
                    }),
                    void
                >
            >{},
            MapEntry<
                S<"input_int">,
                Func<"input_int",
                    decltype([](const auto&) {
                        std::string out;
                        std::getline(std::cin, out);
                        if (not std::ranges::all_of(out, isdigit)) error("'__builtin_input_int' recieved a non-int \"" + out + "\"!");

                        return std::stoll(out);
                    }),
                    void
                >
            >{},

            //* UNARY FUNCTIONS
            MapEntry<
                S<"neg">,
                Func<"neg",
                    decltype([](auto&& x, const auto&) { return -x; }),
                    TypeList<ssize_t>,
                    TypeList<double>
                >
            >{},

            MapEntry<
                S<"not">,
                Func<"not",
                    decltype([](auto&& x, const auto&) { return not x; }),
                    // TypeList<ssize_t>,
                    // TypeList<double>,
                    TypeList<bool>
                >
            >{},

            MapEntry<
                S<"eval">,
                Func<"eval",
                    decltype([](auto&& x, const auto& that) {
                        return std::visit(*that, x);
                    }),
                    TypeList<expr::Node>
                >
            >{},

            MapEntry<
                S<"pack_len">,
                Func<"pack_len",
                    decltype([](auto&& pack, const auto&) {
                        return static_cast<ssize_t>(pack->values.size()); // have to narrow to `int` since I don't support any other integer type (nor big int)
                    }),
                    TypeList<PackList>
                >
            >{},


            //* BINARY FUNCTIONS
            MapEntry<
                S<"add">,
                Func<"add",
                    decltype([](auto&& a, auto&& b, const auto&) { return a + b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"sub">,
                Func<"sub",
                    decltype([](auto&& a, auto&& b, const auto&) { return a - b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"mul">,
                Func<"mul",
                    decltype([](auto&& a, auto&& b, const auto&) { return a * b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"div">,
                Func<"div",
                    decltype([](auto&& a, auto&& b, const auto&) { return a / b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"mod">,
                Func<"mod",
                    decltype([](auto&& a, auto&& b, const auto&) { return a % b; }),
                    TypeList<ssize_t, ssize_t>
                >
            >{},

            MapEntry<
                S<"pow">,
                Func<"pow",
                    decltype(
                        [](auto&& a, auto&& b, const auto&) -> std::common_type_t<decltype(a), decltype(b)> { return std::pow(a, b); }
                    ),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"gt">,
                Func<"gt",
                    decltype([](auto&& a, auto&& b, const auto&) { return a > b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"geq">,
                Func<"geq",
                    decltype([](auto&& a, auto&& b, const auto&) { return a >= b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"eq">,
                Func<"eq",
                    decltype([](auto a, auto b, const auto&) { return a == b; }),
                    TypeList<Any, Any>
                >
            >{},

            MapEntry<
                S<"leq">,
                Func<"leq",
                    decltype([](auto&& a, auto&& b, const auto&) { return a <= b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"lt">,
                Func<"lt",
                    decltype([](auto&& a, auto&& b, const auto&) { return a < b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{}
        );



        name = name.substr(10); // cutout the "__builtin_"

        const auto arity_check = [&name, &args] (const size_t arity) {
            if (args.size() != arity) error("Wrong arity with call to \"__builtin_" + name + "\"");
        };

        // const auto int_check = [&name] (const Value& val) {
        //     if (not std::holds_alternative<ssize_t>(val)) error("Wrong argument passed to to \"__builtin_" + name + "\"");
        // };

        if (name == "true" or name == "false") arity_check(0);
        if (name == "true")  return execute<0>(stdx::get<S<"true">>(functions).value, {}, this);
        if (name == "false") return execute<0>(stdx::get<S<"false">>(functions).value, {}, this);
        if (name == "input_str") return execute<0>(stdx::get<S<"input_str">>(functions).value, {}, this);
        if (name == "input_int") return execute<0>(stdx::get<S<"input_int">>(functions).value, {}, this);



        // for now, can only implement variadic functions inlined in this function
        if (name == "print") {
            if (args.empty()) error("'print' requires at least 1 argument passed!");

            using std::operator""sv;
            const auto allowed_params = {"sep"sv, "end"sv};

            for (auto&& [name, _] : named_args)
                if (std::ranges::find(allowed_params, name) == allowed_params.end())
                    error("Can only have the named argument 'end'/'sep' in call to '__builtin_print': found '" + name + "'!");


            const Value sep = named_args.contains("sep") ? std::visit(*this, named_args.at("sep")->variant()) : " ";

            constexpr bool no_newline = false;

            std::optional<Value> separator;
            Value ret;
            for(size_t i{}, curr{}; auto& arg : args) {
                if (curr < expand_at.size() and i++ == expand_at[curr].first) {
                    for (auto&& e : expand_at[curr++].second) {
                        if (separator) print(*separator, no_newline);

                        print(e, no_newline);

                        if (not separator) separator = sep;
                    }
                }
                else {
                    if (separator) print(*separator, no_newline);

                    ret = std::visit(*this, arg->variant());
                    print(ret, no_newline);

                    if (not separator) separator = sep;
                }
            }

            if (named_args.contains("end"))
                 print(std::visit(*this, named_args.at("end")->variant()), no_newline);
            else puts(""); // print the new line in the end.

            return ret;
        }

        if (name == "concat") {
            if (args.size() < 2) error("'concat' requires at least 2 argument passed!");

            std::string s;
            for(const auto& arg : args) {
                const Value& v = std::visit(*this, arg->variant());
                if (not std::holds_alternative<std::string>(v)) error("'concat' only accepts strings as arguments: " + stringify(v));

                s += get<std::string>(v);
            }

            return s;
        }



        if (name == "neg" or name == "not" or name == "reset") arity_check(1); // just for now..


        // evaluating arguments from left to right as needed
        // first argument is always evaluated
        const auto& value1 = std::visit(*this, args[0]->variant());

        // Since this is a meta function that operates on AST nodes rather than values
        // it gets its special treatment here..
        if (name == "reset") {
            const std::string& s = args[0]->stringify();
            if (const auto& v = getVar(s); not v) error("Reseting an unset value: " + s);
            else removeVar(s);

            // return to_bigint(num->num);
            return value1;
        }



        if (name == "eval" )    return execute<1>(stdx::get<S<"eval"     >>(functions).value, {value1}, this);
        if (name == "neg"  )    return execute<1>(stdx::get<S<"neg"      >>(functions).value, {value1}, this);
        if (name == "not"  )    return execute<1>(stdx::get<S<"not"      >>(functions).value, {value1}, this);
        if (name == "pack_len") return execute<1>(stdx::get<S<"pack_len" >>(functions).value, {value1}, this);


        // all the rest of those funcs expect 2 arguments

        using std::operator""sv;

        const auto eager = {"add"sv, "sub"sv, "mul"sv, "div"sv, "mod"sv, "pow"sv, "gt"sv, "geq"sv, "eq"sv, "leq"sv, "lt"sv};
        if (std::ranges::find(eager, name) != eager.end()) {
            arity_check(2);
            const auto& value2 = std::visit(*this, args[1]->variant());

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


            return std::visit(*this, args[1]->variant()); // last truthy value
        }

        if (name == "or" ) {
            arity_check(2);
            if (not std::holds_alternative<bool>(value1)) return std::visit(*this, args[1]->variant()); // last falsey value


            if(get<bool>(value1)) return value1; // first truthy value
            return std::visit(*this, args[1]->variant()); // last falsey value
        }


        if (name == "conditional") {
            arity_check(3);
            const auto& then      = args[1]->variant();
            const auto& otherwise = args[2]->variant();

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


    type::TypePtr validateType(type::TypePtr type) noexcept {

        //* comment this if statement if you want builtin types to remain unchanged even when they're assigned to
        if (auto&& var = getVar(type->text()); var) {
            if (typeOf(var->first)->text() != "Type") error("'" + stringify(var->first) + "' does not name a type!");

            return std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(std::get<ClassValue>(var->first)));
        }

        if (const auto var_type = dynamic_cast<type::VarType*>(type.get())) {
            if (type::isBuiltin(type)) return type;

            auto&& value = std::visit(*this, var_type->t->variant());

            if (std::holds_alternative<ClassValue>(value))
                return std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(std::get<ClassValue>(value)));
        }

        else if (dynamic_cast<type::LiteralType*>(type.get())) return type;

        else if (type::isFunction(type)) {
            const auto func_type = dynamic_cast<type::FuncType*>(type.get());

            for (auto& t : func_type->params) t = validateType(t);


            // all param types are valid. Only thing left to check is return type
            func_type->ret = validateType(func_type->ret);

            return type;
        }
        else if (type::isVariadic(type)) {
            const auto variadic_type = dynamic_cast<type::VariadicType*>(type.get());

            variadic_type->type = validateType(variadic_type->type);

            return type;
        }


        error("'" + type->text() + "' does not name a type!");
    }


    type::TypePtr typeOf(const Value& value) noexcept {
        if (std::holds_alternative<expr::Node>(value))   return type::builtins::Syntax();
        if (std::holds_alternative<ssize_t>(value))      return type::builtins::Int();
        if (std::holds_alternative<double>(value))       return type::builtins::Double();
        if (std::holds_alternative<bool>(value))         return type::builtins::Bool();
        if (std::holds_alternative<std::string>(value))  return type::builtins::String();
        if (std::holds_alternative<ClassValue>(value))   return type::builtins::Type();

        if (std::holds_alternative<expr::Closure>(value)) {
            const auto& func = get<expr::Closure>(value);

            type::FuncType type{{}, {}};
            for (const auto& t : func.type.params)
                type.params.push_back(t);


            type.ret = func.type.ret;

            return std::make_shared<type::FuncType>(std::move(type));
        }

        if (std::holds_alternative<Object>(value)) {
            // return std::make_shared<type::VarType>("class" + stringify(value).substr(6)); // skip the "Object " and add "class"

            // std::string s = stringify(get<Object>(value).first);

            return std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(get<Object>(value).first));
            // return std::make_shared<type::VarType>(std::make_shared<expr::Name>(std::move(s)));
            // return std::make_shared<type::VarType>(std::make_shared<expr::Name>("class" + stringify(value).substr(6)));
        }

        if (std::holds_alternative<PackList>(value)) {
            auto values = std::ranges::fold_left(
                get<PackList>(value)->values,
                std::vector<type::TypePtr>{},
                [this] (auto&& acc, auto&& elt) {
                    acc.push_back(typeOf(elt));
                    return acc;
                }
            );

            auto non_typed_pack = std::make_shared<type::VariadicType>(type::builtins::_());
            if (values.empty()) return non_typed_pack;

            const bool same = std::ranges::all_of(values, [tp = values[0]] (auto&& t) { return *t == *tp; });

            if (same) return std::make_shared<type::VariadicType>(values[0]);
            else return non_typed_pack;

            // return same ? std::make_shared<type::VariadicType>(values[0]) : non_typed_pack;
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

    bool changeVar(const std::string& name, const Value& v) {
        for (auto rev_it = env.rbegin(); rev_it != env.rend(); ++rev_it)
            if (rev_it->contains(name)) {
                const auto& t = rev_it->at(name).second;
                (*rev_it)[name] = {v, t};
                return true;
            }

        return false;
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

