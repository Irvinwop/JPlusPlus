# How J++ works

```text
GNU++23 source
    │  macros, includes, overload resolution, type deduction, templates
    ▼
Clang AST
    │  semantic translation, library mappings, source-located diagnostics
    ▼
Readable Java + selected support helpers
    │  formatting, optional compiler validation, atomic output replacement
    ▼
One standalone .java file
```

The implementation is C++23 and uses Clang's C++ AST API directly. It does not parse C++ with regular expressions, translate through a VM, or embed an executable. Preprocessor macros and included local headers go through the same frontend as normal C++ compilation. Instantiated user function templates are emitted as concrete methods.

## Java mappings

`src/transpiler.cpp` walks user declarations and translates expressions and statements. Primitive C++ arrays become primitive Java arrays. `std::vector` uses `ArrayList`. Helpers use `TreeMap`, `HashMap`, `ArrayDeque`, and `PriorityQueue` for the corresponding library operations. A mutable string wrapper uses `StringBuilder` to support C++ character assignment.

C++ copies are semantically different from Java reference assignment. Copying a vector or struct must keep later mutations independent. Generated copy methods and `jppCopy` preserve that behavior in supported paths. Assignment uses separate methods that update the existing object, so retained references continue to observe its fields. Member initializers run during construction and are omitted when the C++ copy constructor supplies those members.

Reference parameters and mutable lambda captures use small reference objects where shared mutation is required. Typed pointers carry a storage view and a position. The view reads and writes the original Java array, collection, object field, or reference cell; pointer arithmetic changes the position. References retain their original address information so an address taken inside a callee still identifies the caller's allocation. This supports typed access without a byte-addressed memory emulator; layout-dependent operations remain a separate compatibility gap.

Assignment expressions retain their C++ grouping. Arithmetic compound assignments use Clang's computation types, including integer promotion of booleans and conversion back to the destination type. Simple updates stay as ordinary Java assignments. When evaluation order matters, typed helpers evaluate the right operand before the destination and evaluate the destination once, preserving C++17 sequencing in expressions such as `values[index++] += (index = 3)`.

Local statics use persistent storage fields with lazy initialization at the declaration. Template instantiations receive distinct storage. Scalar and array storage is available before the initializer runs, retaining addresses used by self references. Owned static file streams register in construction order and close in reverse order at normal shutdown. `std::_Exit` bypasses that cleanup and buffered output.

Java's integer switch accepts only narrow integer conditions. Wider switches use decimal string keys; GNU case ranges first select a case ordinal using comparisons with the original C++ signedness. Both forms preserve condition evaluation and ordinary fallthrough.

Unsigned 32/64-bit ordering, division, remainder, and right shifts use Java's unsigned operations. `__int128` arithmetic uses `BigInteger`; it is slower than native arithmetic and currently targets defined signed calculations, not an emulation of every overflow behavior.

Library comparisons also need the original C++ types. Generated predicates retain signedness and integer promotion rules for sorting, ordered containers, extrema, and heterogeneous bounds searches. Pair and vector predicates compare their elements recursively. User comparators are bound once when constructing the Java comparator, so their constructors do not run for each comparison.

Callable adapters bridge compatible C++ signatures when their Java interfaces differ. They convert parameters and results explicitly, including conversion to boolean and discarding a result for a void callback. This keeps the callable body in an ordinary translated method or lambda.

The bundled support declarations live in `runtime/Runtime.java`, a source fragment embedded into the C++ binary at build time. A declaration dependency pass selects the helpers referenced by the translated program. Clang's Java formatter formats the complete output.

The build embeds the fragment in bounded string chunks, avoiding a compiler's single-literal size limit as support grows.

## Diagnostics and execution

Unsupported nodes report a diagnostic at their C++ source location where one is available. There is no fallback that copies C++ syntax into Java. `--check` compiles a temporary source file before replacing an existing submission. File replacement uses a temporary file on the destination filesystem and an atomic rename.

The POSIX CLI launches `javac` and `java` with argument vectors, without interpolating filenames into a shell. Java compilation uses `--release 8`, `17`, or `21`. `-r` inherits input/output and propagates the program's exit status.
