#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <stdexcept>
#include "TestSuite.hxx"

#include "../Type/Type.hxx"




TEST_CASE("Mutual Recursion", "[Func]") {
    const auto src = R"(
print = __builtin_print;

infix + = (a: Int, b: Int) => __builtin_add(a, b);
infix - = (a, b) => __builtin_sub(a, b);
infix < = (a, b) => __builtin_lt(a, b);
infix > = (a, b) => __builtin_gt(a, b);

mixfix(LOW +) if : then : else : = (cond: Bool, thn: Syntax, els: Syntax)
    => __builtin_eval(__builtin_conditional(cond, thn, els));

xact = (f1, g, r) => {
    if g.k then {
        g.k = f1();
        if (r > 0) then { xact(f1, g, r - 1); } else {};
        if (r > 0) then { xact(f1, g, r - 1); } else {};
    } else {};
};

act = (f2) => {
  xact(f2, class{ k = true; }(), 32);
};

while = (c, f) => {
    act(() => {
        if c() then { f(); true; } else false;
    });
};

x = 0;
while(() => x < 10, () => {
    x = x + 1;
    print(x);
});
)";

    REQUIRE(run(src) == R"(1
2
3
4
5
6
7
8
9
10)");
}



TEST_CASE("Structural Subtyping + Functions", "[Class][Func]") {
    const auto src = R"(
print = __builtin_print;

Named = class { name = "doesn't matter"; };

changeName = (x: Named) => x.name = "bye";

Human = class {
    name = "";
    age = 0;
};

h = Human("hi");
print(h);
changeName(h);
print(h);
)";

    REQUIRE(run(src) == R"(Object {
    name = "hi";
    age = 0;
}
Object {
    name = "bye";
    age = 0;
})");
}


TEST_CASE("Shaw's Shinanegans", "[Class]") {
    const auto src = R"(
print = __builtin_print;

A = class {};
B = class {};
f = (x: B) => x;

a = A();
print(__builtin_eq(f(a), a));
)";

    REQUIRE(run(src) == "true");
}



TEST_CASE("Structural Subtyping", "[Class]") {
    const auto src = R"(
print = __builtin_print;

Named = class {
    name = "";
};

Human = class {
    name = "John";
    age = 0;
};


h = Human("Ali", 22);

x: Named = h;

print(h);
print(x);

x.name = "meow";
)";

    REQUIRE(run(src) == R"(Object {
    name = "Ali";
    age = 22;
}
Object {
    name = "Ali";
})");
}


TEST_CASE("Self Outside Class", "[Class]") {
    const auto src = R"(
f = () => std::print(self);
C = class {
    x = 1;

    func = () => {
        f();
    };
};
C(10).func();
)";

    REQUIRE_THROWS_AS(run(src), pie::except::NameLookup);
}


TEST_CASE("Using Variable Before Definition", "[Var]") {
    const auto src = R"(
func1 = () => std::print(x);
func2 = () => {
    x = 22;
    func1();
};
func2();

)";

    REQUIRE_THROWS_AS(run(src), pie::except::NameLookup);
}



TEST_CASE("Mutual Rec still Valid in Class Definition", "[Class]") {
    const auto src = R"(
    class {
        f1 = () => f2();
        f2 = () => f1();
    };
)";

    REQUIRE_NOTHROW(run(src));
}




TEST_CASE("Previous Member in Other Member Init", "[Class]") {
    const auto src = R"(
    class {
        x = 1;
        y = x;
    };
)";

    REQUIRE_THROWS_MATCHES(run(src), std::runtime_error, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("not defined!")));
}


TEST_CASE("Match Invalid Name Case Scope", "[Match][Type][Lex]") {
    const auto src = R"(
print = __builtin_print;

C = class {
    T: Type = Int;
    v = 0;
};


c = C(Bool, "meow");

match c {
    C(type, v: type) => print(type, v);

    C(type) => print("not matched:", v);
};
)";

    REQUIRE_THROWS_AS(run(src), pie::except::NameLookup);
}


