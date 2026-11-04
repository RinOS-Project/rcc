#!/bin/sh
set -eu

out=${1:?expected build output directory}
exception_runtime_i686=${2:?expected i686 exception runtime object}
exception_runtime_x64=${3:?expected x86-64 exception runtime object}

build_and_run() {
    arch=$1
    start=$2
    name=$3
    if [ "$arch" = x86 ]; then
        gcc -m32 -c -o "$out/$name-$arch.o" "$out/$name-$arch.s"
        gcc -m32 -c -o "$out/$name-start-$arch.o" "$start"
        gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
            -o "$out/$name-$arch" \
            "$out/$name-start-$arch.o" "$out/$name-$arch.o" \
            "$exception_runtime_i686"
    else
        gcc -c -o "$out/$name-$arch.o" "$out/$name-$arch.s"
        gcc -c -o "$out/$name-start-$arch.o" "$start"
        gcc -nostdlib -static -no-pie -Wl,--entry=_start \
            -o "$out/$name-$arch" \
            "$out/$name-start-$arch.o" "$out/$name-$arch.o" \
            "$exception_runtime_x64"
    fi
    "$out/$name-$arch"
}

build_and_run x86 tests/cxx_new_delete_i686_start.s new-delete
build_and_run x64 tests/cxx_new_delete_x64_start.s new-delete
build_and_run x86 tests/cxx_member_methods_i686_start.s adl
build_and_run x64 tests/cxx_member_methods_x64_start.s adl
build_and_run x86 tests/cxx_member_methods_i686_start.s virtual-dispatch
build_and_run x64 tests/cxx_member_methods_x64_start.s virtual-dispatch
build_and_run x86 tests/cxx_member_methods_i686_start.s field-initializers
build_and_run x64 tests/cxx_member_methods_x64_start.s field-initializers
build_and_run x86 tests/cxx_member_methods_i686_start.s inheritance
build_and_run x64 tests/cxx_member_methods_x64_start.s inheritance
