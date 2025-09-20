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

- [Variables](#variables)
- [Closures](#closures)
- [Classes](#classes)
- [Scopes](#scopes)
- [Operators](#operators)
- [Overloading](#overloading)
- [Built-in Functions](#builtins)
- [Types](#types)
- [Keywords List](#keywords-list)
- [Reserved Punctuaion](#reserved-punctuaion)
- [Comments](#comments)
- [Install](#install)
- [Update](#update)

## Variables

Define variables using assignment with an optional type:

```pie
x = 5;
y: Int = 5;
```

## Closures

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

### Named Arguments

Any parameter can be named in the argument list in any order:

```pie
func = (one, two) => one;

func(two=2, one=1);
func(two=1, 1);
```

### Variadic Functions

A function can have at most **one** variadic parameter. The variadic paramater can be anywhere in the parameter list.

The variadic argument has to be annotated with a type with leading ellipsis `...<type>`:

```pie
getLast = (all: ...Any, last) => last;

x = getLast(1, 2, 3);
```

`x` will be 3.

Use the trailing `...` to expand a pack:
```pie
getFirst = (first, rest: ...Any) => first;

forward = (args: ...Any) => getFirst(args...);
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

## Scopes

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
- `mixfix`:\
    e.g. `if 1 then 2 else 3`

Here is how to define your own operator:

```pie
prefix(PREFIX) always_one = (x) => 1;
```

`always_one` is now a prefix operator that when applied, always returns `1`!\
In this example, `a` will come out as 1.

```pie
a = always_one 5;
```

`infix` has to be assigned to a binary closure while `prefix` and `suffix` have to be assigned to a unary closure.

### Precedence
What goes between the parenthesis after the keyword (i.e.`prefix(SUM)`) is the precedence.\
Here is the list of the precedence levels (from lowest to highest)

- `ASSIGNMENT`
- `INFIX`
- `SUM`
- `PROD`
- `PREFIX`
- `POSTFIX`
- `CALL`

To create an operator that has a precedence higher than `SUM`, one can attach a plus sign, making the precedence `SUM +`. Note that a higher level with a `-` is still higher than a lower level with a `+`.\
i.e: `(PROD -) > (SUM +)`

An operator can also have the precedence of another operator:
```pie
infix(SUM)   add = (a, b) => __builtin_add(a, b);
infix(add)   sub = (a, b) => __builtin_sub(a, b);
infix(add +) mul = (a, b) => __builtin_mul(a, b);
```
Operator `sub` has a precedence that is equal to operator `add`'s precedence. Operator `mul`, on the other hand, has a higher precedence.

<!-- ### Operator Example

```pie

``` -->

## Overloading

You can overload operators based on the parameter types"
```pie
infix(SUM) + = (a: Int, b: Int): Int => __builtin_add(a, b);
infix(SUM) + = (a: String, b: String): String => __builtin_concat(a, b);

1 + 2;
"Hi" + "Bye";
```
The  `1 + 2` calls the first operator. `"Hi" + "Bye"` calls the second!


## Builtins

Since Pie doesn't provide any operators, how does one achieve ANYTHING at all with Pie?\
Pie reserves the names starting with `__builtin_`.

### Nullary Functions

- `__builtin_true`
- `__builtin_false`

### Unary Functions

- `__builtin_neg`
- `__builtin_not`
- `__builtin_mod`
- `__builtin_reset`
- `__builtin_eval`

### Binary Functions

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

### Trinary Functions

- `__builtin_conditional`

### Variadic Functions

- `__builtin_print` (returns the last argument)
- `__builtin_concat`

## Types

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

### Syntax Type

The `Syntax` type is a special type that gives a handle onto the AST node used to represent an expression.\
Take this example:

```pie
operator(SUM) + = (a, b) => __builtin_add(a, b);

x: Syntax = 1 + a;
```

`x` is a hadle to the AST which represents the expression `1 + a`.
To evaluate `x`, you just need to call __builtin_eval on it:

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

## Comments

### Line comments

```pie
.: this is a comment
this isn't
```

#### Block comments

```pie
.::
    this is a comment
this is also a comment
so is this::.
this isn't
```


## Keywords List

<!-- - import -->
- `prefix`
- `infix`
- `suffix`
- `exfix`
- `mixfix`
- `class`
- `true`
- `false`

## Reserved Punctuaion

- `__builtin_*`
- `( )`
- `{ }`
- `,`
- `.`
- `:`
- `;`

## Install

Make sure you have `git`, and C++ compiler that supports C++23. Then paste the following script in the terminal:

```pie
mkdir PieLang
cd PieLang

git clone https://github.com/intel/cpp-std-extensions includes/cpp-std-extensions
git clone https://github.com/boostorg/mp11 includes/mp11
git clone https://github.com/PiCake314/Pie

g++ -std=c++23  -Iincludes/mp11/include/ -Iincludes/cpp-std-extensions/include/ -O2 Pie/main.cc -o pielang
```

## Update

To update the language, paste this into the terminal:

```pie
cd Pie
git pull
cd ..

g++ -std=c++23  -Iincludes/mp11/include/ -Iincludes/cpp-std-extensions/include/ -O2 Pie/main.cc -o pielang
```

### Todo

#### in order of priority

- [ ] Add input method (file & console)
- [ ] Add namespaces
- [ ] Add an import system (modules)
- [ ] Add match expression (like scala)
- [ ] Make `=` and `=>` overloadable
- [ ] Add fold expressions (like C++)
- [ ] Use Big Int instead of `int32_t`;
- [ ] Fix builin reset (value-rest, reset/name-reset) 
- [ ] World domination
- [ ] Add REPL
- [ ] Remove depedency on stdx and boost
- [ ] Add recursive operators
- [ ] Add LLVM backend

---

### Done

- [x] Fixed infix operators parsing right to left!
- [x] Implemnted `__builtin_eq` for all values!
- [x] Add named parameters to some builtin functions
- [x] Add variadic arguments
- [x] Add named arguments
- [x] Change __builtin_{true|false} to true/false;
- [x] Fix operator return type checking..?
- [x] Add overloading operators
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

### Discarded

- ~~Add collections~~
- ~~Add iterators~~
- ~~Add looping~~
- ~~Add method operators..?~~
