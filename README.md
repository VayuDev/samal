# samal - Simple And Memory-wasting Awesomely-functional Language

## The development has moved to [https://github.com/VayuDev/samal-haxe](https://github.com/VayuDev/samal-haxe). The new version is implemented in Haxe and compiles to C++ for greater portability, performance & maintainability.

Samal is a new programming language which attempts to be the perfect general-purpose (embeddable) language.
It mixes the great type system and syntax of Rust with the functional style of Elixir and Haskell.

Further information can be found at [samal.dev](https://samal.dev).

The language is currently in heavy development and not usable.

## Features

Features already implemented include:
* JIT-Compiler for most instructions on x86-64
* Lambdas
* Match expressions
* Templates
  * Automatic template type inference for function calls
* Easy API for embedding samal into your project
* Structs
* Enums
* Pointer type for recursive structs/enums (all structs/enums are on the stack)
* Strings/Chars (Strings are implemented as lists of chars, similar to Haskell)
  * Unicode (UTF-32) support; a single char can contain any unicode codepoint
* Explicit tail recursion optimization
* Copying garbage collector
* Native functions
* Typedefs (type shorthands like 'typedef String = [char]') 
* Type casting (using native functions)
* Using declarations (to avoid typing the module name every time)
* Very basic standard library

## Planned Features

Planned features (vaguely order):

* A proper cli
* Floating point support
* More features & error checking in match statements
* Tuple deconstruction in assignment

## Examples

Fibonacci:
```rust
fn fib(n : i32) -> i32 {
    if n < 2 {
        n
    } else {
        fib(n - 1) + fib(n - 2)
    }
}
```

Sum up all fields of a list, making use of explicit tail recursion:

```rust
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
```

Iterate over a list, calling the specified callback on each element and returning a list of the return values:

```rust
fn map<T, S>(l : [T], callback : fn(T) -> S) -> [S] {
    if l == [] {
        [:S]
    } else {
        callback(l:head) + map<T, S>(l:tail, callback)
    }
}
```


Combine two lists, creating one list where each element is a tuple with one element from each list:

```rust
fn zip<S, T>(l1 : [S], l2 : [T]) -> [(S, T)] {
    if l1 == [] || l2 == [] {
        [:(S, T)]
    } else {
        (l1:head, l2:head) + zip<S, T>(l1:tail, l2:tail)
    }
}
```

Parse a http header (definitely not spec compliant, skips method+version+url line):

```rust
fn parseHTTPHeader(socket : i32) -> [([char], [char])] {
    str = readUntilEmptyHeader(socket, "", "")
    lines = Core.splitByChar(str, '\n')
    
    headers =
        lines:tail
        |> Core.map(fn(line : [char]) -> Maybe<([char], [char])> {
            splitList =
                Core.takeWhile(line, fn(ch : char) -> bool {
                    ch != '\r'
                })
                |> Core.takeWhileEx(fn(ch : char) -> bool {
                    ch != ':'
                })
            if splitList:1 == "" {
                Maybe<([char], [char])>::None{}
            } else {
                Maybe<([char], [char])>::Some{(splitList:0, splitList:1:tail:tail)}
            }
        })
        |> Core.filter(fn(line : Maybe<([char], [char])>) -> bool {
            match line {
                Some{m} -> true,
                None{} -> false
            }
        })
        |> Core.map(fn(e : Maybe<([char], [char])>) -> ([char], [char]) {
            match e {
                Some{n} -> n,
                None{} -> ("", "")
            }
        })

    Core.map(headers, fn(line : ([char], [char])) -> () {
        Core.print(line)
    })
    headers
}
```

Note: List syntax will maybe change in the future.

## License

The software is licensed under the very permissive MIT License. If you open a pull request, you implicitly 
license your code under the MIT license as well.

## Platforms

The only fully supported platform is Linux, but samal seems to work on FreeBSD as well.
I haven't tested the language on Windows yet, though everything except the JIT-Compiler
should work. The JIT-Compiler won't work because Windows uses different calling conventions.
To enable the JIT-Compiler, pass -DSAMAL_ENABLE_JIT=ON to cmake.

## Build

samal uses cmake as a build system. If you want to use it as a library, the process should be intuitive and the
same as with any other cmake library. If you want to build the CLI, it should be even easier. Make sure to adjust your
working directory so that it finds all required files.

The basic process to build & run the CLI should be:
* mkdir cmake-build-release
* cd cmake-build-release
* cmake .. -DCMAKE_BUILD_TYPE=Release -DSAMAL_ENABLE_JIT=ON
* make -j8
* cd ..
* cmake-build-release/samal_cli/samal

The following build options exist:
* -DSAMAL_ENABLE_JIT=ON to enable the jit compiler
* -DSAMAL_ALIGNED_ACCESS=ON to force aligned access of data. This may be required for some ARM-CPUs and can increase
  performance in general for all systems, but increases memory usage by up to 16 times.
* -DSAMAL_ENABLE_GFX_CAIRO=ON to enable use of cairomm in the standard library. Make sure to have pkg-config and cairomm
  installed. This is required for the webserver to work, though you should be able to delete the rendering code to make
  it work without cairo.

## Design notes

### Part 1: Why every programming language sucks

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
of the boxes, but it's lack of generics, weird interface type, lack of VM and being neither fully functional
or object oriented makes it hard for me to enjoy using.

### Part 2: What makes samal different

So, where does this leave us? We want a language that:

* is functional
* has a great type system
* is fast enough for must stuff
* achieves the previous three points without becoming bloated and complicated

This is pretty much all I want to achieve with samal. I don't care about memory consumption (as long as it's
not well beyond bounds) and I don't care about compilation speed (as long as it stays faster than C++ compiled with
-O3).

### Part 3: Why samal sucks as well

So, what's bad about samal right now?

* It uses a custom-written peg parser with horrendous error messages - if you have a parsing error, the fastest
  thing will probably be to just try to find it yourself using the line information that it gives you (which is only
  sometimes correct)
* Calling native functions is slow
* There are unit tests, but they don't cover all features
* The Compiler class is bigger and messier than I want it to be
* We're still missing a lot of basic features

But with your help, I'm sure we can fix all of these problems! Feel free to open a pull request if you want to
improve the language, in any way, shape or form. Just ask yourself: What does the perfect language look like for me?
