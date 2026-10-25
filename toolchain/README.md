# GNU compatibility frontend

The optional frontend adds GNU C++ byte offsets and increments on `void*`, plus `sizeof(void)` and GNU alignment queries. It also stores repeated trivial array values compactly during Clang constant evaluation. The declared array size and generated allocation retain their original bounds.

The patch targets **LLVM/Clang 18.1.3**, using the installed LLVM libraries. It preserves strict C++ diagnostics and evaluates nontrivial constructors separately for each element. The standard build continues to use the installed Clang frontend.

## Build and install

On Ubuntu 24.04, install the normal J++ build dependencies plus `libzstd-dev`, `curl`, and `patch`. Then run from the repository:

```sh
tools/build-gnu.sh
ctest --test-dir build-gnu --output-on-failure
cmake --install build-gnu --prefix "$HOME/.local"
```

The first build compiles Clang; later builds reuse its outputs. `JPP_BUILD_JOBS` sets parallelism, defaulting to four. `JPP_FRONTEND_WORK_DIR`, `JPP_BUILD_DIR`, and `JPP_FRONTEND_PREFIX` select the source/build cache, J++ output directory, and frontend library directory.

The script verifies the upstream archive, applies the patch, and copies the resulting library into a directory named by its SHA-256. J++ links to that copy, so subsequent compiler builds do not replace a frontend used by a tested executable. Keep that library directory alongside an installed J++.

Clang builtin headers are located independently of the executable's path. `JPP_CLANG_RESOURCE_DIR` can override their location when configuring CMake.

## Validation

`JPP_TEST_GNU_FRONTEND=ON` enables the GNU pointer fixture. The array fixture checks constant-evaluated allocation, mutation, copying, self-address initialization, and ordinary global arrays against native GCC. GNU pointer checks compare byte offsets, reference updates, partial memory writes, unevaluated alignment operands, and null-pointer cases.

The private corpus sweep also exercises the original 60-million-record declaration. This optimization changes Clang's representation of identical values; it does not shorten or omit the declaration.

The patch and rebuilt LLVM components retain [Apache 2.0 with LLVM exceptions](LICENSE-LLVM.txt). J++ itself remains [Apache 2.0](../LICENSE).
