# Testing the translation

`tests/integration.cpp` is a C++ end-to-end harness. The oracle is the independently compiled native C++ program. Each Java target is generated separately, compiled into real class files, and run with the same input. Output is compared byte for byte. The harness also checks class-file versions and failure behavior of the CLI.

Randomized inputs have a fixed seed so failures can be reproduced. Fixtures exercise negative values, duplicates, 64-bit magnitudes, graph paths, nested-container mutations, value copies, shared references, recursion, bit operations, unsigned values above the signed limit, and intermediate products requiring 128 bits. Expression regressions cover assignment precedence, boolean promotions and arithmetic, and C++17 assignment sequencing, including destinations with side effects and compound assignments that modify their own operands.

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

The source corpus used to guide development is private and is not distributed. A corpus compile check means only that generated output compiles. Behavioral claims come from programs executed against the native oracle. Incomplete sources, missing headers, files without `main`, and native frontend failures must not be counted as successful translations.

## Adding a regression

Choose a small program where a plausible mistranslation changes its output. Keep the relevant behavior intact: do not replace a container, I/O path, compiler, or VM with a mock. Add inputs that distinguish the correct semantics from the defect. Avoid tests that only search for an emitted token or assert a fixture's assigned constant.
