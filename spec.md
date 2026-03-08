# The Pie Programming Language Official Specification


## Chapter 1: Philosophy
Pie is an expression-only language with dynamic strong typing & user-defined operators.
At its core, Pie is just a collection of features deemed "cool" by its creator, Ali Almutawa Jr.




## Chapter 2: Lexical Grammer


### 2.1 Reserved Keywords
<!-- Before listing out the keyword, punctuation, and values lists, it's important to define what each of those words mean in Pie. -->

**Keywords** are modifiers, in the sense that they modify the next token (or tokens) to something of a <!-- potentially -->  different meaning than it would be otherwise. Keywords may **not** be assigned to.

Pie has 12 keywords:
- `class`
- `exfix`
- `import`
- `infix`
- `loop`
- `match`
- `mixfix`
- `prefix`
- `space`
- `suffix`
- `union`
- `use`

### 2.2 Reserved Punctuation

**Punctuation** are sigils and symbols (not including letters).
Punctuation may **not** be assigned to.


Pie reserves 12 punctuation:
- `(`
- `)`
- `{`
- `}`
- `,`
- `.`
- `..`
- `...`
- `=`
- `=>`
- `:`
- `;`


### 2.3 Literal Values
**Literal Values or Built-in Values** are values which need not any imports. They are readily available in the language. These values include numbers, strings, built-in types, and functions with names that start with `__builtin_`.
These values may be assigned to.

Literal Values are summerized in the following list:
<!-- since types are values, we'll add them here too! -->
- `Any`
- `Bool`
- `Double`
- `false`
- `Int`
- `String`
- `Syntax`
- `true`
- `Type`
- `__builtin_*`
- All String Literals
- All number literals


### 2.4 Comments
There 2 types of comments in Pie:
#### 2.4.1 Line Comments
Any text that is between `.:` and either a newline character `'\n'` or the end of file.

#### 2.4.2 Block Comments
Any text that is between `.::` and `::.`.



## Chapter 3: Program Structure
A Pie program consists of 0 or more expressions:
`program := expression*`



## Chapter 4: Expressions

### 4.1 Numbers
Any number, integer or double, is a valid expression in Pie.
Pie **must** implement big nums for its integer types

### 4.2 Booleans
Pie booleans have 2 literals: `true` and `false`.

### 4.3 Strings
Anything that starts and ends with quotes (`"`) is considered a string.


### 4.4 Identifiers
Identifiers are names that bind to values. What you know as "variables".
Identifiers can consist of any combonation of the following letters, numbers, symbols:
`abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&|*+~-_\'/<>[]`

#### 4.4.1 Proper Names
These are identifiers that contain a letter or one of the following symbols `!@#$%^&|*+~-_\'/<>[]` in them. These identifiers are special because they can be annotated with a type, unlike improper names.

#### 4.4.1 Imprroper Names
Any valid Pie expression is a valid identifier. This means that all the following are valid identifiers:
- `5`
- `"hello"`
- `func(x, y)`
- `1 + 2`

These identifiers are improper, which means they cannot be annotatetd with a type.


### 4.5 Assignments
When using the infix operator `=`, its LHS gets assigned to the value of the RHS.
`x = y` will set `x` to the value of `y`.

Assignments yield their new value.

#### 4.5.1 Order Of Evaluation
Assignments evaluate their RHS before evaluating the LHS.

This means chained assignments will work as expected:
`x = y = z` will assign both `x` and `y` to the value of `z`.

#### 4.5.2 Generality Of Assignments
Any expression may be assigned to any other expression, which means the following assignments are all valid:
- `1 + 2 = "hi"`
- `func(1, 2) = true`
- `3 = 2 + 3`
- `1 = 0`

#### 4.5.3 Type Annotations
Assignments can optionally have a type annotation.
Only proper names may be annotated:
- `x: Int = 1;`
- `y: String = "hi";`
- `true: Int = 5;`

The following programs are ill-formed:
- `1: Int = 5;`
- `1: String = "hi";`
- `1 + 2: Any = "hi";`
- `"bye": Any = true;`

#### Declarations vs Assignments
When assigning to a variable for the first time, the assignment also declares the variable as a new variable. Any subsequent assignments are will reassign to the existing variable.
*However*, if a subsequent assignment contained a type annotation, then that assignment will declare a new variable with a possible different type.

**Example:**
The following program is well-formed:
```pie
x = 1;
x = 2;
x = "hi";


a: Bool = truel
a = false;


var: Int = 5;
var: String = "five";
```

The following program is ill-formed:
```pie
var: Int = 5;
var = "five";
```

Bringing scopes into it, the following program is well-formed:
```pie
x = 5;
y = "hi";
{
    x = 10;
    y: Any = "bye";
};
.: x is 10 here
.: y is "hi" here
```


