#!/usr/bin/env bash
set -euo pipefail

host_cc=${CC:-gcc}
host_cxx=${CXX:-g++}
out=build/tests/ci-nested-class-templates
mkdir -p "$out"

extract_itanium_symbols() {
	sed -n 's/^\([_A-Za-z][_A-Za-z0-9.$]*\):$/\1/p' "$1" |
		grep '^_Z' |
		sort -u
}

compare_symbols() {
	local gcc_asm=$1
	local rcc_asm=$2
	local label=$3
	local expected="$out/$label.gcc-symbols"
	local actual="$out/$label.rcc-symbols"

	extract_itanium_symbols "$gcc_asm" > "$expected"
	extract_itanium_symbols "$rcc_asm" > "$actual"
	if [[ ! -s "$expected" ]]; then
		printf 'GCC emitted no Itanium symbols for %s\n' "$label" >&2
		exit 1
	fi
	diff -u "$expected" "$actual"
}

compare_symbols_subset() {
	local gcc_asm=$1
	local rcc_asm=$2
	local label=$3
	local expected="$out/$label.gcc-symbols"
	local actual="$out/$label.rcc-symbols"
	local missing="$out/$label.missing-symbols"

	extract_itanium_symbols "$gcc_asm" > "$expected"
	extract_itanium_symbols "$rcc_asm" > "$actual"
	if [[ ! -s "$expected" ]]; then
		printf 'GCC emitted no Itanium symbols for %s\n' "$label" >&2
		exit 1
	fi
	# RCC currently emits unused members for an instantiated class template;
	# every symbol GCC does emit must nevertheless match exactly.
	comm -23 "$expected" "$actual" > "$missing"
	if [[ -s "$missing" ]]; then
		printf 'RCC is missing GCC Itanium symbols for %s:\n' "$label" >&2
		sed -n '1,40p' "$missing" >&2
		exit 1
	fi
}

expect_rcc_failure() {
	local source=$1
	local target=$2
	local expected=$3
	local label=$4
	local object="$out/$label-$$.ro"
	local log="$out/$label-$$.log"

	if ./rcc++ --target "$target" -std=c++20 -c -o "$object" "$source" >"$log" 2>&1; then
		printf 'rcc++ unexpectedly accepted %s for %s\n' "$source" "$target" >&2
		exit 1
	fi
	grep -F -q "$expected" "$log"
	if [[ -e "$object" ]]; then
		printf 'rcc++ emitted an object for rejected input %s\n' "$source" >&2
		exit 1
	fi
}

expect_gcc_failure() {
	local source=$1
	local expected=$2
	local label=$3
	local log="$out/$label-gcc-$$.log"

	if "$host_cxx" -std=c++20 -fsyntax-only "$source" >"$log" 2>&1; then
		printf 'GCC unexpectedly accepted %s\n' "$source" >&2
		exit 1
	fi
	grep -F -q "$expected" "$log"
}

sources=(
	tests/cxx_nested_class_templates.cpp
	tests/cxx_nested_class_template_specializations.cpp
	tests/cxx_nested_class_template_template_owner_specialization.cpp
	tests/cxx_nested_class_template_owner_partial_specialization.cpp
	tests/cxx_nested_class_template_owner_structural_value_pack.cpp
	tests/cxx_structural_nonnull_pointer_nttp.cpp
	tests/cxx_structural_computed_pointer_nttp.cpp
	tests/cxx_structural_member_pointer_nttp.cpp
	tests/cxx_structural_member_function_pointer_nttp.cpp
	tests/cxx_nested_explicit_chain_ten_levels.cpp
	tests/cxx_nested_explicit_chain_twelve_levels.cpp
	tests/cxx_nested_explicit_chain_thirteen_levels.cpp
)

