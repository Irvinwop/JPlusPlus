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

Insertion cases mutate and reuse records after pushing them into vectors, queues, deques, stacks, and heaps. The oracle checks independent stored values and user move-constructor effects. String input cases retain references to a string across extraction and line reading, then mutate through the retained reference.

Static-storage cases compare once-only initialization, repeated and recursive calls, template instantiations, arrays and self pointers, reference bindings, lambda-local statics, and local extern access. Static output streams are inspected after `std::exit`; a separate `std::_Exit` fixture checks that buffered console and file output is discarded.

Switch cases distinguish values with matching low 32 bits, signed and unsigned boundaries, 128-bit conditions, GNU case ranges, enum conditions, condition declarations, and fallthrough. Call cases compare default-expression side effects and global bindings, default reference parameters, user methods named like library operations, and evaluation of static-method receivers.

Standard-value cases cover partial, nested, and zero-length `std::array` initialization, copies, assignment, and element references retained across fill. They also exercise reference-taking constructors, conversion of callable arguments and results, discarded results for void callbacks, recursive converted callbacks, and floating-point to `__int128` conversion above the 64-bit range.

Ordering cases use unsigned values on both sides of the signed limit, pairs, raw arrays, range-constructed sets, initializer-list sets, map insertion, custom comparators, and min-heaps. Native output checks both ordering and comparator construction effects. Heterogeneous bounds searches use signed and unsigned elements, 64-bit keys, and descending transparent comparators; extrema and direct transparent comparisons cover C++ integer conversion rules.

Stream-state cases consume input through chained `tie` and `sync_with_stdio` calls and check returned stream pointers and synchronization flags. Immediate termination distinguishes tied and unit-buffered output from pending buffered output. Standard error is compared byte for byte for the stream fixtures, including shared `cerr`/`clog` buffering and flushing a pending log on `std::exit`.

Map-iterator cases retain entry, value, and address aliases across insertion and merging. They distinguish a copied pair from a live entry, mutate through `at`, insert during traversal, walk in both directions, and erase individual iterators and ranges. Duplicate-key set ranges exercise iterator distance in both directions. Assignment cases use containers constructed with different comparator state to check propagation of ordering.

Binding cases mutate structs, pairs, built-in arrays, and `std::array` through structured bindings and compare addresses with the original members. A user copy constructor makes a pointer refer to the copied object's own field; mutating that binding must change the pointee while preserving the source object. Range-loop cases compare value copies with reference bindings for nested arrays.

Bitset cases instantiate sizes 0, 1, 16, 63, 64, 65, and 129. They compare word-boundary and oversized shifts, masked complements, numeric and string construction, string prefixes with custom digits, unsigned conversion, GNU bit searches, and hashed keys copied before mutation. Proxy copies remain aliases across assignment and swap. Sequencing cases mutate an index or source bit during assignment so reading a proxy at the wrong time changes the result.

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

`JPP_TEST_CASES` selects colon-separated fixture names, such as `calls:statics:switches`. Include `cli` to run the CLI checks. Leaving it unset runs the entire suite. Each harness process uses its own temporary directory, so disjoint selections can run concurrently.

When the selected runtime is Java 8, Ubuntu's ECJ may itself need a newer JVM. Use `tests/ecj.sh` as the compiler path and set `JPP_ECJ_JAVA` to a current Java executable. This separates the JVM running the compiler from the JVM running the generated program.

The source corpus used to guide development is private and is not distributed. A corpus compile check means only that generated output compiles. Behavioral claims come from programs executed against the native oracle. Incomplete sources, missing headers, and native frontend failures must not be counted as successful translations. A valid library file without `main` can count toward compile coverage; execution requires a driver.

## Adding a regression

Choose a small program where a plausible mistranslation changes its output. Keep the relevant behavior intact: do not replace a container, I/O path, compiler, or VM with a mock. Add inputs that distinguish the correct semantics from the defect. Avoid tests that only search for an emitted token or assert a fixture's assigned constant.
