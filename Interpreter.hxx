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


#include "utils.hxx"
#include "Expr.hxx"
#include "Parser.hxx"
#include "Declarations.hxx"


#include <stdx/tuple.hpp>



struct Dict;

// carrys the default values to be copied into an object. essentially const
// carrys objects' values. could be changed

using Object = std::shared_ptr<Dict>; 

using Value = std::variant<int, double, bool, std::string, Closure, ClassValue, Object>;

struct Dict { std::vector<std::pair<std::string, Value>> members; };

using Environment = std::unordered_map<std::string, std::pair<Value, Type>>;



struct Visitor {
    // struct Builtin { std::string name; }; // for later

    std::vector<Environment> env;
    const Operators ops;


    // inline static const std::unordered_map<const char*, 


    Visitor(Operators ops) noexcept : env(1), ops{std::move(ops)} { }


    Value operator()(const Num *n) {
        if (auto&& var = getVar(n->stringify(0)); var) return *var;


        // have to do an if rather than ternary so the return value isn't always coerced into doubles
        if (n->num.find('.') != std::string::npos) return std::stod(n->num);
        else return std::stoi(n->num);
    }

    Value operator()(const String *s) {
        if (auto&& var = getVar(s->stringify(0)); var) return *var;

        return s->str; 
    }

    Value operator()(const Name *n) {
        // what should builtins evaluate to?
        // If I return the string back, then expressions like `"__builtin_neg"(1)` are valid now :)))))
        // interesting!
        // how about a special value?
        if (isBuiltIn(n->name)) return n->name;

        if (auto&& opt = getVar(n->name); opt) return *opt;


        error("Name \"" + n->name + "\" is not defined");
    }

    Value operator()(const Assignment *ass) {
        const auto& value = std::visit(*this, ass->rhs->variant());

        // assigning to x.y should never create a variable "x.y" bu access x and change y;
        if (const auto acc = dynamic_cast<Access*>(ass->lhs.get())) {
            const auto& left = std::visit(*this, acc->var->variant());

            if (not std::holds_alternative<Object>(left)) error("Can't access a non-class type!");

            const auto& obj = get<Object>(left);

            const auto& found = std::ranges::find_if(obj->members, [name = acc->name](auto&& member) { return member.first == name; });

            if (found == obj->members.end()) error("Name '" + acc->name + "' doesn't exist in object!");

            found->second = value;

            // for (auto&& [name, member_value] : obj->members) {
            //     if (name == acc->name) {
            //         member_value = value;
            //         break; // found the field. no need to keep iterating
            //     }

            //     // obj->members[acc->name] = std::visit(*this, ass->rhs->variant());
            // }

            return value;
            // return obj->members[acc->name];
        }

        if (const auto name = dynamic_cast<Name*>(ass->lhs.get())){

            if (name->type != "Any") {
                if(not isValidType(name->type)) error("Invalid Type: " + name->type);


                auto type = name->type;
                if (const auto& c = getVar(type); c and std::holds_alternative<ClassValue>(*c)) type = stringify(*c);


                if (const auto& type_of_value = typeOf(value); type != type_of_value) {
                    std::cerr << "In assignment: " << ass->stringify(0) << '\n';
                    error("Assignment type mis-match! Expected: " + type + ", got: " + type_of_value);
                }
            }


            return addVar(ass->lhs->stringify(0), value, name->type);
        }

        return addVar(ass->lhs->stringify(0), value);

        // return std::visit(
        //     [this, ass] (const auto& value) { return addVar(value->stringify(0), std::visit(*this, ass->rhs->variant())); },
        //     ass->lhs->variant()
        // );
    }


