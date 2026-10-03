#### Note: please use [cmark-gfm](https://github.com/github/cmark-gfm) to transform this document into HTML.  Many markdown processors do not correctly handle embedded code blocks.

## Overview

`cdecl` is a C parser written in C.  The code interprets pre-processed
declarations and removes initializations, which are arguably part of the runtime
rather than declarations.  Nearly all of the C11 standard is implemented
excepting Unicode support, which would require a replacement of all
string-mangling functions with their wide-character alternatives.

The output of `cdecl` is English sentences which are generated from an
array-backed stack of tokens at program exit.  It should be straightforward to
modify the code to output an actual AST since the program implicitly assembles
the data from which one is constituted.

## How to run the program

```console
$ make cdecl

$ ./cdecl "static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old, unsigned long new, int size);"
__cmpxchg is a(n) inline function which returns unsigned long and takes param(s) ptr is a(n) pointer to volatile void and old is a(n) unsigned long and new is a(n) unsigned long and size is a(n) int and which has static storage duration and internal linkage
```
Input may alternatively be piped to cdecl's stdin when a '-' is provided as an argument:

```console
$ echo "uint64_t hash(const char *seed, uint64_t key);" | ./cdecl -
hash is a(n) function which returns uint64_t and takes param(s) seed is a(n) pointer to const char and key is a(n) uint64_t
```

## Internals

The stack is constructed in parallel with the creation of a parser context in `struct parser_props`.  The parser context retains the state variables which are determined as parsing proceeds.

```c
struct parser_props {
  /* These latching bools keep track of which elements the parser has
   * encountered.  The output stage makes use of them.
   */
  bool have_type;
  bool have_qualifier;
  /* These bools describe the high-level identity of the parsed object. */
  bool is_function;
  bool is_enum;
  bool is_struct_or_union;
  bool is_pointer;
  bool is_function_ptr;
  bool is_typedef;
  bool is_declarator_list;
  bool is_inline;
  bool is_comment;
  /* Enumeration, function and struct objects contain subsidiary objects. */
  bool has_enum_constants;
  bool has_function_params;
  bool has_struct_or_union_members;
  char enumerator_list[MAXTOKENLEN];
  size_t num_identifiers;
  /* These parameters describe the internal parser state. */
  size_t cursor;
  size_t stacklen;
  char start_delim;
  char end_delim;
  char separator;
  struct token stack[MAXTOKENS];
  struct identifier_props ident;
  struct parser_props *prev;
  struct parser_props *next;
  struct parser_props *parent;
  /* The I/O streams are settable for the convenience of the tests. */
  FILE *out_stream;
  FILE *err_stream;
};
```

The `cdecl-debug` binary shows the accumulation of items in the stack as parsing proceeds:

```console
$ make cdecl-debug

$ ./cdecl-debug "uint32_t a, b[];"
Stack at 2573 is:
Token number 0 has kind type and string uint32_t
Stack at 2573 is:
Token number 0 has kind type and string uint32_t
Token number 1 has kind identifier and string a
Stack at 2949 is:
Token number 0 has kind type and string uint32_t
Token number 1 has kind identifier and string a
Token number 2 has kind identifier and string b
Stack at 2986 is:
Token number 0 has kind type and string uint32_t
Token number 1 has kind identifier and string a
Token number 2 has kind identifier and string b
b is a(n) array of and a is a(n) uint32_t
```

## Functions, structs and unions

In order to support compound objects like functions, structs and unions, the parser spawns subparsers whose order is maintained in a singly linked list. Parsers are spawned every time the logic encounters a new function parameter or struct/union member.  Subparsers are necessary because the state variables which apply to the top-level struct, union or function may not apply to function parameters or struct/union members.  For example, one function parameter may be const char*, while another may be an enum.  The output section of the program walks the parser list and frees the elements as their output is printed.  The progressive linking of new parsers into the list is made visible via the cdecl-debug binary.

```console
$ make cdecl-debug
$ ./cdecl-debug "static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old, unsigned long new, int size);" | grep HEAD
HEAD at 1419: 0x7bb077cf00d0-->0x7e80787e0400
HEAD at 1419: 0x7bb077cf00d0-->0x7e80787e0400-->0x7e80787ea400
HEAD at 1419: 0x7bb077cf00d0-->0x7e80787e0400-->0x7e80787ea400-->0x7e80787f4400
HEAD at 1437: 0x7bb077cf00d0-->0x7e80787e0400-->0x7e80787ea400-->0x7e80787f4400-->0x7e80787fe400
```

Here there are 5 parsers, the top-level one for `__cmpxchg` and one subparser for each of the 4 function parameters.

