# Testing the translation

`tests/integration.cpp` is a C++ end-to-end harness. The oracle is the independently compiled native C++ program. Each Java target is generated separately, compiled into real class files, and run with the same input. Output is compared byte for byte. The harness also checks class-file versions and failure behavior of the CLI.

Randomized inputs have a fixed seed so failures can be reproduced. Fixtures exercise negative values, duplicates, 64-bit magnitudes, graph paths, nested-container mutations, value copies, shared references, recursion, bit operations, unsigned values above the signed limit, and intermediate products requiring 128 bits. Expression regressions cover assignment precedence, boolean promotions and arithmetic, and C++17 assignment sequencing, including destinations with side effects and compound assignments that modify their own operands.

Algorithm and conversion fixtures exercise reference callbacks, returned stateful function objects, generic comparators, overlapping backward copies, cross-type output iterators, pair conversions, conditional lvalues, emplacement, and map/set merging. Library translation units without `main` are checked using separate native and Java drivers that call the translated methods.

File fixtures run in an isolated working directory and compare both console output and files against native C++. They cover I/O during global initialization, closing on return, append mode, stream conditions, missing files, and C stream redirection. Formatted I/O fixtures cover unsigned boundaries, assignment counts, field widths, scansets, suppressed conversions, `%n`, matching failures, and EOF. Sequencing fixtures exercise void calls in comma expressions and conditional branches with mutable locals.

Extraction regressions compare signed and unsigned integer limits, narrow integer overflow, boolean failure states, floating prefixes and overflow, unchanged destinations after failure, and source/destination expressions with side effects. Line-reading cases include carriage returns and a final line without a newline.

Forward-jump cases compare nested exits, crossing destinations, declarations used after a label, unreachable statements, switch cases, and file closing on a jump. Iterator cases exercise both operand orders of array/iterator addition, raw-array bound differences, compound iterator movement, reverse iterators, and deferred iterator assignment.

Numeric cases compare raw IEEE bits for scalar bit casts, rounding, signed zero and NaNs, and unsigned-to-floating conversions. They include integer widening above the signed limit and values where conversion through `double` would change the result of a direct conversion to `float`. Formatting checks cover subnormals, infinities, NaNs, rounding ties, hexadecimal precision, and decimal precision beyond the shortest representation.

Constant and naming cases exercise compile-time versus runtime evaluation, scalar static members, numeric limits and traits, unsigned and boolean enums, zero-initialized 128-bit arrays, signed character promotion, and C++ names that collide with Java library classes or the generated entrypoint. Numeric equality is also tested through boxed container elements and references, including integers outside the boxing cache, NaN, and signed zero.

GNU expression cases compare omitted-middle conditionals with side effects, lazy fallback evaluation, lvalue assignment and increment, immediate map insertion when binding a reference, local variable shadowing, record value copies, scalar rvalue-reference aliases, and temporary materialization. Statement expressions also open and close a file before the caller reads it, so resource lifetime is part of the execution check.

Pointer cases compare offsets, differences, unrelated allocations, scalar and pointer reference parameters, function-address identity and indirect calls, linked nodes, member access and addresses, array allocation and partial initialization, multidimensional arrays, void-pointer round trips, string-buffer identity, and vector data. Taking an address through a reference must retain the original array position and allocation; another function then uses that pointer to access a neighboring element.

Object cases retain references and pointers across assignment, including members and array elements. They compare constructor side effects, aggregate default-member initialization, self references, user copy/move constructors and assignment operators, nested self-assignment, collection aliases, and returns of void expressions. These checks distinguish constructing a copy from assigning into an existing object.

## Cross-compiler and cross-VM checks

The local torture matrix has been exercised with these independent compilers and runtimes:

| Compiler | Java releases |
| :-- | :-- |
| OpenJDK `javac` 21 | 8, 17, 21 |
| OpenJDK `javac` 17 | 8, 17 |
| Temurin `javac` 8 | 8 |
| Eclipse ECJ | 8, 17 |

| Runtime | Class-file releases |
| :-- | :-- |
| HotSpot 21 | 8, 17, 21 |
| HotSpot 17 | 8, 17 |
| Temurin HotSpot 8 | 8 |
| IBM Semeru/OpenJ9 17 | 8, 17 |

Set `JPP_TEST_JAVACS` and `JPP_TEST_JAVAS` to colon-separated executable paths. The harness skips releases that a selected compiler or runtime cannot support. ECJ is capped at release 17 for compatibility with Ubuntu's packaged version.

When the selected runtime is Java 8, Ubuntu's ECJ may itself need a newer JVM. Use `tests/ecj.sh` as the compiler path and set `JPP_ECJ_JAVA` to a current Java executable. This separates the JVM running the compiler from the JVM running the generated program.

The source corpus used to guide development is private and is not distributed. A corpus compile check means only that generated output compiles. Behavioral claims come from programs executed against the native oracle. Incomplete sources, missing headers, and native frontend failures must not be counted as successful translations. A valid library file without `main` can count toward compile coverage; execution requires a driver.

## Adding a regression

Choose a small program where a plausible mistranslation changes its output. Keep the relevant behavior intact: do not replace a container, I/O path, compiler, or VM with a mock. Add inputs that distinguish the correct semantics from the defect. Avoid tests that only search for an emitted token or assert a fixture's assigned constant.
