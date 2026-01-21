# The Pie Programming Language Specification


## Chapter 1: Lexical Grammer


#### Reserved Keywords, Punctuation, and Built-in Values 
Before listing out the keyword, punctuation, and values lists, it's important to define what each of those words mean in Pie.

**Keyword:** These are modifiers, in the sense that they modify the next token (or tokens) to something of a potentially different meaning that it would be otherwise. They may not be assigned to.

**Punctuation:** Sigils and symbols (not including letters). They also may not be assigned to.

**Built-in Value:** Values which need not any imports. They are readily available in the language. These values include numbers, strings, built-in types, and functions with names that start with `__builtin_`. These values may be assigned to.

##### Keywords List
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

##### Punctuation List
- `(`
- `)`
- `{`
- `}`
- `,`
- `.`
<!-- - `..` -->
- `...`
- `=`
- `=>`
- `:`
- `;`

##### Value List
<!-- since types are values, we'll add them here too! -->
- `__builtin_*`
- `Any`
- `Bool`
- `Double`
- `false`
- `Int`
- `String`
- `Syntax`
- `true`
- `Type`


#### Comments
There 2 types of comments in Pie:
##### Line Comments
Any text that is between `.:` and either a newline character `'\n'` or the end of file.

##### Block Comments
Any text that is between `.::` and `::.`.


