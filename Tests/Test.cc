#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "TestSuite.hxx"

#include "../Type/Type.hxx"

#include "catch.hpp"




TEST_CASE("Empty Pack", "[General]") {
    const auto src = R"(
print = __builtin_print;

printPack = (first, rest: ...Any) => __builtin_conditional(
    __builtin_eq(
        __builtin_len(rest), 0),
            print(first),
            {
                print(first);
                printPack(rest...);
            }
    );

printPack("Ali", "Ben", "Con", "Byte");
)";

    REQUIRE(run(src) == R"(Ali
Ben
Con
Byte)");
}



TEST_CASE("linked lists", "[General]") {
    const auto src = R"(
print = __builtin_print;


Node: Type = class {
    val = "None";
    next = "None";
};

printList = (l: Node) => match l {
    Node(n, :Node) => {
        print(n);
        printList(l.next);
    };

    Node(n: Int, ="None") => print(n);
    Node(_, ="None") => print("Empty List!");
};

list = Node(1, Node(2, Node(3)));

printList(Node());
printList(list);
)";

    REQUIRE(run(src) == R"(Empty List!
1
2
3)");
}


TEST_CASE("unary separated left fold", "[Fold Exprs]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (args - ... - 10); .: (1 - 10 - 2 - 10 - 3 - 10 - 4)
print(func(1, 2, 3, 4));
)";

    REQUIRE(run(src) == R"(-38)");
}


TEST_CASE("unary separated right fold", "[Fold Exprs]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (10 - ... - args); .: (1 - (10 - (2 - (10 - (3 - (4 - 10))))))
print(func(1, 2, 3, 4));
)";

    REQUIRE(run(src) == R"(-8)");
}


TEST_CASE("binary separated left fold", "[Fold Exprs]") {
    const auto src = R"(
print = __builtin_print;
infix + = (a: Int, b: Int) => __builtin_add(a, b);
infix + = (a: String, b: String) => __builtin_concat(a, b);


greet = (greetings: String, names: ...String, delim: String) => greetings + ("Teach" + names + ... + delim) + "!";
print(greet("Hello ", "Ali", "Ben", "Byt", ", "));
)";

    REQUIRE(run(src) == R"(Hello Teach, Ali, Ben, Byt!)");
}


TEST_CASE("binary left fold", "[Fold Exprs]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (10 - args - ...); .: ((((10 - 1) - 2) - 3) - 4)

print(func(1, 2, 3, 4));
)";

    REQUIRE(run(src) == R"(0)");
}



TEST_CASE("unary right fold", "[Fold Exprs]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (... - args); .: (1 - (2 - (3 - 4)))

print(func(1, 2, 3, 4));
)";

    REQUIRE(run(src) == R"(-2)");
}



TEST_CASE("binary right fold", "[Fold Exprs]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (... - args - 10); .: (1 - (2 - (3 - (4 - 10))))

print(func(1, 2, 3, 4));
)";

    REQUIRE(run(src) == R"(8)");
}


TEST_CASE("left fold", "[Fold Exprs]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (args - ...); .: (((1 - 2) - 3) - 4)

print(func(1, 2, 3, 4));
)";

    REQUIRE(run(src) == R"(-8)");
}



TEST_CASE("literals", "[Assingment]") {
    const auto src = R"(
print = __builtin_print;
1 = "hi";
true = 5;
print(1);
print(true);
)";

    REQUIRE(run(src) == R"(hi
5)");
}



TEST_CASE("Arbitrary function", "[Parameters]") {
    const auto src = R"(
print = __builtin_print;
infix(+) + = (a, b) => __builtin_add(a, b);

x: Int = 5;

func = (x: String, x + 2, "hi") => {
    print(x);
    print(x + 2);
    print("hi");
};

func("meow", "hehe", "bye");
)";

    REQUIRE(run(src) == R"(meow
hehe
bye)");
}



TEST_CASE("Operator Overloading", "[Overload]") {
    const auto src = R"(
print = __builtin_print;

mixfix(HIGH -) if : : else : = (cond: Bool  , thn, els) => 1;
mixfix(HIGH -) if : : else : = (cond: Int   , thn, els) => 2;
mixfix(HIGH -) if::else: = (cond: String, thn, els) => 3;

print(if (true) { 1; } else { 2; });
print(if (0) { 1; } else { 2; });
print(if ("") { 1; } else { 2; });

)";

    REQUIRE(run(src) == R"(1
2
3)");

}



