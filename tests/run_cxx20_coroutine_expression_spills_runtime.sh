#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

rcc_root=$(cd -- "$(dirname -- "$0")/.." && pwd)
rinos_root=$(cd -- "$rcc_root/../../.." && pwd)
sdk_root="$rinos_root/public-base/RinOS-SDK"
libc_root="$rinos_root/public-base/libs/libc"
libcxx_root="$rinos_root/public-base/libs/libcxx"
out="$rcc_root/build/tests/cxx20-coroutine-expression-spills-runtime"
mkdir -p "$out"

build_and_run() {
    local arch="$1"
    local input_assembly="$2"
    shift 2
    local flags=("$@")
    local arch_out="$out/$arch"
    mkdir -p "$arch_out"

    gcc "${flags[@]}" -O2 -g -ffunction-sections -fdata-sections \
        -fno-builtin -DRINCRT_HOST_TEST -I"$sdk_root/include" \
        -c "$sdk_root/src/rincrt_bootstrap_v1.c" \
        -o "$arch_out/rincrt_host.o"
    gcc "${flags[@]}" -O2 -g -ffunction-sections -fdata-sections \
        -fno-builtin -I"$sdk_root/include" -I"$libc_root/include" \
        -I"$libcxx_root" -c "$libcxx_root/new_runtime.c" \
        -o "$arch_out/new_runtime_host.o"
    gcc "${flags[@]}" -O2 -g -ffunction-sections -fdata-sections \
        -c "$rcc_root/tests/cxx20_coroutine_expression_spills_host.c" \
        -o "$arch_out/coroutine_host.o"
    gcc "${flags[@]}" -O2 -g -ffunction-sections -fdata-sections \
        -c "$rcc_root/tests/cxx20_coroutine_runtime_host_allocator.c" \
        -o "$arch_out/host_allocator.o"
    gcc "${flags[@]}" -O2 -g -ffunction-sections -fdata-sections \
        -c "$input_assembly" -o "$arch_out/generated_coroutines.o"
    g++ "${flags[@]}" -O2 -no-pie -Wl,--gc-sections \
        -o "$arch_out/coroutine-expression-spills-host" \
        "$arch_out/coroutine_host.o" \
        "$arch_out/generated_coroutines.o" "$arch_out/rincrt_host.o" \
        "$arch_out/new_runtime_host.o" "$arch_out/host_allocator.o"

    "$arch_out/coroutine-expression-spills-host"
}

build_and_run i686 "$out/verified-x86.s" -m32
build_and_run x86_64 "$out/verified-x64.s"