TEST_CASE("Match Invalid Structure Type Name", "[Match][Type][Lex]") {
    const auto src = R"(
print = __builtin_print;

C = class {
    T: Type = Int;
    v = 0;
};

c = C(Bool, "meow");

match c {
    C(type, v: type) => print(type, v);

    SomeNonexistantTypeName(type) => print("not matched");
};
)";

    REQUIRE_THROWS_AS(run(src), pie::except::NameLookup);
}



TEST_CASE("Assignment in Union", "[Type][Union]") {
    const auto src = R"(
U = union {
    (T = Int);
    T;
};
__builtin_print(U);
)";

    REQUIRE(run(src) == R"(union { Int; Int; })");
}



TEST_CASE("Type Of", "[Type][Builtin]") {
    const auto src1 = R"(
type = __builtin_type_of;

f = (x, y: type(x), z: type(y)) => 1;
f("1", "2", "3");


f = (x, y: type(x), z: type(y)) => 2;
f(1, 2, 3);
)";

    REQUIRE_NOTHROW(run(src1));

    const auto src2 = R"(
type = __builtin_type_of;

f = (x, y: type(x), z: type(y)) => 3;
f("1", "2", 3);
)";

    REQUIRE_THROWS(run(src2));
}


TEST_CASE("Self 2", "[Class][Var]") {
    const auto src1 = R"(
    self = 5;
    __builtin_print(self);
)";

    REQUIRE(run(src1) == R"(5)");


    const auto src2 = R"(
    __builtin_print(self);
)";

    REQUIRE_THROWS(run(src2));
}



TEST_CASE("Self", "[Class][Var]") {
    const auto src = R"(
Human = class {
    name = "";
    age = 0;

    printSelf = () => __builtin_print(self);
};

Human("x", 2).printSelf();
)";

    REQUIRE(run(src) == R"(Object {
    name = "x";
    age = 2;
    printSelf = (): Any => __builtin_print(self);
})");
}



TEST_CASE("Types as Values 2", "[Func][Type]") {
    const auto src = R"(
:Int = 4;
f = (x: Int): Int => x;
f(5);
)";

    REQUIRE_THROWS(run(src));
}

TEST_CASE("Types as Values", "[Func][Type]") {
    const auto src = R"(
:(Int, Double): String = :(Any, Any): Any;
f: (Int, Double): String = (a, b) => 1;
f(4, false);
)";

    REQUIRE_NOTHROW(run(src));
}

TEST_CASE("Lazy Function Return Type", "[Func][Param][Type]") {
    const auto src = R"(
f = (T): T => 1;
f(Int);
)";

    REQUIRE_NOTHROW(run(src));
}


TEST_CASE("Lazy Function Parameter Types", "[Func][Param][Type]") {
    const auto src = R"(
House1 = class { T = String; };
House2 = class { T = Int; };

X = Int;
func = (h, a: h.T, x: X) => 1;

func(House1(), "1", 1);

X = Bool;
func(House2(), 1, 1);
)";

    REQUIRE_NOTHROW(run(src));
}



TEST_CASE("Function Type Checking", "[Func][Type]") {
    const auto src = R"(
func: (Int): Bool = (x: Int, s: String): Bool => true;
)";

    REQUIRE_THROWS(run(src));
}


TEST_CASE("Complex Lazy Param Types", "[Func][Param][Type]") {
    const auto src1 = R"(
infix + = (a, b) => __builtin_add(a, b);
infix | = (a, b) => union {a; b;};

func = (1 + 2, x: (1 + 2) | 4) => x;
func(String, "hi");
func(String, 4);
)";

    REQUIRE_NOTHROW(run(src1));

    const auto src2 = R"(
infix + = (a, b) => __builtin_add(a, b);
infix | = (a, b) => union {a; b;};

func = (1 + 2, x: (1 + 2) | 4) => x;
func(String, 1 + 2);
)";

    REQUIRE_THROWS(run(src2));
}


