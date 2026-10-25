# Compatibility

J++ is an experimental source transpiler for competitive programming. **It does not yet support all GNU++23 features or all compiler intrinsics.** Clang 18 determines which input syntax is accepted; Java lowering is a separate, smaller compatibility boundary.

## Current focus

| Area | Implemented paths |
| :-- | :-- |
| Frontend | GNU++23, preprocessing, local includes, `bits/stdc++.h`, aliases, resolved overloads, concrete function template instantiations |
| Control flow | Functions, recursion, blocks, `if`, `if constexpr`, runtime branches of `if consteval`, loops, switch, break, continue, return |
| Types | Boolean, character, signed integer and floating primitives; common unsigned 32/64-bit operations; enums; signed `__int128` arithmetic via `BigInteger` |
| Arrays | Fixed bounds, GNU variable length arrays, multidimensional primitive arrays, partial initialization |
| Containers | `vector` via `ArrayList`; pairs; common map, set, multiset, queue, stack, deque, and priority-queue operations |
| Algorithms | Sort, stable sort, reverse, fill, bounds, binary search, unique/erase, find/count, extrema, accumulate, iota, next permutation |
| Objects | Aggregate structs, fields, constructors, methods, selected overloaded operators, supported value copies |
| Functions | Instantiated function templates, `std::function`, typed lambdas with reference and value captures, scalar reference parameters and aliases, reference range loops |
| I/O | Buffered `cin`/`cout` and error streams, strings, basic stream formatting, `getline`, a subset of C formatted output |
| Intrinsics | GNU popcount, parity, clz, ctz, byte swaps, expect, trap/unreachable; selected C++ bit operations |

Implemented paths have limits. A container appearing in this table does not imply coverage of every constructor, member, iterator invalidation case, allocator, or custom comparator. Use `--check` when preparing a submission and compare program behavior on relevant inputs.

## Known gaps

- Arbitrary raw pointers, pointer arithmetic outside supported array ranges, general address-taking, and storage layout dependent code.
- Full object lifetimes and RAII, user-defined destructors, inheritance, unions, bitfields, RTTI, and exceptions.
- Generic lambdas, every capture mode, all reference-returning expressions, and full iterator/reference invalidation semantics.
- Coroutines, modules, concurrency, atomics, platform APIs, inline assembly, and target-specific SIMD intrinsics.
- GNU policy based data structures, arbitrary third-party libraries, full ranges/views, and complete C++23 library coverage.
- Exact `long double` behavior, floating environment differences, locale-sensitive I/O, and complete C `printf`/`scanf` compatibility.
- Exact unsigned `__int128` wraparound and all edge cases of C++ integer promotion and conversion rules.

Java's stack depth, boxing and collection memory overhead, and `BigInteger` costs differ from native C++. Passing a differential test is evidence for that tested behavior, not a full conformance claim or a time-limit guarantee.

## Diagnostics

Most unsupported constructs produce an explicit `j++` error, for example:

```text
solution.cpp:12:5: error: j++: unsupported statement GCCAsmStmt
```

Some unsupported combinations are currently caught only by Java compiler validation. This is why `--check` is available and why new lowering paths need execution tests, not just source snapshots.
