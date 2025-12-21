# Welcome to Pie Lang!

## Philosophy
<!-- This language aims to be unique, but it also should still feel familiar. Here are some of it's "features": -->
- Pie aims to be unique yet feel familiar
- Everything is an expression (null/unit/none type)
- Bare-bones (if it doesn't need to be keyword, then it isn't + no operators)
- Still quirky (different even if the difference is not good)

## Introduction

- [Variables](#variables)
- [Closures](#closures)
- [Classes](#classes)
- [Unions](#unions)
- [Lists](#lists)
- [Maps](#maps)
- [Loops](#loops)
- [Match Expressions](#match-expressions)
- [Namespaces](#namespaces)
- [Scopes](#scopes)
- [Operators](#operators)
- [Overloading](#overloading)
- [Packs](#packs)
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

## Unions
Unions in Pie are what other languages call "Sum Types":

depricated:
~~U: Type = Union { Int | Double | String };~~
```pie
U: Type = Union { Int; Double; String; };

x: U = 1;
y: U = 3.14;
z: U = "Hello";
```
Note that they also work with user defined types.


## Lists
`name: {type} = {expr1, expr2, expr3};`


## Maps
`name: {type1: type2} = {key1: value1, key2: value2};`


## Loops
First, let's explore the general syntax of the loop construct, then we'll explore the other kinds.


```pie
loop 10 => {
    __builtin_print("Hi");
};
```
This programs prints "Hi" 10 times.\
We can also introduce a loop variable:
```pie
loop 10 => i {
    __builtin_print(i);
};
```
Note that the braces can be omitted (with or without the loop variable).


**Kinds of Loops:**
There are 4 kinds of loops in Pie. They all utilize the `loop` keyword. The kind of the loops depends on the type of the loop operand

#### For Loop
When the type of the operand is an `Int`

#### While Loop
When the type of the operand is a `Bool`

#### Pack Loop
When the type of the operand is a pack of any kind (i.e. `...Any`)

#### Iterator Loop
When the type of the operand is an Object. The object **MUST** follow the **Iterator Protocol** which is defined as follows:
For an object to be qualified as an iterator, it must define 2 methods:
- `hasNext(): Bool`
- `next()`

`hasNext` must return a boolean indicating whether the loops should continue or terminate. `next` yields the next value.


#### Break/Continue


## Match Expressions

Match expressions can match against 3 things.
1. Value
2. Type
3. Structure


**Value Matching**:
```pie
x = 1;

match x {
    =1 => print("one");
    =2 => print("two");
    ="hi" => print("some string");
};
```

**Type Matching**:
```pie
x = 1;

match x {
    :String => print("str");
    :Int => print("num");
    :Double => print("float");
};
```


**Type Matching**:
```pie
C = class { a = 0; b = "";};
c = C(314, C(1, "two"));

match c {
    C(x: Int, ="two") => print(x);
    C(y=3, :String = "something") => print(2);
    C(n: Int = 314, C(=1, ="two")) => print(n);
};
```
The code above ends up printing `314`.

Note how you can match both the value and the type at the same time. You can even give the matched value a name.
The collection of the tokens:\
`<name>: <type> = <value>`\
is called a `Single`.



 **Guards and such**:
 You can match against multiple patterns in a single case by using the pipe symbol `|` and you can guard against any case by using the ampersand `&`:

 ```pie
x = 5;

match x {
    =1 | =2 | =3 => print("one two three");
    a & __builtin_lt(a, 0) => print("negative");
    a & __builtin_gt(a, 0) => print("positive (not 1, 2, or 3)");
};
```

Of course, you can have both conditions and pipes in the same case.


## Namespaces

Like everything else in this language, Namespaces are expressions too, and they yield a value!
Declare a namespace by using the `space` keyword. Assign a namespace to a variable in order to name it:

```pie
my_space = space {
    __builtin_print("start");

    decl1: Int = 1;
    ID: (Any): Any = (x) => x;

    nested = space {
        __builtin_print("inner");
        decl2 = "Hi";
    };

    __builtin_print("finish");
};
```

Namespaces could seem like just a syntactic for `class`'s, but they're not! There is a major difference which is the fact that you can run arbitrary code inside namespaces. A class may only have assignments.

To access a member of a `namespace`, use the "scope resolution operator", or `::`:

```pie
x = space { a = 1; };
__builtin_print(x::a);
```

Assigning a namespace to an existing namespace will consolidate the two namespaces onto the first:

```pie
ns = space {
    a = 1;
    b = 2;
    c = 3;
};

ns = space {
    a = 5;
    x = 10;
    y = 20;
};

__builtin_print(ns::a); .: prints 5
__builtin_print(ns::b); .: prints 2
__builtin_print(ns::x); .: prints 10
```
This allows you to split code that belongs to a single namespace in multiple different files and have all the declarations be in the same namespace.

**Keep in mind**, if you assign a namespace to another value, it loses it's content:

```pie
x = space { a = 1; };
x = 5;
x = space { b = 1; };

__builtin_print(x::a); .: ERROR!
```
### `use` directive

The `use` directive pulls all the names in from a namespace into the current namespace.
```pie
ns = space {
    x = 1;
    y = "hi";
    z = 3.14;
};

use ns;

__builtin_print(x);
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
prefix(!) always_one = (x) => 1;
```

`always_one` is now a prefix operator that when applied, always returns `1`!\
In this example, `a` will come out as 1.

```pie
a = always_one 5;
```

`infix` has to be assigned to a binary closure while `prefix` and `suffix` have to be assigned to a unary closure.

### Precedence
What goes between the parenthesis after the keyword (i.e.`prefix(+)`) is the precedence. You can use any operator you want and Pie will figure it out automatically. For example, operators with precedence level `+` have a lower precedence than operators annotated with precedence level `*`. You can use user-defined-operators as precedence level.

One can nudge the precedence level by attaching a `+` or a `-` after a precedence level:
```pie
infix(*)   star = (a, b) => 1;
infix(* -) plus = (a, b) => 2;
```

User defined operator `` has a lower

An operator can also have the precedence of another operator:
```pie
infix(+)     add = (a, b) => __builtin_add(a, b);
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
infix(+) + = (a: Int, b: Int): Int => __builtin_add(a, b);
infix(+) + = (a: String, b: String): String => __builtin_concat(a, b);

1 + 2;
"Hi" + "Bye";
```
The `1 + 2` calls the first operator. `"Hi" + "Bye"` calls the second!

## Packs
Pakcs in Pie are analogous to C++'s packs. One can only declare packs as a function parameter:
```pie
func = (pack: ...Any) => __builtin_print(pack);
func(1, "Hello", 3.14);
```
Note that to declare a pack, the argument **MUST** be given a type preceeded by ellipses. Packs may be empty.

#### Fold Expressions:
Pie supports Fold Expressions, much like C++:

##### Unary left fold
`(pack + ...)` 

##### Unary right fold
`(... + pack)` 

#### Binary left fold
`(init + pack + ...)` 
`init` will be used as an initial value. Helps in the case where the pack is empty:

##### Binary right fold
`(... + pack + init)`


##### Separated unary left fold
`(pack + ... + sep)`
The above expressions evaluates like this:
`((((arg1 + sep) + arg2) + sep) + arg3)`
This can be useful if you wanted to create a CSV entry from a bunch of strings for example.

##### Separated unary right fold
`(pack + ... + sep)`


##### Separated binary left fold
`(init + pack + ... + sep)`

##### Separated binary right fold
`(sep + ... + pack + init)`



# Import System

`import` is the only keyword that is not recognized by the interpreter. Instead, it's a pre-processor directive:

in `../folder/file.pie`:
```pie
x = 1;
```

in `main.pie`
```pie
import ../folder/file;
import ../folder/file;

__builtin_print(x);
```

The resulting file:
```pie
x = 1;

__builtin_print(x);
```
Note that `.pie` is omitted in the `import` directive.

## Builtins

Since Pie doesn't provide any operators, how does one achieve ANYTHING at all with Pie?\
Pie reserves the names starting with `__builtin_`.

### Nullary Functions

- `__builtin_true`
- `__builtin_false`
- `__builtin_input_int`
- `__builtin_input_str`

### Unary Functions

- `__builtin_neg`
- `__builtin_not`
- `__builtin_mod`
- `__builtin_reset`
- `__builtin_eval`
- `__builtin_len` (for strings and packs)
- `__builtin_str_get`
- `__builtin_split`
- `__builtin_to_string`
- `__builtin_to_int`
- `__builtin_to_double`

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

### Quaternary Functions

- `__builtin_str_slice`

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
infix + = (a, b) => __builtin_add(a, b);

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

##### Modules
- `import`
- `space`
- `use`
<!-- - `space` -->

##### Operators
- `prefix`
- `infix`
- `suffix`
- `exfix`
- `mixfix`

##### Object Orient Stuff
- `class`
- `union`
- `match`

##### Control Flow
- `loop`


###### Phantom keywords
- `true`
- `false`

## Reserved Punctuaion

- `__builtin_*`
- `( )`
- `{ }`
- `,`
- `.`
- `=`
- `=>`
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

g++ -std=c++23 -Iincludes/mp11/include/ -Iincludes/cpp-std-extensions/include/ -O2 Pie/main.cc -o pielang
```

## Update

To update the language, paste this into the terminal:

```pie
cd Pie
git pull
cd ..

g++ -std=c++23 -Iincludes/mp11/include/ -Iincludes/cpp-std-extensions/include/ -O2 Pie/main.cc -o pielang
```

### Todo

#### in order of priority

- [ ] add `lex` and `value` namespaces to codebase
- [ ] Lexically Scoped Operators
- [ ] Remove preprocessor
- [ ] Allow variadics of Syntax type
- [ ] Add default values to function parameters
- [ ] Make `=` and `=>` overloadable
- [ ] File IO
- [ ] Fix builin reset (value-reset, reset/name-reset) 
- [ ] Use Big Int instead of `int32_t`
- [ ] World domination
- [ ] Improve error messages (add line and column numbers)
- [ ] Add recursive operators
- [ ] Remove depedency on stdx and boost
- [ ] Compile to WASM & a web interface
- [ ] Add LLVM backend

---

### Done
- [x] Add REPL
- [x] Changed error system to exceptions (will allow REPL)
- [x] Looping....maybee (added iterators!!!);
- [x] Fix arbitrary names typing issue
- [x] Same for loop^^
- [x] Casting ~~(maybe using `as`)~~(using builtins)
- [x] Implement separated fold expressions (like C++)
- [x] Add fold expressions (like C++)
- [x] Arbitrary function parameters
- [x] Add unions
- [x] Add `use` directive
- [x] Add an import system (modules)
- [x] Added global namespace access syntax
- [x] Add namespaces
- [x] Add match expression (like scala)
- [x] Add add overloaded operators at runtime (instead of parse-time)
- [x] Fixed infix operators parsing right to left!
- [x] Implemnted `__builtin_eq` for all values!
- [x] Add named parameters to some builtin functions
- [x] Add variadic arguments
- [x] Add named arguments
- [x] Change __builtin_{true|false} to true/false;
- [x] Fix operator return type checking..?
- [x] Add overloading operators
- [x] Change comments to `.::` and `::.`
- [x] Added `input_{str|int}`
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
- ~~Cascade operator `..`~~
