#!/usr/bin/env bash
set -eu

ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
RCC="$ROOT/rcc.exe"
RLD="$ROOT/rld.exe"
BOOTSTRAP="$ROOT/build/bootstrap"
WIN_ROOT=$(wslpath -m "$ROOT")
WIN_BOOTSTRAP="$WIN_ROOT/build/bootstrap"
cd -- "$ROOT"
INCLUDES=(-nostdinc -Ibootstrap/include -Iinclude -I../../RinOS-SDK/include)

CORE_SOURCES=(
    src/ast.c src/symtab.c src/lexer.c src/sema.c src/parser.c
    src/ir.c src/ir_pass.c src/mir.c src/mir_alloc.c src/mir_phi.c
    src/x86_abi.c src/x86_select.c src/x86_legalize.c src/x86_encode.c
    src/x86_object.c src/x86_pipeline.c src/verified_codegen.c
    src/ir_lower.c src/optimize.c src/codegen.c src/codegen64.c
    src/preproc.c src/driver_policy.c src/emit_asm.c src/emit_ro.c
    src/emit_rin.c src/emit_rll.c src/emit_drv.c src/archive.c src/linker.c
    src/ast_cxx.c src/parser_cxx.c src/build_manifest.c src/utils.c src/main.c
    src/main_cxx.c src/main_rld.c src/main_rar.c
)

RCC_OBJECTS=(
    utils lexer parser ast symtab sema codegen codegen64 preproc ir ir_pass
    mir mir_alloc mir_phi x86_abi x86_select x86_legalize x86_encode
    x86_object x86_pipeline verified_codegen ir_lower optimize emit_rin emit_rll
    emit_drv emit_ro emit_asm ast_cxx parser_cxx build_manifest driver_policy main
)

RUNTIME_FUNCTIONS=(
    __errno_location __rin_stderr _exit atexit atoi getenv close execvp exit
    fclose feof ferror fgets fopen fork fprintf fputc fputs fread free fseek
    ftell fwrite isalnum isalpha isdigit isspace isxdigit malloc memchr memcmp
    memcpy memmove memset mkstemp perror printf qsort realloc remove rename
    snprintf strchr strcmp strcpy strlen strcat strncat strncmp strncpy strrchr
    strstr strtod strtol strtoul strtoll strtoull tolower vfprintf vsnprintf
    waitpid setjmp longjmp time gmtime localtime rin_cpp_exception_install
    rin_cpp_exception_leave rin_cpp_exception_throw rin_cpp_exception_rethrow
    rin_cpp_exception_throw_object rin_cpp_exception_throw_object_with_cleanup
    rin_cpp_exception_rethrow_frame rin_cpp_exception_release_frame
    rin_cpp_exception_register_cleanup rin_cpp_exception_unregister_cleanup
    rin_cpp_exception_unwind_cleanups
)

runtime_imports=()
for symbol in "${RUNTIME_FUNCTIONS[@]}"; do
    runtime_imports+=(--import "${symbol}=rincrt.rll@function")
done

