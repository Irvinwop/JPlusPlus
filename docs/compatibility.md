# Compatibility

J++ is an experimental source transpiler for competitive programming. **It does not yet support all GNU++23 features or all compiler intrinsics.** Clang 18 determines which input syntax is accepted; Java lowering is a separate, smaller compatibility boundary.

## Current focus

| Area | Implemented paths |
| :-- | :-- |
| Frontend | GNU++23, preprocessing, local includes, `bits/stdc++.h`, aliases, resolved overloads, concrete function template instantiations |
| Control flow | Functions, recursion, blocks, `if`, `if constexpr`, runtime branches of `if consteval`, loops, integer switches including 64/128-bit conditions and GNU case ranges, break, continue, return, forward goto exits to enclosing blocks |
| Expressions | Chained assignments, boolean arithmetic and numeric conversions, arithmetic compound assignments, void comma/conditional expressions, discarded values in statements and loop updates, GNU omitted-middle conditionals and statement expressions with local control flow; RHS-before-destination sequencing and one destination evaluation for supported lvalues |
| Types | Boolean, character, signed integer and floating primitives; common unsigned 32/64-bit operations, widening and floating conversions; enums; signed `__int128` arithmetic via `BigInteger`; typed pointers into supported arrays, objects, and reference cells |
| Arrays | Fixed bounds, GNU variable length arrays with captured dimensions and nested `sizeof`, multidimensional primitive arrays, partial initialization, string-initialized character arrays; primitive array references and value copies; `std::array` via `ArrayList`, including nested/partial/empty aggregate initialization, fill, memberwise moves and assignment retaining inline element aliases; nested built-in array elements and zero-length arrays of immovable elements |
| Containers | `vector` via `ArrayList`, with supported allocation transfers across swaps and moves retaining outer-container aliases and element references, pointers, and iterators; elementwise swapping of built-in and standard arrays; indexed `deque` via a circular `ArrayList` store, end operations, copies, swapping, supported references and iterators across end removals, range construction and erasure; value copies on supported insertion paths; converting pairs and range loops; selected emplacement; live map entries and iterators, bounds, iterator/range erasure, mapped-value references, map node identity across merging and swapping, and range/initializer-list construction; selected set range and initializer-list constructors; common map, set, multiset, queue, stack, and priority-queue operations |
| Tuples and pairs | Typed tuple classes with named fields; empty, nested, and converting tuples, value copies, reference elements, sequential assignments, `get` by index or type, `make_tuple`, `make_pair`, `tie`, `ignore`, `forward_as_tuple`, `tuple_cat`, `apply`, and `make_from_tuple` for supported element and callable types; element swaps with overload resolution and selected user copy/move effects; `reference_wrapper`, including mutable wrapper-object references, rebinding and supported callable wrappers |
| Ordering | Typed default comparisons for sorting, ordered containers, heaps, extrema, and bounds searches; unsigned and pair/tuple/vector lexicographic ordering; selected arithmetic and tuple three-way comparisons, strong/weak/partial comparison categories, unordered floating results, and comparison-category predicates; selected typed/transparent and user comparators; explicit heap comparators and range/container construction; heterogeneous lower/upper bounds and binary search |
| Algorithms | Sort, stable sort, reverse, fill, bounds, binary search, unique/erase, find/count, extrema, accumulate, iota, next permutation, copy/copy_n/copy_backward, merge, for_each, back_inserter |
| Objects | Global and local structs, aggregate and default-member initialization, fields, constructors and methods, scalar static data members with constant initialization, static methods, selected copy/move constructors and assignment operators, defaulted memberwise moves for supported field types, supported value copies and assignment retaining object identity |
| Scoped destruction | Automatic local records with a defined user destructor and trivially destructed fields; reverse cleanup on block exit, return, break, continue, and supported forward goto; supported direct and conditional elision |
| Pointers | Address-taking, dereference, scalar and array allocation, arithmetic and differences within an allocation, pointer parameters and aliases, pointer reference parameters, member access, selected void-pointer conversions, vector data and string-buffer pointers |
| Byte memory | Scalar and multidimensional primitive storage views with Clang-derived widths and target byte order; flat standard-layout, trivially copyable records with scalar fields, including packed records; pointer casts retain the allocation origin and stride; constant `offsetof`, partial `memcpy`/`memset`, overlapping `memmove`, `memcmp`, `memchr`, corresponding GNU builtins, and supported `malloc`/`calloc`/`realloc`/`free`; aliases remain attached to Java arrays, lists, records, fields, reference cells, and string buffers |
| GNU frontend | With the [compatibility frontend](../toolchain/README.md): byte offsets and increments on supported primitive `void*` views, `sizeof(void)`, and GNU alignment queries; compact constant evaluation of repeated trivial record elements while preserving the original array bound |
| Functions | Instantiated function templates, `std::function` with selected callable argument/return conversions, function pointers, typed lambdas with reference and value captures, generic lambdas with one concrete instantiated signature, scalar and primitive-array reference parameters, aliases and results, default arguments on direct user calls and constructors, reference range loops, translation units without main |
| Static storage | Lazy local statics with storage per template instantiation, static references and arrays, local extern declarations, owned static/global file-stream closing on normal return and `std::exit`, immediate termination with `std::_Exit` |
| Structured bindings | Pairs, tuples, built-in arrays, `std::array`, and user structs; reference bindings, range-loop bindings, and addresses retaining their element or member identity |
| Bitsets | `std::bitset` via Java `BitSet`, numeric and string construction, proxy references, value copies, assignment and swap, bitwise operations, fixed-width shifts, counts and predicates, string/integer conversion, GNU `_Find_first`/`_Find_next`, and unordered-map keys |
| Random engines | `mt19937`, `mt19937_64`, and selected Mersenne Twister template configurations up to 64 bits; scalar and `std::seed_seq` seeds, reseeding, discard, independent copies, assignment, equality, and output bounds; seed-sequence initializer/range construction, generation, and parameter output |
| Time | `system_clock` and `steady_clock` readings; selected signed 64-bit durations and time points, period conversions, arithmetic and comparisons, floor/ceil/ties-to-even rounding, duration literals and default text output, `time_t` conversion, and `sleep_for` |
| Narrow strings | Byte-preserving literals, input and output, embedded NULs, high-bit bytes, pointer/count and iterator-range construction, initializer lists, append and assign overloads, reserve/resize, unsigned substring bounds and bounded find |
| I/O | Buffered `cin`/`cout` and error/log streams, stream ties and receiver evaluation in chained stream-setting calls, `unitbuf`/`nounitbuf`, file input/output streams with lexical closing, default and append file output, standard-stream `freopen` redirection, `fread` from standard input and `fwrite` to standard output/error over supported primitive byte views, strings and C character arrays, basic stream formatting, `getline`, `ignore`, selected `scanf`/`printf` conversions including widths, precision, suppression, scansets, and character counts |
| Intrinsics | GNU popcount, parity, clz, ctz, byte swaps, expect, trap/unreachable; selected C++ bit operations including bit_ceil, bit_floor, bit_width, has_single_bit; scalar float/integer bit_cast |
| Numeric library | Scalar numeric limits, type traits and mathematical constants; scalar constexpr/constinit initialization and consteval calls; selected math functions, C++ rounding, classification and sign tests, exponent scaling, copy-sign, decimal/scientific/hexadecimal floating output with ties-to-even formatting |

