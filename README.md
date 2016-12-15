sormod
====

**sormod** is an interpreted stack-based language.  It is comprised of a byte-code interpreter written in C++ as well as a one-to-one 'compiler' written in Racket.  The language isn't much more than an exercise for C++ (since most of my last language-based projects were written in C) and shouldn't be used for any serious work.

sormod borrows the idea of quotes from Joy and Factor.  It also has a 'mode' system, where different modes dictate how the program behaves when passed an item.  For example, there are two modes currently in the language (at some point it should be possible for the user to define new modes): `read-mode` and `quote-mode`.  When passed an item, a program in `read-mode` will 'call' that item, where if a literal symbol is called, like `6`, `symbol`, or `"hello"`, it will simply be pushed, while if it is a function, that function will be called on the stack.  In `quote-mode`, the stack will continuously push everything until a well-formed quote is made (e.g. `[ a b ]`, `[ a [ b ] ]`).

Defining a function in sormod is fairly simple:

```
[ 1 + print-int ] acc-print :d
```

Here, the function `acc-print` is defined.  Since the language supports quotes, there is no special syntax needed for defining functions, rather all that is needed is a define function: `:d`.  Note that `acc-print` is a symbol.  Until bound, a symbol remains a symbol.  If bound, it will become a function.  This is important, as in `read-mode`, this means that `acc-print` will be pushed.  However, subsequent references to `acc-print` will result in calling that function.

Quotes can be used like lambdas are used in other languages:

```
[ 1 2 3 ] [ 1 + ] map
```
**NOTE**: map not yet implemented.

This maps an accumulator function to the list `[ 1 2 3 ]` (lists and quotes are technically the same in sormod).

All functions available in every run of sormod are documented (though not well) in `sormod.cpp`.

Again, this was just a quick project with C++.  I may use a variation of this for the next project (or continue this one).
