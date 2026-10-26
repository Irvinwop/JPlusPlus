#!/usr/bin/env bash
set -euo pipefail

jpp_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
jpp_patch="$jpp_repo/toolchain/llvm18-compat.patch"
jpp_patch_sha=$(sha256sum "$jpp_patch")
jpp_patch_sha=${jpp_patch_sha%% *}
jpp_work=${JPP_FRONTEND_WORK_DIR:-"$jpp_repo/build/frontend-18-${jpp_patch_sha:0:12}"}
jpp_output=${JPP_BUILD_DIR:-"$jpp_repo/build-gnu"}
jpp_jobs=${JPP_BUILD_JOBS:-4}
jpp_prefix=${JPP_FRONTEND_PREFIX:-"$HOME/.local/lib/jpp/frontends"}

for jpp_command in cmake ninja curl tar patch sha256sum llvm-config-18 clang-18 clang++-18; do
    command -v "$jpp_command" >/dev/null || { printf 'Missing build tool: %s\n' "$jpp_command" >&2; exit 1; }
done
if [[ $(llvm-config-18 --version) != 18.1.3 ]]; then
    printf 'This frontend patch targets LLVM 18.1.3; use that development package.\n' >&2
    exit 1
fi
jpp_llvm_cmake=$(llvm-config-18 --cmakedir)
jpp_llvm_lib=$(llvm-config-18 --libdir)
mkdir -p "$jpp_work"
jpp_work=$(cd "$jpp_work" && pwd)
jpp_source="$jpp_work/llvm-project-llvmorg-18.1.3"
jpp_frontend_build="$jpp_work/clang-build"
jpp_archive="$jpp_work/llvmorg-18.1.3.tar.gz"
if [[ ! -f $jpp_archive ]]; then
    curl --fail --location --retry 3 \
        https://codeload.github.com/llvm/llvm-project/tar.gz/refs/tags/llvmorg-18.1.3 \
        -o "$jpp_archive.download"
    mv "$jpp_archive.download" "$jpp_archive"
fi
printf '%s  %s\n' fc5a2fd176d73ceb17f4e522f8fe96d8dde23300b8c233476d3609f55d995a7a "$jpp_archive" | sha256sum --check -
if [[ ! -d $jpp_source ]]; then
    tar -xzf "$jpp_archive" -C "$jpp_work"
fi
if patch --dry-run --reverse --force --silent --fuzz=0 -p1 -d "$jpp_source" < "$jpp_patch" > "$jpp_work/patch-state.log" 2>&1; then
    printf 'Compatibility patch is already applied.\n'
elif patch --dry-run --forward --force --silent --fuzz=0 -p1 -d "$jpp_source" < "$jpp_patch" > "$jpp_work/patch-state.log" 2>&1; then
    patch --forward --force --fuzz=0 -p1 -d "$jpp_source" < "$jpp_patch"
else
    cat "$jpp_work/patch-state.log" >&2
    printf 'Source differs from the expected LLVM tree; choose a fresh JPP_FRONTEND_WORK_DIR.\n' >&2
    exit 1
fi

cmake -S "$jpp_source/clang" -B "$jpp_frontend_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
    -DLLVM_DIR="$jpp_llvm_cmake" -DCMAKE_INSTALL_PREFIX="$jpp_work/clang-install" \
    -DCLANG_REPOSITORY_STRING="JPlusPlus GNU compatibility $jpp_patch_sha" \
    -DLLVM_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_DOCS=OFF \
    -DCLANG_BUILD_EXAMPLES=OFF -DCLANG_ENABLE_STATIC_ANALYZER=OFF -DCLANG_ENABLE_ARCMT=OFF \
    -DCLANG_BUILD_CLANG_DYLIB=ON -DCLANG_LINK_CLANG_DYLIB=ON \
    -DLLVM_LINK_LLVM_DYLIB=ON -DLLVM_ENABLE_ASSERTIONS=OFF -DLLVM_PARALLEL_LINK_JOBS=1
cmake --build "$jpp_frontend_build" --target clang-cpp -j "$jpp_jobs"

jpp_library="$jpp_frontend_build/lib/libclang-cpp.so.18.1"
jpp_library_sha=$(sha256sum "$jpp_library")
jpp_library_sha=${jpp_library_sha%% *}
jpp_library_dir="$jpp_prefix/llvm18-$jpp_library_sha"
mkdir -p "$jpp_library_dir"
jpp_library_dir=$(cd "$jpp_library_dir" && pwd)
if [[ ! -e $jpp_library_dir/libclang-cpp.so.18.1 ]]; then
    install -m 444 "$jpp_library" "$jpp_library_dir/libclang-cpp.so.18.1"
fi
printf '%s  %s\n' "$jpp_library_sha" "$jpp_library_dir/libclang-cpp.so.18.1" | sha256sum --check -
install -m 444 "$jpp_repo/toolchain/LICENSE-LLVM.txt" "$jpp_library_dir/LICENSE-LLVM.txt"

cmake -S "$jpp_repo" -B "$jpp_output" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++-18 -DLLVM_DIR="$jpp_llvm_cmake" \
    -DClang_DIR="$jpp_frontend_build/lib/cmake/clang" -DJPP_TEST_GNU_FRONTEND=ON \
    -DCMAKE_BUILD_RPATH="$jpp_library_dir;$jpp_llvm_lib" \
    -DCMAKE_INSTALL_RPATH="$jpp_library_dir;$jpp_llvm_lib"
cmake --build "$jpp_output" -j "$jpp_jobs"
printf '\nBuilt %s/j++\nFrontend library: %s\n' "$jpp_output" "$jpp_library_dir"
