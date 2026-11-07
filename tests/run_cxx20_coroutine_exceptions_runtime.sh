#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

rcc_root=$(cd -- "$(dirname -- "$0")/.." && pwd)
rinos_root=$(cd -- "$rcc_root/../../.." && pwd)
sdk_root="$rinos_root/public-base/RinOS-SDK"
libc_root="$rinos_root/public-base/libs/libc"
libcxx_root="$rinos_root/public-base/libs/libcxx"
out="$rcc_root/build/tests/cxx20-coroutine-exceptions-runtime"
mkdir -p "$out"

gcc -O2 -g -ffunction-sections -fdata-sections -fno-builtin \
    -DRINCRT_HOST_TEST -I"$sdk_root/include" \
    -c "$sdk_root/src/rincrt_bootstrap_v1.c" -o "$out/rincrt_host.o"
gcc -O2 -g -ffunction-sections -fdata-sections -fno-builtin \
    -I"$sdk_root/include" -I"$libc_root/include" -I"$libcxx_root" \
    -c "$libcxx_root/new_runtime.c" -o "$out/new_runtime_host.o"
gcc -O2 -g -ffunction-sections -fdata-sections \
    -c "$rcc_root/tests/cxx20_coroutine_exceptions_host.c" \
    -o "$out/coroutine_host.o"
gcc -O2 -g -ffunction-sections -fdata-sections \
    -c "$rcc_root/tests/cxx20_coroutine_runtime_host_allocator.c" \
    -o "$out/host_allocator.o"
gcc -O2 -g -ffunction-sections -fdata-sections \
    -c "$out/verified-x64.s" -o "$out/generated_coroutines.o"
g++ -O2 -no-pie -Wl,--gc-sections -o "$out/coroutine-exceptions-host" \
    "$out/coroutine_host.o" \
    "$out/generated_coroutines.o" "$out/rincrt_host.o" \
    "$out/new_runtime_host.o" "$out/host_allocator.o"

"$out/coroutine-exceptions-host"
