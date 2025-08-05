#include <print>
#include <vector>

#include "Lexer.hxx"
#include "Parser.hxx"


int main() {
    // const auto src = "1 + func(a, -+b)";
    const auto src =
R"(
    prefix(PROD--) y = (a) => a;
    infix(SUM+++) x = (a, b) => y b;
    suffix(HIGH) z = (a) => a x a;
    y y 1;
    4 z z;
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