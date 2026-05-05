#pragma once


#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <ranges>
#include <variant>

#include "../Utils/utils.hxx"
#include "../Utils/Exceptions.hxx"
#include "../Expr/Expr.hxx"
#include "../Type/Type.hxx"


inline namespace pie {
namespace analysis {

std::string stringify(const std::vector<std::string>& spaces) {
    if (spaces.size() == 1) return spaces[0];

    std::string s = spaces[0];
    for (const auto& space : spaces | std::views::drop(1))
        s += "::" + space;

    return s;
}

struct NameSpace {
    std::string name;

    NameSpace *parent = nullptr;
    std::vector<std::unique_ptr<NameSpace>> children;
};


struct LexicalAnalysis {

    size_t variable_index{};

    // scopes
    std::vector<std::unordered_map<std::string, size_t>> env;

    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> namespaces;
    std::string space_dir;
    std::vector<std::unique_ptr<NameSpace>> global_spaces;


    LexicalAnalysis() {
        env.push_back({});

        const auto builtins = {
            "Any",
            "Int",
            "Double",
            "String",
            "Bool",
            "Syntax",
            "Type",

            "__builtin_print",
            "__builtin_concat", 
            "__builtin_panic",
            "__builtin_input_str",
            "__builtin_input_int",
            "__builtin_type_of",
            "__builtin_len",
            "__builtin_reset",
            "__builtin_eval",
            "__builtin_neg",
            "__builtin_not",
            "__builtin_to_int",
            "__builtin_to_double",
            "__builtin_to_string",
            "__builtin_get",
            "__builtin_push",
            "__builtin_pop",
            "__builtin_add",
            "__builtin_sub",
            "__builtin_mul",
            "__builtin_div",
            "__builtin_mod",
            "__builtin_pow",
            "__builtin_gt",
            "__builtin_geq",
            "__builtin_eq",
            "__builtin_leq",
            "__builtin_lt",
            "__builtin_and",
            "__builtin_or",  
            "__builtin_set",
            "__builtin_conditional",
            "__builtin_str_slice",
            // //* File IO
            // "__builtin_read_file",
            // "__builtin_read_whole",
            // "__builtin_read_line",
            // "__builtin_read_lines"
        };


        for (const auto builtin : builtins)
            env[0][builtin] = variable_index++;
    }



    void operator()(const auto *) { } // default case. No error!
    // void operator()(const expr::Num*) { }
    // void operator()(const expr::Bool*) { }
    // void operator()(const expr::String*) { }
    // void operator()(const expr::Type*) { }
    // void operator()(const expr::Cascade*) { }
    // void operator()(const expr::Fix*) { }


    void operator()(const expr::Assignment *ass) {

        // this allows for recursion
        const bool is_closure = dynamic_cast<expr::Closure*>(ass->rhs.get());
        if (is_closure) {
            // if there is a type explicitly stated, then a new variable should be create no matter what
            if (not type::shouldReassign(ass->type)) {
                ass->lhs->ID = variable_index++;
                addVar(ass->lhs->stringify(), ass->lhs->ID);
            }
            else if (const auto id = findVar(ass->lhs->stringify()); id)
                ass->lhs->ID = *id;
            else {
                ass->lhs->ID = variable_index++;
                addVar(ass->lhs->stringify(), ass->lhs->ID);
            }
        }

        std::visit(*this, expr::Type{ass->type}.variant());
        std::visit(*this, ass->rhs->variant());


        if (not is_closure) {
            // if there is a type explicitly stated, then a new variable should be create no matter what
            if (not type::shouldReassign(ass->type)) {
                ass->lhs->ID = variable_index++;
                addVar(ass->lhs->stringify(), ass->lhs->ID);
            }
            else if (const auto id = findVar(ass->lhs->stringify()); id)
                ass->lhs->ID = *id;
            else { // I could probably shorten this to only use an "if-else" and not "if-elif-else"
                ass->lhs->ID = variable_index++;
                addVar(ass->lhs->stringify(), ass->lhs->ID);
            }
        }
    }

    // void operator()(const expr::Type *type) {
    //     if (auto t = type::isExpr(type->type)) return std::visit(*this, t->t->variant());
    // }

    void operator()(expr::Name *name) {
        name->ID = checkName(name->name);
    }


    void operator()(const expr::Block *b) {
        if (findVar(b->stringify())) return;

        ScopeGuard sg{this};

        for (const auto& e : b->lines)
            std::visit(*this, e->variant());
    }

    void operator()(expr::Closure *c) {
        if (findVar(c->stringify())) return;

        ScopeGuard sg{this};

        for (const auto& [name, type] : std::views::zip(c->params, c->type.params)) {
            std::visit(*this, expr::Type{type}.variant());

            if (auto expr_type = type::isExpr(type)) {
                if (const auto id = findVar(type->text()); id)
                    expr_type->t->ID = *id;
            }

            name.ID = variable_index++;
            addVar(name.name, name.ID);
        }

        std::visit(*this, c->body->variant());
    }

    void operator()(const expr::Call *call) {
        if (findVar(call->stringify())) return;

        std::visit(*this, call->func->variant());

        for (const auto& [_, named_arg] : call->named_args)
            std::visit(*this, named_arg->variant());


        for (const auto& arg : call->args)
            std::visit(*this, arg->variant());
    }

    void operator()(const expr::List *list) {
        if (findVar(list->stringify())) return;

        for (const auto& e : list->elements)
            std::visit(*this, e->variant());
    }

    void operator()(const expr::Map *map) {
        if (findVar(map->stringify())) return;

        for (const auto& [key, value] : map->items)
            std::visit(*this, key->variant()), std::visit(*this, value->variant());
    }

    void operator()(const expr::Expansion *e) {
        if (findVar(e->stringify())) return;

        std::visit(*this, e->pack->variant());
    }

    void operator()(const expr::UnaryFold *fold) {
        if (findVar(fold->stringify())) return;

        std::visit(*this, fold->pack->variant());
    }

    void operator()(const expr::SeparatedUnaryFold *fold) {
        if (findVar(fold->stringify())) return;

        std::visit(*this, fold->lhs->variant());
        std::visit(*this, fold->rhs->variant());
    }

    void operator()(const expr::BinaryFold *fold) {
        if (findVar(fold->stringify())) return;

        std::visit(*this, fold->pack->variant());
        std::visit(*this, fold->init->variant());
    }

    void operator()(expr::Class *cls) {
        if (findVar(cls->stringify())) return;

        ScopeGuard sg{this};
        addVar("self", variable_index++);

         // needed for methods to access member variables
        for (auto& [name, _, __] : cls->fields) {
            name.ID = variable_index++;
            addVar(name.name, variable_index);
        }

        for (const auto& [_, type, value] : cls->fields) {
            std::visit(*this, value->variant());
            std::visit(*this, std::make_shared<expr::Type>(type)->variant());
        }
    }

    void operator()(const expr::Union *onion) {
        if (findVar(onion->stringify())) return;

        ScopeGuard sg{this};

        for (const auto& type : onion->types) {
            std::visit(*this, std::make_shared<expr::Type>(type)->variant());
            addVar(type->text(), variable_index++);
        }
    }


    void checkPattern(const expr::Match::Case::Pattern& pat) {
        if (std::holds_alternative<expr::Match::Case::Pattern::Single>(pat.pattern)) {
            const auto& pattern = get<expr::Match::Case::Pattern::Single>(pat.pattern);

            if (not pattern.name.empty()) addVar(pattern.name, variable_index++);

            if (pattern.type)
                std::visit(*this, std::make_shared<expr::Type>(pattern.type)->variant());

            if (pattern.value)
                std::visit(*this, pattern.value->variant());

            // if (not pattern.name.empty()) addVar(pattern.name);
        }
        else { // holds expr::Match::Case::Pattern::Structure
            const auto& [name, patterns] = get<expr::Match::Case::Pattern::Structure>(pat.pattern);

            checkName(name);

            for (const auto& pat : patterns) checkPattern(*pat);
        }
    }


    void operator()(const expr::Match *match) {
        if (findVar(match->stringify())) return;

        ScopeGuard sg{this};

        std::visit(*this, match->expr->variant());


        for (const auto& kase : match->cases) {
            ScopeGuard sg{this};

            checkPattern(*kase.pattern);

            if (kase.guard) std::visit(*this, kase.guard->variant());

            std::visit(*this, kase.body->variant());
        }
    }


    void operator()(const expr::Loop *loop) {
        if (findVar(loop->stringify())) return;

        ScopeGuard sg{this};

        if (loop->kind) std::visit(*this, loop->kind->variant());
        if (loop->var) {
            if (dynamic_cast<expr::Name*>(loop->var.get())) {
                addVar(loop->var->stringify(), variable_index++);
            }
        }
        std::visit(*this, loop->body->variant());
        if (loop->els) std::visit(*this, loop->els ->variant());
    }


    void operator()(const expr::Break    *br  ) {
        if (findVar(br->stringify())) return;

        std::visit(*this, br  ->expr->variant());
    }

    void operator()(const expr::Continue *cont) {
        if (findVar(cont->stringify())) return;

        std::visit(*this, cont->expr->variant());
    }


    void operator()(const expr::Access *acc) {
        std::visit(*this, acc->var->variant());
        // can't check the accessee
        // since the type of the accessor is not know until runtime
        // so..we just allow it (which is fine since it doesn't fall under lexical scoping)
        // checkName(acc->name);
    }


    void operator()(const expr::Import *import) {
        if (findVar(import->stringify())) return;
        else util::error();


        // const auto src = util::readFile(auto{import->path}.replace_extension(".pie").string());
        // const Tokens v = lex::lex(src);
        // if (v.empty()) util::error("Can't import an empty file!");

        // Parser p{v, import->path};

        // auto [exprs, _] = p.parse();

        // for (const auto& expr : exprs)
        //     std::visit(*this, std::move(expr)->variant());
    }


    void operator()(const expr::Namespace *ns) {
        if (findVar(ns->stringify())) return;

        addSpace(ns->name);

        for (const auto& expr : ns->space) 
            std::visit(*this, expr->variant());

        popSpace();
    }


    void operator()(expr::UseSpace *use) {
        if (findVar(use->stringify())) return;


        const auto space = findSpace(use->spaces, use->global);
        if (not space) util::error();

        for (const auto& [var, id] : namespaces[stringify(use->spaces)]) {
            addVar(var, id);
            use->last_item_id = id;
        }
    }

    void operator()(expr::Use *use) {
        if (findVar(use->stringify())) return;


        const auto space = findSpace(use->spaces, use->global);
        if (not space) util::error();

        for (const auto& [var, id] : namespaces[stringify(use->spaces)]) {
            if (var == use->name.name) {
                use->name.ID = id;
                addVar(var, id);
                return;
            }
        }

        util::error("Name " + use->name.name + " not found in space " + stringify(use->spaces));
    }


    void operator()(expr::SpaceAccess *acc) {
        const auto space = findSpace(acc->spaces, acc->global);

        if (not space) util::error();

        for (const auto& [var, id] : namespaces[stringify(acc->spaces)]) {
            if (var == acc->name.name) {
                acc->name.ID = id;
                return;
            }
        }

        util::error("Name " + acc->name.name + " not found in space " + stringify(acc->spaces));
    }


    void operator()(const expr::Grouping *group) {
        if (findVar(group->stringify())) return;

        std::visit(*this, group->expr->variant());
    }


    void operator()(const expr::UnaryOp *up) {
        if (findVar(up->stringify())) return;

        std::visit(*this, up->expr->variant());
    }


    void operator()(const expr::BinOp *bp) {
        if (findVar(bp->stringify())) return;

        std::visit(*this, bp->lhs->variant());
        std::visit(*this, bp->rhs->variant());
    }


    void operator()(const expr::PostOp   *pp) {
        if (findVar(pp->stringify())) return;

        std::visit(*this, pp->expr->variant());
    }


    void operator()(const expr::CircumOp *cp) {
        if (findVar(cp->stringify())) return;

        std::visit(*this, cp->expr->variant());
    }


    void operator()(const expr::OpCall *oc) {
        if (findVar(oc->stringify())) return;

        for (const auto& expr : oc->exprs)
            std::visit(*this, expr->variant());
    }




    size_t checkName(
        const std::string& name,
        const std::source_location& location = std::source_location::current()
    ) {
        if (const auto ID = findVar(name); not ID) util::error<except::NameLookup>("Name `" + name + "` not found!", location);
        else return *ID;
    }


    // void checkName(
    //     const std::string& name,
    //     const std::string& space,
    //     const std::source_location& location = std::source_location::current()
    // ) {
    //     if (not namespaces.contains(space)) util::error<except::NameLookup>("Name `" + name + "` not found!", location);

    //     for (const auto& n : namespaces[space])
    //         if (n == name) return;


    //     util::error<except::NameLookup>("Name `" + name + "` not found!", location);
    // }



    static NameSpace* getNamespaceAt(
        const std::string& dir,
        const std::vector<std::unique_ptr<NameSpace>>& spaces
    ) {
        if (dir.empty()) return nullptr;

        const size_t ind = dir[0] - 0X30;

        return dir.size() == 1 ?
            spaces[ind].get()  :
            getNamespaceAt(dir.substr(0, dir.size() - 1), spaces[ind]->children);
    }

    std::string fullName(const NameSpace* ns) const {
        std::vector<std::string> spaces;

        for (auto ptr = ns; ptr; ptr = ptr->parent)
            spaces.push_back(ptr->name);

        std::ranges::reverse(spaces);

        std::string name = spaces[0];

        for (const auto& s : spaces | std::views::drop(1))
            name += "::" + s;

        return name;
    }


    void addSpace(std::string name) {
        const auto parent = getNamespaceAt(space_dir, global_spaces);

        if (not parent) {
            global_spaces.push_back({std::make_unique<NameSpace>(std::move(name))});
            space_dir += std::to_string(global_spaces.size() - 1);
            return;
        }


        // if it already exists, then direct to it
        for (size_t i{}; const auto& ns : parent->children) {
            if (ns->name == name) {
                space_dir += std::to_string(i);
                return;
            }
            ++i;
        }

        // not found, add it

        parent->children.push_back({std::make_unique<NameSpace>(name)});
        parent->children.back()->parent = parent;


        space_dir += std::to_string(parent->children.size() - 1);
    }


    void popSpace() { space_dir.pop_back(); }


    // NameSpace* findSpace(const std::string& name, bool global_search = false) {

    //     if (not global_search) {
    //         const auto *space = getNamespaceAt(space_dir, global_spaces);

    //         while (space) {
    //             for (const auto& ns : space->children)
    //                 if (ns->name == name) return ns.get();

    //             space = space->parent;
    //         }
    //     }


    //     for (const auto& ns : global_spaces)
    //         if (ns->name == name) return ns.get();


    //     util::error();
    // }



    bool matchChain(const std::vector<std::string>& names, const NameSpace *space) {
        if (names.empty()) return true;
        if (names[0] != space->name) return false;

        for (const auto& name : names | std::views::drop(1)) {
            for (const auto& child : space->children) {
                // if the space is found
                // move the current down the chain to look for the nested name
                if (name == child->name) {
                    space = child.get();
                    goto keep_going;
                }
            }
            return false;

            keep_going:
        }

        return true;
    }


    // ideally, should be called findSpaces!
    NameSpace* findSpace(const std::vector<std::string>& names, const bool global_search_only = false) {
        if (not global_search_only) {
            auto space = getNamespaceAt(space_dir, global_spaces);

            while (space) {
                for (const auto& child : space->children) // !!this could be a bug. Why search the children?
                    if (matchChain(names, child.get())) return child.get();
                // if not found, then move up the chain and look again
                space = space->parent;
            }
        }

        for (const auto& ns : global_spaces)
            if (matchChain(names, ns.get())) return ns.get();



        util::error("Space '" + stringify(names) + "' not found!");
    }

    void addVar(std::string name, const size_t index) {
        if (space_dir.empty()) env.back()[std::move(name)] = index;
        else {
            const auto space = getNamespaceAt(space_dir, global_spaces);
            namespaces[fullName(space)][std::move(name)] = index;
        }
    }


    std::optional<size_t> findVarInSpace(const std::string& name) const {
        auto space = getNamespaceAt(space_dir, global_spaces);

        while (space) {
            for (const auto& child : space->children) {
                const auto fullname = fullName(child.get());
                if (namespaces.at(fullname).contains(name))
                    return namespaces.at(fullname).at(name);
            }

            // if not found, then move up the chain and look again
            space = space->parent;
        }

        return {};
    }


    [[nodiscard]] std::optional<size_t> findVar(const std::string& name) const {
        if (not space_dir.empty())
            if (const auto id = findVarInSpace(name); id) return id;


        // previously, the order of traversal didn't matter
        // since once the name was found, that was enough proof that it was lexically available
        // now we need to assign IDs, which means a reverse traversal is needed
        for (const auto& e : std::views::reverse(env))
            if (e.contains(name)) return e.at(name);


        return {};
    }



    struct ScopeGuard {
        LexicalAnalysis* that;
         ScopeGuard(LexicalAnalysis* t) : that{t} { that->env.emplace_back(); }
        ~ScopeGuard() { that->env.pop_back(); }
    };



};




} // namespace analysis
} // namespace pie