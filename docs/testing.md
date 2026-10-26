# Testing the translation

`tests/integration.cpp` is a C++ end-to-end harness. The oracle is the independently compiled native C++ program. Each Java target is generated separately, compiled into real class files, and run with the same input. Output is compared byte for byte. The harness also checks class-file versions and failure behavior of the CLI.

Randomized inputs have a fixed seed so failures can be reproduced. Fixtures exercise negative values, duplicates, 64-bit magnitudes, graph paths, nested-container mutations, value copies, shared references, recursion, bit operations, unsigned values above the signed limit, and intermediate products requiring 128 bits. Expression regressions cover assignment precedence, boolean promotions and arithmetic, and C++17 assignment sequencing, including destinations with side effects and compound assignments that modify their own operands.

Algorithm and conversion fixtures exercise reference callbacks, returned stateful function objects, generic comparators, overlapping backward copies, cross-type output iterators, pair conversions, conditional lvalues, emplacement, and map/set merging. Library translation units without `main` are checked using separate native and Java drivers that call the translated methods.

File fixtures run in an isolated working directory and compare both console output and files against native C++. They cover I/O during global initialization, closing on return, append mode, stream conditions, missing files, and C stream redirection. Formatted I/O fixtures cover unsigned boundaries, assignment counts, field widths, scansets, suppressed conversions, `%n`, matching failures, and EOF. Sequencing fixtures exercise void calls in comma expressions and conditional branches with mutable locals.

Standard-stream byte I/O consumes more than 64 KiB of deterministic binary input, including zero and high-bit bytes. Native comparisons check unaligned writes into integer representations, live standard-array element aliases, guards around buffers, zero-length requests, complete-element counts at EOF, and subsequent reads after a partial final element. Both standard output and standard error are compared as bytes. The fixture reads only complete returned elements when inspecting data.

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

Tuple cases distinguish independent value copies from retained reference bindings, overlapping `tie` assignments, ignored elements, nested and narrow conversions, structured bindings, and array addresses from raw allocations. Wrapper cases check rebinding separately from assignment through a reference and exercise callable wrappers. Invocation cases pass tuple elements to free functions, lambdas, and constructors, including void and mutable-reference results. User-object cases compare observable copy/move counts, converting assignments, and ADL-selected swaps, including pair and tuple insertion and one-element tuple emplacement in vectors, queues, and deques. A 201-element tuple exercises constructors beyond the JVM's parameter limit.

Wrapper-object aliases remain attached to local bindings, struct fields, and array elements across rebinding and array assignment. Cases distinguish copied wrapper values from references to a wrapper, nest wrappers, retain assignment-result addresses, and pass or return wrapper references through functions and `std::function`. Fully initialized arrays and nested aggregates also contain elements without default constructors; only the constructors selected by C++ may appear in generated Java.

Comparison-category cases include unsigned tuple fields, signed zero, infinities, NaNs, unordered results, comparisons in both operand orders, assignment aliases, and independent category copies. Loop-reachability cases distinguish nested breaks from exits from an infinite loop, discarded constexpr branches, forward jumps, and constant-false loops whose initializers still run.

Ordering cases use unsigned values on both sides of the signed limit, pairs, raw arrays, range-constructed sets, initializer-list sets, map insertion, custom comparators, and min-heaps. Native output checks both ordering and comparator construction effects. Heterogeneous bounds searches use signed and unsigned elements, 64-bit keys, and descending transparent comparators; extrema and direct transparent comparisons cover C++ integer conversion rules.

Stream-state cases consume input through chained `tie` and `sync_with_stdio` calls and check returned stream pointers and synchronization flags. Immediate termination distinguishes tied and unit-buffered output from pending buffered output. Standard error is compared byte for byte for the stream fixtures, including shared `cerr`/`clog` buffering and flushing a pending log on `std::exit`.

Map-iterator cases retain entry, value, and address aliases across insertion and merging. They distinguish a copied pair from a live entry, mutate through `at`, insert during traversal, walk in both directions, and erase individual iterators and ranges. Duplicate-key set ranges exercise iterator distance in both directions. Assignment cases use containers constructed with different comparator state to check propagation of ordering.

Binding cases mutate structs, pairs, built-in arrays, and `std::array` through structured bindings and compare addresses with the original members. A user copy constructor makes a pointer refer to the copied object's own field; mutating that binding must change the pointee while preserving the source object. Range-loop cases compare value copies with reference bindings for nested arrays.

Bitset cases instantiate sizes 0, 1, 16, 63, 64, 65, and 129. They compare word-boundary and oversized shifts, masked complements, numeric and string construction, string prefixes with custom digits, unsigned conversion, GNU bit searches, and hashed keys copied before mutation. Proxy copies remain aliases across assignment and swap. Sequencing cases mutate an index or source bit during assignment so reading a proxy at the wrong time changes the result.

Cast regressions chain methods after temporary construction and mutate arrays through explicitly cast pointers. These check that a Java cast applies to its operand before a following member access or subscript.

Byte-string cases compare literal and input bytes for UTF-8, invalid UTF-8, embedded NULs, and high-bit characters. Pointer/count and iterator-range construction, self-overlapping assignment and append, initializer lists, large substring counts, and byte-wise mutation distinguish bounded strings from C strings. Formatted output checks byte precision and return counts; a file round-trip uses a UTF-8 filename with a NUL suffix. The harness compares the file contents as bytes as well as console output.