## Tight-binding of array, bitfield and pointer properties

Arrays and bitfields inside comma-separated declarator lists also require special handling.  Spawning a list of parsers makes no sense for declarator lists, as the elements in the list are all at top level.  Thus all objects are const, or none are.  However, whether each element in a list describes a pointer, array or bitfield varies on a per-identifier basis.  The state variables which describe these properties must therefore bind to each of the identifiers individually, not to the overall parsing context.  The authors and maintainers of the C language made this tight-binding clear via the syntax, in that square brackets, asterisk and colon are by convention adjacent to the identifier name, or at least are separated from the next identifier by a comma. The result is

```console
$ ./cdecl "int a[3][], *b, c : 8;" 
c is a(n) bitfield of width 8 and b is a(n) pointer to  and a is a(n) array of 3x? int 

$ ./cdecl-debug "int a[3][], *b, c : 8;" | grep Identifier
Identifier 0:  has dimensions 2 and lengths 1
Identifier 1:  has no array dimensions or bitfield
Identifier 2:  bitfield of width 8
```

### Structs of arrays and arrays of structs

The identifier properties are stored in arrays which are members of `struct identifier_props`.  Each element in these arrays corresponds to an identifier on the stack.  The names of the identifiers are not stored in the struct, but the identifiers on the stack are kept in one-to-one correspondence with the struct-member array elements.

```c
struct identifier_props {
  size_t array_dimensions[MAXIDENTIFIERS];
  size_t array_lengths[MAXIDENTIFIERS];
  enum specifier_state last_dimension[MAXIDENTIFIERS];
  bool is_bitfield[MAXIDENTIFIERS];
  size_t bitfield_width[MAXIDENTIFIERS];
};
```

Note that whether an identifier is a pointer is not represented in the `identifier_props` struct.  Since pointerness is completely described by a bool and has no associated data, the easiest way to represent it was to put '*' on the stack.  Bitfields and arrays, on the other hand, have more associated data.

One might naively expect instead an array of structs parser_props

```c
struct identifier_props iprops[MAXIDENTIFERS] {};
```

That kind of data structure exemplifies the [array-of-structs antipattern](https://en.wikipedia.org/wiki/AoS_and_SoA) which maximizes thrashing of cache lines.  While efficiency and SIMD friendliness are hardly critical for this toy program, the code is written in the more performant way.   Thanks to Glenn for explaining these concepts.

### Unit tests

`cdecl` has over 400 unit tests based on the [googletest](https://github.com/google/googletest) framework.  As noted in the [README](https://github.com/chaiken/C-Exercises/blob/master/README) file, compilation proceeds via a hack copied from [Mike Long](https://github.com/meekrosoft).   Because of the baroque manner in which the tests are compiled, I've not been able to get gcov to work with them.  [Valgrind](https://valgrind.org) works fine with the binary:

```console
$ valgrind ./cdecl-valgrind "static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old, unsigned long new, int size);"
==447977== Memcheck, a memory error detector
==447977== Copyright (C) 2002-2026, and GNU GPL'd, by Julian Seward et al.
==447977== Using Valgrind-3.27.1 and LibVEX; rerun with -h for copyright info
==447977== Command: ./cdecl-valgrind static\ inline\ unsigned\ long\ __cmpxchg(volatile\ void\ *ptr,\ unsigned\ long\ old,\ unsigned\ long\ new,\ int\ size);
==447977== 
__cmpxchg is a(n) inline function which returns unsigned long and takes param(s) ptr is a(n) pointer to volatile void and old is a(n) unsigned long and new is a(n) unsigned long and size is a(n) int and which has static storage duration and internal linkage

==447977== 
==447977== HEAP SUMMARY:
==447977==     in use at exit: 0 bytes in 0 blocks
==447977==   total heap usage: 108 allocs, 108 frees, 141,669 bytes allocated
==447977== 
==447977== All heap blocks were freed -- no leaks are possible
==447977== 
==447977== For lists of detected and suppressed errors, rerun with: -s
==447977== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

The tests are compiled with [ASAN and UBSAN](https://github.com/google/sanitizers).  [clangtidy](https://clang.llvm.org/extra/clang-tidy/) reports one use-after-free bug in the handled_struct_or_union_mebers() function which traverses the parser list in the output stage.   Exactly the same code occurs in handled_function_params() where it is not flagged.  (TODO: factor this common code out.)   While clang-tidy finds many actual bugs, it is (unlike valgrind) not aware of the [`__cleanup__` attribute](https://accu.org/journals/overload/34/192/chaiken/), which both GCC and clang have supported since 2006.  The
clang-tidy output is therefore polluted by many false-positive memory-leak reports.
