# Welcome to Pie Lang!


## Philosophy
<!-- This language aims to be unique, but it also should still feel familiar. Here are some of it's "features": -->
- Pie aims to be unique yet feel familiar
- Everything is an expression
- Bare-bones (if it doesn't need to be keyword, then it isn't)
- No operators defined for you
- Still quirky (different even if the difference is not good)
- No null/unit/none type or any type indicating nothing

## Introduction
- [Variables](#Variables)
- [Closures](#Closures)
- [Classes](#Classes)
- [Scopes](#Scopes)
- [Operators](#Operators)
- [Built-in Functions](#Builtins)
- [Types](#Types)
- [Comments](#Comments)


## Variables:
Define variables using assignment with an optional type:
```pie
x = 5;
y: Int = 5;
```


## Closures:
Closures have a familiar syntax (JS syntax).\
A nullary closure that returns `1` when called:
```pie
() => 1;
```
A closure that takes two arguments and returns the first:
```pie
(x, y) => x;
```
Closures can be assigned to variables:
```pie
func = (x) => "yay";
```

## Classes
A class is a block (scope) preceded by the `class` keyword. The block must consist of **ONLY** assignments:
```pie
Human: Type = class {
    name: String = "";
    age: Int = 0;

    prettyPrint = () => {
        __builtin_print(name);
        __builtin_print(age);
    };
};
```
Construct an object by calling the constructor of the class. Optionally pass initial values to the data members:
```pie
h: Human = Human("Pie", 1);
h.age = 10;
h.prettyPrint();
```
## Scopes:
Since everything is an expression, so are scopes! They take the value of the last expression in them.\
Here, `x` will be assigned to 3.
```pie
x = {
    1;
    2;
    3;
};
```
Since scopes take the value of their last expression, scopes cannot be empty!\
The following line will error:
```pie
x = { };
```
## Operators
Pie doesn't provide any operators. One has to define their own. For that reason, any operator symbol (+, -, *, /, etc...) can be used as a variable name.

There are `5` types of operators that can be defined:
- `prefix`:\
    e.g. `- 1`
- `infix`:\
    e.g. `1 + 2`
- `suffix`:\
    e.g. `5 !`
- `exfix`:\
    e.g. `[ 0 ]`
- `operator`:\
    e.g. [example](#Operator-Example)

Here is how to define your own operator:
```pie
prefix(LOW +) always_one = (x) => 1;
```
`always_one` is now a prefix operator that when applied, always returns `1`!\
In this example, `a` will come out as 1.
```
a = always_one 5;
```
`infix` has to be assigned to a binary closure while `prefix` and `suffix` have to be assigned to a unary closure.

What goes between the parenthesis after the keyword `prefix` is the precedence.\
Here is the list of the precedence levels (from lowest to highest)
- `ASSIGNMENT`
- `INFIX`
- `SUM`
- `PROD`
- `PREFIX`
- `POSTFIX`
- `CALL`

To create an operator that has a precedence higher than `SUM` but lower than `PROD`, then attach a `+` or `-` after the level. Note that a higher level with a `-` is still higher than a lower level with a `+`.\
i.e: `(PROD -) > (SUM +)`

### Operator Example

## Builtins
Since Pie doesn't provide any operators, how does one achieve ANYTHING at all with Pie?\
Pie reserves the names starting with `__builtin_`.

#### Nullary Functions:
- `__builtin_true`
- `__builtin_false`

#### Unary Functions:
- `__builtin_neg`
- `__builtin_not`
- `__builtin_mod`
- `__builtin_reset`
- `__builtin_eval`

#### Binary Functions:
- `__builtin_add`
- `__builtin_sub`
- `__builtin_mul`
- `__builtin_div`
- `__builtin_pow`
- `__builtin_gt`
- `__builtin_geq`
- `__builtin_eq`
- `__builtin_leq`
- `__builtin_lt`
- `__builtin_and`
- `__builtin_or`

#### Trinary Functions:
- `__builtin_conditional`

#### Variadic Functions:
- `__builtin_print` (returns the last argument)

## Types:
Pie has 7 types:
- `Int`
- `Double`
- `Bool`
- `String`
- `Any`
- `Type`
- `Syntax`

If something is left un-typed, the `Any` type will be given to it.

Functions can also be typed: `(T1, T2): T3`
```pie
one: (Int): Int = (x: Int): Int => 1;
```

### Syntax Type:
The `Syntax` type is a special type that gives a handle onto the AST node used to represent an expression.\
Take this example:
```pie
operator(SUM) + = (a, b) => __builtin_add(a, b);

x: Syntax = 1 + a;
```
`x` is a hadle to the AST which represents the expression `1 + a`. 
To evaluate `x`, you just need to call __builtin_eval on it:\
```pie
result = __builtin_eval(x);
```
However, evaluating `x` right now will error since `a` is not defined. All we need to do is define `a` before evaluating `x`.
```pie
a = 5;
result = __builtin_eval(x);
__builtin_print(result);
```
`6` will be printed.


## Comments:
#### Line comments:
```pie
.: this is a comment
this isn't
```
#### Block comments:
```pie
.::
    this is a comment
this is also a comment
so is this::.
this isn't
```

## Install
Make sure you have `git`, and C++ compiler that supports C++23. Then paste the following script in the terminal:
```
mkdir PieLang
cd PieLang

git clone https://github.com/intel/cpp-std-extensions includes/cpp-std-extensions
git clone https://github.com/boostorg/mp11 includes/mp11
git clone https://github.com/PiCake314/Pie

g++ -std=c++23  -Iincludes/mp11/include/ -Iincludes/cpp-std-extensions/include/ -O2 Pie/main.cc -o pielang
```
## Update
To update the language, paste this into the terminal:
```
cd Pie
git pull
cd ..

g++ -std=c++23  -Iincludes/mp11/include/ -Iincludes/cpp-std-extensions/include/ -O2 Pie/main.cc -o pielang
```


### Todo:
##### in order of priority:
- [ ] Add method operators..?
= [ ] Add overloading operators
- [ ] Add variadic arguments
- [ ] Change __builtin_{true|false} to true/false;
- [ ] Add namespaces
- [ ] Add an import system (modules)
- [ ] Add collections
- [ ] Add named arguments
- [ ] Use Big Int instead of `int32_t`;
- [ ] World domination
- [ ] Add iterators
- [ ] Add REPL
- [ ] Remove depedency on stdx and boost
- [ ] Add recursive operators
- [ ] Add LLVM backend

---
### Done:
- [x] Change comments to `.::` and `::.`
- [x] Add Arbitrary Operators!!!
- [x] Add circumfix operators
- [x] Add lazy evaluation
- [x] Add constructor to classes..somehow
- [x] Allow recursion..somhow
- [x] Add classes
- [x] Add assignment to any epxression
- [x] Add booleans
- [x] Add closures
---
### Discarded:
- ~~[ ] Add looping~~