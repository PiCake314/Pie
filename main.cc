#include <print>
#include <vector>

#include "Lexer.hxx"
#include "Parser.hxx"
#include "Interpreter.hxx"


int main() {
    // const auto src = "1 + func(a, -+b)";
    const auto src =
R"(
prefix(PROD--) neg = (a) => __builtin_neg(a);
infix(SUM+++) x = (a, b) => __builtin_mul(a, b);
suffix(HIGH) z = (a) => __builtin_not(a);

ID = (a) => a;
print = (something) => __builtin_print(something);
ID(1);
print(a);

w = ID(4);
v = 10;
s = "string test";

__builtin_print(v x v x v);
__builtin_print(neg neg v);
__builtin_print(v z z);
print(ID);
)";
// y y v x v x y v z z;

    std::println("{}", src);

    const auto v = lex(src);
    // std::println("{}", v);

    Parser p{v};
    const auto [exprs, ops] = p.parse();

    // puts("Parsed..");

    // for(const auto& expr : exprs)
    //     (expr->print(), puts(";"));


    puts("Output:\n");
    Visitor visitor{ops};
    for (const auto& expr : exprs)
        std::visit(visitor, expr->variant());

}