### 4.6 Blocks
Blocks allow you to run multiple expression-statements as a single expression. They evaluate the expressions in order before then yielding the last value in them.
```pie
a = {
    x = 1;
    func(true);
    "hi";
};
```
`a` is assigned to `"hi"`.

<!-- todo: add docu-link -->
See the scopes sections for the scoping rules of blocks.

**Note:** there is no such thing as an empty scope. The expression `{}` is an empty list.

### 4.7 Functions

Functions consist of 2 main part, the parameter list, and the body.
`(a, b) => 1;`
This is a function that takes 2 arguments, `a` and `b`, and returns 1.

The parameter list may optionally contain type annotations for any of the parameters. The return type is also optioinal (before the fat arrow):
```pie
(i: Int, s: String, b: Bool, a: Any): Int => 0;
```
If a parameter or the return type is left un-annotated, its type becomes `Any`.

#### 4.7.1 Named arguments
```pie
func = (a, b, c) => b;
result = func(5, c=10, a = 20);
```
`result` will be `5`.

Mixing between named arguments and positional arguments is allowed. If the name of a named-argument argument is not found in the parameter list, the program is ill-formed.

#### 4.7.2 Parameter Packs
A parameter pack is a parameter that swallows all the extra arguments in a function call.
To indicate that a parameter is a pack, it must be annotated with a type preceded by an ellipsis (`...`).
Packs are allowed to be empty!
```pie
reduce_to_one = (x: ...Any): Any => 1;

one      = reduce_to_one(1, "hi", true, {});
also_one = reduce_to_one();
```
At most, one parameter pack may be introduced per function.
The following program is ill-formed:
`(p1: ...Int, p2: ...String) => 0;`


However, other regular parameters are allowed:

```pie
getFirstNum  = (first: Int,      rest: ...Int) => first;
getFirstNum(1);       .: returns 1
getFirstNum(1, 2, 3); .: returns 1

getLastString = (pack: ...String, last: String) => last;
getLastString("hi");                 .: returns "hi"
getLastString("hi", "bye", "what!"); .: returns "what!"

someOtherFunc = (a, b, pack: ...Any, z) => z;
someOtherFunc(1, 2, 3);          .: returns 3
someOtherFunc(1, 2, 3, 4, 5, 6); .: returns 6
```

<!-- todo: add a docu-link -->
For more information about packs, check out the pack section in this document.

#### 4.7.3 Auto Currying
Functions in Pie will be curried if the number of arguments is less than the number of parameters.

```pie
add = (a, b) => __builtin_add(a, b);
add3 = add(3);

eight = add3(5);
```

Calling a function that expects arguments without any arguments will yield the same function back:
```pie
f1 = (a, b, c) => 1;
f2 = f1();
```
`f1` and `f2` are the same function.

\
If a parameter pack is present in the function's parameter list, then currying will only happen if the number of arguments is less than the number of non-pack parameters.
If the number of arguments is more than the index of the position of the pack in the parameter list, then the pack is consumed from the parameter list and is introduced as an empty pack inside the environment of the resulting function. Basically, as if an empty pack was passed as an implicit argument to the pack parameter.
Example:
```pie
func = (a, b, pack: ...Any, c, d) => 0;

f1 = func(1);
f2 = func(1, 2);
f3 = func(1, 2, 3);
f4 = func(1, 2, 3, 4);
f5 = func(1, 2, 3, 4, 5);
```
- `f1` is `(b, pack: ...Any, c, d) => 0`
- `f2` is `(pack: ...Any, c, d) => 0`
- `f3` is `(d) => 0`
- `f4` is `0`
- `f5` is `0`

#### 4.7.4 Function Types
Variables assigned to functions could be annotated with function types:
```pie
Int2Int: (Int): Int = (x: Int): Int => 1;

String2Int: (String): Int = (s: String): Int => 1;

getBool: (): Bool = (): Bool => true;
```

<!-- todo: add a docu-link -->
See the types section to see what conversions are allowed.


#### 4.7.5 Eveything Is A Function
Any variable is considered to be a nullary function which yields its own value:
```pie
x = 5;
a = x;
b = x();
```
Both `a` and `b` are `5`.