TEST_CASE("Leaky Argument 2", "[Func][Param][Type]") {
    const auto src1 = R"(
    func = (T, a: T) => __builtin_print(1);
    func2 = func(Type);
    func2(T);
)";

    REQUIRE_THROWS(run(src1));
}


TEST_CASE("Testing Leaky Argument", "[Func][Param][Type]") {
    const auto src1 = R"(
func = (T, x: T) => __builtin_print(x);
func(Int, 1);
)";

    REQUIRE(run(src1) == "1");


    const auto src2 = R"(
func = (T, x: T) => __builtin_print(x);
func(Int, T);
)";

    REQUIRE_THROWS_WITH(run(src2), Catch::Matchers::ContainsSubstring("not found"));
}


TEST_CASE("Concepts 2", "[Type]") {
    const auto src = R"(
MoreThan10 = (x: Int) => __builtin_gt(x, 10);
LessThan20 = (x: Int) => __builtin_lt(x, 20);
func = (x: MoreThan10): LessThan20 => __builtin_add(x, 5);
func("meow");
)";

    REQUIRE_THROWS_AS(run(src), pie::except::TypeMismatch);
}


TEST_CASE("Concepts", "[Type]") {
    const auto src1 = R"(
MoreThan10 = (x: Int) => __builtin_gt(x, 10);
LessThan20 = (x: Int) => __builtin_lt(x, 20);
a: MoreThan10 = 15;
func = (x: MoreThan10): LessThan20 => __builtin_add(x, 5);
func(13);
)";

    REQUIRE_NOTHROW(run(src1));

    const auto src2 = R"(
MoreThan10 = (x: Int) => __builtin_gt(x, 10);
a: MoreThan10 = 5;
)";

    REQUIRE_THROWS_AS(run(src2), pie::except::TypeMismatch);

    const auto src3 = R"(
MoreThan10 = (x: Int) => __builtin_gt(x, 10);
LessThan20 = (x: Int) => __builtin_lt(x, 20);
func = (x: MoreThan10): LessThan20 => __builtin_add(x, 5);
func(17);
)";

    REQUIRE_THROWS_AS(run(src3), pie::except::TypeMismatch);
}



TEST_CASE("Types as Values - List of Bool vs {Bool}", "[Type]") {
    const auto src1 = R"(
a: {Bool} = {true, false};
v: {Type} = {Bool};
x: v = {Bool};
)";

    REQUIRE_NOTHROW(run(src1));

    const auto src2 = R"(
x: {Bool} = {Bool};
)";

    REQUIRE_THROWS_AS(run(src2), pie::except::TypeMismatch);
}



TEST_CASE("Expanding Pack in Partial Application", "[Func]") {
    const auto src = R"(
print = __builtin_print;
f = (a, b, c, z) => print(a, b, c, z);
caller = (args: ...Any) => f(10, args...);
new_closure = caller(1, 2);
new_closure(1000);
)";


    REQUIRE(run(src) == "10 1 2 1000");
}




TEST_CASE("Values as types w unions", "[Type]") {
    const auto src1 = R"(
print = __builtin_print;

l1 = {1, 2, 3};
l2 = {5, 6};
x: union { l1; l2; } = {5, 6};
print(x);
x = {1, 2,3};
print(x);

y: union { 1; "hi"; Bool; } = 1;
print(y);
y = "hi";
print(y);
y = true;
print(y);
y = false;
print(y);
)";

    REQUIRE(run(src1) == R"({5, 6}
{1, 2, 3}
1
hi
true
false)");

    const auto src2 = R"(
l1 = {1, 2, 3};
l2 = {5, 6};
x: union { l1; l2; } = {1, 2, 3, 5, 6};
)";

    REQUIRE_THROWS_AS(run(src2), pie::except::TypeMismatch);

    const auto src3 = R"(
y: union { 1; "hi"; Bool; } = "bye";
)";

    REQUIRE_THROWS_AS(run(src3), pie::except::TypeMismatch);
}


