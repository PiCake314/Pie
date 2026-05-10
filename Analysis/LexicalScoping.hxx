#pragma once


#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
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

inline std::string stringify(const std::vector<std::string>& spaces) {
    if (spaces.size() == 1) return spaces[0];

    std::string s = spaces[0];
    for (const auto& space : spaces | std::views::drop(1))
        s += "::" + space;

    return s;
}

struct NameSpace {
    std::string name;

    NameSpace *parent = nullptr;
    std::vector<std::shared_ptr<NameSpace>> children;
};


struct LexicalAnalysis {

    size_t variable_index{};

    // scopes
    std::vector<std::unordered_map<std::string, size_t>> env;

    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> namespaces;
    std::string space_dir;
    std::vector<std::shared_ptr<NameSpace>> global_spaces;


    bool in_loop = false;


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
            "__builtin_print_env",
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



    // void operator()(auto *node) { }

    void operator()(expr::Num *n) {
        if (auto id = findVar(n->stringify())) {
            n->ID = *id;
            return;
        }
    }
    void operator()(expr::Bool *b) {
        if (auto id = findVar(b->stringify())) {
            b->ID = *id;
            return;
        }
    }
    void operator()(expr::String *s) {
        if (auto id = findVar(s->stringify())) {
            s->ID = *id;
            return;
        }
    }

    void operator()(expr::Cascade *c) {
        if (auto id = findVar(c->stringify())) {
            c->ID = *id;
            return;
        }
    }
    void operator()(expr::Fix *f) {
        if (auto id = findVar(f->stringify())) {
            f->ID = *id;
            return;
        }

        std::visit(*this, f->funcs[0]->variant());
    }


    void accessAssign(expr::Access *acc, expr::Assignment *ass) {
        if (const auto id = findVar(acc->var->stringify()); id) {
            acc->var->ID = *id;
            std::visit(*this, ass->rhs->variant());
        }
        else util::error<except::NameLookup>("Name `" + acc->var->stringify() + "` not found in assignment: " + ass->stringify());
    }



    void spaceAccessAssign(expr::SpaceAccess *acc, expr::Assignment *ass) {
        const auto space = findSpace(acc->spaces, acc->global);
        if (not space) util::error("Namespace `" + stringify(acc->spaces) + "` is expression `" + ass->stringify() + "` was not found!");

        for (const auto& [var, id] : namespaces[fullName(space)]) {
            if (var == acc->name.name) {
                acc->name.ID = id;
                return;
            }
        }

        std::visit(*this, ass->rhs->variant());
    }


    void operator()(expr::Assignment *ass) {
        if (auto *acc = dynamic_cast<expr::Access*>(ass->lhs.get()))
            return accessAssign(acc, ass);

        if (auto *acc = dynamic_cast<expr::SpaceAccess*>(ass->lhs.get()))
            return spaceAccessAssign(acc, ass);




        // this allows for recursion
        const bool is_closure = dynamic_cast<expr::Closure*>(ass->rhs.get());
        if (is_closure) {
            // if there is a type explicitly stated, then a new variable should be create no matter what
            if (not type::shouldReassign(ass->type)) {
                ass->lhs->ID = variable_index++;
                addVar(ass->lhs->stringify(), ass->lhs->ID);
            }
            else if (const auto id = findVar(ass->lhs->stringify()); id) {
                ass->lhs->ID = *id;
            }
            else {
                ass->lhs->ID = variable_index++;
                addVar(ass->lhs->stringify(), ass->lhs->ID);
            }
        }

        std::visit(*this, ass->rhs->variant());
        if (const auto id = findVar(ass->type->text()); id) {
            ass->type->ID = *id;
        }
        else std::visit(*this, expr::Type{ass->type}.variant());


        if (not is_closure) {
            // if there is a type explicitly stated, then a new variable should be create no matter what
            if (not type::shouldReassign(ass->type)) {
                ass->lhs->ID = variable_index++;
                addVar(ass->lhs->stringify(), ass->lhs->ID);
            }
            else if (const auto id = findVar(ass->lhs->stringify()); id) {
                ass->lhs->ID = *id;
            }
            else { // I could probably shorten this to only use an "if-else" and not "if-elif-else"
                ass->lhs->ID = variable_index++;
                addVar(ass->lhs->stringify(), ass->lhs->ID);
            }
        }
    }