### 4.8 Packs
Packs can only be introduced as parameters to functions as shown in section [4.7.2 (Parameter Packs)](#472-parameter-packs).

#### 4.8.1 Expansions
Packs may be expanded into other function calls using trailing ellipsis (`...`).
```pie
makePack = (ps: ...Any) => ps;
func = (a, b, c) => c;

pack = makePack(1, 2, 3);

func(pack...);
```
The last line is the expansion. Parameters `a`, `b`, and `c` become `1`, `2`, and `3` respectively.

Note that if the ellipsis weren't present, then parameter `a` would become the pack containing `1, 2, 3`:
```pie
func(pack, true, 3.14);
```
`b` and `c` become `true` and `3.14` respectively.


#### 4.8.2 Fold Expressions
Fold expressions are expressions that operate on packs (see section [4.7.2 Parameter Packs](#472-parameter-packs)).

A fold expression is a binary operator that operates on a pack and ellipsis. The epxression must be surrounded by parenthesis.

There are 8 kinds of fold expressions, which are the cartesian product of these types of folds:
- Unary vs Binary Fold
- Left vs Right Fold
- Normal vs Separated Fold


##### 4.8.2.1 Unary Fold Expression:
Unary fold expressions do **not** have an initial value. This means one **cannot** fold over an empty pack.

- Left Folds:
    ```pie
    func = (pack: ...Any) => (pack + ...);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(((1 + 2) + 3) + 4)`.

- Right Folds:
    ```pie
    func = (pack: ...Any) => (... + pack);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(1 + (2 + (3 + 4)))`.

The following program is ill-formed because the pack is empty:
```pie
func = (pack: ...Any) => (pack + ...);
func();
```


##### 4.8.2.2 Binary Fold Expression
Binary fold expressions have an initial value, which goes at the opposite side of the ellipsis.

- Left Folds:
    ```pie
    init = 10;
    func = (pack: ...Any) => (init + pack + ...);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `((((10 + 1) + 2) + 3) + 4)`.

- Right Folds:
    ```pie
    init = 10;
    func = (pack: ...Any) => (... + pack + init);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(1 + (2 + (3 + (4 + 10))))`.

In the case of an empty pack, the expression yields the initial value:
```pie
inti = 10;
func = (pack: ...Any) => (init + pack + ...);
x = func();
```
`x` is `10`.



##### 4.8.2.3 Separated Fold Expressions
Separated fold expressions add the ability to add seperators between each operation. The separator goes on the opposite side of the pack.
Spearated fold expressions, too, have a left-right, unary-binary counterpart.

- Unary Left Folds:
    ```pie
    seperator = 5;
    func = (pack: ...Any) => (pack + ... + seperator);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(((((((1 + 5) + 2) + 5) + 3) + 5) + 4)`.

- Unary Right Folds:
    ```pie
    seperator = 5;
    func = (pack: ...Any) => (seperator + ... + pack);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(1 + (5 + (2 + (5 + (3 + (4 + 5))))))`.

Separated unary folds also don't support empty packs.

<!-- these seem wrong...check them again -->
- Binary Left Folds:
    ```pie
    init = 10;
    seperator = 5;
    func = (pack: ...Any) => (init + pack + ... + seperator);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `((((((((10 + 1) + 5) + 2) + 5) + 3) + 5) + 4)`.

- Unary Right Folds:
    ```pie
    init = 10;
    seperator = 5;
    func = (pack: ...Any) => (seperator + ... + pack + init);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(1 + (5 + (2 + (5 + (3 + (5 + (4 + (5 + 10))))))))`.

Just like normal binary fold expression, if the pack is empty, the expression yields the initial value


### 4.9 Classes
Classes are first class citizens in Pie:
```pie
Human = class {
    name: String = "";
    age = 0;

    setAge = (a: Int) => age = a;
};
```
The class must consist of assignment expressions only.

<!-- todo: add docu-link -->
See the scopes sections for the scoping rules of classes.

#### 4.9.1 Self
Data members that are assigned to functions, AKA methods, have access to a special variable with the name `self`. This variable is a reference to the object itself. Anything that can be done to an object, can also be done with a `self` reference.

#### 4.9.2 Constructors
Every class is provided with a constructor that initializes the data members in the order they appear in the class.

If a class member is initialized through the constructor, its initializer expression is never executed.

If no value is provided for a data members in the constructor call, its initializer value is used as a default.

```pie
C = class {
    a = 0;
    b: String = "";
    c: bool = false;
};

object1 = C();
object2 = C(10, "hi");
object3 = C(10, "hi", true);
```
- In `object1`, `a` is `0`, `b` is `""`, and `c` is `false`
- In `object2`, `a` is `10`, `b` is `"hi"`, and `c` is `false`
- In `object3`, `a` is `10`, `b` is `"hi"`, and `c` is `true`


### 4.10 Collections

#### 4.10.1 Lists
Lists in Pie are a comma-separated-expressions wrapped with openning and closing curly braces.

- `{}` is an empty list
- `{1}` is a list containing the integer `1`
- `{true, 5, "hi"}` is a list with 3 elements


#### 4.19.2 Maps
Key-value pairs where the key and the value are separated by a colon (`:`) and the pairs are separated by commas (`,`).
- `{:}` is an empty map
- `{"one": 1}` map with key `"one"` mapped to value `1`
- `{1: 2, true: "yes"}` map containing 2 pairs








<!-- no immutablity -->