    Value operator()(const Class *cls) {
        if (auto&& var = getVar(cls->stringify(0)); var) return *var;


        std::vector<std::pair<std::string, Value>> members;

        for (const auto& field : cls->fields) {
            // const auto& name = std::visit(*this, field.lhs->variant());

            // lots of a std::move 's lol
            members.push_back({field.lhs->stringify(0),  std::visit(*this, field.rhs->variant())});
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


    Value operator()(const Access *acc) {

        const auto& left = std::visit(*this, acc->var->variant());

        if (not std::holds_alternative<Object>(left)) error("Can't access a non-class type!");

        const auto& obj = std::get<Object>(left);

        const auto& found = std::ranges::find_if(obj->members, [&name = acc->name] (auto&& member) { return member.first == name; });
        if (found == obj->members.end()) error("Name '" + acc->name + "' doesn't exist in object!");


        if (std::holds_alternative<Closure>(found->second)) {
            const auto& closure = get<Closure>(found->second);

            Environment capture_list;
            for (const auto& [name, value] : obj->members)
                capture_list[name] = {value, typeOf(value)};

            closure.capture(capture_list);

            return closure;
        }

        return found->second;
    }

    Value operator()(const UnaryOp *up) {
        if (auto&& var = getVar(up->stringify(0)); var) return *var;


        ScopeGuard sg{this}; // RAII is so cool
        // scope();

        const Fix* op = ops.at(up->text);
        Closure* func = dynamic_cast<Closure*>(op->func.get());

        if (not func) error("This shouldn't happen, but also Unary Operator not equal to a closure!");

        assert(func->params.size() == 1);
        // env.back()[func->params.front()] = std::visit(*this, up->expr->variant());
        addVar(func->params.front().first, std::visit(*this, up->expr->variant()));

        return std::visit(*this, func->body.get()->variant()); // RAII takes care of unscoping

        // unscope();
    }

    Value operator()(const BinOp *bp) {
        if (auto&& var = getVar(bp->stringify(0)); var) return *var;


        ScopeGuard sg{this};

        const Fix* op = ops.at(bp->text);
        Closure* func = dynamic_cast<Closure*>(op->func.get());

        if (not func) error("This shouldn't happen, but also Unary Operator not equal to a closure!");

        assert(func->params.size() == 2);
        addVar(func->params[0].first, std::visit(*this, bp->lhs->variant()));
        addVar(func->params[1].first, std::visit(*this, bp->rhs->variant()));
        // env.back()[func->params[0]] = std::visit(*this, bp->lhs->variant());
        // env.back()[func->params[1]] = std::visit(*this, bp->rhs->variant());

        return std::visit(*this, func->body.get()->variant());
    }

    Value operator()(const PostOp *pp) {
        if (auto&& var = getVar(pp->stringify(0)); var) return *var;


        ScopeGuard sg{this};

        const Fix* op = ops.at(pp->text);
        Closure* func = dynamic_cast<Closure*>(op->func.get());

        if (not func) error("This shouldn't happen, but also Unary Operator not equal to a closure!");

        assert(func->params.size() == 1);
        addVar(func->params.front().first, std::visit(*this, pp->expr->variant()));
        // env.back()[func->params.front()] = std::visit(*this, pp->expr->variant());

        return std::visit(*this, func->body.get()->variant());
    }

    Value operator()(const Call* call) {
        if (auto&& var = getVar(call->stringify(0)); var) return *var;


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
                Closure closure{std::vector<std::pair<std::string, Type>>{func.params.begin() + args_size, func.params.end()}, func.return_type, func.body};

                Environment argument_capture_list = func.env;
                for(const auto& [param, expr] : std::views::zip(func.params, call->args)) {
                    const auto& [name, type] = param;
                    const auto& value = std::visit(*this, expr->variant());
                    if (const auto& type_of_value = typeOf(value); type != "Any" and type_of_value != type) error("Type mis-match! Expected: " + type + ", got: " + type_of_value);

                    argument_capture_list[name] = {value, type};
                }

                closure.capture(argument_capture_list);

                return closure;
            }


            // full call. Don't curry!
            for (const auto& [param, arg] : std::views::zip(func.params, call->args)){
                const auto& [name, type] = param;
                const auto& value = std::visit(*this, arg->variant());
                if (const auto& type_of_value = typeOf(value); type != "Any" and type_of_value != type) error("Type mis-match! Expected: " + type + ", got: " + type_of_value);

                addVar(name, std::visit(*this, arg->variant()));
            }
                // env.back()[param] = std::visit(*this, arg->variant());

            ScopeGuard sg{this, func.env};

            const auto& ret = std::visit(*this, func.body->variant());

            const auto& type_of_ret = typeOf(ret);


            if(not isValidType(func.return_type)) error("Invalid Type: " + func.return_type);

            auto return_type = func.return_type;
            if (const auto& c = getVar(return_type); c and std::holds_alternative<ClassValue>(*c)) return_type = stringify(*c);

            if (type_of_ret != return_type) error("Type mis-match! Expected: " + return_type + ", got: " + type_of_ret);

            return ret;
        }

        if (std::holds_alternative<ClassValue>(var)) {
            const auto& cls = std::get<ClassValue>(var);

            Object obj{ std::make_shared<Dict>(cls.blueprint->members) }; // copying defaults field values from class definition

            return obj;
        }

        // error("operator() applied on not-a-function");
        // applied on a value. Just return the value for now
        // maybe I should make it so that x(1) == { t = x; x = 1; t; }; ...?

        if (call->args.size() != 0) error("Can't pass arguments to values!");
        return var;
    }

    Value operator()(const Closure *c) {
        if (auto&& var = getVar(c->stringify(0)); var) return *var;

        // take a snapshot of the current env (capture by value). comment this line if you want capture by reference..
        c->capture(envStackToEnvMap());
        return *c;
    }


    Value operator()(const Block *block) {
        if (auto&& var = getVar(block->stringify(0)); var) return *var;


        ScopeGuard sg{this};

        Value ret;
        for (const auto& line : block->lines) 
            ret = std::visit(*this, line->variant()); // a scope's value is the last expression

        return ret;
    }

    Value operator()(const Fix *fix) {
        if (auto&& var = getVar(fix->stringify(0)); var) return *var;

        return std::visit(*this, fix->func->variant());
    }

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