    void operator()(expr::Name *name) {
        if (const auto id = findVar(name->name)) {
            name->ID = *id;
            return;
        }

        util::error<except::NameLookup>("Name `" + name->name + " with ID [" + std::to_string(name->ID) + "] not found!");
    }


    void operator()(expr::Block *b) {
        if (auto id = findVar(b->stringify())) {
            b->ID = *id;
            return;
        }

        ScopeGuard sg{this};

        for (const auto& e : b->lines)
            std::visit(*this, e->variant());
    }

    void operator()(expr::Closure *c) {
        if (auto id = findVar(c->stringify())) {
            c->ID = *id;
            return;
        }

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

        std::visit(*this, expr::Type{c->type.ret}.variant());

        std::visit(*this, c->body->variant());
    }

    void operator()(expr::Call *call) {
        if (auto id = findVar(call->stringify())) {
            call->ID = *id;
            return;
        }

        std::visit(*this, call->func->variant());

        for (const auto& [_, named_arg] : call->named_args)
            std::visit(*this, named_arg->variant());


        for (const auto& arg : call->args)
            std::visit(*this, arg->variant());
    }

    void operator()(expr::List *list) {
        if (auto id = findVar(list->stringify())) {
            list->ID = *id;
            return;
        }

        for (const auto& e : list->elements)
            std::visit(*this, e->variant());
    }

    void operator()(expr::Map *map) {
        if (auto id = findVar(map->stringify())) {
            map->ID = *id;
            return;
        }

        for (const auto& [key, value] : map->items)
            std::visit(*this, key->variant()), std::visit(*this, value->variant());
    }

    void operator()(expr::Expansion *e) {
        if (auto id = findVar(e->stringify())) {
            e->ID = *id;
            return;
        }

        std::visit(*this, e->pack->variant());
    }

    void operator()(expr::UnaryFold *fold) {
        if (auto id = findVar(fold->stringify())) {
            fold->ID = *id;
            return;
        }

        std::visit(*this, fold->pack->variant());
    }

    void operator()(expr::SeparatedUnaryFold *fold) {
        if (auto id = findVar(fold->stringify())) {
            fold->ID = *id;
            return;
        }

        std::visit(*this, fold->lhs->variant());
        std::visit(*this, fold->rhs->variant());
    }

    void operator()(expr::BinaryFold *fold) {
        if (auto id = findVar(fold->stringify())) {
            fold->ID = *id;
            return;
        }

        std::visit(*this, fold->pack->variant());
        std::visit(*this, fold->init->variant());

        if (fold->sep) std::visit(*this, fold->sep->variant());
    }


    void operator()(expr::Class *cls) {
        if (auto id = findVar(cls->stringify())) {
            cls->ID = *id;
            return;
        }

        ScopeGuard sg{this};
        addVar("self", variable_index++);

         // needed for methods to access member variables
        for (auto& [name, _, __] : cls->fields) {
            name.ID = variable_index++;
            addVar(name.name, name.ID);
        }

        for (const auto& [_, type, expr] : cls->fields) {
            std::visit(*this, expr::Type{type}.variant());
            std::visit(*this, expr->variant());
        }

    }