TEST_CASE("Redefining builtin type, espacially Any", "[Type]") {
    const auto src = R"(
Any = class { };

f = (a) => a;
f("");

f = (a: Any): Any => a;
f(Any());
)";


    REQUIRE_NOTHROW(run(src));
}


TEST_CASE("Class + Object Equality", "[Object][Class]") {
    const auto src = R"(
print = __builtin_print;

x = 1;
y = 1;
ns = space {
    x: Any = 2;
    y = 2;

    printX1 = () => print(::x);
    printX2 = () => print(x);

    printY1 = () => print(::y);
    printY2 = () => print(y);
};

ns::printX1();
ns::printX2();

ns::printY1();
ns::printY2();
)";


    REQUIRE(run(src) == R"(1
2
2
2)");
}


// TEST_CASE("Class + Object Equality", "[Object][Class]") {
//     const auto src = R"(
// print = __builtin_print;

// X = class { a = 1; b = a; };
// Y = class { a = 1; b = 1; };
// print(__builtin_eq(X, Y));
// print(__builtin_eq(X(2), Y(2)));
// print(X(2));
// print(Y(2));
// )";

// // maybe the first shouldn't be true
//     REQUIRE(run(src) == R"(true
// true
// Object {
//     a = 2;
//     b = 1;
// }
// Object {
//     a = 2;
//     b = 1;
// })");
// }


TEST_CASE("Class + Object Equality with NaN", "[Object][Class][Type]") {
    const auto src = R"(
print = __builtin_print;

A = class { value = __builtin_div(0.0, 0.0); };
x = __builtin_div(0.0, 0.0);
print(__builtin_eq(A(), A()));
print(__builtin_eq(x, x));
)";

    REQUIRE(run(src) == "false\nfalse");
}



TEST_CASE("Class + Object Equality", "[Object][Class][Type]") {
    const auto src = R"(
print = __builtin_print;

B = class { value = __builtin_neg(0.0); };
C = class { value = 0.0; };
print(__builtin_eq(B, C));
print(__builtin_eq(B(), C()));

)";

    REQUIRE(run(src) == "true\ntrue");
}


TEST_CASE("Returning Function with Member Variable Capture", "[Var][Func][Class]") {
    const auto src = R"(
print = __builtin_print;

C = class {
    s = "";

    makeSModifier = () => {
        (new_s) => s = new_s;
    };
};

o = C();

func = o.makeSModifier();
func("hmm");
print(o.s);
)";

    REQUIRE(run(src) == "hmm");
}


TEST_CASE("Returning Function with Local Variable Capture - Multiple Scopes", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    x: Int = 1;
    {
        x: Int = 2;
        () => print(x);
    };
};

func = makeFunc(100);
func();
)";

    REQUIRE(run(src) == "2");
}


TEST_CASE("Returning Function with Local Variable Capture (Eager Params) - Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = () => {
    f = (x, T, a: T) => print(T);

    T = Double;
    f(5);
};
func = makeFunc();
func(Int, 10);
)";

    REQUIRE(run(src) == R"(Int)");
}


TEST_CASE("Returning Function with Local Variable Capture (Multiple Decls) - Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;


makeFunc = (z) => {
    print(z);
    z: Any = 999;
    print(z);
    z: String = "what!";
    print(z);
    () => print(z);
};
func = makeFunc(100);
func();
)";

    REQUIRE(run(src) == R"(100
999
what!
what!)");
}


TEST_CASE("Returning Function with Local Variable Capture (Declare new var) - Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    {
        z: Any = "Fuck";
        print("z =", z);
    };
    print("z =", z);
    () => print("z =", z);
};
func = makeFunc(100);
func();

)";

    REQUIRE(run(src) == R"(z = Fuck
z = 100
z = 100)");
}


TEST_CASE("Returning Function with Local Variable Capture (Reassign) - Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    {
        z = "Fuck";
        print("z =", z);
    };
    print("z =", z);
    () => print("z =", z);
};
func = makeFunc(100);
func();

)";

    REQUIRE(run(src) == R"(z = Fuck
z = Fuck
z = Fuck)");
}


