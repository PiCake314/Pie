# Welcome to Pie Lang!


## Philosophy
<!-- This language aims to be unique, but it also should still feel familiar. Here are some of it's "features": -->
- Pie aims to be unique yet feel familiar
- Everything is an expression
- Bare-bones (if it doesn't need to be keyword, then it isn't)
- No operators defined for you
- Still quirky (different even if the difference is not good)
- No null/unit/none type or any type indicating nothing.

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

There are `3` types of operators that can be defined:
- `prefix`
- `infix`
- `suffix`

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


## Builtins
Since Pie doesn't provide any operators, how does one achieve ANYTHING at all with Pie?\
Pie reserves the names starting with `__builtin_`.

#### Nullary Functions:
- `__builtin_true`
- `__builtin_false`

#### Unary Functions:
- `__builtin_print`
- `__builtin_neg`
- `__builtin_not`
- `__builtin_reset`
- `__builtin_eval`

#### Binary Functions:
- `__builtin_add`
- `__builtin_sub`
- `__builtin_mul`
- `__builtin_div`
- `__builtin_gt`
- `__builtin_geq`
- `__builtin_eq`
- `__builtin_leq`
- `__builtin_lt`
- `__builtin_and`
- `__builtin_or`

#### Trinary Functions:
- `__builtin_conditional`


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


## Comments:
Any thing following one of the these will be considered a comment until a newline character `'\n'` is encountered (or EOF):
- `comment:`
- `todo:`
- `note:`
- `btw:`
- `ps:`

Casing doesn't matter.

### Todo:
##### in order of priority:
- [ ] Add circumfix operators
- [ ] Add ternary operators
- [ ] Add an import system (modules)
- [ ] Add collections
- [ ] Add looping
- [ ] Add iterators
- [ ] Add variadic arguments
- [ ] Add method operators..?
- [ ] Add recursive operators
---
### Done:
- [x] Add lazy evaluation
- [x] Add constructor to classes..somehow
- [x] Allow recursion..somhow
- [x] Add classes
- [x] Add assignment to any epxression
- [x] Add booleans
- [x] Add closures
---
### Long term:
- World domination