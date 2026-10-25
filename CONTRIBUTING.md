# Contributing

Build with LLVM/Clang 18, then run `ctest --test-dir build --output-on-failure`.

When extending the transpiler:

1. Add an independently meaningful native/Java differential case for the behavior you are implementing.
2. Use Clang's resolved types and declarations. Avoid textual substitutions over C++ source.
3. Prefer standard Java collections and preserve source names unless Java requires a rename.
4. Handle the C++ copy/reference and integer semantics explicitly. Reject unsupported cases.
5. Keep all required support code in the generated source and update the compatibility notes.

Please include a small complete C++ reproducer, input, native output, generated Java output, Java compiler/runtime versions, and the exact `j++` invocation when reporting a bug. Do not post private contest solutions without their author's permission.
