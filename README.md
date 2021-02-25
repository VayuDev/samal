# samal - Simple And Memory-wasting Awesomely-functional Language

This repository contains the my programming language samal which attempts to be the 
perfect combination of Rust and Elixir. It mixes the great type system and syntax of Rust with
the functional style of Elixir and Haskell.

The language is currently in heavy development and not usable.

## Features

Features already implemented include:
 * JIT-Compiler for most instructions on x86-64
 * Lambdas
 * Templates
 * Lists＊
 * Explicit tail recursion optimization
 * Mark-and-sweep garbage collector
 * Structs
 * Native functions
 * Easy API for embedding samal into your project

＊ Lists will change with the introduction of generators.

## Planned Features

Planned features (vaguely order):
 * Enums
 * Strings/Chars (Strings will be implemented as lists of chars, similar to Haskell)
 * Generators (similar to Python)
 * Using declarations (to avoid typing the module name every time)
 * Type casting (probably using native methods)
 * A proper cli
 * Floating point support
 * Byte support
 * Some logical operations like not
 * Bitwise arithmetic

## Examples

Fibonacci:

    fn fib(n : i32) -> i32 {
        if n < 2 {
            n
        } else {
            fib(n - 1) + fib(n - 2)
        }
    }

Sum up all elements of a list, making use of explicit tail recursion:

    fn sumRec<T>(current : T, list : [T]) -> T {
        if list == [] {
            current
        } else {
            @tail_call_self(current + list:head, list:tail)
        }
    }
    fn sum<T>(list : [T]) -> T {
        sumRec<T>(0, list)
    }


Iterate over a list, calling the specified callback on each element and returning a list of the return values:

    fn map<T, S>(l : [T], callback : fn(T) -> S) -> [S] {
        if l == [] {
            [:S]
        } else {
            callback(l:head) + map<T, S>(l:tail, callback)
        }
    }


Combine two lists, creating one list where each element is a tuple with one element from each list:

    fn zip<S, T>(l1 : [S], l2 : [T]) -> [(S, T)] {
        if l1 == [] || l2 == [] {
            [:(S, T)]
        } else {
            (l1:head, l2:head) + zip<S, T>(l1:tail, l2:tail)
        }
    }

Note: List syntax will probably change with the introduction of generators.

## Design notes

## Part 1: Why every programming language sucks

Please take all of this with a grain of salt, it's just my personal opinion, so feel free to disagree.

I really love programming in functional languages like Haskell or Elixir, but I really 
dislike the type systems of both languages. Pattern matching is great, but Elixir is dynamically typed 
which makes large projects quite difficult as I had to experience myself. Especially if you have a 
GenServer which stores a lot of state, remembering the name of every key etc. becomes increasingly taxing. 
Autocompletion just doesn't work most of the time. While Haskell is strongly typed, types are implicit 
almost always which makes code sometimes hard to read and makes error messages hard to comprehend.

I like Rust a lot as well, but it's just not functional enough for me and seems overkill for most jobs. It's after
all a systems programming language and most of the time you don't need one. Rust (and C++) force you to always
think about memory management and ownership; but if I want to write a Compiler, I don't really care if a garbage 
collector runs sometimes or not and rather take the speedup in development time.

Most garbage-collected languages have other problems: they are either not strongly typed, making large projects
impossible, or have other weird quirks like forcing OOP onto you like Java. Go might seem like it ticks a lot
of the boxes, but it's lack of generics, weird interface type and lack of VM and being neither fully functional 
or object oriented make it hard for me to enjoy using.

## Part 2: What makes samal different

So, where does this leave us? We want a language that:

 * is functional.
 * has a great type system.
 * is fast enough for must stuff.
 * achieves the previous three points without becoming bloated and complicated.

This is pretty much all I want to achieve with samal. I don't care about memory consumption (as long as it's
not well beyond bounds) and I don't care about compilation speed (as long as it stays faster than C++ compiled with
-O3).

## Part 3: Why samal sucks as well

So, what's bad about samal right now?

 * It uses a custom-written peg parser with horrendous error messages - if you have a parsing error, the fastest 
   thing will probably be to just try to find it yourself using the line information that it gives you (which is only
   sometimes correct).
 * As we don't have generators yet, the GC pressure is just insane, which means that the most time is actually
   spend in the GC itself. Once we have generators, we can skip allocating temporary lists and instead create them
   on the fly when they're needed.
 * Calling native functions is slow.
 * There are unit tests, but they don't cover all features.
 * The Compiler class is bigger and messier than I want it to be.
 * We're still missing a lot of basic features.

But with your help, I'm sure we can fix all of these problems! Feel free to open a pull request if you want to
improve the language, in any way, shape or form. Just ask yourself: What does the perfect language look like for me?