TEST_CASE("Returning Function with Local Variable Capture - No Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => () => print(z);

func = makeFunc(100);
func();
)";

    REQUIRE(run(src) == "100");
}


TEST_CASE("Returning Function with Local Variable Capture", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    () => print(z);
};

func = makeFunc(100);
func();
)";

    REQUIRE(run(src) == "100");
}


TEST_CASE("Reassignment", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    z = 999;
    () => print(z);
};

func = makeFunc(100);
func();
)";

    REQUIRE(run(src) == "999");
}


TEST_CASE("Shadowing", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    z: Any = 999;
    z: String = "what!";
    () => print(z);
};

func = makeFunc(100);
func();
)";

    REQUIRE(run(src) == "what!");
}


TEST_CASE("Eager Type Parameters For Expanded Calls", "[Type][Func]") {
    const auto src = R"(
func = (T, a: Int, x) => __builtin_print("called with T =", T, "and a =", a, "and x =", x);
call = (args: ...Any) => func(args...);
f = call(Int, 1, "hi");
)";

    REQUIRE(run(src) == "called with T = Int and a = 1 and x = hi");
}


TEST_CASE("Eager Type Parameters For Partial Application", "[Type][Func]") {
    const auto src = R"(
T = Type;
func = (t: T, T: t, a: T, T) => T;
f = func(Type, Int);
f(1, String);
)";

    REQUIRE_NOTHROW(run(src));
}


TEST_CASE("Eager Type Parameters 2", "[Type]") {
    const auto src = R"(
func = (T, a: T) => T;

func(Int, 1);
)";

    REQUIRE_NOTHROW(run(src));
}


TEST_CASE("Multiple Named Arguments", "[Func]") {
    const auto src = R"(
func = (T, T: T) => T;

func(T=Type, T=Int);
)";

    REQUIRE_THROWS(run(src));
}


TEST_CASE("Invalid Eager Type Parameters", "[Type]") {
    const auto src = R"(
func = (T, T: T) => T;

func(Type, T=Int);
)";

    REQUIRE_THROWS(run(src));
}

TEST_CASE("Eager Type Parameters", "[Type]") {
    const auto src = R"(
func = (T, T: T) => T;

func(Type, Int);
)";

    REQUIRE_NOTHROW(run(src));
}


TEST_CASE("Type Alias 3", "[Type]") {
    const auto src = R"(
print = __builtin_print;

Func = :(Int, Int, Double): Any;

f: Func = (a: Int, c: Any, b: String): String => "hi";
)";

    REQUIRE_THROWS(run(src));
}


TEST_CASE("Type Alias 2", "[Type]") {
    const auto src = R"(
print = __builtin_print;

Func = :(Int, Int, String): Any;

f: Func = (a: Int, c: Any, b: String): String => "hi";

print(Func);
)";

    REQUIRE(run(src) == R"((Int, Int, String): Any)");
}


// do I want this behaviour to change?
TEST_CASE("Eager Function Parameter Type Evaluation", "[Type]") {
    const auto src = R"(
print = __builtin_print;


d: Type = Double;

func = (x: d) => print(x);

d = Int;

func(1.2);
)";

    REQUIRE(run(src) == R"(1.200000)");
}


TEST_CASE("Type Alias", "[Type]") {
    const auto src = R"(
d: Type = Double;
i = Int;
x: i = 1;
i = d;
y: i = 1.2;
)";

    REQUIRE_NOTHROW(run(src));
}


TEST_CASE("Invalid Type Alias", "[Type]") {
    const auto src = R"(
d = Int;
x: d = 1.2;
)";

    REQUIRE_THROWS(run(src));
    REQUIRE_THROWS_MATCHES(run(src), pie::except::TypeMismatch, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Type mis-match!")));
}


