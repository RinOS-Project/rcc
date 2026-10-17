#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/rcc" >&2
    exit 2
fi

rcc=$1
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output_dir=$(mktemp -d "${TMPDIR:-/tmp}/rcc-c23-empty-init.XXXXXX")
trap 'rm -rf "$output_dir"' EXIT HUP INT TERM

if "$rcc" --target x86_64-unknown-rinos -std=c17 -c \
    -o "$output_dir/c17.ro" "$test_dir/empty_initializer.c" \
    >"$output_dir/c17.log" 2>&1; then
    echo "rcc accepted the C23 empty initializer in C17 mode" >&2
    exit 1
fi
if ! grep -q "empty initializer list is not valid C17" "$output_dir/c17.log"; then
    cat "$output_dir/c17.log" >&2
    exit 1
fi

for target in i686-unknown-rinos x86_64-unknown-rinos; do
    output_name=$(printf '%s' "$target" | tr - _)
    "$rcc" --target "$target" -std=c23 -c \
        -o "$output_dir/$output_name.ro" \
        "$test_dir/empty_initializer.c"
done

echo "RCC C23 empty initializer test passed"