TEST_CASE("nested unions", "[Unions]") {
    const auto src = R"(
print = __builtin_print;

Class = class {
    x: Int = 0;
    s: String = "";
};

Num = union { Int | Double };
Union = union { Num | Class  };

u: Union = 1;
print(u);
u: Union = 1.4;
print(u);
u = Class(10, "hi");
print(u);
WithStr = union { Union | String };
u: WithStr = "a string";
print(u);

print(Union);
)";

    REQUIRE(run(src) == R"(1
1.400000
Object {
    x = 10;
    s = "hi";
}
a string
union {
    union { Int | Double };
    class {
        x: Int = 0;
        s: String = "";
    };
})");

}


TEST_CASE("operator overloading", "[Type]") {
    const auto src = R"(
print = __builtin_print;

cls = class { woof: String = ""; };

infix(+) + = (a: cls, b: cls) => 1;
infix(+) + = (a: Int, b: Int) => 2;

a = cls();
b: cls = cls();
print(cls() + cls());
print(a + b);
print(1 + 1);
)";

    REQUIRE(run(src) == "1\n1\n2");
}


TEST_CASE("recursive namespaces", "[Namespace]") {
    const auto src = R"(
print = __builtin_print;

x = namespace {
    a = 1;
    y = 1;
};

x::y = x;

print(x::y::y::y::y::a);

)";

    REQUIRE(run(src) == "1");
}

TEST_CASE("namespaces2", "[Namespace]") {
    const auto src = R"(
print = __builtin_print;

x = namespace {
    cls = class { meow: Int = 0; };

    v: String = "hi";

    o: cls = cls(1);

    func = (e: cls) => e.meow;

    infix(+) + = (a: cls, b: cls) => __builtin_add(a.meow, b.meow);

    y = namespace {
        n = 1;
    };
};


a = x::y::n;
print(a);
print(x::y::n);
x::y::n = 10;
print(a);
print(x::y::n);
a = 20;
print(a);
print(x::y::n);

)";

    REQUIRE(run(src) == "1\n1\n1\n10\n20\n10");
}


TEST_CASE("namespaces1", "[Namespace]") {
    const auto src = R"(
print = __builtin_print;

x = namespace {
    cls = class { meow: Int = 0; };

    v: String = "hi";

    o: cls = cls(1);

    func = (e: cls) => e.meow;

    infix(+) + = (a: cls, b: cls) => __builtin_add(a.meow, b.meow);

    y = namespace {
        n = 1;
    };
};


v = x :: v;
print(v);
print(x::v);

x::v = "hello";
print(v);
print(x::v);

v = "bye";
print(v);
print(x::v);

)";

    REQUIRE(run(src) == "hi\nhi\nhi\nhello\nbye\nhello");
}





TEST_CASE("Trees", "[Match]") {
    const auto src = R"(
print = __builtin_print;
Nil = "";
Node = class {
    v = 0;
    l = Nil;
    r = Nil;
};
Leaf = class { v = 0; };


test = (x) => match x {
    Leaf(k) & __builtin_geq(k, 0) => 1;
    Leaf(k) & __builtin_leq(k, 0) => 2;
    Leaf(k) & __builtin_eq (k, 0) => 3;
    Node(k, Leaf(_), rChild) & __builtin_geq(k, 0) => 4;
    Node(k, _, _) => 5;
};


x = Leaf(10);
result = test(x);
print(result);

x = Leaf(0);
result = test(x);
print(result);

x = Node(10, Leaf(20), Node(5, Leaf(10), Leaf(10)));
result = test(x);
print(result);

x = Node(10, Node(5, Leaf(10), Leaf(10)), Leaf(20));
result = test(x);
print(result);

x = Node(__builtin_neg(5), Leaf(__builtin_neg(5)), Node(5, Leaf(10), Leaf(10)));
result = test(x);
print(result);
)";


    REQUIRE(run(src) == "1\n1\n4\n5\n5");

}



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
infix(+) + = (a, b) => __builtin_add(a, b);
infix(+) - = (a, b) => __builtin_sub(a, b);
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




