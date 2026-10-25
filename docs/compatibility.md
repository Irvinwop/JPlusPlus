# Compatibility

J++ is an experimental source transpiler for competitive programming. **It does not yet support all GNU++23 features or all compiler intrinsics.** Clang 18 determines which input syntax is accepted; Java lowering is a separate, smaller compatibility boundary.

## Current focus

| Area | Implemented paths |
| :-- | :-- |
| Frontend | GNU++23, preprocessing, local includes, `bits/stdc++.h`, aliases, resolved overloads, concrete function template instantiations |
| Control flow | Functions, recursion, blocks, `if`, `if constexpr`, runtime branches of `if consteval`, loops, switch, break, continue, return, forward goto exits to enclosing blocks |
| Expressions | Chained assignments, boolean arithmetic and numeric conversions, arithmetic compound assignments, void comma/conditional expressions, GNU omitted-middle conditionals and statement expressions with local control flow; RHS-before-destination sequencing and one destination evaluation for supported lvalues |
| Types | Boolean, character, signed integer and floating primitives; common unsigned 32/64-bit operations, widening and floating conversions; enums; signed `__int128` arithmetic via `BigInteger`; typed pointers into supported arrays, objects, and reference cells |
| Arrays | Fixed bounds, GNU variable length arrays, multidimensional primitive arrays, partial initialization, string-initialized character arrays |
| Containers | `vector` via `ArrayList`; converting pairs and range loops; selected emplacement; map/set node-value merging; common map, set, multiset, queue, stack, deque, and priority-queue operations |
| Algorithms | Sort, stable sort, reverse, fill, bounds, binary search, unique/erase, find/count, extrema, accumulate, iota, next permutation, copy/copy_n/copy_backward, merge, for_each, back_inserter |
| Objects | Global and local structs, aggregate and default-member initialization, fields, constructors and methods, scalar static data members with constant initialization, static methods, selected copy/move constructors and assignment operators, supported value copies and assignment retaining object identity |
| Pointers | Address-taking, dereference, scalar and array allocation, arithmetic and differences within an allocation, pointer parameters and aliases, pointer reference parameters, member access, selected void-pointer conversions, vector data and string-buffer pointers |
| Functions | Instantiated function templates, `std::function`, function pointers, typed lambdas with reference and value captures, generic lambdas with one concrete instantiated signature, scalar reference parameters and aliases, reference range loops, translation units without main |
| I/O | Buffered `cin`/`cout` and error streams, file input/output streams with lexical closing, default and append file output, standard-stream `freopen` redirection, strings and C character arrays, basic stream formatting, `getline`, `ignore`, selected `scanf`/`printf` conversions including widths, precision, suppression, scansets, and character counts |
| Intrinsics | GNU popcount, parity, clz, ctz, byte swaps, expect, trap/unreachable; selected C++ bit operations including bit_ceil, bit_floor, bit_width, has_single_bit; scalar float/integer bit_cast |
| Numeric library | Scalar numeric limits, type traits and mathematical constants; scalar constexpr/constinit initialization and consteval calls; selected math functions, C++ rounding, classification and sign tests, exponent scaling, copy-sign, decimal/scientific/hexadecimal floating output with ties-to-even formatting |

Implemented paths have limits. A container appearing in this table does not imply coverage of every constructor, member, iterator invalidation case, allocator, or custom comparator. Use `--check` when preparing a submission and compare program behavior on relevant inputs.

## Known gaps

- Byte-addressed storage, pointer/integer conversions, arbitrary pointer reinterpretation, placement construction, and storage layout dependent code. Typed pointer views do not provide a complete C++ memory model.
- Backward and computed jumps, jumps into nested scopes, and full unstructured control flow.
- Return/goto and break/continue escaping a GNU statement expression; consteval results requiring object snapshots; dynamic initialization of static data members.
- Full object lifetimes and RAII, user-defined destructors, inheritance, unions, bitfields, RTTI, and exceptions. Deallocation currently relies on Java garbage collection for supported objects without user destructors.
- Generic lambdas with multiple concrete call signatures, every capture mode, all reference-returning expressions, and full iterator/reference invalidation semantics. Node-value merging does not yet preserve iterator identity across containers.
- All constructor overloads after Java type erasure, all default-argument and reference-member cases, and complete user-defined move semantics.
- Coroutines, modules, concurrency, atomics, platform APIs, inline assembly, and target-specific SIMD intrinsics.
- GNU policy based data structures, arbitrary third-party libraries, full ranges/views, and complete C++23 library coverage.
- Exact `long double` behavior, floating environment differences, locale-sensitive I/O, and complete C `printf`/`scanf` compatibility.
- Complete file open-mode combinations, seeking, mixed read/write streams, and file-stream exception masks.
- Exact unsigned `__int128` wraparound and all edge cases of C++ integer promotion and conversion rules.

Java's stack depth, boxing and collection memory overhead, and `BigInteger` costs differ from native C++. Passing a differential test is evidence for that tested behavior, not a full conformance claim or a time-limit guarantee.

## Diagnostics

Most unsupported constructs produce an explicit `j++` error, for example:

```text
solution.cpp:12:5: error: j++: unsupported statement GCCAsmStmt
```

Some unsupported combinations are currently caught only by Java compiler validation. This is why `--check` is available and why new lowering paths need execution tests, not just source snapshots.
