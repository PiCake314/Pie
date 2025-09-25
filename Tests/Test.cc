#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "TestSuite.hxx"

#include "../Type/Type.hxx"

#include "catch.hpp"





TEST_CASE("Class member", "[Type]") {
    const auto src = R"(
wow = class { inner = class { hmm = 1; }; };
w = wow();
a: w.inner = w.inner();
)";

    run(src);

    SUCCEED();
}



TEST_CASE("Var args4", "[Variadic]") {
    const auto src = R"(
print = __builtin_print;
f = (a, b, c, es: ...String, x, y, z) => {
    print(a);
    print(b);
    print(c);
    print(es);
    print(x);
    print(y);
    print(z);
};

print(f(4, 2, 0, "Hi", "there", 1, 0, 1));
)";


    REQUIRE(run(src) == "4\n2\n0\nHi, there\n1\n0\n1\n1");
}



TEST_CASE("Infix operators", "[Parsing]") {
    const auto src =
R"(
print = __builtin_print;
infix(SUM) + = (a, b) => __builtin_add(a, b);
infix(SUM) - = (a, b) => __builtin_sub(a, b);
x = 1 + 2 + 3 + 4;
y = 4 - 3 - 2 - 1;
print(x);
print(y);
)";

    REQUIRE(run(src) == "10\n-2");
}


TEST_CASE("Var args3", "[Variadic]") {
    const auto src =
R"(
print = __builtin_print;

func1 = (a: String, args: ...Any, b: String): ...Int => args;

x: ...Int = func1("first", "last");
y: ...Int = func1("first", 1, 2, 3, "last");
print(x);
print(x...);
)";

    REQUIRE(run(src) == ""); // not sure why it's not "\n\n" :)! I think Catch2 strips it
}


TEST_CASE("Var args2", "[Variadic]") {
    const auto src =
R"(
print = __builtin_print;

(es) => 1;
f = (es: ...Any) => 1;
f();

f = (x, es: ...Any) => 1;
f();

f = (x, y, es: ...Any) => 1;
f();

f = (es: ...Any, a) => 1;
f();

f = (es: ...Any, a, b) => 1;
f();

f = (a, es: ...Any, b) => 1;
f();

)";

    run(src);

    SUCCEED();
}


TEST_CASE("Var args", "[Variadic]") {
    const auto src =
R"(
print = __builtin_print;

func2 = (x, y, z, a) => {
    print("x = ", x);
    print("y = ", y);
    print("z = ", z);
    print("a = ", a);

    "done";
};

out = (As: ...Any) => {
    func = (a, b, c, args: ...Any) => func2(a=300, As..., args...);

    func(1, 2, 3, 5);
};

out(10, 20);
)";



    REQUIRE(
        run(src)
        == R"(x =  10
y =  20
z =  5
a =  300)"
    );
}



TEST_CASE("Any vs Func", "[Type]") {
    auto Any  = type::builtins::Any();

    auto func = std::make_shared<type::FuncType>( // functions taking 2 ints and returns an int
        std::vector{type::builtins::Int(), type::builtins::Int()}, type::builtins::Int()
    );


    REQUIRE(*Any  >  *func);
    REQUIRE(*Any  >= *func);

    REQUIRE_FALSE(*func >  *Any);
    REQUIRE_FALSE(*func >= *Any);
}


TEST_CASE("Variadics", "[Type]") {
    auto Any  = type::builtins::Any();
    auto Int  = type::builtins::Int();
    auto Int_var = std::make_shared<type::VariadicType>(type::builtins::Int());
    auto Any_var = std::make_shared<type::VariadicType>(type::builtins::Any());


    REQUIRE(*Int_var >  *Int);
    REQUIRE(*Int_var >= *Int);

    REQUIRE(*Any_var >  *Int_var);
    REQUIRE(*Any_var >= *Int_var);

    REQUIRE(*Any_var >  *Any);
    REQUIRE(*Any_var >= *Any);

    REQUIRE(*Any >  *Any_var);
    REQUIRE(*Any >= *Any_var);

    REQUIRE_FALSE(*Int_var == *Int);
    REQUIRE_FALSE(*Int == *Int_var);

    REQUIRE_FALSE(*Int >  *Int_var);
    REQUIRE_FALSE(*Int >= *Int_var);
}

TEST_CASE("Named Arguments") {

    const auto src =
R"(
print = __builtin_print;
add = (x: Int, y, a: Int, b, c, d: Int, e: String, f, g, z: Double): Int => 0;

print(add(g = 7, 1, c = 3, 2, 4, 5, f = 6, 1, "", 1.));
)";


    REQUIRE(run(src) == "0");
}