Implemented paths have limits. A container appearing in this table does not imply coverage of every constructor, member, iterator invalidation case, allocator, or custom comparator. Use `--check` when preparing a submission and compare program behavior on relevant inputs.

## Known gaps

- Byte views of nested records, record fields containing arrays, pointers or references, unions, bitfields, and nontrivially copyable records; runtime `offsetof` indices, pointer/integer conversions, arbitrary object reinterpretation, placement construction, and complete storage layout semantics. Allocation sizes and byte-view indices are bounded by Java's indexed storage limits. Noncanonical bool bytes and floating representations that a JVM changes when materializing a value are explicitly rejected.
- Backward and computed jumps, jumps into nested scopes, and full unstructured control flow.
- Return/goto and break/continue escaping a GNU statement expression; consteval results requiring object snapshots; dynamic initialization of static data members.
- Full object lifetimes and RAII: destruction of members, arrays, containers, value parameters/results, captured objects, other full-expression temporaries, static/global objects, heap objects, and explicit destructor calls. Scoped user destructors currently require a direct block-local declaration and trivially destructed fields. Reference lifetime extension and GNU cleanup attributes need additional lowering.
- Inheritance, unions, bitfields, RTTI, and exceptions. Deallocation relies on Java garbage collection for supported objects without user destructors.
- Generic lambdas with multiple concrete call signatures, every capture mode, all reference-returning expressions, and full iterator/reference invalidation semantics. Set node-value merging does not yet preserve iterator identity across containers.
- Complete comparator propagation on container assignment, all heterogeneous comparator overloads, and independent mutable callable state when copying `std::function`.
- All constructor overloads after Java type erasure, all emplacement/default-argument and reference-member cases, and complete user-defined move semantics.
- Constant/zero initialization preceding all dynamic global initialization, thread-local storage, and static initialization that observes partially constructed records.
- Coroutines, modules, concurrency, atomics, platform APIs, inline assembly, and target-specific SIMD intrinsics.
- GNU policy based data structures, arbitrary third-party libraries, full ranges/views, and complete C++23 library coverage.
- Custom seed-sequence types, other random-engine families and adaptors, random distributions, engine stream serialization, and complete random-library coverage.
- Floating, unsigned, narrow, and user-defined chrono representations; calendar/time-zone facilities, time-point text formatting, clock serialization, and complete chrono/thread-library coverage. Clock resolution and scheduling follow the selected JVM and operating system.
- Direct formatted input into bitsets and complete bitset constexpr object evaluation. Bitset exception paths need the general C++ exception backend.
- Exact `long double` behavior, floating environment differences, locale-sensitive I/O, and complete C `printf`/`scanf` compatibility.
- Wide-character strings and streams, complete locale semantics, and Unix filenames that are not valid UTF-8. Narrow string sizes are limited by Java's 32-bit indexing; string exception paths need the general C++ exception backend.
- C file handles and their error indicators, complete file open-mode combinations, seeking, mixed read/write streams, and file-stream exception masks.
- Complete synchronized C/C++ I/O interleaving and terminal-specific buffering behavior. The stream synchronization flag follows the GNU library's one-way disabling behavior; it does not create native C stream buffers.
- Exact unsigned `__int128` wraparound and all edge cases of C++ integer promotion and conversion rules.

Java's stack depth, boxing and collection memory overhead, and `BigInteger` costs differ from native C++. Passing a differential test is evidence for that tested behavior, not a full conformance claim or a time-limit guarantee.

## Diagnostics

Most unsupported constructs produce an explicit `j++` error, for example:

```text
solution.cpp:12:5: error: j++: unsupported statement GCCAsmStmt
```

Some unsupported combinations are currently caught only by Java compiler validation. This is why `--check` is available and why new lowering paths need execution tests, not just source snapshots.