    void operator()(expr::Union *onion) {
        if (auto id = findVar(onion->stringify())) {
            onion->ID = *id;
            return;
        }

        ScopeGuard sg{this};

        for (auto& type : onion->types) {
            if (const auto id = findVar(type->text()); id)
                type->ID = *id;

            else std::visit(*this, expr::Type{type}.variant());

            // if (auto expr_type = type::isExpr(type)) {
            //     std::clog << "expression type: " << type->text() << std::endl;
            //     if (const auto id = findVar(type->text()); id)
            //         expr_type->t->ID = *id;

            // }

            // if (auto expr_type = type::isExpr(type)) {
            //     if (const auto id = findVar(type->text()); id)
            //         expr_type->t->ID = *id;
            //     else {
            //         std::clog << "type: " << type->text() << " doesn't have an ID!" << std::endl;
            //         std::cin.get();
            //     }
            // }
            // addVar(type->text(), variable_index++);
        }
    }


    void checkPattern(expr::Match::Case::Pattern& pat) {
        if (std::holds_alternative<expr::Match::Case::Pattern::Single>(pat.pattern)) {
            auto& pattern = get<expr::Match::Case::Pattern::Single>(pat.pattern);

            if (not pattern.name.name.empty()) {
                pattern.name.ID = variable_index++;
                addVar(pattern.name.name, pattern.name.ID);
            }

            if (pattern.type)
                std::visit(*this, std::make_shared<expr::Type>(pattern.type)->variant());

            if (pattern.value)
                std::visit(*this, pattern.value->variant());

            // if (not pattern.name.empty()) addVar(pattern.name);
        }
        else { // holds expr::Match::Case::Pattern::Structure
            auto& [name, patterns] = get<expr::Match::Case::Pattern::Structure>(pat.pattern);

            if (const auto id = findVar(name.name); not id)
                util::error<except::NameLookup>("Name `" + name.name + "` not found!");
            else name.ID = *id;

            for (const auto& pat : patterns) checkPattern(*pat);
        }
    }


    void operator()(expr::Match *match) {
        if (auto id = findVar(match->stringify())) {
            match->ID = *id;
            return;
        }

        ScopeGuard sg{this};

        std::visit(*this, match->expr->variant());


        for (const auto& kase : match->cases) {
            ScopeGuard sg{this};

            checkPattern(*kase.pattern);

            if (kase.guard) std::visit(*this, kase.guard->variant());

            std::visit(*this, kase.body->variant());
        }
    }


    void operator()(expr::Loop *loop) {
        if (auto id = findVar(loop->stringify())) {
            loop->ID = *id;
            return;
        }

        ScopeGuard sg{this};

        if (loop->kind) std::visit(*this, loop->kind->variant());
        if (not loop->var.name.empty()) {
            loop->var.ID = variable_index++;
            addVar(loop->var.name, loop->var.ID);
        }

        const auto was_in_loop = in_loop;
        in_loop = true;
        std::visit(*this, loop->body->variant());
        in_loop = was_in_loop;

        if (loop->els) std::visit(*this, loop->els ->variant());
    }


    void operator()(expr::Break *br) {
        if (auto id = findVar(br->stringify())) {
            br->ID = *id;
            return;
        }

        if (not in_loop) util::error("Can't use `break` outside a loop: " + br->stringify());

        std::visit(*this, br->expr->variant());
    }

    void operator()(expr::Continue *cont) {
        if (auto id = findVar(cont->stringify())) {
            cont->ID = *id;
            return;
        }

        if (not in_loop) util::error("Can't use `continue` outside a loop: " + cont->stringify());

        if (cont->expr) std::visit(*this, cont->expr->variant());
    }


    void operator()(const expr::Access *acc) {
        std::visit(*this, acc->var->variant());

        // if (auto id = findVar(acc->var->stringify())) acc->var->ID = *id;
        // else util::error("Name `" + acc->var->stringify() + "` not found in access expression: " + acc->stringify());

        // can't check the accessee
        // since the type of the accessor is not know until runtime
        // so..we just allow it (which is fine since it doesn't fall under lexical scoping)
        // checkName(acc->name);
    }


