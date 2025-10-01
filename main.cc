#include <print>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include "Lexer/Lexer.hxx"
#include "Preprocessor/Preprocessor.hxx"
#include "Parser/Parser.hxx"
#include "Interp/Interpreter.hxx"



int main(int argc, char *argv[]) {
    using std::operator""sv;
    if (argc < 2) error("Please pass a file name!");


    bool print_tokens{};
    bool print_parsed{};
    bool run = true;

    // this would leave file name at argv[1]
    for(; argc > 2; --argc, ++argv) {
        if (argv[1] == "-t"sv) print_tokens = true;
        if (argv[1] == "-p"sv) print_parsed = true;
        if (argv[1] == "-n"sv) run          = false;
    }



    const auto src = readFile(argv[1]);

    const auto preprocessed_src = preprocess(std::move(src), argv[1]);

    const Tokens v = lex(std::move(preprocessed_src));

    if (v.empty()) return 0;

    Parser p{v, argv[1]};
    if (print_tokens) std::println("{}", v);

    auto [exprs, ops] = p.parse();


    if (print_parsed) {
        for(const auto& expr : exprs)
            std::println("{};", expr->stringify(0));

        if(run) puts("Output:\n");
    }

    if (run) {
        Visitor visitor{std::move(ops)};
        for (auto&& expr : exprs)
            std::visit(visitor, std::move(expr)->variant());
    }
}