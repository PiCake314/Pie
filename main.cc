#include <print>
#include <vector>

#include "Lexer.hxx"
#include "Parser.hxx"


int main() {
    // const auto src = "1 + func(a, -+b)";
    const auto src =
R"(
    prefix(PROD--) y = (a) => -a;
)";
    std::println("{}", src);

    const auto v = lex(src);
    std::println("{}", v);

    Parser p{v};
    const auto exprs = p.parse();

    puts("Parsed..");

    for(const auto& expr : exprs)
        (expr->print(), puts(";"));

}