#include <print>
#include <vector>

#include "Lexer.hxx"
#include "Parser.hxx"


int main() {
    // const auto src = "1 + func(a, -+b)";
    auto src = "() => 3 + x;";
    src = "";
    std::println("{}", src);

    const auto [v, _] = lex(src);
    std::println("{}", v);

    Parser p{v.front()};
    const auto exprs = p.parse();

    puts("Parsed..");

    for(const auto& expr : exprs)
        (expr->print(), puts(";"));

}