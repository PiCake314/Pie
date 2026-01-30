#pragma once


#include <string>
#include <vector>
// #include <algorithm>
#include <ranges>
#include <variant>

#include "../Utils/utils.hxx"
#include "../Utils/Exceptions.hxx"
#include "../Expr/Expr.hxx"
#include "../Type/Type.hxx"


inline namespace pie {
namespace analysis {

struct LexicalAnalysis {
    std::vector<std::vector<std::string>> vars;

    LexicalAnalysis() {
        vars.push_back({
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
        });
    }


    void operator()(const expr::Assignment *ass) {
        addVar(ass->lhs->stringify());

        std::visit(*this, std::make_shared<expr::Type>(ass->type)->variant());

        std::visit(*this, ass->rhs->variant());
    }

    // void operator()(const expr::Type *type) {
    //     if (auto t = type::isExpr(type->type)) return std::visit(*this, t->t->variant());
    // }

    void operator()(const expr::Name *name) { checkName(name->name); }


    void operator()(const expr::Block *b) {
        ScopeGuard sg{this};

        for (const auto& e : b->lines)
            std::visit(*this, e->variant());
    }

    void operator()(const expr::Closure *c) {
        ScopeGuard sg{this};

        for (const auto& param : c->params)
            addVar(param);

        std::visit(*this, c->body->variant());
    }

    void operator()(const expr::Call *call) {
        std::visit(*this, call->func->variant());

        for (const auto& [_, named_arg] : call->named_args)
            std::visit(*this, named_arg->variant());


        for (const auto& arg : call->args)
            std::visit(*this, arg->variant());
    }

    void operator()(const expr::List *list) {
        for (const auto& e : list->elements)
            std::visit(*this, e->variant());
    }

    void operator()(const expr::Map *map) {
        for (const auto& [key, value] : map->items)
            std::visit(*this, key->variant()), std::visit(*this, value->variant());
    }

    void operator()(const expr::Expansion *e) {
        std::visit(*this, e->pack->variant());
    }

    void operator()(const expr::UnaryFold *fold) {
        std::visit(*this, fold->pack->variant());
    }

    void operator()(const expr::SeparatedUnaryFold *fold) {
        std::visit(*this, fold->lhs->variant());
        std::visit(*this, fold->rhs->variant());
    }

    void operator()(const expr::BinaryFold *fold) {
        std::visit(*this, fold->pack->variant());
        std::visit(*this, fold->init->variant());
    }

    void operator()(const expr::Class *cls) {
        ScopeGuard sg{this};
        addVar("self");

        for (const auto& [name, type, value] : cls->fields) {
            std::visit(*this, value->variant());
            std::visit(*this, std::make_shared<expr::Type>(type)->variant());
            addVar(name.name);
        }
    }

    void operator()(const expr::Union *onion) {
        ScopeGuard sg{this};

        for (const auto& type : onion->types) {
            std::visit(*this, std::make_shared<expr::Type>(type)->variant());
            addVar(type->text());
        }
    }


    void checkPattern(const expr::Match::Case::Pattern& pat) {
        if (std::holds_alternative<expr::Match::Case::Pattern::Single>(pat.pattern)) {
            const auto& pattern = get<expr::Match::Case::Pattern::Single>(pat.pattern);

            if (not pattern.name.empty()) addVar(pattern.name);

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
        ScopeGuard sg{this};

        if (loop->kind) std::visit(*this, loop->kind->variant());
        if (loop->var) addVar(loop->var->stringify());
        std::visit(*this, loop->body->variant());
        if (loop->els) std::visit(*this, loop->els ->variant());
    }


    void operator()(const expr::Break    *br  ) { std::visit(*this, br  ->expr->variant()); }
    void operator()(const expr::Continue *cont) { std::visit(*this, cont->expr->variant()); }


    void operator()(const expr::Access *acc) {
        std::visit(*this, acc->var->variant());
        // can't check the accessee
        // since the type of the accessor is not know until runtime
        // so..we just allow it (which is fine since it doesn't fall under lexical scoping)
        // checkName(acc->name);
    }


    void operator()(const expr::Namespace *ns) {
        for (const auto& expr : ns->expressions) 
            std::visit(*this, expr->variant());
    }


    void operator()(const expr::Use *use) { std::visit(*this, use->ns->variant()); }


    void operator()(const expr::SpaceAccess *acc) {
        checkName(acc->member);
        if (acc->space) // in case of global ns access `::x`
            std::visit(*this, acc->space->variant());
    }


    void operator()(const expr::Grouping *group) { std::visit(*this, group->expr->variant()); }


    void operator()(const expr::UnaryOp *up) { std::visit(*this, up->expr->variant()); }


    void operator()(const expr::BinOp *bp) {
        std::visit(*this, bp->lhs->variant());
        std::visit(*this, bp->rhs->variant());
    }


    void operator()(const expr::PostOp   *pp) { std::visit(*this, pp->expr->variant()); }


    void operator()(const expr::CircumOp *cp) { std::visit(*this, cp->expr->variant()); }


    void operator()(const expr::OpCall *oc) {
        for (const auto& expr : oc->exprs)
            std::visit(*this, expr->variant());
    }



    void operator()(const auto *) { } // default case. No error!


    void checkName(const std::string& name) {
        if (not findVar(name)) util::error<except::NameLookup>("Name `" + name + "` not found!");
    }

    void addVar(std::string name) { vars.back().push_back(std::move(name)); }

    bool findVar(const std::string& name) const {
        for (const auto& e : vars)
            if (std::ranges::find_if(e, [&name](const auto& n) { return n == name; }) != e.cend())
                return true;

        return false;
    }



    struct ScopeGuard {
        LexicalAnalysis* that;
         ScopeGuard(LexicalAnalysis* t) : that{t} { that->vars.emplace_back(); }
        ~ScopeGuard() { that->vars.pop_back(); }
    };

};



}
} // namespace pie