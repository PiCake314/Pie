#include <print>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <utility>

#include "Lexer.hxx"
#include "Parser.hxx"
#include "Interpreter.hxx"


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


int main(const int argc, const char* argv[]) {
    if (argc != 2) error("Wrong number of arguments!");

    const auto src = readFile(argv[1]);

// y y v x v x y v z z;

    // std::println("{}", src);

    auto v = lex(src);
    // std::println("{}", v); // uncomment this, and everything break!!!

    const auto flattened_v = flattenLines(std::move(v));
    Parser p{flattened_v};
    // std::println("{}", flattened_v);

    const auto [exprs, ops] = p.parse();


    // puts("\nPrecedences:");
    // for (const auto& ops = p.operators();  const auto& [name, fix] : ops) 
    //     std::println("OP: {} = {}", name, precedence::calculate(fix->high, fix->low, ops));


    // puts("Parsed..");
    // for(const auto& expr : exprs)
    //     std::println("{};", expr->print(0));

    // puts("Output:\n");

    Visitor visitor{ops};
    for (const auto& expr : exprs)
        std::visit(visitor, expr->variant());

}