    void operator()(expr::Import *import) {
        if (auto id = findVar(import->stringify())) {
            import->ID = *id;
            return;
        }

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


    void operator()(expr::Namespace *ns) {
        if (auto id = findVar(ns->stringify())) {
            ns->ID = *id;
            return;
        }

        addSpace(ns->name);

        for (const auto& expr : ns->space)
            std::visit(*this, expr->variant());

        popSpace();
    }


    void operator()(expr::UseSpace *use) {
        if (auto id = findVar(use->stringify())) {
            use->ID = *id;
            return;
        }


        auto space = findSpace(use->spaces, use->global);
        if (not space) util::error();

        for (const auto& [var, id] : namespaces[fullName(space)]) {
            addVar(var, id);
            use->last_item_id = id;
        }

        // pull child namespaces into the parent's
        if (space->parent) {
            for (auto& child : space->children) {
                space->parent->children.push_back(child);
                use->children.push_back({fullName(child.get()), child->name});
            }
        }
        else {
            for (auto& child : space->children) {
                global_spaces.push_back(child);
                use->children.push_back({fullName(child.get()), child->name});
            }
        }


    }

    void operator()(expr::Use *use) {
        if (auto id = findVar(use->stringify())) {
            use->ID = *id;
            return;
        }


        const auto space = findSpace(use->spaces, use->global);
        if (not space) util::error();

        for (const auto& [var, id] : namespaces[fullName(space)]) {
            if (var == use->name.name) {
                use->name.ID = id;
                addVar(var, id);
                return;
            }
        }

        util::error<except::NameLookup>("Name " + use->name.name + " not found in space " + stringify(use->spaces));
    }


    void operator()(expr::SpaceAccess *acc) {
        const auto space = findSpace(acc->spaces, acc->global);

        if (not space) util::error();

        for (const auto& [var, id] : namespaces[fullName(space)]) {
            if (var == acc->name.name) {
                acc->name.ID = id;
                return;
            }
        }

        util::error<except::NameLookup>("Name " + acc->name.name + " not found in space " + stringify(acc->spaces));
    }


    void operator()(expr::Grouping *group) {
        if (auto id = findVar(group->stringify())) {
            group->ID = *id;
            return;
        }

        std::visit(*this, group->expr->variant());
    }


    void operator()(expr::UnaryOp *up) {
        if (auto id = findVar(up->stringify())) {
            up->ID = *id;
            return;
        }

        std::visit(*this, up->expr->variant());
    }


    void operator()(expr::BinOp *bp) {
        if (auto id = findVar(bp->stringify())) {
            bp->ID = *id;
            return;
        }

        std::visit(*this, bp->lhs->variant());
        std::visit(*this, bp->rhs->variant());
    }


    void operator()(expr::PostOp   *pp) {
        if (auto id = findVar(pp->stringify())) {
            pp->ID = *id;
            return;
        }

        std::visit(*this, pp->expr->variant());
    }


    void operator()(expr::CircumOp *cp) {
        if (auto id = findVar(cp->stringify())) {
            cp->ID = *id;
            return;
        }

        std::visit(*this, cp->expr->variant());
    }


    void operator()(expr::OpCall *oc) {
        if (auto id = findVar(oc->stringify())) {
            oc->ID = *id;
            return;
        }

        for (const auto& expr : oc->exprs)
            std::visit(*this, expr->variant());
    }


    void visitType(const type::TypePtr& type) {
        if (auto expr_type = type::isExpr(type)) {
            // leave em empty for now...
            if (const auto n = dynamic_cast<const expr::Num*>(expr_type->t.get()));
            else if (const auto b = dynamic_cast<const expr::Bool*>(expr_type->t.get()));
            else if (const auto s = dynamic_cast<const expr::String*>(expr_type->t.get()));

            else if (const auto id = findVar(type->text()); id) {
                expr_type->ID = *id;
                expr_type->t->ID = *id;
            }
            else if (auto unio = dynamic_cast<expr::Union*>(expr_type->t.get())) {
                for (auto& type : unio->types) {
                    if (const auto id = findVar(type->text()); id) type->ID = *id;
                    else visitType(type);
                }
            }

            std::visit(*this, expr_type->t->variant());
            // else util::error<except::NameLookup>("Type `" + type->text() + "` not found!");
        }
        else if (const auto func = type::isFunction(type)) {

        }
        else if(const auto var = type::isVariadic(type)) {
            visitType(var->type);
        }
    };


    void operator()(expr::Type *type) {
        if (auto id = findVar(type->stringify())) {
            type->ID = *id;
            type->type->ID = *id;
            return;
        }

        visitType(type->type);
    }



    // size_t checkName(
    //     const std::string& name,
    //     const std::source_location& location = std::source_location::current()
    // ) {
    //     if (const auto ID = findVar(name); not ID) util::error<except::NameLookup>("Name `" + name + "` not found!", location);
    //     else return *ID;
    // }


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
        const std::vector<std::shared_ptr<NameSpace>>& spaces
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


    void addSpace(const std::string& name) {
        const auto parent = getNamespaceAt(space_dir, global_spaces);

        if (not parent) {
            global_spaces.push_back({std::make_shared<NameSpace>(std::move(name))});
            space_dir += std::to_string(global_spaces.size() - 1);
            namespaces[name];
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

        const auto space = getNamespaceAt(space_dir, global_spaces);
        const auto fullname = fullName(space);
        namespaces[fullname]; // inserts an empty namespace in case a lookup happens, it needs to find a namespace even if its empty
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



    NameSpace* matchChain(const std::vector<std::string>& names, NameSpace *space) {
        if (names.empty()) return nullptr;
        if (names[0] != space->name) return nullptr;


        for (const auto& name : names | std::views::drop(1)) {
            for (const auto& child : space->children) {
                // if the space is found
                // move the current down the chain to look for the nested name
                if (name == child->name) {
                    space = child.get();
                    goto keep_going;
                }
            }
            return nullptr;

            keep_going:
        }

        return space;
    }


    // ideally, should be called findSpaces!
    NameSpace* findSpace(const std::vector<std::string>& names, const bool global_search_only = false) {
        if (not global_search_only) {
            auto space = getNamespaceAt(space_dir, global_spaces);

            while (space) {
                // for (const auto& child : space->children) // !!this could be a bug. Why search the children?
                //     if (matchChain(names, child.get())) return child.get();

                if (const auto s = matchChain(names, space)) return s;

                // if not found, then move up the chain and look again
                space = space->parent;
            }
        }


        for (const auto& ns : global_spaces)
            if (auto s = matchChain(names, ns.get()))
                return s;



        util::error<except::NameLookup>("Space `" + stringify(names) + "` not found!");
    }

    void addVar(std::string name, const size_t index) {
        if (space_dir.empty()) env.back()[std::move(name)] = index;
        else {
            const auto space = getNamespaceAt(space_dir, global_spaces);
            namespaces[fullName(space)][std::move(name)] = index;
        }
    }


    std::optional<size_t> findVarInSpace(const std::string& name) const {
        const auto* space = getNamespaceAt(space_dir, global_spaces);

        while (space) {
            const auto fullname = fullName(space);

            if (namespaces.at(fullname).contains(name))
                return namespaces.at(fullname).at(name);

            // for (const auto& child : space->children) {
            //     const auto fullname = fullName(child.get());
            //     if (namespaces.at(fullname).contains(name))
            //         return namespaces.at(fullname).at(name);
            // }

            // if not found, then move up the chain and look again
            space = space->parent;
        }

        return {};
    }


    [[nodiscard]] std::optional<size_t> findVar(const std::string& name) const {
        if (name.empty()) return {}; // no reason to search

        if (not space_dir.empty()) {
            if (const auto id = findVarInSpace(name); id) return id;
        }


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



    void printSpaces() {
        for (const auto& [spacename, space] : namespaces) {
            std::clog << "space " << spacename << std::endl;
            for (const auto& [var, id] : space) {
                std::clog << "var: " << var << " [" << id << "]\n";
            }
        }
    }
};




} // namespace analysis
} // namespace pie