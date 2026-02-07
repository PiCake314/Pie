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

#include <cassert>
#include <cctype>

#include <stdx/tuple.hpp>


// #include "../Declarations.hxx"
#include "../Utils/utils.hxx"
#include "../Utils/Exceptions.hxx"
#include "../Utils/ConstexprLookup.hxx"
#include "../Lex/Lexer.hxx"
#include "../Expr/Expr.hxx"
#include "../Type/Type.hxx"
#include "../Parser/Parser.hxx"

#include "Value.hxx"

inline namespace pie {

namespace interp {

struct Visitor {

    enum class EnvTag {
        NONE,
        FUNC,
        SCOPE,
    };

    std::vector<std::pair<Environment, EnvTag>> env;
    Operators ops;

    // _this_ (or self) context
    std::vector<Object> selves{};


    // loop context
    ssize_t loop_counter{};
    bool broken{}, continued{};


    // v_table ahh name
    std::unordered_map<std::string, std::vector<size_t>> co_map;


    Visitor(Operators ops = {}) noexcept : env(1), ops{std::move(ops)} { }

    void addOperators(Operators os) {
        // ops.insert(os.begin(), os.end());
        // ops.merge(std::move(os));

        for (auto& [name, fix] : os) {
            if (ops.contains(name)) {
                for (auto& func : fix->funcs) {
                    ops.at(name)->funcs.push_back(std::move(func));
                }
            }
            else ops[std::move(name)] = std::move(fix);
        }
    }


    Value operator()(const expr::Num *n) {
        if (const auto& var = getVar(n->stringify()); var) return var->first;


        // have to do an if rather than ternary so the return value isn't always coerced into doubles
        if (n->num.find('.') != std::string::npos) return std::stod(n->num);
        else return std::stoll(n->num);
    }

    Value operator()(const expr::Bool *b) {
        if (const auto& var = getVar(b->stringify()); var) return var->first;

        return b->boo;
    }

    Value operator()(const expr::String *s) {
        if (const auto& var = getVar(s->stringify()); var) return var->first;

        return s->str;
    }


    std::optional<Value> checkThis(const std::string& name) {
        if (selves.empty()) return {};

        for (const auto& [member, _, value] : selves.back().second->members) {
            if (member.name == name) return *value;
        }

        return {};
    }

    void changeThis(const std::string& name, Value val) {
        if (selves.empty()) error();

        for (auto& [member, _, value] : selves.back().second->members) {
            if (member.name == name) {
                *value = val;
                return;
            }
        }

        error("Name '" + name + "' not found in object: " + stringify(selves.back()));
    }

    Value operator()(const expr::Name *n) {
        // what should builtins evaluate to?
        // If I return the string back, then expressions like `"__builtin_neg"(1)` are valid now :))))
        // interesting!
        // how about a special value?

        if (const auto& var = getVar(n->name); var) return var->first;
        if (const auto var = checkThis(n->name); var) return *var;
        if (n->name == "self" and not selves.empty()) return selves.back();

        // for now, buitlin functions just return their names as strings...
        // maybe i need to return some builtin type or smth. IDK
        if (isBuiltin(n->name)) return n->name;


        if (n->name == "Any"    ) return type::builtins::Any   ();
        if (n->name == "Int"    ) return type::builtins::Int   ();
        if (n->name == "Double" ) return type::builtins::Double();
        if (n->name == "String" ) return type::builtins::String();
        if (n->name == "Bool"   ) return type::builtins::Bool  ();
        if (n->name == "Type"   ) return type::builtins::Type  ();
        if (n->name == "Syntax" ) return type::builtins::Syntax();


        error("Name \"" + n->name + "\" is not defined!");
    }


    // Value operator()(const expr::Pack*) { error(); }
    Value operator()(const expr::Expansion*) { error("Can only expand in function calls or fold expressions!"); }


    Value operator()(const expr::List* list) {
        if (const auto& var = getVar(list->stringify()); var) return var->first;

        std::vector<Value> values;
        std::transform(
            list->elements.cbegin(), list->elements.cend(), std::back_inserter(values),
            [this] (const auto& expr) { return std::visit(*this, expr->variant()); }
        );

        return makeList(std::move(values));
    }

    Value operator()(const expr::Map* map) {
        MapValue map_value{std::make_shared<Items>()};

        for (auto [key, expr] : map->items) {
            // evaluating key here first instead of at function arguments
            // because functions arguments are indeterminantly evaluated

            auto key_value = std::visit(*this, std::move(key)->variant());
            map_value.items->map.insert_or_assign(
                std::move(key_value),
                std::visit(*this, std::move(expr)->variant())
            );
        }

        return map_value;
    }


    value::Value typeCheck(Value value, const type::TypePtr& type, std::string err_msg = "", const std::source_location& location = std::source_location::current()) {
        const auto value_type = typeOf(value);
        if (err_msg.empty()) err_msg = "Expected type '" + type->text() + "', got type '" + value_type->text() + '\'';

        if (not type->typeCheck(this, value, value_type)) error<except::TypeMismatch>(err_msg, location);

        if (const auto cls = type::isClass(type)) {
            auto obj = get<value::Object>(value);
            obj.second = std::make_shared<Members>(obj.second->members);

            std::erase_if(
                obj.second->members,
                [cls] (const auto& member) {
                    const auto& [name, _, __] = member;

                    return std::ranges::find_if(cls->cls->blueprint->members, [&name](const auto& field) {
                        return get<expr::Name>(field).name == name.name;
                    }) == cls->cls->blueprint->members.cend();
                }
            );
            obj.first = type;

            return obj;
        }

        return value;
    }


    Value operator()(const expr::UnaryFold *fold) {
        if (const auto& var = getVar(fold->stringify()); var) return var->first;


        Value pack = std::visit(*this, fold->pack->variant());

        if (not std::holds_alternative<PackList>(pack)) error("Folding over a non-pack: " + stringify(pack));

        auto& packlist = get<PackList>(pack);

        if (packlist->values.empty()) error("Folding over an empty pack: " + fold->stringify());
        if (packlist->values.size() == 1) return packlist->values[0];


        Value ret = fold->left_to_right ? packlist->values.front() : packlist->values.back(); // [packlist->values.size() - 2];
        const auto values = fold->left_to_right?
            packlist->values |                       std::views::drop(1) | std::views::as_rvalue | std::ranges::to<std::vector<Value>>():
            packlist->values | std::views::reverse | std::views::drop(1) | std::views::as_rvalue | std::ranges::to<std::vector<Value>>();

        const auto& op = ops.at(fold->op);

        // can't have any syntax type since the pack consists of values, not expressions..
        // unless...!
        // TODO: allow for folding over syntax...maybe
        checkNoSyntaxType(op->funcs);

        expr::Closure* func;

        const size_t  first_idx = 1 - fold->left_to_right;
        const size_t second_idx =     fold->left_to_right;

        // no overload resolution required
        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            func->type.ret =                validateType(std::move(func)->type.ret               );
            func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
            func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


            for (const auto& value : values) {

                // these two lines could be dried out...
                // along with at least 7 more instances of the same line scattered througout the codebase

                // if (const auto& type_of_arg = typeOf(ret); not (*func->type.params[first_idx] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                //         "', parameter '" + func->params[0] +
                //         "' expected: " + func->type.params[0]->text() +
                //         ", got: " + stringify(ret) + " which is " + type_of_arg->text()
                //     );


                typeCheck(ret, func->type.params[first_idx],
                    "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
                );

                // if (const auto& type_of_arg = typeOf(value); not (*func->type.params[second_idx] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                //         "', parameter '" + func->params[1] +
                //         "' expected: " + func->type.params[1]->text() +
                //         ", got: " + stringify(ret) + " which is " + type_of_arg->text()
                //     );


                typeCheck(value, func->type.params[second_idx], 
                    "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                    "', parameter '" + func->params[1] +
                    "' expected: " + func->type.params[1]->text() +
                    ", got: " + stringify(value) + " which is " + typeOf(value)->text()
                );

                Environment args_env;
                args_env[func->params[ first_idx]] = {std::make_shared<Value>(ret  ), func->type.params[ first_idx]};
                args_env[func->params[second_idx]] = {std::make_shared<Value>(value), func->type.params[second_idx]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
            }
        }
        else { // fuck me
            checkNoSyntaxType(op->funcs);

            for (const auto& value : values) {

                // if the overload set is resolved, no need to type check again...i think!!!
                func = resolveOverloadSet(
                    op->OpName(), op->funcs,
                    {ret, value}
                    // {validateType(typeOf(ret)), validateType(typeOf(value))},
                );
                func->type.ret                = validateType(std::move(func)->type.ret               );
                func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
                func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


                Environment args_env;
                args_env[func->params[1 - fold->left_to_right]] = {std::make_shared<Value>(ret)  , func->type.params[1 - fold->left_to_right]};
                args_env[func->params[    fold->left_to_right]] = {std::make_shared<Value>(value), func->type.params[    fold->left_to_right]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
            }
        }

        return ret;
    }


    Value operator()(const expr::SeparatedUnaryFold *fold) {
        if (const auto& var = getVar(fold->stringify()); var) return var->first;


        Value lhs = std::visit(*this, fold->lhs->variant());
        Value rhs = std::visit(*this, fold->rhs->variant());


        const auto l_pack = std::holds_alternative<PackList>(lhs), l2r = l_pack;
        const auto r_pack = std::holds_alternative<PackList>(rhs);


        if (l_pack == r_pack) {
            std::string err = l_pack ? "Folding over 2 packs: '" : "Folding over non-packs: '";
            error(err + fold->lhs->stringify() + "' and '" + fold->rhs->stringify() + '\'');
        }

        Value pack = l_pack? std::move(lhs) : std::move(rhs);
        Value sep  = r_pack? std::move(lhs) : std::move(rhs);

        auto& packlist = get<PackList>(pack);

        if (packlist->values.empty()) error("Folding over an empty pack: " + fold->stringify());
        if (packlist->values.size() == 1) return packlist->values[0];


        Value ret;
        std::vector<Value> values;
        values.reserve(packlist->values.size() * 2 - 1);
        if (l2r) {
            ret = std::move(packlist)->values[0];

            for (auto& value : packlist->values | std::views::drop(1)) {
                values.push_back(sep);
                values.push_back(std::move(value));
            }
        }
        else {
            ret = sep;

            const auto len = packlist->values.size();

            values.push_back(std::move(packlist)->values[len - 1]);
            values.push_back(std::move(packlist)->values[len - 2]);

            for (auto& value : packlist->values | std::views::reverse | std::views::drop(2)) {
                values.push_back(sep);
                values.push_back(std::move(value));
            }
        }



        const auto& op = ops.at(fold->op);

        // can't have any syntax type since the pack consists of values, not expressions..
        // unless...!
        // TODO: allow for folding over syntax...maybe
        checkNoSyntaxType(op->funcs);

        expr::Closure* func;

        const size_t  first_idx = 1 - l2r;
        const size_t second_idx =     l2r;

        // no overload resolution required
        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());
            func->type.ret                = validateType(std::move(func)->type.ret               );
            func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
            func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);



            for (const auto& value : values) {

                typeCheck(ret, func->type.params[first_idx],
                    "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
                );

                // // these two lines could be dried out...
                // // along with at least 7 more instances of the same line scattered througout the codebase
                // if (const auto& type_of_arg = typeOf(ret); not (*func->type.params[first_idx] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                //         "', parameter '" + func->params[0] +
                //         "' expected: " + func->type.params[0]->text() +
                //         ", got: " + stringify(ret) + " which is " + type_of_arg->text()
                //     );


                typeCheck(ret, func->type.params[second_idx],
                    "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                    "', parameter '" + func->params[1] +
                    "' expected: " + func->type.params[1]->text() +
                    ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
                );

                // if (const auto& type_of_arg = typeOf(value); not (*func->type.params[second_idx] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                //         "', parameter '" + func->params[1] +
                //         "' expected: " + func->type.params[1]->text() +
                //         ", got: " + stringify(ret) + " which is " + type_of_arg->text()
                //     );


                Environment args_env;
                args_env[func->params[ first_idx]] = {std::make_shared<Value>(ret)  , func->type.params[ first_idx]};
                args_env[func->params[second_idx]] = {std::make_shared<Value>(value), func->type.params[second_idx]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
            }
        }
        else { // fuck me
            checkNoSyntaxType(op->funcs);

            for (const auto& value : values) {

                // auto type1 = validateType(typeOf(ret  ));
                // auto type2 = validateType(typeOf(value));

                func = resolveOverloadSet(op->OpName(), op->funcs, {ret, value});
                // I think these lines are needed. Have to check 
                func->type.ret                = validateType(std::move(func)->type.ret             );
                func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
                func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


                Environment args_env;
                args_env[func->params[ first_idx]] = {std::make_shared<Value>(ret)  , func->type.params[ first_idx]};
                args_env[func->params[second_idx]] = {std::make_shared<Value>(value), func->type.params[second_idx]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
            }
        }

        return ret;
    }


    Value operator()(const expr::BinaryFold *fold) {
        if (const auto& var = getVar(fold->stringify()); var) return var->first;


        Value pack = std::visit(*this, fold->pack->variant());
        Value init = std::visit(*this, fold->init->variant());

        auto& packlist = get<PackList>(pack);

        if (packlist->values.empty()) return init;


        // ! check this line later
        // Value ret = fold->left_to_right ? std::move(init) : packlist->values.back();
        Value ret = std::move(init);

        // std::vector<Value> values = std::move(packlist)->values;

        // if (not fold->left_to_right) std::ranges::reverse(packlist->values);

        std::vector<Value> values;
        if (fold->left_to_right) {
            if (fold->sep) {
                const auto sep = std::visit(*this, fold->sep->variant());

                for (auto& value : packlist->values) {
                    values.push_back(sep);
                    values.push_back(std::move(value));
                }
            }
            else for (auto& value : packlist->values) values.push_back(std::move(value));
        }
        else {
            if (fold->sep) {
                const auto sep = std::visit(*this, fold->sep->variant());

                for (auto& value : packlist->values) {
                    values.push_back(std::move(value));
                    values.push_back(sep);
                }
            }
            else for (auto& value : packlist->values) values.push_back(std::move(value));

            std::ranges::reverse(values);
        }


        const auto& op = ops.at(fold->op);

        // can't have any syntax type since the pack consists of values, not expressions..
        // unless...!
        // TODO: allow for folding over syntax...maybe
        checkNoSyntaxType(op->funcs);

        expr::Closure* func;

        const auto  first_idx = 1 - fold->left_to_right;
        const auto second_idx =     fold->left_to_right;

        // no overload resolution required
        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());
            func->type.ret                = validateType(std::move(func)->type.ret               );
            func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
            func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


            for (Environment args_env; const auto& value : values) {

                typeCheck(ret, func->type.params[first_idx],
                    "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
                );

                // // these two lines could be dried out...
                // // along with at least 7 more instances of the same line scattered througout the codebase
                // if (const auto& type_of_arg = typeOf(ret); not (*func->type.params[first_idx] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                //         "', parameter '" + func->params[0] +
                //         "' expected: " + func->type.params[0]->text() +
                //         ", got: " + stringify(ret) + " which is " + type_of_arg->text()
                //     );



                typeCheck(ret, func->type.params[second_idx],
                        "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                        "', parameter '" + func->params[1] +
                        "' expected: " + func->type.params[1]->text() +
                        ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
                );

                // if (const auto& type_of_arg = typeOf(value); not (*func->type.params[second_idx] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                //         "', parameter '" + func->params[1] +
                //         "' expected: " + func->type.params[1]->text() +
                //         ", got: " + stringify(ret) + " which is " + type_of_arg->text()
                //     );



                args_env[func->params[ first_idx]] = {std::make_shared<Value>(ret)  , func->type.params[ first_idx]};
                args_env[func->params[second_idx]] = {std::make_shared<Value>(value), func->type.params[second_idx]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
            }
        }
        else { // fuck me
            checkNoSyntaxType(op->funcs);

            for (Environment args_env; const auto& value : values) {
                // auto type1 = validateType(typeOf(ret  ));
                // auto type2 = validateType(typeOf(value));

                func = resolveOverloadSet(op->OpName(), op->funcs, {ret, value});
                // * may be not needed....idk tho ;-;
                func->type.ret                = validateType(std::move(func)->type.ret               );
                func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
                func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


                args_env[func->params[ first_idx]] = {std::make_shared<Value>(ret)  , func->type.params[ first_idx]};
                args_env[func->params[second_idx]] = {std::make_shared<Value>(value), func->type.params[second_idx]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
            }
        }

        return ret;
    }


    Value accessAssign(const expr::Assignment *ass, expr::Access *acc) {
        if (auto name = dynamic_cast<const expr::Name*>(acc->var.get()); name and name->name == "self") {
            if (selves.empty())
                error("Can't use 'self' outside of class scope: " + ass->stringify());

            if (not checkThis(acc->name))
                error("Name '" + acc->name + "' not found in object '" + acc->var->stringify() + "' in assignment: " + ass->stringify());

            const auto value = std::visit(*this, ass->rhs->variant());
            changeThis(acc->name, value);
            return value;
        }

        const auto& left = std::visit(*this, acc->var->variant());

        if (not std::holds_alternative<Object>(left)) error("Can't access a non-class type!");

        const auto& obj = get<Object>(left);

        const auto& found = std::ranges::find_if(obj.second->members,
            [name = acc->name] (const auto& member) { return get<expr::Name>(member).stringify() == name; }
        );
        if (found == obj.second->members.end()) error("In assignment '" + ass->stringify() + "', Name '" + acc->name + "' doesn't exist in object: " + stringify(obj));

        const Value value = get<type::TypePtr>(*found)->text() == "Syntax" ? ass->rhs->variant() : std::visit(*this, ass->rhs->variant());

        typeCheck(value, get<type::TypePtr>(*found),
            "In assignment: " + ass->stringify() +
            "\nType mis-match! Expected: " + get<type::TypePtr>(*found)->text() + ", got: " + typeOf(value)->text()
        );

        // if (const auto& type_of_value = typeOf(value); not (*get<type::TypePtr>(*found) >= *type_of_value)) { // if not a super type..
        //     error<except::TypeMismatch>(
        //         "In assignment: " + ass->stringify() +
        //         "\nType mis-match! Expected: " + get<type::TypePtr>(*found)->text() + ", got: " + type_of_value->text()
        //     );
        // }

        // get<Value>(*found) = value;
        *get<ValuePtr>(*found) = value;

        return value;
    }



    Value spaceAccessAssign(const expr::Assignment *ass, expr::SpaceAccess *sa) {
        // const auto& var = getVar(sa->spacename);

        // if (not var) error("Name '" + sa->spacename + "' does is not defined!");

        Value space = std::visit(*this, sa->space->variant());

        if (not std::holds_alternative<NameSpace>(space)) error("Can't apply scope-resolution-operator '::' on a non-namespace: " + sa->space->stringify());


        const auto& ns = std::get<NameSpace>(space);

        const auto& found = std::ranges::find_if(ns.members->members,
            [&name = sa->member] (const auto& member) { return get<0>(member).stringify() == name;}
        );
        if (found == ns.members->members.end()) error("Name '" + sa->member + "' doesn't exist inside namesapce '" + sa->space->stringify() + '\'');

        // Value value = get<type::TypePtr>(*found)->text() == "Syntax" ? ass->rhs->variant() : std::visit(*this, ass->rhs->variant());

        Value value;
        if (get<type::TypePtr>(*found)->text() == "Syntax")
            value = ass->rhs->variant();
        else
            value = std::visit(*this, ass->rhs->variant());



        typeCheck(value, get<type::TypePtr>(*found),
            "In assignment: " + ass->stringify() +
            "\nType mis-match! Expected: " + get<type::TypePtr>(*found)->text() + ", got: " + typeOf(value)->text()
        );

        // if (const auto& type_of_value = typeOf(value); not (*get<type::TypePtr>(*found) >= *type_of_value)) { // if not a super type..
        //     error<except::TypeMismatch>(
        //         "In assignment: " + ass->stringify() +
        //         "\nType mis-match! Expected: " + get<type::TypePtr>(*found)->text() + ", got: " + type_of_value->text()
        //     );
        // }

        // get<Value>(*found) = value;
        *get<ValuePtr>(*found) = value;

        return value;
    }


    Value spaceAssign(const NameSpace& ns1, const NameSpace& ns2) {
        for (const auto& [name, type, value] : ns2.members->members) {
            if (
                const auto& it = std::ranges::find_if(ns1.members->members, [&name] (const auto& e) { return get<expr::Name>(e).stringify() == name.stringify(); });
                it != ns1.members->members.end()
            ) {
                // explicit copying
                get<type::TypePtr>(*it) = type;
                get<ValuePtr>(*it) = std::make_shared<Value>(*value);
            }
            // same here
            else ns1.members->members.push_back({name ,type, value});
        }


        return ns1;
    }


    Value nameAssign(const expr::Assignment *ass, const expr::Name* name) {
            // type::TypePtr type = type::builtins::Any();
            // type::TypePtr type = name->type;
            type::TypePtr type = ass->type;
            bool change{};

            // variable already exists. Check that type matches the rhs type
            if (const auto& var = getVar(name->name); var and type::shouldReassign(type)) {
                // no need to check if it's a valid type since that already was checked when it was creeated
                type = var->second;
                change = true;
            }
            else if (checkThis(name->name)) {
                const auto val = std::visit(*this, ass->rhs->variant());
                changeThis(name->name, val);
                return val;
            }
            else { // New var

                type = type::shouldReassign(type) ? type::builtins::Any() : validateType(std::move(type));

                // if (type::shouldReassign(type))
                //  type = type::builtins::Any();
                // else
                //  type = validateType(std::move(type));
            }


            if (type->text() == "Syntax")
                return addVar(name->stringify(), ass->rhs->variant(), type);


            auto value = std::visit(*this, ass->rhs->variant());


            // reassigning to an existing namespace. special | I think I don't have to use typeCheck here..maybe!
            if (change and std::holds_alternative<NameSpace>(value) and (*type >= *typeOf(value))) {
                if (not std::holds_alternative<NameSpace>(getVar(name->name)->first))
                    changeVar(name->name, NameSpace{std::make_shared<Members>()});

                return spaceAssign(get<NameSpace>(getVar(name->name)->first), get<NameSpace>(value));
            }


            value = typeCheck(value, type,
                "In assignment: " + ass->stringify() +
                "\nType mis-match! Expected: " + type->text() + ", got: " + typeOf(value)->text()
            );



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


            if (change) {
                if (not changeVar(name->name, value)) error();
            }
            else addVar(name->stringify(), value, type);

            return value;
    }


    Value operator()(const expr::Assignment *ass) {
        // assigning to x.y should never create a variable "x.y" bu access x and change y;
        if (auto *acc = dynamic_cast<expr::Access*>(ass->lhs.get())) return accessAssign(ass, acc);

        // ns::x = 1;
        if (auto *sa  = dynamic_cast<expr::SpaceAccess*>(ass->lhs.get())) return spaceAccessAssign(ass, sa);

        // name = 1;
        if (auto name = dynamic_cast<expr::Name*>(ass->lhs.get())) return nameAssign(ass, name);


        // can't assign non-names to namespaces
        // makes 1::2::3 impossible, which is consistent with the fact that 1 + 2::3 + 4; is ambiguous
        // ... unless! (1 + 2)::3 is different than 1 + 2::3
        if (dynamic_cast<expr::Namespace*>(ass->rhs.get()) and not dynamic_cast<expr::Name*>(ass->lhs.get()))
            error("Cannot assign namespaces to non-names: " + ass->stringify());

        // assign to the serialization (stringification) of the AST node
        return addVar(ass->lhs->stringify(), std::visit(*this, ass->rhs->variant()));
    }


    Value operator()(const expr::Class *cls) {
        if (const auto& var = getVar(cls->stringify()); var) return var->first;


        std::vector<std::tuple<expr::Name, type::TypePtr, ValuePtr>> members;

        ScopeGuard sg{this};
        for (const auto& [name, typ, expr] : cls->fields) {

            type::TypePtr type = typ;

            Value v;
            if (type->text() == "Syntax") {
                // members.push_back({{field.first.stringify(), type::builtins::Syntax()}, field.second->variant()});
                type = type::builtins::Syntax();
                v = expr->variant();
            }
            else {
                type = validateType(std::move(type));

                v = std::visit(*this, expr->variant());

                typeCheck(v, type,
                    "In class member assignment '" +
                    name.stringify() + ": " + typ->text() + " = " + expr->stringify() +
                    "Type mis-match! Expected: " + type->text() + ", got: " + typeOf(v)->text()
                );
            }

            // maybe not allowing the usage of previous members in the initializers of other members is the way? not sure
            // addVar(name.stringify(), v, type);
            members.push_back({name, type, std::make_shared<Value>(v)});
        }

        return // getting lispy :sob: fuck this memory ass shit
            std::make_shared<type::LiteralType>(
                std::make_shared<ClassValue>(
                    std::make_shared<Members>(
                        std::move(members)
                    )
                )
            );
    }


    Value operator()(const expr::Union *onion) {
        std::vector<type::TypePtr> types;
        for (auto& type : onion->types)
            // type = validateType(std::move(type));
            types.push_back(validateType(std::move(type)));

        return std::make_shared<type::UnionType>(std::move(types));
    }


    Value objectAccess(const Object& obj, const std::string& name) {
        const auto& found = std::ranges::find_if(obj.second->members, [&name] (const auto& member) { return get<0>(member).stringify() == name; });
        if (found == obj.second->members.end()) error("Name '" + name + "' doesn't exist in object '" + /*acc->var->*/ stringify(obj) + '\'');

        if (std::holds_alternative<expr::Closure>(*get<ValuePtr>(*found))) {
            const auto& closure = get<expr::Closure>(*get<ValuePtr>(*found));

            // Environment capture_list;
            // for (const auto& [name, value] : obj.second->members)
            //     capture_list[name.stringify()] = {value, typeOf(value)};

            // closure.capture(capture_list);
            closure.captureThis(obj);


            return closure;
        }

        return *get<ValuePtr>(*found);
    }


    Value operator()(const expr::Access *acc) {
        if (auto var = dynamic_cast<const expr::Name*>(acc->var.get()); var and var->name == "self") {
            if (selves.empty())
                error("Can't use 'self' outside of class scope: " + acc->stringify());

            const auto value = checkThis(acc->name);
            if (not value)
                error("Name '" + acc->name + "' not found in object '" + acc->var->stringify());

            return *value;
        }

        const auto& left = std::visit(*this, acc->var->variant());
        if (std::holds_alternative<Object>(left)) return objectAccess(std::get<Object>(left), acc->name);


        error("Can't access a non-class type!");
    }


    Value operator()(const expr::Namespace *ns) {
        if (const auto& var = getVar(ns->stringify()); var) return var->first;

        // Value value;

        ScopeGuard sg{this};
        // execute all the expressions in the namespace
        for (const auto& expr : ns->expressions)
            // value =
            std::visit(*this, expr->variant());

        // then add the variables that resulted from that execution

        std::vector<std::tuple<expr::Name, type::TypePtr, ValuePtr>> members;
        for (auto& [name, val] : env.back().first) {
            auto& [value, type] = val;

            members.push_back({expr::Name{name}, std::move(type), std::move(value)});
        }


        std::ranges::reverse(members);
        return NameSpace{std::make_shared<Members>(std::move(members))};
    }


    Value operator()(const expr::Use *use) {
        if (const auto& var = getVar(use->stringify()); var) return var->first;

        const auto ns = std::visit(*this, use->ns->variant());

        if (not std::holds_alternative<NameSpace>(ns)) error("Can't apply keyword 'use' on a non-namespace: " + use->ns->stringify());

        const auto& space = get<NameSpace>(ns);

        Value ret;
        for (const auto& [name, type, value] : space.members->members)
            ret = addVar(name.stringify(), *value, type);

        return ret;
    }

    // Value operator()(const expr::Import *import) {
    //     const auto src = readFile(auto{import->path}.replace_extension(".pie"));
    //     const Tokens v = lex(src);
    //     if (v.empty()) error("Can't import an empty file!");

    //     Parser p{v, import->path};

    //     auto [exprs, ops] = p.parse();

    //     Value value;
    //     for (const auto& expr : exprs)
    //         value = std::visit(*this, std::move(expr)->variant());

    //     return value;
    // }


    Value operator()(const expr::SpaceAccess* sa) {
        if (const auto& var = getVar(sa->stringify()); var) return var->first;

        if (not sa->space) { // global namespace `::x`
            if (const auto& var = globalLookup(sa->member); var) return var->first; 
            else error("Name '" + sa->member + "' not found in global namespace!");
        }

        // const auto& var = getVar(sa->spacename);
        // if (not var) error("Name '" + sa->spacename + "' does is not defined!");

        Value space = std::visit(*this, sa->space->variant());

        if (not std::holds_alternative<NameSpace>(space)) error("Can't apply scope-resolution-operator '::' on a non-namespace: " + sa->space->stringify());


        const auto& ns = std::get<NameSpace>(space);

        const auto& found = std::ranges::find_if(ns.members->members, [&name = sa->member] (const auto& member) { return get<expr::Name>(member).stringify() == name; });

        if (found == ns.members->members.end()) error("Name '" + sa->member + "' doesn't exist inside namesapce '" + sa->space->stringify() + '\'');


        if (std::holds_alternative<expr::Closure>(*get<ValuePtr>(*found))) {
            const auto& closure = get<expr::Closure>(*get<ValuePtr>(*found));

            Environment capture_list;
            for (const auto& [name, type, value] : ns.members->members)
                capture_list[name.stringify()] = {std::make_shared<Value>(*value), typeOf(*value)};

            closure.capture(capture_list);

            return closure;
        }

        return *get<ValuePtr>(*found);
    }


    bool match(const Value& value, const expr::Match::Case::Pattern& pattern) {
        if (std::holds_alternative<expr::Match::Case::Pattern::Single>(pattern.pattern)) {
            const auto& [name, typ, val_expr] = get<expr::Match::Case::Pattern::Single>(pattern.pattern);
            const auto type = validateType(typ);

            // not gonna use typeCheck for now. Let's see how it goes
            if (not (*type >= *typeOf(value))) return false;

            if (val_expr) {
                const auto val = std::visit(*this, val_expr->variant());
                if (value != val) return false;
            }

            if (name.length() != 0) {
                addVar(name, value, type);
            }

            return true;
        }

        const auto& [type_name, patterns] = get<expr::Match::Case::Pattern::Structure>(pattern.pattern);

        const auto var = getVar(type_name);
        if (not var)
            error("Name `" + type_name + "` in match expression does not name a constructor");

        const Value type_value = var->first;
        // if (not std::holds_alternative<ClassValue>(type_value))
        if (not std::holds_alternative<type::TypePtr>(type_value) and not type::isClass(get<type::TypePtr>(type_value)))
            error("Name `" + type_name + "` in match expression does not name a constructor");


        // const auto& cls = get<ClassValue>(type_value);
        // const type::TypePtr type = std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(cls));
        const auto& type = get<type::TypePtr>(type_value);
        // if (not (*type == *typeOf(value))) return false;
        if (not (*type == *typeOf(value))) return false;

        if (
            type::isClass(type) and
            patterns.size() > dynamic_cast<type::LiteralType*>(type.get())->cls->blueprint->members.size()
        )
            error("Number of singles is greater than number of fields in class " + stringify(type));


        const auto& obj = get<Object>(value);
        if (obj.second->members.size() != obj.second->members.size()) error("idek what error message this should be..!");

        for (const auto& [member, pat] : std::views::zip(get<Object>(value).second->members, patterns)) {
            if (not match(*get<ValuePtr>(member), *pat)) return false;
        }

        return true;
    }


    Value operator()(const expr::Match *m) {
        if (const auto& var = getVar(m->stringify()); var) return var->first;

        const Value value = std::visit(*this, m->expr->variant());

        for (const auto& kase : m->cases) {
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


    Value operator()(const expr::Type* type) {
        if (const auto& var = getVar(type->stringify()); var) return var->first;

        return validateType(type->type);
    };


    Value operator()(const expr::Loop *loop) {
        if (const auto& var = getVar(loop->stringify()); var) return var->first;


        enum class Type { NONE = 0, INT, BOOL, LIST, PACK, OBJECT };
        const auto classify = [](const Value& v) {
            if (std::holds_alternative<ssize_t  >(v)) return Type::INT ;
            if (std::holds_alternative<bool     >(v)) return Type::BOOL;
            if (std::holds_alternative<ListValue>(v)) return Type::LIST;
            if (std::holds_alternative<PackList >(v)) return Type::PACK;
            if (std::holds_alternative<Object   >(v)) return Type::OBJECT;

            return Type::NONE;
        };

        ScopeGuard sg{this};

        // push
        const auto current_counter = loop_counter;

        Value ret;
        if (loop->kind) {
            const Value kind = std::visit(*this, loop->kind->variant());

            switch (classify(kind)) {
                // for loop
                case Type::INT: {
                    const auto limit = get<ssize_t>(kind);
                    if (limit <= 0) {
                        if (not loop->els) error("Loop which didn't run doesn't have else branch: " + loop->stringify());
                        return std::visit(*this, loop->els->variant());
                    }


                    if (loop->var) {
                        std::string var_name = loop->var->stringify();

                        for (loop_counter = 0; loop_counter < limit; ++loop_counter) {
                            continued = false;

                            addVar(var_name, loop_counter); // will change to "proper type" soon. for now, `Any` will do

                            ret = std::visit(*this, loop->body->variant());

                            if (broken) break;
                            // if (continued) continue;
                        }
                    }
                    else for (loop_counter = 0; loop_counter < limit; ++loop_counter) {
                        continued = false;

                        ret = std::visit(*this, loop->body->variant());

                        if (broken) break;
                    }

                } break;

                // while loop
                case Type::BOOL: {
                    if (not get<bool>(kind)) {
                        if (not loop->els) error("Loop which didn't run doesn't have else branch: " + loop->stringify());
                        return std::visit(*this, loop->els->variant());
                    }

                    if (auto cond = kind; loop->var) {
                        std::string var_name = loop->var->stringify();

                        for (loop_counter = 0; get<bool>(cond); ++loop_counter) {
                            continued = false;

                            addVar(var_name, loop_counter);

                            ret = std::visit(*this, loop->body->variant());

                            if (broken) break;
                            // if (continued) continue;

                            cond = std::visit(*this, loop->kind->variant());
                        }

                    }
                    else for (loop_counter = 0; get<bool>(cond); ++loop_counter) {
                        continued = false;

                        ret = std::visit(*this, loop->body->variant());

                        if (broken) break;

                        cond = std::visit(*this, loop->kind->variant());
                    }
                } break;

                case Type::LIST: {
                    const auto& list = get<ListValue>(kind);
                    if (list.elts->values.empty()) {
                        if (not loop->els) error("Loop which didn't run doesn't have else branch: " + loop->stringify());
                        return std::visit(*this, loop->els->variant());
                    }


                    if (loop->var) {
                        std::string var_name = loop->var->stringify();

                        for (const auto& elt : list.elts->values) {
                            continued = false;

                            addVar(var_name, elt);

                            ret = std::visit(*this, loop->body->variant());

                            if (broken) break;
                        }

                    }
                    else for ([[maybe_unused]] const auto& _ : list.elts->values) {
                        continued = false;

                        ret = std::visit(*this, loop->body->variant());

                        if (broken) break;
                    }
                } break;

                case Type::PACK: {
                    const auto& pack = get<PackList>(kind);
                    if (pack->values.empty()) {
                        if (not loop->els) error("Loop which didn't run doesn't have else branch: " + loop->stringify());
                        return std::visit(*this, loop->els->variant());
                    }


                    if (loop->var) {
                        std::string var_name = loop->var->stringify();

                        for (const auto& elt : pack->values) {
                            continued = false;

                            addVar(var_name, elt);

                            ret = std::visit(*this, loop->body->variant());

                            if (broken) break;
                        }

                    }
                    else for ([[maybe_unused]] const auto& _ : pack->values) {
                        continued = false;

                        ret = std::visit(*this, loop->body->variant());

                        if (broken) break;
                    }
                } break;

                // use iterators (future update)
                case Type::OBJECT: {
                    const auto& obj = get<Object>(kind);

                    // const auto& next_it    = std::ranges::find_if(obj.second->members, [](const auto& p) { return p.first.name == "next";    });
                    // const auto& hasNext_it = std::ranges::find_if(obj.second->members, [](const auto& p) { return p.first.name == "hasNext"; });
                    // if (next_it == obj.second->members.cend() or hasNext_it == obj.second->members.cend())
                    //     error("Object in loop: " + loop->stringify() + "\ndoesn't follow the iterator protocol!");


                    // // I know objectAccess errors if the accessee is not found, but more specific err messages are nicer
                    const auto hasNext = objectAccess(obj, "hasNext");
                    const auto    next = objectAccess(obj, "next");

                    if (not std::holds_alternative<expr::Closure>(hasNext) or not std::holds_alternative<expr::Closure>(next))
                        error("Object in loop: " + loop->stringify() + " doesn't follow the iterator protocol!");

                    const auto& hasNext_func = get<expr::Closure>(hasNext);
                    const auto&    next_func = get<expr::Closure>(next   );

                    if (
                        not hasNext_func.type.params.empty() or hasNext_func.type.ret->text() != "Bool"
                        or not next_func.type.params.empty()
                    )
                    error("Object in loop: " + loop->stringify() + " doesn't follow the iterator protocol!");


                    expr::Call hasNext_call{std::make_shared<expr::Closure>(hasNext_func)};
                    expr::Call    next_call{std::make_shared<expr::Closure>(   next_func)};

                    if (loop->var) {
                        std::string var_name = loop->var->stringify();

                        while(get<bool>(std::visit(*this, hasNext_call.variant()))) {
                            continued = false;

                            addVar(var_name, std::visit(*this, next_call.variant()));

                            ret = std::visit(*this, loop->body->variant());

                            if (broken) break;
                        }
                    }
                    else while(get<bool>(std::visit(*this, hasNext_call.variant()))) {
                        continued = false;

                        std::visit(*this, next_call.variant());

                        ret = std::visit(*this, loop->body->variant());

                        if (broken) break;
                    }

                } break;

                case Type::NONE:
                    error("Loop type no supported: " + loop->stringify());
            }
        }
        // loop till break
        else if (loop->var) {
            continued = false;

            std::string var_name = loop->var->stringify();

            for (loop_counter = 0; ; ++loop_counter) {
                addVar(var_name, loop_counter); // will change to "proper type" soon. for now, `Any` will do

                ret = std::visit(*this, loop->body->variant());

                if (broken) break;
            }

        }
        else for (loop_counter = 0; ; ++loop_counter) {
            continued = false;

            ret = std::visit(*this, loop->body->variant());

            if (broken) break;
        }

        // pop
        loop_counter = current_counter;

        broken = continued = false;
        return ret;
    }


    Value operator()(const expr::Break *brake) {
        broken = true;
        if (brake->expr) return std::visit(*this, brake->expr->variant());
        // if (brake->expr) return eval(brake->expr);

        return loop_counter;
    }

    Value operator()(const expr::Continue *cont) {
        continued = true;
        if (cont->expr) return std::visit(*this, cont->expr->variant());

        return loop_counter;
    }


    //* only added to differentiate between expressions such as: 1 + 2 and (1 + 2)
    Value operator()(const expr::Grouping *g) {
        if (const auto& var = getVar(g->stringify()); var) return var->first;

        return std::visit(*this, g->expr->variant());
    }


    static void checkNoSyntaxType(const std::vector<expr::ExprPtr>& funcs) {
        for (const auto& func : funcs) {
            const auto& closure = dynamic_cast<const expr::Closure*>(func.get());
            for (const auto& param_type : closure->type.params) {
                if (param_type->text() == "Syntax") error("Cannot have paramater of 'Syntax' in an overload set!");
            }
        }

        return;
    }

    expr::Closure* resolveOverloadSet(
        const std::string& name,
        const std::vector< expr::ExprPtr >& funcs,
        // const std::vector< type::TypePtr >& types,
        const std::vector<value::Value>& values
    ) {
        std::vector<expr::Closure*> set;

        std::vector<type::TypePtr> types;
        types.reserve(values.size());
        for (const auto& v : values) types.push_back(validateType(typeOf(v)));

        // std::transform(values.begin(), values.end(),
        //     std::back_inserter(types),
        //     [this] (const auto& v) { return validateType(typeOf(v)); }
        // );


        for (const auto& func : funcs) {
            auto closure = dynamic_cast<expr::Closure*>(func.get());

            bool found = true;
            for (const auto& [arg_value, arg_type, param_type] : std::views::zip(values, types, closure->type.params)) {
                // if (not (*param_type >= *arg_type)) {
                //     found = false;
                //     break;
                // }

                if (not param_type->typeCheck(this, arg_value, arg_type)) {
                    found = false;
                    break;
                }
            }

            if (found) set.push_back(closure);
        }


        if (set.size() > 1) {
            // remove the functions that take Any's to favour the concrete functions rather than "templates"
            std::erase_if(set, [] (const expr::Closure *c) { return std::ranges::any_of(c->type.params, &type::isAny); });
        }


        if (set.size() != 1) error("Could not resolve overload set for operator: " + name);

        return set[0];
    }



    const Value& checkReturnType(const Value& ret, const type::TypePtr return_type, const std::source_location& location = std::source_location::current()) {

        typeCheck(ret, return_type,
            "Type mis-match! Function return type expected: " +
            return_type->text() + ", got: " + typeOf(ret)->text(),
            location
        );

        // if (not (*return_type >= *type_of_return_value))
        //     error<except::TypeMismatch>(
        //         "Type mis-match! Function return type expected: " +
        //         return_type->text() + ", got: " + type_of_return_value->text(),
        //         location
        //     );


        return ret;
    }

    Value operator()(const expr::UnaryOp *up) {
        if (const auto& var = getVar(up->stringify()); var) return var->first;


        const auto& op = ops.at(up->op);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            //* this could be dried out between all the OPs and function calls in general
            if (func->type.params[0]->text() == "Syntax") {
                // addVar(func->params.front(), up->expr->variant());
                //* maybe should use Syntax() instead of Any();
                args_env[func->params[0]] = {std::make_shared<Value>(up->expr->variant()), func->type.params[0]}; //? fixed
            }
            else {
                func->type.params[0] = validateType(std::move(func)->type.params[0]);

                const auto& arg = std::visit(*this, up->expr->variant());

                typeCheck(arg, func->type.params[0],
                    "Type mis-match! Prefix operator '" + up->op + 
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + typeOf(arg)->text()
                );

                // if (const auto& type_of_arg = typeOf(arg); not (*func->type.params[0] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match! Prefix operator '" + up->op + 
                //         "' expected: " + func->type.params[0]->text() +
                //         ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                //     );

                // addVar(func->params.front(), arg);
                //* maybe should use Syntax() instead of Any();
                args_env[func->params[0]] = {std::make_shared<Value>(arg), func->type.params[0]}; //? fixed
            }
        }
        else { // do selection based on type
            checkNoSyntaxType(op->funcs);

            const auto arg = std::visit(*this, up->expr->variant());

            func = resolveOverloadSet(op->OpName(), op->funcs, {arg});

            args_env[func->params[0]] = {std::make_shared<Value>(arg), func->type.params[0]}; //? fixed
        }


        ScopeGuard sg{this, args_env};

        Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant());

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant()); // capturing logic will be done by the scope's visitor

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            const auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return ret;
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }

    Value operator()(const expr::BinOp *bp) {
        if (const auto& var = getVar(bp->stringify()); var) return var->first;


        const auto& op = ops.at(bp->op);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            // LHS
            if (func->type.params[0]->text() == "Syntax") {
                // addVar(func->params[0], bp->lhs->variant());
                args_env[func->params[0]] = {std::make_shared<Value>(bp->lhs->variant()), func->type.params[0]}; //? fixed
            }
            else {
                func->type.params[0] = validateType(std::move(func)->type.params[0]);

                const auto& arg1 = std::visit(*this, bp->lhs->variant());

                typeCheck(arg1, func->type.params[0],
                    "Type mis-match! Infix operator '" + bp->op + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg1) + " which is " + typeOf(arg1)->text()
                );

                // if (const auto& type_of_arg = typeOf(arg1); not (*func->type.params[0] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match! Infix operator '" + bp->op + 
                //         "', parameter '" + func->params[0] +
                //         "' expected: " + func->type.params[0]->text() +
                //         ", got: " + stringify(arg1) + " which is " + type_of_arg->text()
                //     );

                // addVar(func->params[0], arg1);
                args_env[func->params[0]] = {std::make_shared<Value>(arg1), func->type.params[0]};
            }

            // RHS
            if (func->type.params[1]->text() == "Syntax") {
                args_env[func->params[1]] = {std::make_shared<Value>(bp->rhs->variant()), func->type.params[1]};
            }
            else {
                func->type.params[1] = validateType(std::move(func)->type.params[1]);

                const auto& arg2 = std::visit(*this, bp->rhs->variant());

                typeCheck(arg2, func->type.params[1],
                    "Type mis-match! Infix operator '" + bp->op + 
                    "', parameter '" + func->params[1] +
                    "' expected: " + func->type.params[1]->text() +
                    ", got: " + stringify(arg2) + " which is " + typeOf(arg2)->text()
                );

                // if (const auto& type_of_arg = typeOf(arg2); not (*func->type.params[1] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match! Infix operator '" + bp->op + 
                //         "', parameter '" + func->params[1] +
                //         "' expected: " + func->type.params[1]->text() +
                //         ", got: " + stringify(arg2) + " which is " + type_of_arg->text()
                //     );


                args_env[func->params[1]] = {std::make_shared<Value>(arg2), func->type.params[1]};
            }

        }
        else {
            checkNoSyntaxType(op->funcs);

            const auto arg1  = std::visit(*this, bp->lhs->variant());
            const auto arg2  = std::visit(*this, bp->rhs->variant());

            func = resolveOverloadSet(op->OpName(), op->funcs, {arg1, arg2});

            args_env[func->params[0]] = {std::make_shared<Value>(arg1), func->type.params[0]};
            args_env[func->params[1]] = {std::make_shared<Value>(arg2), func->type.params[1]};
        }


        // !for binary fold
        // Value ret;
        // if (not dynamic_cast<expr::Block*>(func->body.get())) {
        //     ret = std::visit(*this, func->body->variant());

        //     if (std::holds_alternative<expr::Closure>(ret)) {
        //         captureEnvForClosure(get<expr::Closure>(ret));
        //     }
        // }
        // else ret = std::visit(*this, func->body->variant());

        // if (func->self and std::holds_alternative<expr::Closure>(ret)) {
        //     const auto& f = get<expr::Closure>(ret);
        //     f.captureThis(*func->self);
        // }


        ScopeGuard sg{this, args_env};

        Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant());

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant());

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            const auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return ret;
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }


    Value operator()(const expr::PostOp *pp) {
        if (const auto& var = getVar(pp->stringify()); var) return var->first;


        const auto& op = ops.at(pp->op);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {

            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());


            if (func->type.params[0]->text() == "Syntax") {
                // addVar(func->params[0], pp->expr->variant());
                args_env[func->params[0]] = {std::make_shared<Value>(pp->expr->variant()), func->type.params[0]}; //? fixed
            }
            else {
                func->type.params[0] = validateType(std::move(func)->type.params[0]);

                const auto& arg = std::visit(*this, pp->expr->variant());

                typeCheck(arg, func->type.params[0],
                    "Type mis-match! Suffix operator '" + pp->op + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + typeOf(arg)->text()
                );

                // if (const auto& type_of_arg = typeOf(arg); not (*func->type.params[0] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match! Suffix operator '" + pp->op + 
                //         "', parameter '" + func->params[0] +
                //         "' expected: " + func->type.params[0]->text() +
                //         ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                //     );

                args_env[func->params[0]] = {std::make_shared<Value>(arg), func->type.params[0]}; //? fixed
            }

        }
        else {
            checkNoSyntaxType(op->funcs);

            const auto arg  = std::visit(*this, pp->expr->variant());

            func = resolveOverloadSet(op->OpName(), op->funcs, {arg});

            args_env[func->params[0]] = {std::make_shared<Value>(arg), func->type.params[0]};
        }

        ScopeGuard sg{this, args_env};

        Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant());

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant());

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            const auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return ret;
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }



    Value operator()(const expr::CircumOp *cp) {
        if (const auto& var = getVar(cp->stringify()); var) return var->first;

        const auto& op = ops.at(cp->op1);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {

            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());


            if (func->type.params[0]->text() == "Syntax") {
                // addVar(func->params[0], co->expr->variant());
                args_env[func->params[0]] = {std::make_shared<Value>(cp->expr->variant()), func->type.params[0]}; //? fixed
            }
            else {
                func->type.params[0] = validateType(std::move(func)->type.params[0]);

                const auto& arg = std::visit(*this, cp->expr->variant());

                typeCheck(arg, func->type.params[0],
                    "Type mis-match! Suffix operator '" + cp->op1 + 
                    "', parameter '" + func->params[0] +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + typeOf(arg)->text()
                );

                // if (const auto& type_of_arg = typeOf(arg); not (*func->type.params[0] >= *type_of_arg))
                //     error<except::TypeMismatch>(
                //         "Type mis-match! Suffix operator '" + cp->op1 + 
                //         "', parameter '" + func->params[0] +
                //         "' expected: " + func->type.params[0]->text() +
                //         ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                //         // ", got: " + type_of_arg->text()
                //     );

                // addVar(func->params[0], std::visit(*this, co->expr->variant()));
                args_env[func->params[0]] = {std::make_shared<Value>(arg), func->type.params[0]}; //? fixed
            }
        }
        else {
            checkNoSyntaxType(op->funcs);

            const auto arg  = std::visit(*this, cp->expr->variant());

            func = resolveOverloadSet(op->OpName(), op->funcs, {arg});

            args_env[func->params[0]] = {std::make_shared<Value>(arg), func->type.params[0]};
        }


        ScopeGuard sg{this, args_env};

        Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant());

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant());

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            const auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return ret;
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    };

    Value operator()(const expr::OpCall *oc) {
        if (const auto& var = getVar(oc->stringify()); var) return var->first;


        const auto& op = ops.at(oc->first);
        expr::Closure* func;
        Environment args_env;

        if (op->funcs.size() == 1) {

            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            // this may be not needed
            // if (oc->exprs.size() != func->params.size()) error();

            for (auto [arg_expr, param, param_type] : std::views::zip(oc->exprs, func->params, func->type.params)) {
                if (param_type->text() == "Syntax") {
                    args_env[param] = {std::make_shared<Value>(arg_expr->variant()), param_type}; //?
                }
                else {
                    param_type = validateType(std::move(param_type));

                    const auto& arg = std::visit(*this, arg_expr->variant());

                    const auto op_name = op->OpName();
                    typeCheck(arg, param_type,
                        "Type mis-match! Parameter '" +
                        op_name + "' expected type: " + param_type->text() + ", got: " + typeOf(arg)->text()
                    );

                    // if (const auto& type_of_arg = typeOf(arg); not (*param_type >= *type_of_arg)) {
                    //     const auto& op_name = oc->first + 
                    //         std::accumulate(oc->rest.cbegin(), oc->rest.cend(), std::string{}, [](const auto& acc, const auto& e) { return acc + ':' + e; });
                    //     error<except::TypeMismatch>(
                    //         "Type mis-match! Operator '" + op_name + 
                    //         "', parameter '" + param +
                    //         "' expected: " + param_type->text() +
                    //         ", got: " + stringify(arg) + " which is " + type_of_arg->text()
                    //     );
                    // }

                    // addVar(func->params[0], std::visit(*this, co->expr->variant()));
                    args_env[param] = {std::make_shared<Value>(arg), param_type}; //? fix Any Type!!
                }
            }
        }
        else {
            checkNoSyntaxType(op->funcs);

            std::vector<Value> args;
            std::vector<type::TypePtr> types;
            for (const auto& expr : oc->exprs) {
                args .push_back(std::visit(*this, expr->variant()));
                // types.push_back(validateType(typeOf(args.back())));
                // // types.back() = validateType(std::move(types).back());
            }

            func = resolveOverloadSet(op->OpName(), op->funcs, args);

            for (const auto& [param, arg, type] : std::views::zip(func->params, args, func->type.params))
                args_env[param] = {std::make_shared<Value>(arg), type};
        }


        ScopeGuard sg{this, args_env};

        Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant());

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant());

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            const auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return ret;
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }

    std::optional<value::Value> objectIsCallable(const value::Object& obj) {
        for (const auto& [name, type, value] : obj.second->members) {
            if (name.name == "call" and type::isFunction(typeOf(*value))) {
                get<expr::Closure>(*value).captureThis(obj);
                return *value;
            }
        }

        return {};
    };

    Value operator()(const expr::Call *call) {
        if (const auto& var = getVar(call->stringify()); var) return var->first;

        // const auto args = std::move(call)->args;
        const auto args = call->args;


        // for pack expansions at call site (there could be multiple of em)
        // i.e: func(args1..., "Hi", "hello", args2..., args3...)
        std::vector<std::pair<size_t, std::vector<Value>>> expand_at;
        for (size_t i{}; i < args.size(); ++i) {
            if (const auto expand = dynamic_cast<const expr::Expansion*>(args[i].get())) {
                const auto pack = std::visit(*this, expand->pack->variant());

                if (not std::holds_alternative<PackList>(pack))
                    error("Expansion applied on a non-pack variable: " + args[i]->stringify());

                expand_at.push_back({i, get<PackList>(pack)->values});
            }
        }


        auto var = std::visit(*this, call->func->variant());
        if (std::holds_alternative<std::string>(var)) { // that dumb lol. but now it works
            const auto& name = std::get<std::string>(var);                                  // vvv not sure if this is moveable
            if (isBuiltin(name)) return evaluateBuiltin(std::move(args), std::move(expand_at), call->named_args, name);
        }


        if (std::holds_alternative<expr::Closure>(var)) {
            return closureCall(call, var, std::move(args), std::move(expand_at));
        }

        // else if (std::holds_alternative<ClassValue>(var)) return constructorCall(call, std::move(var));
        if (std::holds_alternative<type::TypePtr>(var)) return constructorCall(call, std::move(var));



        if (std::holds_alternative<value::Object>(var)) {
            const auto& obj = get<value::Object>(var);
            if (const auto callable = objectIsCallable(obj); callable) {
                return closureCall(call, *callable, std::move(args), std::move(expand_at));
            }
        }


        if (args.empty()) return var; // get

        if (args.size() == 1) {       // set
            const auto val = std::visit(*this, args[0]->variant());

            addVar(stringify(var), val, typeOf(val));
            // addVar(call->func->stringify(), val, typeOf(val));
            return var;
        }

        error("Can't pass arguments to values!");
    }

    Value closureCall(
        const expr::Call *call,
        const Value& var,
        std::vector<pie::expr::ExprPtr> args,
        std::vector<std::pair<size_t, std::vector<Value>>> expand_at
    ) {
        auto func = std::get<expr::Closure>(var);

        // // types are validate in operator()(const expr::Closure* c) for now
        // for (auto& type : func.type.params) type = validateType(std::move(type));
        // func.type.ret = validateType(std::move(func.type.ret));
        // // func.type.ret = validateType(std::move(func).type.ret); // is this better?

        if (func.self) selves.push_back(*func.self);
        Deferred d{[this, cond = static_cast<bool>(func.self)] { if (cond) selves.pop_back(); }};


        // check for invalid named arguments
        for (const auto& [name, _] : call->named_args) {
            if (std::ranges::find(func.params, name) == func.params.end())
                error("Named argument '" + name + "' does not name a parameter name!");
        }


        const bool is_variadic = std::ranges::any_of(func.type.params, [] (const auto& e) { return type::isVariadic(e); });
        const size_t args_size =
        args.size() +
        std::ranges::fold_left(expand_at, size_t{}, [] (const auto& acc, const auto& elt) { return acc + elt.second.size(); }) // plus the expansions
        - expand_at.size(); // minus redundant packs (already expanded)

        if (not is_variadic and args_size + call->named_args.size() > func.params.size()) error("Too many arguments passed to function: " + call->stringify());

        // curry! 
        if (args_size + call->named_args.size() < func.params.size() - is_variadic) {
            return partialApplication(call, func, args_size, std::move(expand_at), std::move(args), is_variadic);
        }


        //* full call. Don't curry!
        // ScopeGuard sg{this, EnvTag::FUNC, func.args_env, func.env};
        ScopeGuard sg{this, EnvTag::FUNC, func.env};
        Environment args_env; // in case the lambda needs to capture 


        // !
        for (const auto& [name, expr] : call->named_args) {
            type::TypePtr type;
            for (const auto& [n, t] : std::views::zip(func.params, func.type.params)) {
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
                // if (const auto& type_of_value = typeOf(value); not (*type >= *type_of_value))
                //     error<except::TypeMismatch>("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());


                value = typeCheck(value, type,
                    "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                );
            }

            // sg.addEnv({{name, {std::make_shared<Value>(value), type}}});
            args_env[name] = {std::make_shared<Value>(std::move(value)), std::move(type)};
        }


        std::vector<std::pair<std::string, type::TypePtr>> pos_params;
        for (const auto& [param, type] : std::views::zip(func.params, func.type.params)) pos_params.push_back({param, type});

        std::erase_if(pos_params, [named_args = call->named_args] (const auto& p)  mutable {
            const auto cond = std::ranges::find_if(named_args, [&p] (const auto& n) { return n.first == p.first; }) != named_args.cend();
            if (cond) named_args.erase(p.first);
            return cond;
        });


        if (is_variadic) {
            const auto it = std::ranges::find_if(pos_params, [] (const auto& e) { return type::isVariadic(e.second); });
            const size_t variadic_index = std::distance(pos_params.begin(), it);

            const auto pre_variadic = std::ranges::subrange(pos_params.begin(), it);
            const auto post_variadic  = std::ranges::subrange(it + 1, pos_params.end());

            const size_t pre_variadic_size = std::ranges::size(pre_variadic);
            const size_t post_variadic_size = std::ranges::size(post_variadic);
            const size_t variadic_size = args_size - pre_variadic_size - post_variadic_size;

            auto pack = makePack();
            for (
                size_t arg_index{}, param_index{}, pack_index{}, curr_expansion{};
                arg_index < args.size(); // can't be args_size since arg_index is only used to index into args
            ) {
                auto& [name, type] = pos_params[param_index];

                Value value;

                if (param_index == variadic_index){
                    for (size_t i{}; i < variadic_size; ++i) {

                        if (curr_expansion < expand_at.size() and arg_index == expand_at[curr_expansion].first) {
                            value = std::move(expand_at[curr_expansion].second[pack_index++]);

                            // if (const auto& type_of_value = typeOf(value); not (*type >= *type_of_value))
                            //     error<except::TypeMismatch>("Type mis-match! Parameter '" + name
                            //         + "' expected type: "+ type->text()
                            //         + ", got: " + type_of_value->text()
                            //     );

                            value = typeCheck(value, type,
                                "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                            );


                            if (pack_index >= expand_at[curr_expansion].second.size()) {
                                ++arg_index;
                                ++curr_expansion;
                                pack_index = 0;
                            }
                        }
                        else {
                            const auto& expr = args[arg_index];

                            if (type->text() == "Syntax") {
                                error(); //* allow this the future
                            }

                            else {
                                value = std::visit(*this, expr->variant());


                                // if (const auto& type_of_value = typeOf(value); not (*type >= *type_of_value))
                                //     error<except::TypeMismatch>("Type mis-match! Parameter '" + name
                                //         + "' expected type: "+ type->text() 
                                //         + ", got: " + type_of_value->text()
                                //     );

                                value = typeCheck(value, type,
                                    "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                                );
                            }

                            ++arg_index;
                        }

                        pack->values.push_back(std::move(value));
                    }

                    ++param_index;

                    // sg.addEnv({{name, {std::make_shared<Value>(value), type}}});
                    args_env[name] = {std::make_shared<Value>(std::move(pack)), std::move(type)};
                }
                else {
                    if (curr_expansion < expand_at.size() and arg_index == expand_at[curr_expansion].first) {
                        value = expand_at[curr_expansion].second[pack_index++];

                        // if (const auto& type_of_value = typeOf(value); not (*type >= *type_of_value))
                        //     error<except::TypeMismatch>("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());

                        value = typeCheck(value, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                        );


                        if (pack_index >= expand_at[curr_expansion].second.size()) {
                            ++arg_index;
                            ++curr_expansion;
                            pack_index = 0;
                        }
                    }
                    else {
                        const auto& expr = args[arg_index];

                        if (type->text() == "Syntax") value = expr->variant();
                        else {
                            value = std::visit(*this, expr->variant());

                            // if (const auto& type_of_value = typeOf(value); not (*type >= *type_of_value))
                            //     error<except::TypeMismatch>("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());

                            value = typeCheck(value, type,
                                "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                            );
                        }

                        ++arg_index;
                    }

                    ++param_index;
                    // sg.addEnv({{name, {std::make_shared<Value>(value), type}}});
                    args_env[name] = {std::make_shared<Value>(std::move(value)), std::move(type)};
                }
            }


            if (variadic_size == 0) {
                sg.addEnv({{
                    pos_params[variadic_index].first,
                    { std::make_shared<Value>(makePack()), pos_params[variadic_index].second }
                }});
                args_env[pos_params[variadic_index].first] = {std::make_shared<Value>(makePack()), std::move(pos_params)[variadic_index].second};
            }
        }
        else {
            const auto findType = [&func] (const size_t p, const type::TypePtr& type) {
                // it doesn't matter if there are multiple arguments with this name
                // `validateType` will choose the lastly-bounded one
                // we just need to proof that A parameter exists in order to call `validateType`
                for (size_t i{}; i <= p; ++i)
                    if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(func.params[i])}))
                        return true;

                // // look in the arguments env (from a partially evaluated function that yielded this function)
                // for (const auto& [key, _] : func.args_env)
                for (const auto& [key, _] : func.env)
                    if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(key)}))
                        return true;

                return false;
            };

            for (size_t i{}, p{}, curr{}; p < args_size; ++p, ++i) {

                if (curr < expand_at.size() and i == expand_at[curr].first) {
                    for (auto& val : expand_at[curr++].second) {
                        auto& [name, type] = pos_params[p];
                        if (findType(p, type)) {
                            // ScopeGuard sg{this, func.args_env, args_env};
                            ScopeGuard sg{this, func.env, args_env};
                            type = validateType(std::move(type));
                        }

                        ++p; // important!

                        val = typeCheck(val, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(val)->text()
                        );


                        // sg.addEnv({{name, {std::make_shared<Value>(val), type}}});
                        args_env[name] = {std::make_shared<Value>(std::move(val)), std::move(type)};
                    }
                    --p; // the parameter index will have gone one too far. bring it back
                }
                else {
                    auto& [name, type] = pos_params[p];
                    if (findType(p, type)) {
                        // ScopeGuard sg{this, func.args_env, args_env};
                        ScopeGuard sg{this, func.env, args_env};
                        type = validateType(std::move(type));
                    }

                    const auto& expr = args[i];

                    Value value;
                    if (type->text() == "Syntax") value = expr->variant();
                    else {
                        value = std::visit(*this, expr->variant());

                        value = typeCheck(value, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                        );
                    }

                    // sg.addEnv({{name, {std::make_shared<Value>(value), type}}});
                    args_env[name] = {std::make_shared<Value>(std::move(value)), std::move(type)};
                }
            }
        }



        if (
            std::ranges::find_if(
                func.params, [&type = func.type.ret](const auto& param) {
                    return type->involvesT(type::ExprType{std::make_shared<expr::Name>(param)});
                }
            ) != func.params.cend()
        ) {
            // ScopeGuard sg{this, func.args_env, args_env};
            ScopeGuard sg{this, func.env, args_env};
            func.type.ret = validateType(std::move(func.type.ret));
        }


        //* should I capture the env and bundle it with the function before returning it?
        if (type::isSyntax(func.type.ret)) return func.body->variant();


        // sg.addEnv(func.args_env);
        sg.addEnv(func.env);
        sg.addEnv(args_env);

        Value ret;
        if (not dynamic_cast<const expr::Block*>(func.body.get())) {
            ret = std::visit(*this, func.body->variant());

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func.body->variant());

        if (func.self and std::holds_alternative<expr::Closure>(ret)) {
            get<expr::Closure>(ret).captureThis(*func.self);
        }


        checkReturnType(ret, func.type.ret);

        return ret;
    }


    void captureEnvForClosure(const expr::Closure& c) {
        // for (size_t i = env.size() - 1; /* i >= 0 */; --i) {
        //     // c.capture<expr::Closure::OverrideMode::NO_OVERRIDE>(env[i].first);
        //     c.capture(env[i].first);
        //     if (env[i].second == EnvTag::FUNC) break;

        //     if (i == 0) return; // unsigned nums can never be less than zero
        // }

        size_t found{};
        for (size_t i{}; i < env.size(); ++i) {
            if (env[i].second == EnvTag::FUNC) found = i;
        }

        for (; found < env.size(); ++found) c.capture(env[found].first);
    }


    Value partialApplication(
        const expr::Call *call,
        const expr::Closure& func,
        const size_t args_size,
        std::vector<std::pair<size_t, std::vector<Value>>> expand_at,
        std::vector<expr::ExprPtr> args, 
        const bool is_variadic
    ) {
        // ScopeGuard sg{this, EnvTag::FUNC, func.args_env, func.env};
        ScopeGuard sg{this, EnvTag::FUNC, func.env};
        Environment args_env = func.env;

        for (const auto& [name, expr] : call->named_args) {
            type::TypePtr type;
            for (const auto& [n, t] : std::views::zip(func.params, func.type.params)) {
                if (n == name) {
                    type = t;
                    break;
                }
            }

            if (not type) util::error(); // should never happen anyway

            Value value;
            if (type->text() == "Syntax") value = expr->variant();
            else {
                value = std::visit(*this, expr->variant());

                // // if the param type not a super type of arg type, error out
                // if (const auto& type_of_value = typeOf(value); not (*type >= *type_of_value))
                //     error<except::TypeMismatch>(
                //         "Type mis-match! Parameter '" + name + "' "
                //         "expected type: " + type->text() + ", got: " + type_of_value->text()
                //     );

                value = typeCheck(value, type,
                    "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                );
            }

            // sg.addEnv({{name, {std::make_shared<Value>(value), type}}});
            args_env[name] = {std::make_shared<Value>(std::move(value)), std::move(type)};
        }


        std::vector<std::pair<std::string, type::TypePtr>> pos_params;
        for (const auto& [param, type] : std::views::zip(func.params, func.type.params)) pos_params.push_back({param, type});

        std::erase_if(pos_params, [&named_args = call->named_args] (const auto& p) {
            return std::ranges::find_if(named_args, [&p] (const auto& n) { return n.first == p.first; }) != named_args.cend();
        });

        std::vector<std::string> new_params;
        for (const auto& [name, _] : pos_params | std::views::drop(args_size)) new_params.push_back(name);

        std::vector<type::TypePtr> new_types;
        for (const auto& name : new_params) {
            type::TypePtr type;
            for (const auto& [n, t] : std::views::zip(func.params, func.type.params)) {
                if (n == name) {
                    type = t;
                    break;
                }
            }
            new_types.push_back(std::move(type));
        }

        type::FuncType func_type{std::move(new_types), func.type.ret};
        expr::Closure closure{std::move(new_params), func.body, std::move(func_type)};


        bool normal = true;
        if (is_variadic) {
            // const auto iter = std::ranges::find_if(pos_params, type::isVariadic, &std::pair::second);
            const auto iter = std::ranges::find_if(pos_params, [] (const auto& e) { return type::isVariadic(e.second); });
            const size_t variadic_index = std::distance(pos_params.begin(), iter);

            // call->args.erase(std::next(call->args.begin(), variadic_index));

            if (args_size > variadic_index) {
                normal = false;

                // FIX: first add the empty pack
                sg.addEnv({{pos_params[variadic_index].first, {std::make_shared<Value>(makePack()), pos_params[variadic_index].second}}});
                args_env[pos_params[variadic_index].first] = {std::make_shared<Value>(makePack()), std::move(pos_params[variadic_index]).second};
                // only then should you remove the parameter
                // previously, I only had the following. So the pack was left as undefined instead of empty
                pos_params.erase(std::next(pos_params.begin(), variadic_index));

                // we ignore the pack, so we consume an extra argument. Remove it from the carried function
                closure.params.erase(closure.params.begin());
                closure.type.params.erase(closure.type.params.begin());

                // for(size_t i{}, curr{}; const auto& [param, expr] : std::views::zip(pos_params, args)) {
                for (size_t i{}, p{}, curr{}; p < args_size; ++p, ++i) {

                    if (curr < expand_at.size() and i == expand_at[curr].first) {
                        for (auto& val : expand_at[curr++].second) {
                            // if (const auto& type_of_value = typeOf(val); not (*type >= *type_of_value))
                            //     error<except::TypeMismatch>(
                            //         "Type mis-match! Parameter '" + name + "' "
                            //         "expected type: " + type->text() + ", got: " + type_of_value->text()
                            //     );
                            auto& [name, type] = pos_params[p];
                            // if (findType(p, type)) type = validateType(std::move(type));
                            ++p;

                            val = typeCheck(val, type,
                                "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(val)->text()
                            );

                            // sg.addEnv({{name, {std::make_shared<Value>(val), type}}});
                            args_env[name] = {std::make_shared<Value>(std::move(val)), std::move(type)};
                        }
                        --p;
                    }
                    else {
                        auto& [name, type] = pos_params[p];
                        const auto& expr = args[i];
                        // if (findType(p, type)) type = validateType(std::move(type));

                        Value value;
                        if (type->text() == "Syntax") value = expr->variant();

                        else {
                            value = std::visit(*this, expr->variant());

                            value = typeCheck(value, type,
                                "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                            );

                            // if (const auto& type_of_value = typeOf(value); not (*type >= *type_of_value))
                            //     error<except::TypeMismatch>(
                            //         "Type mis-match! Parameter '" + name + "' "
                            //         "expected type: " + type->text() + ", got: " + type_of_value->text()
                            //     );
                        }
                        // sg.addEnv({{name, {std::make_shared<Value>(value), type}}});
                        args_env[name] = {std::make_shared<Value>(std::move(value)), std::move(type)};
                    }
                }
            }
        }

        if (normal) {
            const auto findType = [&func] (const size_t p, const type::TypePtr& type) {
                // it doesn't matter if there are multiple arguments with this name
                // `validateType` will choose the lastly-bounded one
                // we just need to proof that A parameter exists in order to call `validateType`
                for (size_t i{}; i <= p; ++i)
                    if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(func.params[i])}))
                        return true;

                // // look in the arguments env (from a partially evaluated function that yielded this function)
                // for (const auto& [key, _] : func.args_env)
                for (const auto& [key, _] : func.env)
                    if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(key)}))
                        return true;

                return false;
            };

            // for(size_t i{}, curr{}; const auto& [param, expr] : std::views::zip(pos_params, args)) {
            for (size_t i{}, p{}, curr{}; p < args_size; ++p, ++i) {

                if (curr < expand_at.size() and i == expand_at[curr].first) {
                    for (auto& val : expand_at[curr++].second) {
                        auto& [name, type] = pos_params[p];
                        if (findType(p, type)) {
                            // ScopeGuard sg{this, func.args_env, args_env};
                            ScopeGuard sg{this, func.env, args_env};
                            type = validateType(std::move(type));
                        }
                        ++p;

                        val = typeCheck(val, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(val)->text()
                        );

                        // sg.addEnv({{name, {std::make_shared<Value>(val), type}}});
                        args_env[name] = {std::make_shared<Value>(std::move(val)), std::move(type)};
                    }
                    --p;
                }
                else {
                    auto& [name, type] = pos_params[p];
                    if (findType(p, type)) {
                        // ScopeGuard sg{this, func.args_env, args_env};
                        ScopeGuard sg{this, func.env, args_env};
                        type = validateType(std::move(type));
                    }

                    const auto& expr = args[i];

                    Value value;
                    if (type->text() == "Syntax") value = expr->variant();
                    else {
                        value = std::visit(*this, expr->variant());

                        value = typeCheck(value, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                        );

                        // if (const auto& type_of_value = typeOf(value); not (*type >= *type_of_value))
                        //     error<except::TypeMismatch>("Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + type_of_value->text());
                    }

                    // sg.addEnv({{name, {std::make_shared<Value>(value), type}}});
                    args_env[name] = {std::make_shared<Value>(std::move(value)), std::move(type)};
                }
            }
        }



        // printEnv(args_env);
        // closure.captureArgs(args_env);
        closure.capture(args_env);
        return closure;
    }


    Value constructorCall(const expr::Call *call, Value var) {
        const auto type = validateType(std::get<type::TypePtr>(var));


        if (not type::isClass(type)) {
            if (not call->args.empty()) error("Can't pass arguments to non-class types: " + call->stringify());
            if (type::isFunction(type)) error("Can't default-construct a function type: " + call->stringify());

            if (type::isBuiltin(type)) {
                auto type_name = type->text();

                if (type_name == "Bool"  ) return bool  {};
                if (type_name == "Int"   ) return int   {};
                if (type_name == "Double") return double{};
                if (type_name == "String") return std::string{};

                error("Can't default construct type '" + type_name + "': " + call->stringify());
            }

            if (type::isVariadic(type)) return makePack();
            if (type::isList    (type)) return makeList();
            if (type::isMap     (type)) return makeMap ();


            if (auto onion = type::isUnion(type)) {
                if (onion->types.empty()) error("Cannot default construct type 'Never', or 'union { }': " + call->stringify());

                return constructorCall(call, onion->types[0]); // construct the first type in the union
            }
        }

        const auto *cls = dynamic_cast<const type::LiteralType*>(type.get())->cls.get();

        if (call->args.size() > cls->blueprint->members.size())
            error("Too many arguments passed to constructor of class: " + stringify(type));

        // copying defaults field values from class definition
        auto members = cls->blueprint->members;
        for (auto& [_, __, value_ptr] : members)
            value_ptr = std::make_shared<Value>(*value_ptr);

        Object obj{type, std::make_shared<Members>(std::move(members)) };

        // I woulda used a range for-loop but I need `arg` to be a reference and `value` to be a const ref
        // const auto& [arg, value] : std::views::zip(call->args, obj->members)
        for (size_t i{}; i < call->args.size(); ++i) {
            const auto& v = std::visit(*this, call->args[i]->variant());

            typeCheck(v, get<type::TypePtr>(obj.second->members[i]),
                "Type mis-match in constructor of:\n" + stringify(type) + "\nMember `" +
                get<expr::Name>(obj.second->members[i]).stringify() + "` expected: " + get<type::TypePtr>(obj.second->members[i])->text() + "\n"
                "but got: " + call->args[i]->stringify() + " which is " + typeOf(v)->text()
            );

            *get<ValuePtr>(obj.second->members[i]) = v;
        }

        return obj;
    }


    Value operator()(const expr::Closure *c) {
        if (const auto& var = getVar(c->stringify()); var) return var->first;

        expr::Closure closure = *c; // copy to use for fix the types

        // for (auto& type : closure.type.params) {
        for (size_t i{}; i < closure.type.params.size(); ++i) {
            auto& type = closure.type.params[i];

            bool found{};
            for (size_t j{}; j < i; ++j) {
                auto t = type::ExprType{std::make_shared<expr::Name>(closure.params[j])};
                if (type->involvesT(t)) { found = true; break; }
            }

            if (not found) type = validateType(std::move(type));
        }

        if (
            std::ranges::find_if(
                closure.params, [&type = closure.type.ret](const auto& p) {
                    return type->involvesT(type::ExprType{std::make_shared<expr::Name>(p)});
                }
            ) == closure.params.cend()
        )
            closure.type.ret = validateType(std::move(closure.type.ret));

        // for (auto& type : closure.type.params) { type = validateType(std::move(type)); }
        // closure.type.ret = validateType(std::move(closure.type.ret));

        return closure;
    }


    Value operator()(const expr::Block *block) {
        if (const auto& var = getVar(block->stringify()); var) return var->first;


        ScopeGuard sg{this};


        bool last_expr_is_block{};

        Value ret;
        for (const auto& line : block->lines) {
            last_expr_is_block = dynamic_cast<const expr::Block*>(line.get());

            ret = std::visit(*this, line->variant()); // a scope's value is the last expression

            // if any expression above breaks or continues, stop execution
            if (broken or continued) break;
        }



        if (not last_expr_is_block and std::holds_alternative<expr::Closure>(ret))
            captureEnvForClosure(get<expr::Closure>(ret));

        return ret;
    }


    Value operator()(const expr::Fix *fix) {
        if (const auto& var = getVar(fix->stringify()); var) return var->first;
        // return std::visit(*this, fix->func->variant());

        auto func = dynamic_cast<expr::Closure*>(fix->funcs[0].get());

        for (auto& t : func->type.params) t = validateType(std::move(t));

        func->type.ret = validateType(std::move(func)->type.ret);

        ops.at(fix->name)->funcs.push_back(fix->funcs[0]); // assuming each fix expression has a single func in it

        return *func;
        // return std::visit(*this, fix->funcs[0]->variant());
    }


    [[nodiscard]] bool isBuiltin(const std::string_view func) const {
        const auto make_builtin = [] (const std::string& n) { return "__builtin_" + n; };

        for(const auto& builtin : {
            //* variadic
            "print", "concat", 

            //* nullary
            "panic", "true", "false", "input_str", "input_int",

            //* unary
            "type_of", "len", "reset", "eval","neg", "not", "to_int", "to_double", "to_string", //"read_file"

            //* binary
            "get", "push", "pop",
            "add", "sub", "mul", "div", "mod", "pow", "gt", "geq", "eq", "leq", "lt", "and", "or",  

            //* trinary
            "set",
            "conditional",

            //* quaternary 
            "str_slice", // (str, start, end, step), should I add anothee overload for (str, start, length)??


            // //* File IO
            // "read_whole", "read_line", "read_lines"
        })
            if (make_builtin(builtin) == func) return true;

        return false;
    }


    // the gate into the META operators!
    Value evaluateBuiltin(
        const std::vector<expr::ExprPtr> args,
        const std::vector<std::pair<size_t, std::vector<Value>>> expand_at,
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
                S<"len">,
                Func<"len",
                    decltype([](const auto& x, const auto&) {
                        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, std::string>)
                             return static_cast<ssize_t>(x.length());

                        else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, value::PackList>)
                            return static_cast<ssize_t>(x->values.size());

                        else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, value::ListValue>)
                            return static_cast<ssize_t>(x.elts->values.size());
                        else { // map value
                            return static_cast<ssize_t>(x.items->map.size());
                        }
                    }),
                    TypeList<std::string>,
                    TypeList<value::PackList>,
                    TypeList<value::ListValue>,
                    TypeList<value::MapValue>
                >
            >{},

            MapEntry<
                S<"type_of">,
                Func<"type_of",
                    decltype([](const auto& x, const auto& that) {
                        return that->typeOf(x);
                    }),
                    TypeList<Any>
                >
            >{},

            MapEntry<
                S<"neg">,
                Func<"neg",
                    decltype([](const auto& x, const auto&) { return -x; }),
                    TypeList<ssize_t>,
                    TypeList<double>
                >
            >{},

            MapEntry<
                S<"not">,
                Func<"not",
                    decltype([](const auto& x, const auto&) { return not x; }),
                    TypeList<bool>
                >
            >{},

            MapEntry<
                S<"to_int">,
                Func<"to_int",
                    decltype([](const auto& x, const auto&) -> ssize_t {
                        if constexpr (
                            std::is_same_v<std::remove_cvref_t<decltype(x)>, ssize_t> or
                            std::is_same_v<std::remove_cvref_t<decltype(x)>, double> or
                            std::is_same_v<std::remove_cvref_t<decltype(x)>, bool>
                        ) return x;

                        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, std::string>)
                            return std::stoll(x);
                    }),
                    TypeList<ssize_t>,
                    TypeList<double>,
                    TypeList<bool>,
                    TypeList<std::string>
                >
            >{},

            MapEntry<
                S<"to_double">,
                Func<"to_double",
                    decltype([](const auto& x, const auto&) -> double {
                        if constexpr (
                            std::is_same_v<std::remove_cvref_t<decltype(x)>, ssize_t> or
                            std::is_same_v<std::remove_cvref_t<decltype(x)>, double> or
                            std::is_same_v<std::remove_cvref_t<decltype(x)>, bool>
                        ) return x;


                        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, std::string>)
                        return std::stod(x);
                    }),
                    TypeList<ssize_t>,
                    TypeList<double>,
                    TypeList<bool>,
                    TypeList<std::string>
                >
            >{},

            MapEntry<
                S<"to_string">,
                Func<"to_string",
                    decltype([](const auto& x, const auto&) { return stringify(x); }),
                    TypeList<Any>
                >
            >{},

            MapEntry<
                S<"eval">,
                Func<"eval",
                    decltype([](const auto& x, const auto& that) {
                        return std::visit(*that, x);
                    }),
                    TypeList<expr::Node>
                >
            >{},

            MapEntry<
                S<"pop">,
                Func<"pop",
                    decltype([](const auto& cont, const auto&) -> Value {
                        const auto back = cont.elts->values.back();
                        cont.elts->values.pop_back();
                        return back;
                    }),
                    TypeList<ListValue>
                >
            >{},


            //* BINARY FUNCTIONS
            MapEntry<
                S<"get">,
                Func<"get",
                    decltype([](const auto& a, const auto& ind, const auto&) -> Value {
                        using T = std::remove_cvref_t<decltype(a)>;

                        if constexpr (std::is_same_v<T, ListValue>) {
                            if (ind < 0 or size_t(ind) >= a.elts->values.size())
                                error("Accessing list '" + stringify(a) + "' at index '" + std::to_string(ind) + "' which is out of bounds!");

                            return a.elts->values[ind]; 
                        }

                        else if constexpr (std::is_same_v<T, MapValue>) {
                            auto key = stringify(ind);
                            if (not a.items->map.contains(key))
                                error("Accessing Map '" + stringify(a) + "' at key '" + key + "' which doesn't exist!");

                            return a.items->map.at(key);
                        }

                        else { // if constexpr (std::is_same_v<std::remove_cvref_t<decltype(a)>, std::string>) {
                            if (ind < 0 or size_t(ind) >= a.length())
                                error("Accessing string '" + a + "' at index '" + std::to_string(ind) + "' which is out of bounds!");
                            return std::string{a[ind]};
                        }
                    }),
                    TypeList<ListValue, ssize_t>,
                    TypeList<MapValue, Any>,
                    TypeList<std::string, ssize_t>
                >
            >{},

            MapEntry<
                S<"set">,
                Func<"set",
                    decltype([](const auto& cont, const auto& at, const auto& elt, const auto&) -> Value {
                        using T = std::remove_cvref_t<decltype(cont)>;

                        if constexpr (std::is_same_v<T, ListValue>) {
                            if (at < 0 or size_t(at) >= cont.elts->values.size())
                                error("Accessing list '" + stringify(cont) + "' at index '" + std::to_string(at) + "' which is out of bounds!");

                            return cont.elts->values[at] = elt;
                        }

                        else if constexpr (std::is_same_v<T, MapValue>) {
                            auto key = stringify(at);
                            return cont.items->map[key] = elt;
                        }
                    }),
                    TypeList<ListValue, ssize_t, Any>,
                    TypeList<MapValue, Any, Any>
                    // TypeList<std::string, ssize_t>
                >
            >{},

            MapEntry<
                S<"push">,
                Func<"push",
                    decltype([](const auto& cont, const auto& elt, const auto&) -> Value {
                        cont.elts->values.push_back(elt);
                        return elt;
                    }),
                    TypeList<ListValue, Any>
                >
            >{},

            MapEntry<
                S<"add">,
                Func<"add",
                    decltype([](const auto& a, const auto& b, const auto&) { return a + b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"sub">,
                Func<"sub",
                    decltype([](const auto& a, const auto& b, const auto&) { return a - b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"mul">,
                Func<"mul",
                    decltype([](const auto& a, const auto& b, const auto&) { return a * b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"div">,
                Func<"div",
                    decltype([](const auto& a, const auto& b, const auto&) { return a / b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"mod">,
                Func<"mod",
                    decltype([](const auto& a, const auto& b, const auto&) { return a % b; }),
                    TypeList<ssize_t, ssize_t>
                >
            >{},

            MapEntry<
                S<"pow">,
                Func<"pow",
                    decltype(
                        [](const auto& a, const auto& b, const auto&) -> std::common_type_t<decltype(a), decltype(b)> { return std::pow(a, b); }
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
                    decltype([](const auto& a, const auto& b, const auto&) { return a > b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"geq">,
                Func<"geq",
                    decltype([](const auto& a, const auto& b, const auto&) { return a >= b; }),
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
                    decltype([](const auto& a, const auto& b, const auto&) { return a <= b; }),
                    TypeList<ssize_t, ssize_t>,
                    TypeList<ssize_t, double>,
                    TypeList<double, ssize_t>,
                    TypeList<double, double>
                >
            >{},

            MapEntry<
                S<"lt">,
                Func<"lt",
                    decltype([](const auto& a, const auto& b, const auto&) { return a < b; }),
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


        if (name == "panic") {
            for (const auto& arg : args) {
                std::clog << stringify(std::visit(*this, arg->variant())) << ' ';
            }
            error<std::runtime_error, false>("", {});
        }


        if (name == "true")  {
            arity_check(0);
            return execute<0>(stdx::get<S<"true">>(functions).value, {}, this);
        }

        if (name == "false") {
            arity_check(0);
            return execute<0>(stdx::get<S<"false">>(functions).value, {}, this);
        }

        if (name == "input_str") {
            arity_check(0);
            return execute<0>(stdx::get<S<"input_str">>(functions).value, {}, this);
        }

        if (name == "input_int") {
            arity_check(0);
            return execute<0>(stdx::get<S<"input_int">>(functions).value, {}, this);
        }



        // for now, can only implement variadic functions inlined in this function
        // seems like functions with default named parameters can only be implmented this way for now
        // need a way to sepcify:
        /* // TODO
            {
                name: "print",
                arg_count: VARIADIC,
                code: [] () {},
                default named: {
                    {"end", "\n"},
                    {"sep", " "},
                }
            }
        */
        // would be nice if the system above could be done for member functions on premitive types (Int, Double, String, etc...)
        if (name == "print") {
            return builtinPrint(args, expand_at, named_args);
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



        if (name == "type_of"  ) return execute<1>(stdx::get<S<"type_of"   >>(functions).value, {value1}, this);
        if (name == "len"      ) return execute<1>(stdx::get<S<"len"       >>(functions).value, {value1}, this);
        if (name == "eval"     ) return execute<1>(stdx::get<S<"eval"      >>(functions).value, {value1}, this);
        if (name == "neg"      ) return execute<1>(stdx::get<S<"neg"       >>(functions).value, {value1}, this);
        if (name == "not"      ) return execute<1>(stdx::get<S<"not"       >>(functions).value, {value1}, this);
        if (name == "pop"      ) return execute<1>(stdx::get<S<"pop"       >>(functions).value, {value1}, this);
        if (name == "to_int"   ) return execute<1>(stdx::get<S<"to_int"    >>(functions).value, {value1}, this);
        if (name == "to_double") return execute<1>(stdx::get<S<"to_double" >>(functions).value, {value1}, this);
        if (name == "to_string") return execute<1>(stdx::get<S<"to_string" >>(functions).value, {value1}, this);


        // all the rest of those funcs expect 2 arguments
        using std::operator""sv;

        const auto eager = {"get"sv, "push"sv, "add"sv, "sub"sv, "mul"sv, "div"sv, "mod"sv, "pow"sv, "gt"sv, "geq"sv, "eq"sv, "leq"sv, "lt"sv};
        if (std::ranges::find(eager, name) != eager.end()) {
            arity_check(2);
            const auto& value2 = std::visit(*this, args[1]->variant());

            // this is disgusting..I know
            if (name == "get" ) return execute<2>(stdx::get<S<"get" >>(functions).value, {value1, value2}, this);
            if (name == "push") return execute<2>(stdx::get<S<"push">>(functions).value, {value1, value2}, this);

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


            return std::visit(*this, otherwise);
        }

        if (name == "set") {
            arity_check(3);
            const auto& value2 = std::visit(*this, args[1]->variant());
            const auto& value3 = std::visit(*this, args[2]->variant());

            return execute<3>(stdx::get<S<"set">>(functions).value, {value1, value2, value3}, this);
        }

        if (name == "str_slice") {
            arity_check(4);
            const auto& start_v  = std::visit(*this, args[1]->variant());
            const auto& end_v    = std::visit(*this, args[2]->variant());
            const auto& stride_v = std::visit(*this, args[3]->variant());

            if (
                not std::holds_alternative<std::string>(value1  ) or
                not std::holds_alternative<ssize_t    >( start_v) or
                not std::holds_alternative<ssize_t    >(   end_v) or
                not std::holds_alternative<ssize_t    >(stride_v)
            )
            error(
                "Invalid argument: __builtin_str_slice("
                + args[0]->stringify() + ", "
                + args[1]->stringify() + ", "
                + args[2]->stringify() + ", "
                + args[3]->stringify() + ", "
            );

            const auto& str   = get<std::string>(value1);
                  auto start  = std::max<ssize_t>  (get<ssize_t>(start_v), 0);
            const auto end    = std::clamp<ssize_t>(get<ssize_t>(  end_v), 0, ssize_t(str.length()));
            const auto stride = get<ssize_t>(stride_v);

            std::string ret;
            for (; start < end; start += stride)
                ret += str[start];

            return ret;
        }


        // const auto& value3 = std::visit(*this, call->args[2]->variant());
        // if (name == "conditional") return execute<3>(stdx::get<S<"conditional">>(functions).value, {value1, value2, value3}, this);


        error("Calling a builtin fuction that doesn't exist!");
    }


    Value builtinPrint(
        const std::vector<expr::ExprPtr>& args,
        const std::vector<std::pair<size_t, std::vector<Value>>>& expand_at,
        const std::unordered_map<std::string, expr::ExprPtr>& named_args
    ) {
        if (args.empty()) error("'print' requires at least 1 positional argument passed!");

        using std::operator""sv;
        const auto allowed_params = {"sep"sv, "end"sv};

        for (const auto& [name, _] : named_args)
            if (std::ranges::find(allowed_params, name) == allowed_params.end())
                error("Can only have the named argument 'end'/'sep' in call to '__builtin_print': found '" + name + "'!");


        const Value sep = named_args.contains("sep") ? std::visit(*this, named_args.at("sep")->variant()) : " ";

        constexpr bool no_newline = false;

        std::optional<Value> separator;
        Value ret;
        for(size_t i{}, curr{}; auto& arg : args) {
            if (curr < expand_at.size() and i++ == expand_at[curr].first) {
                for (const auto& e : expand_at[curr++].second) {
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



    void print(const Value& value, const bool new_line = true) const { std::print("{}{}", stringify(value), new_line? '\n' : '\0'); }


    type::TypePtr validateType(const type::TypePtr& type) {
        if (type::shouldReassign(type)) return type::builtins::Any();

        //* comment this if statement if you want builtin types to remain unchanged even when they're assigned to
        if (auto var = getVar(type->text()); var) {

            if (auto t = typeOf(var->first); not type::isType(t)) {
                // error("'" + stringify(var->first) + "' does not name a type!");

                if (type::isFunction(t))
                    return std::make_shared<type::ConceptType>(std::make_shared<value::Value>(std::move(var)->first));

                return std::make_shared<type::ValueType>(std::make_shared<value::Value>(std::move(var)->first));
            }

            if (std::holds_alternative<type::TypePtr>(var->first))
                return get<type::TypePtr>(var->first);
        }
        else if (type::isBuiltin(type)) return type;


        if (const auto var_type = dynamic_cast<type::ExprType*>(type.get())) {
            // if (type::isBuiltin(type)) return type;

            auto value = std::visit(*this, var_type->t->variant()); // evaluate type expression

            if (std::holds_alternative<type::TypePtr>(value))
                return get<type::TypePtr>(value);

            return 
            std::make_shared<type::ValueType>(
                std::make_shared<value::Value>(std::move(value))
            );
        }

        else if (type::isClass(type) or type::isUnion(type)) return type;

        else if (type::isFunction(type)) {
            const auto func_type = dynamic_cast<type::FuncType*>(type.get());

            for (auto& t : func_type->params) t = validateType(std::move(t));

            // all param types are valid. Only thing left to check is return type
            func_type->ret = validateType(std::move(func_type)->ret);

            return type;
        }
        else if (type::isVariadic(type)) {
            const auto variadic_type = dynamic_cast<type::VariadicType*>(type.get());

            // todo: allow this in the future
            if (variadic_type->type->text() == "Syntax") error("Variadics of 'Syntax' is not allowed!");

            variadic_type->type = validateType(std::move(variadic_type)->type);

            return type;
        }
        else if (type::isList(type)) {
            const auto list_type = dynamic_cast<type::ListType*>(type.get());

            // todo: allow this in the future
            if (list_type->type->text() == "Syntax") error("List of 'Syntax' is not allowed!");


            if (type::isVariadic(list_type->type)) error("Lists of variadics types are not allowed!");


            list_type->type = validateType(std::move(list_type)->type);

            return type;
        }
        else if (type::isMap(type)) {
            const auto map_type = dynamic_cast<type::MapType*>(type.get());

            // todo: allow this in the future
            if (map_type->key_type->text() == "Syntax") error("Map of 'Syntax' is not allowed!");
            if (map_type->val_type->text() == "Syntax") error("Map of 'Syntax' is not allowed!");


            if (type::isVariadic(map_type->key_type)) error("Map of variadics types are not allowed!");
            if (type::isVariadic(map_type->val_type)) error("Map of variadics types are not allowed!");


            map_type->key_type = validateType(std::move(map_type)->key_type);
            map_type->val_type = validateType(std::move(map_type)->val_type);

            return type;
        }



        error("'" + type->text() + "' does not name a type!");
    }


    type::TypePtr typeOf(const Value& value) const {
        if (std::holds_alternative<expr::Node > (value)) return type::builtins::Syntax();
        if (std::holds_alternative<ssize_t    > (value)) return type::builtins::Int();
        if (std::holds_alternative<double     > (value)) return type::builtins::Double();
        if (std::holds_alternative<bool       > (value)) return type::builtins::Bool();
        if (std::holds_alternative<std::string> (value)) return type::builtins::String();

        // Type types
        // if (std::holds_alternative<ClassValue > (value)) return type::builtins::Type();
        // if (std::holds_alternative<expr::Union> (value)) return type::builtins::Type();
        if (std::holds_alternative<type::TypePtr> (value)) return type::builtins::Type();

        if (std::holds_alternative<expr::Closure>(value)) {
            const auto& func = get<expr::Closure>(value);

            type::FuncType type{{}, {}};
            for (const auto& t : func.type.params)
                type.params.push_back(t);


            type.ret = func.type.ret;
            return std::make_shared<type::FuncType>(std::move(type));
        }

        if (std::holds_alternative<Object>(value)) {
            // return std::make_shared<type::LiteralType>(std::make_shared<ClassValue>(get<Object>(value).first));
            // ! check if type isn't a class value
            return std::make_shared<type::LiteralType>(
                std::make_shared<ClassValue>(
                    *dynamic_cast<const type::LiteralType*>(get<Object>(value).first.get())->cls
                )
            );
        }

        if (std::holds_alternative<PackList>(value)) {
            auto values = std::ranges::fold_left(
                get<PackList>(value)->values,
                std::vector<type::TypePtr>{},
                [this] (auto acc, const auto& elt) {
                    acc.push_back(typeOf(elt));
                    return acc;
                }
            );


            if (values.empty()) return std::make_shared<type::VariadicType>(type::builtins::_());

            const bool same = std::ranges::all_of(values, [tp = values[0]] (const auto& t) { return *t == *tp; });

            // if (same) return std::make_shared<type::VariadicType>(std::move(values)[0]);
            if (same) return type::VariadicOf(std::move(values)[0]);

            // return std::make_shared<type::VariadicType>(type::builtins::Any());
            return type::VariadicOf(type::builtins::Any());
            // return same ? std::make_shared<type::VariadicType>(values[0]) : non_typed_pack;
        }

        if (std::holds_alternative<ListValue>(value)) {
            auto values = std::ranges::fold_left(
                get<ListValue>(value).elts->values,
                std::vector<type::TypePtr>{},
                [this] (auto acc, const auto& elt) {
                    acc.push_back(typeOf(elt));
                    return acc;
                }
            );


            // if (values.empty()) return std::make_shared<type::ListType>(type::builtins::_());
            if (values.empty()) return type::ListOf(type::builtins::_());

            const bool same = std::ranges::all_of(values, [tp = values[0]] (const auto& t) { return *t == *tp; });

            // if (same) return std::make_shared<type::ListType>(std::move(values)[0]);
            if (same) return type::ListOf(std::move(values)[0]);

            return type::ListOf(type::builtins::Any());
        }

        if (std::holds_alternative<MapValue>(value)) {
            auto values = std::ranges::fold_left(
                get<MapValue>(value).items->map,
                std::vector<std::pair<type::TypePtr, type::TypePtr>>{},
                [this] (auto acc, const auto& elt) {
                    acc.push_back({typeOf(elt.first), typeOf(elt.second), });

                    return acc;
                }
            );


            if (values.empty()) return std::make_shared<type::MapType>(type::builtins::_(), type::builtins::_());

            const bool same_key = std::ranges::all_of(values, [tp = values[0].first ] (const auto& t) { return *t.first  == *tp; });
            const bool same_val = std::ranges::all_of(values, [tp = values[0].second] (const auto& t) { return *t.second == *tp; });

            if (same_key and same_val)
                // return std::make_shared<type::MapType>(std::move(values)[0].first, std::move(values)[0].second);
                return type::MapOf(std::move(values)[0].first, std::move(values)[0].second);

            if (same_key)
                // return std::make_shared<type::MapType>(std::move(values)[0].first, type::builtins::Any()       );
                return type::MapOf(std::move(values)[0].first, type::builtins::Any()       );

            if (same_val)
                // return std::make_shared<type::MapType>(type::builtins::Any()     , std::move(values)[0].second);
                return type::MapOf(type::builtins::Any()     , std::move(values)[0].second);

                // return std::make_shared<type::MapType>(type::builtins::Any()     , type::builtins::Any()      );
            return type::MapOf(type::builtins::Any()     , type::builtins::Any()      );
        }

        if (std::holds_alternative<NameSpace>(value)) return std::make_shared<type::SpaceType>();


        error("Unknown Type for value: " + stringify(value));
    }


    Value eval(expr::ExprPtr& expr) { return std::visit(*this, expr->variant()); }


    struct ScopeGuard {
        Visitor* v;


        template <std::same_as<Environment>... E>
        ScopeGuard(Visitor* t, const E&... es) noexcept : v{t} {
            v->scope();

            (addEnv(es), ...);
        }

        template <std::same_as<Environment>... E>
        ScopeGuard(Visitor* t, Visitor::EnvTag tag, const E&... es) noexcept : v{t} {
            v->scope(tag);

            (addEnv(es), ...);
        }

        void addEnv(const Environment& e) {
            for (const auto& [name, var] : e) {
                const auto& [value, type] = var;
                v->addVar(name, *value, type);
            }
        }


        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;

        ScopeGuard(ScopeGuard&& other) noexcept {
            v = other.v;
            other.v = nullptr;
        }

        ScopeGuard& operator=(ScopeGuard and other) noexcept {
            v = other.v;
            other.v = nullptr;
            return *this;
        }

        ~ScopeGuard() { if (v) v->unscope(); }
    };


    void scope(Visitor::EnvTag tag = Visitor::EnvTag::NONE) {
        env.push_back({{}, tag});
    }

    void unscope() { env.pop_back(); }

    Value addVar(const std::string& name, const Value& v, const type::TypePtr& t = type::builtins::Any()) {
        // if (const auto cls = type::isClass(t)) {
        //     auto obj = get<value::Object>(v);
        //     obj.second = std::make_shared<Members>(obj.second->members);

        //     std::erase_if(
        //         obj.second->members,
        //         [cls] (const auto& member) {
        //             const auto& [name, _, __] = member;

        //             return std::ranges::find_if(cls->cls->blueprint->members, [&name](const auto& field) {
        //                 return get<expr::Name>(field).name == name.name;
        //             }) == cls->cls->blueprint->members.cend();
        //         }
        //     );

        //     env.back().first[name] = {std::make_shared<Value>(obj), t};
        //     return obj;
        // }
        // else {
        // }
        env.back().first[name] = {std::make_shared<Value>(v  ), t};
        return v;
    }

    void addEnv(const Environment& e) {
        for (const auto& [key, var] : e) {
            const auto& [value, type] = var;

            env.back().first[key] = {value, type};
        }
    }

    std::optional<std::pair<Value, type::TypePtr>> getVar(const std::string& name) const {
        for (auto rev_it = env.crbegin(); rev_it != env.crend(); ++rev_it) {
            if (rev_it->first.contains(name)) {
                const auto& [value_ptr, type_ptr] = rev_it->first.at(name);
                return {{*value_ptr, type_ptr}};
            }
        }

        return {};
    }

    bool changeVar(const std::string& name, const value::Value& v) {
        for (auto rev_it = env.rbegin(); rev_it != env.rend(); ++rev_it)
            if (rev_it->first.contains(name)) {
                const auto& t = rev_it->first.at(name).second;
                (*rev_it).first[name] = {std::make_shared<value::Value>(v), t};
                return true;
            }

        return false;
    }

    std::optional<std::pair<Value, type::TypePtr>> globalLookup(const std::string& name) const {
        if (env[0].first.contains(name)) {
            const auto& [value_ptr, type_ptr] = env[0].first.at(name);
            return {{*value_ptr, type_ptr}};
        }

        return {};
    }

    void removeVar(const std::string& name) {
        for(auto& curr_env : env) curr_env.first.erase(name);
    }


    Environment envStackToEnvMap() const {
        Environment e;
        for(const auto& curr_env : env)
            for(const auto& [key, value] : curr_env.first)
                e[key] = value; // I want the recent values (higher in the stack) to be the ones captured
        return e;
    }


    static void printEnv(const Environment& e) noexcept {
        // const auto& e = envStackToEnvMap();

        for (const auto& [name, v] : e) {
            const auto& [value, type] = v;

            std::println("{}: {} = {}", name, type->text(), stringify(*value));
        }
    }
};


} // namespace interp
} // namespace pie