TEST_CASE("Lists vs Maps 2", "[List][Map]") {
    const auto src = R"(
print = __builtin_print;


.: m: {Any: Any} = {"one": 2, class {}: 3, 4: "five", 0: () => 1, () => 0: 1};
.: print(m);

m: {Any: Any} = {"one": 2};
print(m);

m: {Any: Any} = {class {}: 3};
print(m);

m: {Any: Any} = {4: "five"};
print(m);

m: {Any: Any} = {0: () => 1};
print(m);

m: {Any: Any} = {() => 0: 1};
print(m);


name1 = 1;
name3 = 3;
name4 = 4;

res = { name1: Int = name3; }; 
print(res);

res = { name1: String = "hello" }; 
print(res);

res = { (name1: Int = name3): name4 }; .: { expr: expr }; MAP
print(res);

res = { class { }; };
print(res);

res = { class { func = (): Int => 1; }; };
print(res);

res = { class { func = (): Int => 1; } };
print(res);

res = { class { func = (): Int => 1; }: 1 };
print(res);
)";

    REQUIRE(run(src) == R"({one: 2}
{class { }: 3}
{4: five}
{0: (): Any => 1}
{(): Any => 0: 1}
3
{1: hello}
{3: 4}
class { }
class {
    func: Any = (): Int => 1;
}
{class {
    func: Any = (): Int => 1;
}}
{class {
    func: Any = (): Int => 1;
}: 1})");
}



TEST_CASE("Lists vs Maps", "[List][Map]") {
    const auto src = R"(
print = __builtin_print;
name1 = 1;
name3 = 3;
name4 = 4;

scope =  {    name1: Int = name3; };
map1  =  {    name1: String = name3  };
map2  =  {    name1: String: Int = name3  };
list  =  { 1, name1: Int = name3  };

print(scope);
print(map1);
print(map2);
print(list);
)";

    REQUIRE(run(src) == R"(3
{1: 3}
{1: 3}
{1, 3})");
}


TEST_CASE("Single Element List", "[List]") {
    const auto src = R"(
print = __builtin_print;

list1 = {1};
list2: {Int} = {2};
list3: {Any} = {3};

print(list1);
print(list2);
print(list3);
)";

    REQUIRE(run(src) == R"({1}
{2}
{3})");
}

TEST_CASE("Empty List", "[List]") {
    const auto src = R"(
print = __builtin_print;

list1 = {};
list2: {Int} = {};
list3: {Any} = {};

print(list1);
print(list2);
print(list3);
)";

    REQUIRE(run(src) == R"({}
{}
{})");
}

TEST_CASE("Looping Over List", "[List][Loop]") {
    const auto src = R"(
print = __builtin_print;

list = {1, "Hi", 3.14, true, class {}};


loop list => e print(e);
)";

    REQUIRE(run(src) == R"(1
Hi
3.140000
true
class { })");
}


TEST_CASE("List Types", "[Type]") {
    auto Any  = type::builtins::Any();
    auto Int  = type::builtins::Int();
    auto Int_list = std::make_shared<type::ListType>(type::builtins::Int());
    auto Any_list = std::make_shared<type::ListType>(type::builtins::Any());


    REQUIRE_FALSE(*Int_list >  *Int);
    REQUIRE_FALSE(*Int_list >= *Int);

    REQUIRE(*Any_list >  *Int_list);
    REQUIRE(*Any_list >= *Int_list);

    REQUIRE_FALSE(*Any_list >  *Any);
    REQUIRE_FALSE(*Any_list >= *Any);

    REQUIRE(*Any >  *Any_list);
    REQUIRE(*Any >= *Any_list);

    REQUIRE_FALSE(*Int_list == *Int);
    REQUIRE_FALSE(*Int == *Int_list);

    REQUIRE_FALSE(*Int >  *Int_list);
    REQUIRE_FALSE(*Int >= *Int_list);
}


