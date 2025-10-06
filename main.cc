#include <print>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include "Lexer/Lexer.hxx"
#include "Preprocessor/Preprocessor.hxx"
#include "Parser/Parser.hxx"
#include "Interp/Interpreter.hxx"



[[nodiscard]] inline std::string readFile(const std::string& fname) {
    const std::ifstream fin{fname};

    if (not fin.is_open()) error("File \"" + fname + " \"not found!");

    std::stringstream ss;
    ss << fin.rdbuf();

    return ss.str();
}


int main(int argc, char *argv[]) {
    using std::operator""sv;
    if (argc < 2) error("Please pass a file name!");


    bool print_tokens{};
    bool print_parsed{};
    bool print_preprocessed{};
    bool run = true;

    // this would leave file name at argv[1]
    for(; argc > 2; --argc, ++argv) {
        if (argv[1] == "-token"sv) print_tokens       = true ;
        if (argv[1] == "-ast"sv)   print_parsed       = true ;
        if (argv[1] == "-pre"sv)   print_preprocessed = true;
        if (argv[1] == "-run"sv)   run                = false;
    }


    auto src = readFile(argv[1]);

    auto processed_src = preprocess(std::move(src), argv[1]);

    if (print_preprocessed)
        std::println(std::clog, "{}", processed_src);

    Tokens v = lex(std::move(processed_src));
    if (print_tokens) std::println(std::clog, "{}", v);

    if (v.empty()) return 0;

    Parser p{std::move(v), argv[1]};


    auto [exprs, ops] = p.parse();


    if (print_parsed) {
        for(const auto& expr : exprs)
            std::println(std::clog, "{};", expr->stringify(0));

    }
    if(run and (print_parsed or print_preprocessed or print_tokens)) puts("Output:\n");

    if (run) {
        Visitor visitor{std::move(ops)};
        for (auto&& expr : exprs)
            std::visit(visitor, std::move(expr)->variant());
    }
}