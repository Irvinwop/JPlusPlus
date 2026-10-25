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

Narrow strings retain one Java character per C++ byte. Literal emission escapes non-ASCII bytes, and stream encoding uses ISO-8859-1 to preserve those bytes in both directions. This keeps UTF-8 sequences, invalid UTF-8, and embedded NULs intact without treating C++ string lengths as Unicode character counts. C-string operations stop at NUL; explicit lengths and iterator ranges retain it. File paths are truncated at NUL and decoded as UTF-8 for Java's filesystem APIs. Arbitrary non-UTF-8 Unix filenames remain unsupported.

C++ copies are semantically different from Java reference assignment. Copying a vector or struct must keep later mutations independent. Generated copy methods and `jppCopy` preserve that behavior in supported paths. Assignment uses separate methods that update the existing object, so retained references continue to observe its fields. Member initializers run during construction and are omitted when the C++ copy constructor supplies those members.

Reference parameters and mutable lambda captures use small reference objects where shared mutation is required. Typed pointers carry a storage view and a position. The view reads and writes the original Java array, collection, object field, or reference cell; pointer arithmetic changes the position. References retain their original address information so an address taken inside a callee still identifies the caller's allocation.

Primitive pointer producers also retain Clang's element width, signedness, and target byte order. Reinterpreting a pointer changes its view and stride while retaining the original storage. Memory operations encode and update the actual Java elements, including partial writes, so ordinary variable accesses see the same changes. Overlapping moves choose a direction and use bounded scratch storage. Raw C allocation uses byte arrays; `calloc` checks unsigned multiplication overflow, `realloc` retains the original block on allocation failure, and `free` releases the backing array. Record layouts and complete object-lifetime semantics remain separate gaps. These operations follow the C++ library's [byte-memory](https://eel.is/c++draft/cstring.syn) and [allocation](https://eel.is/c++draft/c.malloc) contracts within the documented storage limits.

Scalar and pointer reference results use the same reference objects as reference parameters. A translated call reads the result when a value is needed and retains the reference for assignment, address-taking, or another reference binding. Callable adapters distinguish reference results from value results, including conversions through `std::function`. Reference selection for supported `min` and `max` calls retains the selected argument's storage.

Maps store stable entry objects in `TreeMap` or `HashMap`. An iterator refers to the entry itself, and its mapped value remains the ordinary mutable `second` field. Inserting another key does not replace that entry. Merging transfers the same entry and updates its owning map, retaining value references and allowing an existing ordered iterator to advance in the destination. Structured bindings use member or element references, so writes reach the decomposed object and address-taking retains the original storage identity.

Map entries share an ownership handle with their storage. Swapping maps exchanges the backing maps, comparison state, and those handles while retaining the outer map objects. Updating the two handles keeps existing element iterators attached to their new containers without walking every entry. References to the map objects still identify the original maps.

Bitsets use Java's `BitSet` with the C++ bound retained explicitly. Shifts move 64-bit words and mask unused tail bits. Proxy objects identify an owning bitset and index; copying a proxy preserves that alias, while assigning through it writes the bit. Whole-bitset assignment retains the owning object so existing proxies remain attached. String construction follows the selected-prefix rules in the [C++ bitset constructor specification](https://eel.is/c++draft/bitset.cons).

Mersenne Twister engines carry their C++ template parameters into a compact state-vector implementation. Seeding, twists, and tempering retain the original word width, including unsigned 64-bit wraparound. Copies own independent state arrays, assignment retains the engine object, and discarding advances whole state blocks without calculating unused tempered outputs. Java's `Random` has a different sequence and is not used for these engines.

`std::seed_seq` stores copied 32-bit seed words and performs its mixing with Java's wrapping integer operations. Iterator adapters retain the destination's C++ element type, so the same generated words can fill unsigned 32-bit or wider storage. Engine initialization combines consecutive seed words in the specified order and applies the all-zero-state safeguard.

Deques use a circular `ArrayList` backing store for constant-time indexing and efficient operations at either end. A logical element position identifies references and iterators independently of the current buffer offset. Growth and end operations retain supported element references; removing elements at the front advances the origin while keeping surviving iterators attached. Swapping exchanges the storage objects, preserving element aliases and references to the outer deques. Range erasure shifts the shorter surviving side.

Assignment expressions retain their C++ grouping. Arithmetic compound assignments use Clang's computation types, including integer promotion of booleans and conversion back to the destination type. Simple updates stay as ordinary Java assignments. When evaluation order matters, typed helpers evaluate the right operand before the destination and evaluate the destination once, preserving C++17 sequencing in expressions such as `values[index++] += (index = 3)`.

Local statics use persistent storage fields with lazy initialization at the declaration. Template instantiations receive distinct storage. Scalar and array storage is available before the initializer runs, retaining addresses used by self references. Owned static file streams register in construction order and close in reverse order at normal shutdown. `std::_Exit` bypasses that cleanup and buffered output.

Java's integer switch accepts only narrow integer conditions. Wider switches use decimal string keys; GNU case ranges first select a case ordinal using comparisons with the original C++ signedness. Both forms preserve condition evaluation and ordinary fallthrough.

Unsigned 32/64-bit ordering, division, remainder, and right shifts use Java's unsigned operations. `__int128` arithmetic uses `BigInteger`; it is slower than native arithmetic and currently targets defined signed calculations, not an emulation of every overflow behavior.

Library comparisons also need the original C++ types. Generated predicates retain signedness and integer promotion rules for sorting, ordered containers, extrema, and heterogeneous bounds searches. Pair and vector predicates compare their elements recursively. User comparators are bound once when constructing the Java comparator, so their constructors do not run for each comparison.

Callable adapters bridge compatible C++ signatures when their Java interfaces differ. They convert parameters and results explicitly, including conversion to boolean and discarding a result for a void callback. This keeps the callable body in an ordinary translated method or lambda.

Stream-setting calls retain receiver evaluation, including input expressions chained before `tie` or a static `sync_with_stdio` call. A shared stream-state base stores ties; input and output sentries flush the tied output. `cerr` and `clog` keep separate formatting state over one output buffer. The synchronization flag follows [libstdc++'s disabling behavior](https://github.com/gcc-mirror/gcc/blob/releases/gcc-13/libstdc%2B%2B-v3/src/c%2B%2B98/ios_init.cc), while native C/C++ buffer interleaving remains outside this model.

The bundled support declarations live in `runtime/Runtime.java`, a source fragment embedded into the C++ binary at build time. A declaration dependency pass selects the helpers referenced by the translated program. Clang's Java formatter formats the complete output.

The build embeds the fragment in bounded string chunks, avoiding a compiler's single-literal size limit as support grows.

## Diagnostics and execution

Unsupported nodes report a diagnostic at their C++ source location where one is available. There is no fallback that copies C++ syntax into Java. `--check` compiles a temporary source file before replacing an existing submission. File replacement uses a temporary file on the destination filesystem and an atomic rename.

The POSIX CLI launches `javac` and `java` with argument vectors, without interpolating filenames into a shell. Java compilation uses `--release 8`, `17`, or `21`. `-r` inherits input/output and propagates the program's exit status.