compile_core() {
    mkdir -p "$BOOTSTRAP/stage1-a" "$BOOTSTRAP/stage1-b"
    for target in i686-unknown-rinos x86_64-unknown-rinos; do
        arch=${target%%-*}
        for source in "${CORE_SOURCES[@]}"; do
            name=${source##*/}
            name=${name%.c}
            "$RCC" --target "$target" "${INCLUDES[@]}" -c \
                -o "$WIN_BOOTSTRAP/stage1-a/$name-$arch.ro" "$source"
            "$RCC" --target "$target" "${INCLUDES[@]}" -c \
                -o "$WIN_BOOTSTRAP/stage1-b/$name-$arch.ro" "$source"
            cmp "$BOOTSTRAP/stage1-a/$name-$arch.ro" \
                "$BOOTSTRAP/stage1-b/$name-$arch.ro"
        done
    done
}

link_stage1() {
    mkdir -p "$BOOTSTRAP/images"
    for target in i686-unknown-rinos x86_64-unknown-rinos; do
        arch=${target%%-*}
        for stage in stage1-a stage1-b; do
            objects=()
            for name in "${RCC_OBJECTS[@]}"; do
                objects+=("$WIN_BOOTSTRAP/$stage/$name-$arch.ro")
            done
            "$RLD" --target "$target" --emit-unsigned-v3 \
                --dep rincrt.rll "${runtime_imports[@]}" \
                -o "$WIN_BOOTSTRAP/images/rcc-$stage-$arch.rin" \
                "${objects[@]}"
        done
        cmp "$BOOTSTRAP/images/rcc-stage1-a-$arch.rin" \
            "$BOOTSTRAP/images/rcc-stage1-b-$arch.rin"
    done
}

execute_stage1() {
    mkdir -p "$BOOTSTRAP/execute"
    gcc -m32 -g -O2 -I"$ROOT/include" -I"$ROOT/../../RinOS-SDK/include" -rdynamic \
        -o "$BOOTSTRAP/execute/run-i686" \
        "$ROOT/tests/bootstrap_stage_runner.c" -ldl
    gcc -g -O2 -I"$ROOT/include" -I"$ROOT/../../RinOS-SDK/include" -rdynamic \
        -o "$BOOTSTRAP/execute/run-x86_64" \
        "$ROOT/tests/bootstrap_stage_runner.c" -ldl
    "$RCC" --target i686-unknown-rinos -c \
        -o "$WIN_BOOTSTRAP/execute/reference-i686.ro" tests/hello.c
    "$RCC" --target x86_64-unknown-rinos -c \
        -o "$WIN_BOOTSTRAP/execute/reference-x86_64.ro" tests/hello.c
    "$BOOTSTRAP/execute/run-i686" \
        "$BOOTSTRAP/images/rcc-stage1-a-i686.rin" rcc-stage1 \
        --target i686-unknown-rinos -c \
        -o "$BOOTSTRAP/execute/stage1-i686.ro" tests/hello.c
    cmp "$BOOTSTRAP/execute/reference-i686.ro" \
        "$BOOTSTRAP/execute/stage1-i686.ro"
    "$BOOTSTRAP/execute/run-x86_64" \
        "$BOOTSTRAP/images/rcc-stage1-a-x86_64.rin" rcc-stage1 \
        --target x86_64-unknown-rinos -c \
        -o "$BOOTSTRAP/execute/stage1-x86_64.ro" tests/hello.c
    cmp "$BOOTSTRAP/execute/reference-x86_64.ro" \
        "$BOOTSTRAP/execute/stage1-x86_64.ro"
}

rebuild_stage2() {
    mkdir -p "$BOOTSTRAP/stage2"
    for target in i686-unknown-rinos x86_64-unknown-rinos; do
        arch=${target%%-*}
        runner="$BOOTSTRAP/execute/run-x86_64"
        image="$BOOTSTRAP/images/rcc-stage1-a-x86_64.rin"
        for source in "${CORE_SOURCES[@]}"; do
            name=${source##*/}
            name=${name%.c}
            "$runner" "$image" rcc-stage1 --target "$target" \
                "${INCLUDES[@]}" -c \
                -o "$BOOTSTRAP/stage2/$name-$arch.ro" "$source"
            cmp "$BOOTSTRAP/stage1-a/$name-$arch.ro" \
                "$BOOTSTRAP/stage2/$name-$arch.ro"
        done
        objects=()
        for name in "${RCC_OBJECTS[@]}"; do
            objects+=("$WIN_BOOTSTRAP/stage2/$name-$arch.ro")
        done
        "$RLD" --target "$target" --emit-unsigned-v3 \
            --dep rincrt.rll "${runtime_imports[@]}" \
            -o "$WIN_BOOTSTRAP/images/rcc-stage2-$arch.rin" \
            "${objects[@]}"
        cmp "$BOOTSTRAP/images/rcc-stage1-a-$arch.rin" \
            "$BOOTSTRAP/images/rcc-stage2-$arch.rin"
    done
}

case "${1:-}" in
    core) compile_core ;;
    link) link_stage1 ;;
    execute) execute_stage1 ;;
    stage2) rebuild_stage2 ;;
    *)
        echo "usage: $0 {core|link|execute|stage2}" >&2
        exit 2
        ;;
esac