        if (name == "print" or name == "neg" or name == "not" or name == "reset") arity_check(1); // just for now..


        // evaluating arguments from left to right as needed
        // first argument is always evaluated
        const auto& value1 = std::visit(*this, call->args[0]->variant());

        // Since this is a meta function that operates on AST nodes rather than values
        // it gets its special treatment here..
        if (name == "reset") {

            const std::string& s = call->args[0]->stringify(0);
            if (const auto& v = getVar(s); not v) error("Reseting an unset value: " + s);
            else removeVar(s);

            // return std::stoi(num->num);
            return value1;
        }


        if (name == "print") return execute<1>(stdx::get<S<"print">>(functions).value, {value1}, this);

        if (name == "neg") return execute<1>(stdx::get<S<"neg">>(functions).value, {value1}, this);

        if (name == "not") return execute<1>(stdx::get<S<"not">>(functions).value, {value1}, this);


        // all the rest of those funcs expect 2 arguments

        using std::operator""sv;

        const auto eager = {"add"sv, "sub"sv, "mul"sv, "div"sv, "gt"sv, "geq"sv, "eq"sv, "leq"sv, "lt"sv};
        if (std::ranges::find(eager, name) != eager.end()) {
            arity_check(2);
            const auto& value2 = std::visit(*this, call->args[1]->variant());

            // this is disgusting..I know
            if (name == "add") return execute<2>(stdx::get<S<"add">>(functions).value, {value1, value2}, this);
            if (name == "sub") return execute<2>(stdx::get<S<"sub">>(functions).value, {value1, value2}, this);
            if (name == "mul") return execute<2>(stdx::get<S<"mul">>(functions).value, {value1, value2}, this);
            if (name == "div") return execute<2>(stdx::get<S<"div">>(functions).value, {value1, value2}, this);
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

            if (not std::holds_alternative<bool>(value1)) return std::visit(*this, otherwise);


            if(get<bool>(value1)) return std::visit(*this, then);

            return std::visit(*this, otherwise); // Oh ffs! [for cogs on discord!]
        }


        // const auto& value3 = std::visit(*this, call->args[2]->variant());
        // if (name == "conditional") return execute<3>(stdx::get<S<"conditional">>(functions).value, {value1, value2, value3}, this);


        error("Calling a builtin fuction that doesn't exist!");
    }



    void print(const Value& value, bool new_line = true) const {
        std::cout << stringify(value);

        if (new_line) puts("");
    }

    static std::string stringify(const Value& value, size_t indent = {}) {
        std::string s;

        if (std::holds_alternative<bool>(value)) {
            const auto& v = std::get<bool>(value);
            s = std::to_string(v);
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

        else if (std::holds_alternative<Closure>(value)) {
            const auto& v = std::get<Closure>(value);

            s = v.stringify(indent);
        }


        else if (std::holds_alternative<ClassValue>(value)) {
            const auto& v = std::get<ClassValue>(value);

            s = "class {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, value] : v.blueprint->members) {
                s += space + name + " = ";

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
                s += space + name + " = ";

                const bool is_string = std::holds_alternative<std::string>(value);
                if (is_string) s += '\"';

                s += stringify(value, indent + 4);

                if (is_string) s += '\"';


                s += ";\n";
            }

            s += indent + '}';

        }
        else error("Type not found!");

        return s;
    }

    bool isValidType(const std::string& type) const noexcept {
        if (
            type == "int" or
            type == "double" or
            type == "bool" or
            type == "string" or
            type == "Type"
        )
        return true;

        // TODO: check if it's a valid closure type


        const auto& type_value = getVar(type);
        if (type_value and std::holds_alternative<ClassValue>(*type_value)) return true;

        return false;
    }


    std::string typeOf(const Value& value) noexcept {
        if (std::holds_alternative<int>(value))          return "int";
        if (std::holds_alternative<double>(value))       return "double";
        if (std::holds_alternative<bool>(value))         return "bool";
        if (std::holds_alternative<std::string>(value))  return "string";
        if (std::holds_alternative<Closure>(value)) {
            std::string s = "(";

            for (const auto& [name, value] : get<Closure>(value).params)
                s += name + ": " + value;

            return s;
        }
        if (std::holds_alternative<ClassValue>(value)) {
            // return Visitor::stringify(value);
            return "Type";
        }
        if (std::holds_alternative<Object>(value)) {
            return "class" + stringify(value).substr(6); // skip the "Object " and add "class"
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

    const Value& addVar(const std::string& name, const Value& v, const Type& t = {}) {

        env.back()[name] = {v, t.empty() ? "Any" : t}; // ! might cause issues
        return v;
    }

    std::optional<Value> getVar(const std::string& name) const {
        for (auto rev_it = env.crbegin(); rev_it != env.crend(); ++rev_it)
            if (rev_it->contains(name)) return rev_it->at(name).first;


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

            std::println("{}: {} = {}", name, type, stringify(value));
        }
    }

};

