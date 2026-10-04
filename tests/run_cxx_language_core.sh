#!/bin/sh
set -eu

out=${1:?expected build output directory}

build_and_run() {
    arch=$1
    start=$2
    name=$3
    if [ "$arch" = x86 ]; then
        gcc -m32 -c -o "$out/$name-$arch.o" "$out/$name-$arch.s"
        gcc -m32 -c -o "$out/$name-start-$arch.o" "$start"
        gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
            -o "$out/$name-$arch" \
            "$out/$name-start-$arch.o" "$out/$name-$arch.o"
    else
        gcc -c -o "$out/$name-$arch.o" "$out/$name-$arch.s"
        gcc -c -o "$out/$name-start-$arch.o" "$start"
        gcc -nostdlib -static -no-pie -Wl,--entry=_start \
            -o "$out/$name-$arch" \
            "$out/$name-start-$arch.o" "$out/$name-$arch.o"
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