TEST_CASE("See Through Value Mutability", "[Mutability]") {
    const auto src = R"(
print = __builtin_print;

infix + = (a, b) => __builtin_add(a, b);


x = 1;

old = x("Hello 1");
x = "Hello x"; .: different

print(old);
print(1);
print(x);

(6 + 7)("Hi");
6 + 7 = "Bye";

print(7 + 6); .: 13
print(13); .: Hi
print(6 + 7); .: Bye
)";

    REQUIRE(run(src) == R"(1
Hello 1
Hello x
13
Hi
Bye)");
}



TEST_CASE("Looping Over Pack Without Loop Var", "[Loop]") {
    const auto src = R"(
func = (pack: ...Any) => loop pack => __builtin_print(0);

func(1, "Hi", 3.14);
)";

    REQUIRE(run(src) == R"(0
0
0)");
}


TEST_CASE("Looping Over Pack", "[Loop]") {
    const auto src = R"(
func = (pack: ...Any) => loop pack => e __builtin_print(e);

func(1, "Hi", 3.14);
)";

    REQUIRE(run(src) == R"(1
Hi
3.140000)");
}


TEST_CASE("Looping Over Range Without Loop Var", "[Loop]") {
    const auto src = R"(
loop 5 => __builtin_print("fuck");
)";

    REQUIRE(run(src) == R"(fuck
fuck
fuck
fuck
fuck)");
}


TEST_CASE("Looping Over Range", "[Loop]") {
    const auto src = R"(
loop 5 => i __builtin_print(i);
)";

    REQUIRE(run(src) == R"(0
1
2
3
4)");
}


TEST_CASE("Iterator", "[Loop][Self Mutation]") {
    const auto src = R"(
Iota: Type = class {
    limit = __builtin_neg(1);

    i = 0;

    hasNext = (): Bool => __builtin_not(__builtin_eq(i, limit));
    next = () => {
        x = i;
        i = __builtin_add(i, 1);
        x;
    };
};

loop Iota(10) => num __builtin_print(num);
)";

    REQUIRE(run(src) == R"(0
1
2
3
4
5
6
7
8
9)");
}


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



TEST_CASE("Arbitrary function", "[Params]") {
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



TEST_CASE("nested unions", "[Union]") {
    const auto src = R"(
print = __builtin_print;

Class = class {
    x: Int = 0;
    s: String = "";
};

Num = union { Int; Double; };
Union = union { Num; Class; };

u: Union = 1;
print(u);
u: Union = 1.4;
print(u);
u = Class(10, "hi");
print(u);
WithStr = union { Union; String; };
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
union { union { Int; Double; }; class {
        x: Int = 0;
        s: String = "";
    }; })");

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

x = space {
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

x = space {
    cls = class { meow: Int = 0; };

    v: String = "hi";

    o: cls = cls(1);

    func = (e: cls) => e.meow;

    infix(+) + = (a: cls, b: cls) => __builtin_add(a.meow, b.meow);

    y = space {
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

x = space {
    cls = class { meow: Int = 0; };

    v: String = "hi";

    o: cls = cls(1);

    func = (e: cls) => e.meow;

    infix(+) + = (a: cls, b: cls) => __builtin_add(a.meow, b.meow);

    y = space {
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



TEST_CASE("Invalid Named Arguments", "[Params]") {
    const auto src1 =
R"(
print = __builtin_print;
add = (a, z: Double): Int => 0;

add(h = 1);
)";

    const auto src2 =
R"(
print = __builtin_print;
add = (a, z: Double): Int => 0;

add(z = 1);
)";


    REQUIRE_THROWS_MATCHES(run(src1), std::runtime_error, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Named argument")));
    REQUIRE_THROWS_MATCHES(run(src2), pie::except::TypeMismatch, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Type mis-match!")));
}


TEST_CASE("Named Arguments", "[Params]") {

    const auto src =
R"(
print = __builtin_print;
add = (x: Int, y, a: Int, b, c, d: Int, e: String, f, g, z: Double): Int => 0;

print(add(g = 7, 1, c = 3, 2, 4, 5, f = 6, 1, "", 1.));
)";


    REQUIRE(run(src) == "0");
}

