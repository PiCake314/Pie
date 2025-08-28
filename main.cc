#include <print>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <fstream>
#include <utility>

#include "Lexer/Lexer.hxx"
#include "Parser/Parser.hxx"
#include "Interp/Interpreter.hxx"


[[nodiscard]] std::string readFile(const std::string& fname) {
    const std::ifstream fin{fname};

    if (not fin.is_open()) error("File \"" + fname + " \"not found!");

    std::stringstream ss;
    ss << fin.rdbuf();

    return ss.str();
}


[[nodiscard]] Tokens flattenLines(TokenLines&& lines) {
    Tokens tokens;

    for (auto&& line : lines)
        for (auto&& t : line)
            tokens.push_back(std::move(t));

    return tokens;
}


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

// y y v x v x y v z z;

    // std::println("{}", src);

    TokenLines v = lex(src);


    if (v.empty()) return 0;

    // std::println("{}", v); // uncomment this, and everything break!!!

    const auto flattened_v = flattenLines(std::move(v));
    Parser p{flattened_v};
    if (print_tokens) std::println("{}", flattened_v);

    const auto [exprs, ops] = p.parse();


    // puts("\nPrecedences:");
    // for (const auto& ops = p.operators();  const auto& [name, fix] : ops) 
    //     std::println("OP: {} = {}", name, precedence::calculate(fix->high, fix->low, ops));


    if (print_parsed) {
        for(const auto& expr : exprs)
            std::println("{};", expr->stringify(0));

        if(run) puts("Output:\n");
    }


    if (run) {
        Visitor visitor{ops};
        for (const auto& expr : exprs)
            std::visit(visitor, expr->variant());
    }

}