for source in "${sources[@]}"; do
	name=${source##*/}
	name=${name%.cpp}
	dir="$out/$name"
	mkdir -p "$dir"

	./rcc++ --target i686-unknown-rinos -std=c++20 -S \
		-o "$dir/rcc-x86.s" "$source"
	"$host_cc" -m32 -c -o "$dir/rcc-x86.o" "$dir/rcc-x86.s"
	./rcc++ --target x86_64-unknown-rinos -std=c++20 -S \
		-o "$dir/rcc-x64.s" "$source"
	"$host_cc" -no-pie -Wl,-z,noexecstack -o "$dir/rcc-x64" "$dir/rcc-x64.s"
	"$dir/rcc-x64"

	"$host_cxx" -std=c++20 -Wall -Wextra -Werror -S \
		-o "$dir/gcc-x64.s" "$source"
	"$host_cxx" -m32 -std=c++20 -Wall -Wextra -Werror -S \
		-o "$dir/gcc-x86.s" "$source"
	"$host_cxx" -std=c++20 -Wall -Wextra -Werror \
		-o "$dir/gcc-host" "$source"
	"$dir/gcc-host"

	compare_symbols "$dir/gcc-x86.s" "$dir/rcc-x86.s" "$name-x86"
	compare_symbols "$dir/gcc-x64.s" "$dir/rcc-x64.s" "$name-x64"
	printf 'nested class template regression passed: %s\n' "$source"
done

nested_array_source=tests/cxx_structural_pointer_nested_array_nttp.cpp
nested_array_dir="$out/cxx_structural_pointer_nested_array_nttp"
mkdir -p "$nested_array_dir"
./rcc++ --target i686-unknown-rinos -std=c++20 -S \
	-o "$nested_array_dir/rcc-x86.s" "$nested_array_source"
"$host_cc" -m32 -c -o "$nested_array_dir/rcc-x86.o" \
	"$nested_array_dir/rcc-x86.s"
./rcc++ --target x86_64-unknown-rinos -std=c++20 -S \
	-o "$nested_array_dir/rcc-x64.s" "$nested_array_source"
"$host_cc" -no-pie -Wl,-z,noexecstack \
	-o "$nested_array_dir/rcc-x64" "$nested_array_dir/rcc-x64.s"
"$nested_array_dir/rcc-x64"
"$host_cxx" -m32 -std=c++20 -Wall -Wextra -Werror -S \
	-o "$nested_array_dir/gcc-x86.s" "$nested_array_source"
"$host_cxx" -std=c++20 -Wall -Wextra -Werror -S \
	-o "$nested_array_dir/gcc-x64.s" "$nested_array_source"
"$host_cxx" -std=c++20 -Wall -Wextra -Werror \
	-o "$nested_array_dir/gcc-host" "$nested_array_source"
"$nested_array_dir/gcc-host"
compare_symbols_subset "$nested_array_dir/gcc-x86.s" \
	"$nested_array_dir/rcc-x86.s" nested-array-pointer-x86
compare_symbols_subset "$nested_array_dir/gcc-x64.s" \
	"$nested_array_dir/rcc-x64.s" nested-array-pointer-x64

aggregate_source=tests/cxx_structural_pointer_aggregate_member_nttp.cpp
aggregate_dir="$out/cxx_structural_pointer_aggregate_member_nttp"
mkdir -p "$aggregate_dir"
./rcc++ --target i686-unknown-rinos -std=c++20 -S \
	-o "$aggregate_dir/rcc-x86.s" "$aggregate_source"
"$host_cc" -m32 -c -o "$aggregate_dir/rcc-x86.o" \
	"$aggregate_dir/rcc-x86.s"
./rcc++ --target x86_64-unknown-rinos -std=c++20 -S \
	-o "$aggregate_dir/rcc-x64.s" "$aggregate_source"
"$host_cc" -no-pie -Wl,-z,noexecstack \
	-o "$aggregate_dir/rcc-x64" "$aggregate_dir/rcc-x64.s"
"$aggregate_dir/rcc-x64"
"$host_cxx" -m32 -std=c++20 -Wall -Wextra -Werror -S \
	-o "$aggregate_dir/gcc-x86.s" "$aggregate_source"
"$host_cxx" -std=c++20 -Wall -Wextra -Werror -S \
	-o "$aggregate_dir/gcc-x64.s" "$aggregate_source"
"$host_cxx" -std=c++20 -Wall -Wextra -Werror \
	-o "$aggregate_dir/gcc-host" "$aggregate_source"
"$aggregate_dir/gcc-host"
compare_symbols_subset "$aggregate_dir/gcc-x86.s" \
	"$aggregate_dir/rcc-x86.s" aggregate-member-pointer-x86
compare_symbols_subset "$aggregate_dir/gcc-x64.s" \
	"$aggregate_dir/rcc-x64.s" aggregate-member-pointer-x64

nested_array_invalid=tests/cxx_structural_pointer_nested_array_bounds_invalid.cpp
for target in i686-unknown-rinos x86_64-unknown-rinos; do
	expect_rcc_failure "$nested_array_invalid" "$target" \
		"class template non-type argument must be an integer constant expression" \
		"nested-array-out-of-bounds-$target"
	expect_rcc_failure \
		tests/cxx_structural_pointer_nested_array_decay_bounds_invalid.cpp \
		"$target" \
		"class template non-type argument must be an integer constant expression" \
		"nested-array-decay-out-of-bounds-$target"
	expect_rcc_failure \
		tests/cxx_structural_pointer_nested_array_pointer_bounds_invalid.cpp \
		"$target" \
		"class template non-type argument must be an integer constant expression" \
		"pointer-to-array-out-of-bounds-$target"
done
printf 'nested structural pointer array regression passed\n'

private_source=tests/cxx_nested_class_templates_access_invalid.cpp
for target in i686-unknown-rinos x86_64-unknown-rinos; do
	label="private-access-$target"
	expect_rcc_failure "$private_source" "$target" \
		"nested class template 'Hidden' is inaccessible" "$label-hidden"
	expect_rcc_failure "$private_source" "$target" \
		"nested class template 'HiddenLeaf' is inaccessible" "$label-hidden-leaf"
done
expect_gcc_failure "$private_source" \
	"is private within this context" private-access

owner_source=tests/cxx_nested_member_explicit_specialization_missing_owner_header_invalid.cpp
for target in i686-unknown-rinos x86_64-unknown-rinos; do
	expect_rcc_failure "$owner_source" "$target" \
		"nested member-template explicit specialization requires an explicitly specialized class-template owner" \
		"missing-owner-$target"
done
expect_gcc_failure "$owner_source" "too few template-parameter-lists" missing-owner

printf 'nested class template POSIX CI gate passed\n'