Discarded-expression cases use conditional, comma, arithmetic, cast, and literal expressions in statement and loop-update positions. Native results distinguish skipped updates, duplicate evaluation, and updates incorrectly executed after a break or continue. Heap cases combine explicit lambdas, copied stateful comparators, comparator factories, range element conversion, supplied containers, and independent copies. Mutating the original comparator or container must not change the stored ordering or values.

Reference-result cases mutate through direct and recursive functions, function pointers, methods, overloaded subscripts and dereference, lambdas, and `std::function`. They compare retained aliases, pointer offsets, addresses of neighboring elements, selected extrema, static storage, and conversion to value-returning callables. Side-effect counters catch repeated calls when a returned reference is used as an assignment destination; a compound-assignment result must retain the original element's address.

Map-swap cases retain references to both the containers and their elements, then use existing iterators in the destination. They compare addresses, iterator movement, erasure, self-swap, stateful comparison order, and later assignment and copying. Construction cases use initializer lists and ranges with element conversions, duplicate keys, copied comparison state, and partially initialized nested pair arrays. Unordered-map checks avoid relying on an unspecified traversal order.

Vector swap and move cases retain outer-container references and element references, pointers, and iterators at the same time. The native oracle distinguishes their different destinations after a storage transfer. Cases cover nested vectors, tuples and arrays containing vectors, empty and self-swaps, move construction and assignment, the address of an assignment result, independent copies, returned allocations, and unchanged element-constructor counts during a whole-vector move. Fixed scalar arrays separately retain their original inline addresses across swaps.

Memberwise-move cases cover standard arrays, implicit and explicitly defaulted record operations, nested records, built-in array fields, and const members that must copy. Retained member aliases, moved allocations, assignment-result addresses, and observable constructor and assignment counts distinguish storage transfer from copying or replacing an inline object. Private element operations are selected in the containing record's friend context. A constructor taking a boolean competes with a move constructor after Java erasure. A 50,000-element array checks that generation stays bounded and does not add default construction; zero-length arrays remain usable when their element type cannot be constructed, copied, or moved.

Random-engine cases compare native `mt19937`, `mt19937_64`, an explicitly 32-bit result type with high-bit masks, a 13-bit configuration, and a one-word 64-bit configuration. Seeds include unsigned boundaries and fixed randomized values. Samples and rolling output digests span multiple state refills; discard counts cross 17-, 312-, and 624-word boundaries. Independent copies, assignment aliases, array elements, value parameters and returns, captured engines, default reseeding, and explicit operator calls exercise state ownership. Static bounds called through a factory retain its side effects.

Seed-sequence cases use empty, initializer-list, signed-integer range, and character range construction. Generation lengths cross the 7-, 39-, 68-, and 623-word mixing thresholds and include zero-length and single-element destinations. Guard elements detect writes outside a requested subrange. Native output checks wider destination types, repeated generation, parameter output through an insertion iterator, both standard Mersenne Twister engines, reseeding, and the zero-state safeguard in a two-bit configuration. Signed `labs`/`llabs` cases retain exact integer magnitudes above floating-point precision.

Deque cases grow and wrap the buffer, retain element references and pointers, and keep an iterator across front and back removals. Container swaps distinguish references to the deques from aliases to their elements. Sorting, unique/erase, insertion, prefix and middle range erasure, resizing, unsigned comparisons, value copies, and record-member aliases are compared with native C++. Range construction checks that later source mutation does not affect stored values.

Byte-memory cases compare partial copies between unlike element widths, byte stores through field and local aliases, both directions of overlapping moves, and return-address identity. The native oracle also checks short signedness, 128-bit representations, allocation zeroing and resize preservation, string-buffer terminators, unsigned byte comparisons and searches, quiet-NaN payloads, signed zero, and overlapping transfers larger than the scratch buffer. Ordinary Java array and field accesses must observe mutations made through byte pointers.

Array-view cases retain allocation origins through nested decay, scalar addresses, row-pointer casts, array-reference parameters and results, function pointers, and `std::function`. They mutate malloc-backed multidimensional arrays through range loops and structured bindings, and distinguish independent value copies and captured arrays from aliases. Partial byte writes and overlapping moves cross row and plane boundaries in the original Java arrays. Array `new` also retains its nested shape and byte view.

Literal-array cases compare conditional string literals in formatted and stream output, array-reference parameters and results, function pointers, and `std::function`. Array size, pointer offsets, embedded NULs, UTF-8 bytes, and high-bit characters must survive decay and range iteration through the same literal storage.

VLA cases change dimension variables after allocation and typedef declarations, then compare nested `sizeof` with native GCC. Side-effect counters cover once-only bounds, inner-to-outer evaluation, type formation in conditional operands, GNU alignment queries, `noexcept`, type traits, `decltype`, and unevaluated size operands. A conditional array `new` separately checks ordinary lazy evaluation.

Chrono cases compare positive and negative duration casts, half-way rounding, mixed rational periods, arithmetic, comparisons, sorting, independent copies, assignment through retained references, time-point casts, epoch conversions, literals, and duration suffixes. Live readings must advance after a timed wait and fall within the expected Unix-epoch range. These properties catch constant clock stubs and incorrect timestamp units without comparing nondeterministic readings from separate executions.

Frontend array cases exercise constant-evaluated array construction, sparse mutation, independent copies, self-address initialization, and global record arrays. The optional GNU frontend adds byte offsets on `void*`, reference and indexed updates, zero offsets on null pointers, and declaration alignment. Unevaluated operands must retain their side-effect counters. Compatibility testing also requires strict C++ to reject GNU-only offsets and GNU C++ to reject void-pointer differences, matching GCC.

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
