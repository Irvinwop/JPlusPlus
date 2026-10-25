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

C++ copies are semantically different from Java reference assignment. Copying a vector or struct must keep later mutations independent. Generated copy methods and `jppCopy` preserve that behavior in supported paths. Reference parameters and mutable lambda captures use small reference objects only where shared mutation is required.

Assignment expressions retain their C++ grouping. Arithmetic compound assignments use Clang's computation types, including integer promotion of booleans and conversion back to the destination type. Simple updates stay as ordinary Java assignments. When evaluation order matters, typed helpers evaluate the right operand before the destination and evaluate the destination once, preserving C++17 sequencing in expressions such as `values[index++] += (index = 3)`.

Unsigned 32/64-bit ordering, division, remainder, and right shifts use Java's unsigned operations. `__int128` arithmetic uses `BigInteger`; it is slower than native arithmetic and currently targets defined signed calculations, not an emulation of every overflow behavior.

The bundled support declarations live in `runtime/Runtime.java`, a source fragment embedded into the C++ binary at build time. A declaration dependency pass selects the helpers referenced by the translated program. Clang's Java formatter formats the complete output.

## Diagnostics and execution

Unsupported nodes report a diagnostic at their C++ source location where one is available. There is no fallback that copies C++ syntax into Java. `--check` compiles a temporary source file before replacing an existing submission. File replacement uses a temporary file on the destination filesystem and an atomic rename.

The POSIX CLI launches `javac` and `java` with argument vectors, without interpolating filenames into a shell. Java compilation uses `--release 8`, `17`, or `21`. `-r` inherits input/output and propagates the program's exit status.
