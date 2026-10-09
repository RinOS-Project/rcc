# SPDX-License-Identifier: Apache-2.0
# RCC/RCC++ - RinOS C/C++ Compiler
# Makefile

# Respect CI/user-selected host compilers while keeping GCC as the native
# default when GNU make only supplies its built-in `cc` value.
ifeq ($(origin CC),default)
CC = gcc
endif
CFLAGS = -Wall -Wextra -std=c11 -g -O2 -MMD -MP \
	-I$(RINOS_SDK_ROOT)/include
LDFLAGS =
OBJCOPY ?= objcopy
comma = ,

ifeq ($(OS),Windows_NT)
# Use the ISO printf implementation so the C11 `%z` length modifier remains
# valid when RCC itself is built with MinGW's Windows CRT headers.
CFLAGS += -D__USE_MINGW_ANSI_STDIO=1
EXE_SUFFIX = .exe
else
EXE_SUFFIX =
endif

# Directories
SRCDIR = src
INCDIR = include
ifeq ($(OS),Windows_NT)
HOST_BUILD_TAG ?= windows
else
HOST_BUILD_TAG ?= posix
endif
# Object files and GCC dependency files contain host-specific format and
# paths. Keep native Windows and POSIX/WSL builds in separate ignored output
# directories so one checkout can be reused without cross-host make parsing
# or linker contamination.
OBJDIR ?= obj/$(HOST_BUILD_TAG)
BINDIR = .
RINOS_ROOT ?= ..
RINOS_SDK_ROOT ?= ../../RinOS-SDK
RINGPU_ROOT ?= ../../libs/RinGPU
TEST_OUT = build/tests
SIGN_TEST_DIR = $(TEST_OUT)/signing
SANITIZER_ROOT = build/sanitizers
BOOTSTRAP_ROOT = build/bootstrap
ifeq ($(OS),Windows_NT)
# Override this when the checkout is mounted at a different WSL path.
WSL_RINCOMPILER_ROOT ?= /mnt/e/RinOS/public-base/toolchain/RinCompiler
WINDOWS_TEST_OUT = $(subst /,\,$(TEST_OUT))
CHECK_INIT_ARRAY = findstr /c:".section .init_array"
CHECK_INIT_ARRAY_FILE = $(CHECK_INIT_ARRAY) $(subst /,\,$(1))
CHECK_FINI_ARRAY = findstr /c:".section .fini_array"
CHECK_FINI_ARRAY_FILE = $(CHECK_FINI_ARRAY) $(subst /,\,$(1))
CHECK_TEXT = findstr /c:"$(1)" "$(subst /,\,$(2))" >NUL
CHECK_TEXT_ABSENT = powershell -NoProfile -Command "$$text=Get-Content -Raw -LiteralPath '$(2)'; if ($$text.Contains('$(1)')) { exit 1 }"
GREP = powershell -NoProfile -File "$(CURDIR)/scripts/rcc_grep.ps1"
else
CHECK_INIT_ARRAY = $(GREP) -F -q ".section .init_array"
CHECK_INIT_ARRAY_FILE = $(CHECK_INIT_ARRAY) $(1)
CHECK_FINI_ARRAY = $(GREP) -F -q ".section .fini_array"
CHECK_FINI_ARRAY_FILE = $(CHECK_FINI_ARRAY) $(1)
CHECK_TEXT = $(GREP) -F -q "$(1)" "$(2)"
CHECK_TEXT_ABSENT = ! $(GREP) -F -q '$(1)' '$(2)'
GREP = grep
endif

ifeq ($(OS),Windows_NT)
DATE_TIME_ENV = set SOURCE_DATE_EPOCH=0&&
DATE_TIME_INVALID = set SOURCE_DATE_EPOCH=not-a-timestamp&& $(call EXPECT_FAILURE,$(RCC_TARGET) -E tests/preprocessor_date_time.c,$(TEST_OUT)/preprocessor-date-time/invalid.log)
else
DATE_TIME_ENV = SOURCE_DATE_EPOCH=0
DATE_TIME_INVALID = if SOURCE_DATE_EPOCH=not-a-timestamp $(RCC_TARGET) -E tests/preprocessor_date_time.c >$(TEST_OUT)/preprocessor-date-time/invalid.log 2>&1; then exit 1; fi
endif

ifeq ($(OS),Windows_NT)
define CHECK_BINARY_STRING
strings $(2) > $(2).strings
$(GREP) -F -x -q '$(1)' $(2).strings
endef
else
define CHECK_BINARY_STRING
strings $(2) | $(GREP) -F -x -q '$(1)'
endef
endif

ifeq ($(OS),Windows_NT)
MKDIR_P = if not exist "$(1)\." mkdir "$(1)"
# cmd.exe treats forward slashes in an executable path as option syntax. Make
# Windows expected-failure invocations use native separators before launching.
EXPECT_FAILURE = $(subst /,\,$(subst ./,,$(1))) >$(subst /,\,$(2)) 2>&1 & if not errorlevel 1 exit /b 1
CHECK_NONEMPTY = powershell -NoProfile -Command "if (-not (Test-Path -LiteralPath '$(1)') -or (Get-Item -LiteralPath '$(1)').Length -eq 0) { exit 1 }"
COPY_FILE = powershell -NoProfile -Command "Copy-Item -LiteralPath '$(1)' -Destination '$(2)' -Force"
ASSERT_ABSENT = powershell -NoProfile -Command "if (Test-Path -LiteralPath '$(1)') { exit 1 }"
CHECK_NO_SIGN_TEMP = powershell -NoProfile -Command "$$bad=Get-ChildItem -LiteralPath '$(1)' -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $$_.Name -like '*.rcc-unsigned-*' -or $$_.Name -like '*.rld-unsigned-*' -or $$_.Name -like '*.rcc-signed-*' }; if ($$bad) { exit 1 }"
define PARALLEL_SIGNING
powershell -NoProfile -Command "$$a=Start-Process -FilePath '$(RCC_TARGET)' -ArgumentList @('--target','i686-unknown-rinos','--sign-profile','debug','--python','python3','--rinsign','tests/fake_rinsign.py','--sign-key','tests/signing_test_private.key','--public-key','tests/signing_test_public.der','-o','$(SIGN_TEST_DIR)/parallel.rin','tests/hello.c') -PassThru; $$b=Start-Process -FilePath '$(RCC_TARGET)' -ArgumentList @('--target','i686-unknown-rinos','--sign-profile','debug','--python','python3','--rinsign','tests/fake_rinsign.py','--sign-key','tests/signing_test_private.key','--public-key','tests/signing_test_public.der','-o','$(SIGN_TEST_DIR)/parallel.rin','tests/hello.c') -PassThru; Wait-Process -Id $$a.Id,$$b.Id; $$a.Refresh(); $$b.Refresh(); if ($$a.ExitCode -ne 0 -or $$b.ExitCode -ne 0) { exit 1 }"
endef
# Native MinGW installations commonly provide only the host CRT.  The
# verifier still parses both x86 and x64 objects and executes the native x64
# object, so do not make the whole production gate depend on unavailable
# 32-bit Windows CRT libraries.
VERIFIED_BACKEND_X86_HOST_CFLAGS = $(CFLAGS)
COMPARE_FILES = powershell -NoProfile -File "$(CURDIR)/scripts/rcc_compare_files.ps1" "$(1)" "$(2)"
CHECK_COUNT = powershell -NoProfile -File "$(CURDIR)/scripts/rcc_expect_count.ps1" "$(3)" "$(1)" "$(2)"
else
MKDIR_P = mkdir -p "$(1)"
EXPECT_FAILURE = $(1) >$(2) 2>&1; test $$? -ne 0
CHECK_NONEMPTY = test -s "$(1)"
VERIFIED_BACKEND_X86_HOST_CFLAGS = -m32 $(CFLAGS)
COMPARE_FILES = cmp "$(1)" "$(2)"
COPY_FILE = cp "$(1)" "$(2)"
ASSERT_ABSENT = test ! -e "$(1)"
CHECK_NO_SIGN_TEMP = test -z "$$(find "$(1)" -type f \( -name '*.rcc-unsigned-*' -o -name '*.rld-unsigned-*' -o -name '*.rcc-signed-*' \) -print -quit)"
define PARALLEL_SIGNING
$(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug --python python3 --rinsign tests/fake_rinsign.py --sign-key tests/signing_test_private.key --public-key tests/signing_test_public.der -o "$(SIGN_TEST_DIR)/parallel.rin" tests/hello.c & first=$$!; $(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug --python python3 --rinsign tests/fake_rinsign.py --sign-key tests/signing_test_private.key --public-key tests/signing_test_public.der -o "$(SIGN_TEST_DIR)/parallel.rin" tests/hello.c & second=$$!; wait $$first; wait $$second
endef
CHECK_COUNT = test "$$($(GREP) -F -c '$(1)' '$(2)')" -eq $(3)
endif

ifeq ($(OS),Windows_NT)
# Native Windows MinGW lacks the 32-bit CRT in this checkout.  Build the
# object-structure verifier for the host and make its inspect-only mode
# explicit; the x64 runner below still executes the generated code fully.
CXX_CLEANUP_X86_BUILD = $(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/optimize/cxx-cleanup-run-x86 tests/cxx_value_init_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
CXX_CLEANUP_X86_RUN = $(TEST_OUT)/optimize/cxx-cleanup-run-x86 $(TEST_OUT)/optimize/cxx-cleanup-x86.ro --inspect-only
else
CXX_CLEANUP_X86_BUILD = $(CC) -m32 $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/optimize/cxx-cleanup-run-x86 tests/cxx_value_init_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
CXX_CLEANUP_X86_RUN = $(TEST_OUT)/optimize/cxx-cleanup-run-x86 $(TEST_OUT)/optimize/cxx-cleanup-x86.ro
endif

ifeq ($(OS),Windows_NT)
# The IR/MIR unit fixtures exercise both target policies internally.  Native
# Windows has no 32-bit MinGW CRT, so compile their host-side assertions with
# the available CRT and keep i686 machine-code validation in the explicit
# inspect-only encoder invocation below.
IR_X86_HOST_FLAGS =
IR_X86_INSPECT_CMD = $(TEST_OUT)/x86_encode_run_test-x86 --inspect-i686 \
	$(TEST_OUT)/encoded-native-x86.ro
else
IR_X86_HOST_FLAGS = -m32
IR_X86_INSPECT_CMD =
endif

# Host-side compiler-builtin fixtures execute the assembly produced by RCC.
# These fixtures intentionally use no CRT/API symbols, so native Windows
# MinGW can link them as freestanding 32-bit PE images even when its 32-bit
# CRT is not installed.  An unresolved compiler-generated reference remains
# a link error; this is not an inspect-only or skipped execution path.
ifeq ($(OS),Windows_NT)
define RUN_COMPILER_BUILTINS_X86
$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' -o $(1) $(2)
$(1)
endef
else
define RUN_COMPILER_BUILTINS_X86
$(CC) -m32 -no-pie -o $(1) $(2)
$(1)
endef
endif

ifeq ($(OS),Windows_NT)
# Windows has no RinOS int 0x80/syscall runtime.  Compile the i686 image to a
# PE object for target/ABI verification, then execute the x64 image through a
# real native CRT adapter that calls the generated main function.
define CXX_WINDOWS_MAIN
$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/$(2)-x86.s tests/$(3)
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/$(2)-x86.o $(TEST_OUT)/$(1)/$(2)-x86.s
objdump -f $(TEST_OUT)/$(1)/$(2)-x86.o > $(TEST_OUT)/$(1)/$(2)-x86-arch.log
$(GREP) -F -q "pe-i386" $(TEST_OUT)/$(1)/$(2)-x86-arch.log
$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/$(2)-x64.s tests/$(3)
$(CC) -c -o $(TEST_OUT)/$(1)/$(2)-x64.o $(TEST_OUT)/$(1)/$(2)-x64.s
$(OBJCOPY) --redefine-sym main=rcc_test_main $(TEST_OUT)/$(1)/$(2)-x64.o
$(CC) $(CFLAGS) -o $(TEST_OUT)/$(1)/$(2)-x64-host tests/cxx_language_core_host.c $(TEST_OUT)/$(1)/$(2)-x64.o
$(TEST_OUT)/$(1)/$(2)-x64-host
endef

# Constructor fixtures expose a named C++ function instead of the common
# _rcc_entry ABI.  On Windows, inspect the i686 PE object and execute the
# x86_64 SysV-ABI function through its real native CRT adapter.
define CXX_WINDOWS_CONSTRUCTOR_TEST
$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x86.s tests/$(2)
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
objdump -f $(TEST_OUT)/$(1)/x86.o > $(TEST_OUT)/$(1)/x86-arch.log
$(GREP) -F -q "pe-i386" $(TEST_OUT)/$(1)/x86-arch.log
$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x64.s tests/$(2)
$(CC) -c -o $(TEST_OUT)/$(1)/x64.o $(TEST_OUT)/$(1)/x64.s
$(CC) $(CFLAGS) -o $(TEST_OUT)/$(1)/x64-host tests/$(3) $(TEST_OUT)/$(1)/x64.o
$(TEST_OUT)/$(1)/x64-host
endef

define CXX_WINDOWS_ENTRY_TEST
$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x86.s tests/$(2)
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
objdump -f $(TEST_OUT)/$(1)/x86.o > $(TEST_OUT)/$(1)/x86-arch.log
$(GREP) -F -q "pe-i386" $(TEST_OUT)/$(1)/x86-arch.log
$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x64.s tests/$(2)
$(CC) -c -o $(TEST_OUT)/$(1)/x64.o $(TEST_OUT)/$(1)/x64.s
$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/$(1)/x64.o
$(CC) $(CFLAGS) -o $(TEST_OUT)/$(1)/x64-host tests/cxx_language_core_host.c $(TEST_OUT)/$(1)/x64.o
$(TEST_OUT)/$(1)/x64-host
endef

define CXX_I686_WIDE_MEMBER_INITIALIZER_CHECK
objdump -d -M att $(TEST_OUT)/$(1)/x86.o > $(TEST_OUT)/$(1)/x86-disassembly.log
$(GREP) -F -q "cltd" $(TEST_OUT)/$(1)/x86-disassembly.log
$(GREP) -F -q "ba 00 00 00 00" $(TEST_OUT)/$(1)/x86-disassembly.log
$(GREP) -F -q "ba 01 00 00 00" $(TEST_OUT)/$(1)/x86-disassembly.log
endef

# MinGW represents .weak definitions using synthetic fallback symbols whose
# names include the host entry.  The peer TU's two shared type_info fallbacks
# get distinct names so the native COFF test can exercise weak merging.
define CXX_WINDOWS_ENTRY_TWO_TU_TEST
$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x86.s tests/$(2)
$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x86-peer.s tests/$(3)
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86-peer.o $(TEST_OUT)/$(1)/x86-peer.s
objdump -f $(TEST_OUT)/$(1)/x86.o > $(TEST_OUT)/$(1)/x86-arch.log
$(GREP) -F -q "pe-i386" $(TEST_OUT)/$(1)/x86-arch.log
$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x64.s tests/$(2)
$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x64-peer.s tests/$(3)
$(CC) -c -o $(TEST_OUT)/$(1)/x64.o $(TEST_OUT)/$(1)/x64.s
$(CC) -c -o $(TEST_OUT)/$(1)/x64-peer.o $(TEST_OUT)/$(1)/x64-peer.s
$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/$(1)/x64.o
$(OBJCOPY) --redefine-sym .weak.__rcc_typeinfo_type_T9_0sPT4_0s_name._rcc_entry=.weak.peer_typeinfo_name --redefine-sym .weak.__rcc_typeinfo_type_T9_0sPT4_0s._rcc_entry=.weak.peer_typeinfo $(TEST_OUT)/$(1)/x64-peer.o
$(CC) $(CFLAGS) -o $(TEST_OUT)/$(1)/x64-host tests/cxx_language_core_host.c $(TEST_OUT)/$(1)/x64.o $(TEST_OUT)/$(1)/x64-peer.o
$(TEST_OUT)/$(1)/x64-host
endef

define CXX_POSIX_ENTRY_TEST
$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
	-o $(TEST_OUT)/$(1)/x86.s tests/$(2)
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
	-o $(TEST_OUT)/$(1)/x64.s tests/$(2)
$(CC) -no-pie -o $(TEST_OUT)/$(1)/x64 $(TEST_OUT)/$(1)/x64.s
$(TEST_OUT)/$(1)/x64
endef

define C_WINDOWS_ENTRY_TEST
$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S -o $(TEST_OUT)/$(1)/x86.s tests/$(2)
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
objdump -f $(TEST_OUT)/$(1)/x86.o > $(TEST_OUT)/$(1)/x86-arch.log
$(GREP) -F -q "pe-i386" $(TEST_OUT)/$(1)/x86-arch.log
$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S -o $(TEST_OUT)/$(1)/x64.s tests/$(2)
$(CC) -c -o $(TEST_OUT)/$(1)/x64.o $(TEST_OUT)/$(1)/x64.s
$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/$(1)/x64.o
$(CC) $(CFLAGS) -o $(TEST_OUT)/$(1)/x64-host tests/cxx_language_core_host.c $(TEST_OUT)/$(1)/x64.o
$(TEST_OUT)/$(1)/x64-host
endef

define CXX_WINDOWS_C_RUN_TEST
$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x86.s tests/$(2)
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
objdump -f $(TEST_OUT)/$(1)/x86.o > $(TEST_OUT)/$(1)/x86-arch.log
$(GREP) -F -q "pe-i386" $(TEST_OUT)/$(1)/x86-arch.log
$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x64.s tests/$(2)
$(CC) -c -o $(TEST_OUT)/$(1)/x64.o $(TEST_OUT)/$(1)/x64.s
$(CC) $(CFLAGS) -o $(TEST_OUT)/$(1)/x64-host tests/$(3) $(TEST_OUT)/$(1)/x64.o
$(TEST_OUT)/$(1)/x64-host
endef

define CXX_WINDOWS_CONSTEXPR_TEST
$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x86.s tests/$(2)
$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
objdump -f $(TEST_OUT)/$(1)/x86.o > $(TEST_OUT)/$(1)/x86-arch.log
$(GREP) -F -q "pe-i386" $(TEST_OUT)/$(1)/x86-arch.log
$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/$(1)/x64.s tests/$(2)
$(CC) -c -o $(TEST_OUT)/$(1)/x64.o $(TEST_OUT)/$(1)/x64.s
$(OBJCOPY) --redefine-sym main=rcc_cxx_constexpr_main $(TEST_OUT)/$(1)/x64.o
$(CC) $(CFLAGS) -o $(TEST_OUT)/$(1)/x64-host tests/cxx_constexpr_host.c $(TEST_OUT)/$(1)/x64.o
$(TEST_OUT)/$(1)/x64-host
endef
endif

ifeq ($(OS),Windows_NT)
# Native Windows MinGW installations may not include 32-bit CRT libraries.
# Keep the i686 ABI artifact check, then execute the same generated function
# through the available native x64 CRT.  The x86 object is never silently
# omitted: its PE architecture is checked explicitly before the x64 run.
define AGGREGATE_X86_ABI_TEST
	$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
	objdump -f $(TEST_OUT)/$(1)/x86.o > $(TEST_OUT)/$(1)/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/$(1)/x86-arch.log
endef
else
define AGGREGATE_X86_ABI_TEST
	$(CC) -m32 -c -o $(TEST_OUT)/$(1)/x86.o $(TEST_OUT)/$(1)/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/$(1)/host-x86.o tests/$(2)
	$(CC) -m32 -o $(TEST_OUT)/$(1)/x86 $(TEST_OUT)/$(1)/host-x86.o $(TEST_OUT)/$(1)/x86.o
	$(TEST_OUT)/$(1)/x86
endef
endif

BOOTSTRAP_INCLUDES = -nostdinc -Ibootstrap/include -Iinclude -I$(RINOS_SDK_ROOT)/include
BOOTSTRAP_CORE_SRCS = src/ast.c src/symtab.c src/lexer.c src/sema.c src/parser.c \
                      src/ir.c src/ir_pass.c src/mir.c src/mir_alloc.c \
                      src/mir_phi.c src/x86_abi.c src/x86_select.c \
                      src/x86_legalize.c src/x86_encode.c \
                      src/x86_object.c src/x86_pipeline.c \
                      src/verified_codegen.c \
                      src/ir_lower.c src/optimize.c \
                      src/codegen.c src/codegen64.c \
                      src/preproc.c src/driver_policy.c src/emit_asm.c \
                      src/emit_ro.c src/emit_rin.c src/emit_rll.c \
                      src/emit_drv.c src/archive.c src/linker.c \
                      src/ast_cxx.c src/parser_cxx.c \
                      src/build_manifest.c src/utils.c src/main.c \
                      src/main_cxx.c src/main_rld.c src/main_rar.c
BOOTSTRAP_RCC_OBJECTS = utils lexer parser ast symtab sema codegen codegen64 \
                        preproc ir ir_pass mir mir_alloc mir_phi x86_abi \
                        x86_select x86_legalize x86_encode x86_object \
                        x86_pipeline verified_codegen \
                        ir_lower optimize \
                        emit_rin emit_rll emit_drv emit_ro \
                        emit_asm ast_cxx parser_cxx build_manifest \
                        driver_policy main
BOOTSTRAP_RUNTIME_FUNCTIONS = __errno_location __rin_stderr _exit atexit atoi getenv \
                              close execvp exit fclose feof ferror fgets fopen \
                              fork fprintf fputc fputs fread free fseek ftell \
                              fwrite isalnum isalpha isdigit isspace isxdigit \
                              malloc memchr memcmp memcpy memmove memset mkstemp perror printf \
                              qsort realloc remove rename snprintf strchr strcmp \
                              strcpy strlen strcat strncat strncmp strncpy strrchr strstr strtod \
                              strtol strtoul strtoll strtoull tolower vfprintf vsnprintf waitpid setjmp longjmp \
                              time gmtime localtime \
                              rin_cpp_exception_install rin_cpp_exception_leave \
                              rin_cpp_exception_throw rin_cpp_exception_rethrow \
                              rin_cpp_exception_throw_object \
                              rin_cpp_exception_throw_object_with_cleanup \
                              rin_cpp_exception_rethrow_frame \
                              rin_cpp_exception_release_frame \
                              rin_cpp_exception_register_cleanup \
                              rin_cpp_exception_unregister_cleanup \
                              rin_cpp_exception_unwind_cleanups
BOOTSTRAP_RUNTIME_IMPORTS = $(foreach symbol,$(BOOTSTRAP_RUNTIME_FUNCTIONS),\
                              --import $(symbol)=rincrt.rll@function)

# Common source files (shared between rcc and rcc++)
COMMON_SRCS = $(SRCDIR)/utils.c $(SRCDIR)/lexer.c $(SRCDIR)/parser.c $(SRCDIR)/ast.c \
              $(SRCDIR)/symtab.c $(SRCDIR)/sema.c $(SRCDIR)/codegen.c \
              $(SRCDIR)/codegen64.c $(SRCDIR)/preproc.c \
              $(SRCDIR)/ir.c $(SRCDIR)/ir_pass.c $(SRCDIR)/mir.c \
              $(SRCDIR)/mir_alloc.c $(SRCDIR)/mir_phi.c \
              $(SRCDIR)/x86_abi.c $(SRCDIR)/x86_select.c \
              $(SRCDIR)/x86_legalize.c $(SRCDIR)/x86_encode.c \
              $(SRCDIR)/x86_object.c $(SRCDIR)/x86_pipeline.c \
              $(SRCDIR)/verified_codegen.c \
              $(SRCDIR)/ir_lower.c \
              $(SRCDIR)/optimize.c \
              $(SRCDIR)/emit_rin.c $(SRCDIR)/emit_rll.c $(SRCDIR)/emit_drv.c $(SRCDIR)/emit_ro.c \
              $(SRCDIR)/emit_asm.c $(SRCDIR)/build_manifest.c $(SRCDIR)/driver_policy.c
COMMON_OBJS = $(COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# RCC (C compiler)
RCC_SRCS = $(SRCDIR)/main.c
RCC_OBJS = $(RCC_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# RCC++ (C++ compiler)
RCXX_SRCS = $(SRCDIR)/main_cxx.c $(SRCDIR)/ast_cxx.c $(SRCDIR)/parser_cxx.c
RCXX_OBJS = $(RCXX_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

ifeq ($(OS),Windows_NT)
RCC_TARGET = rcc$(EXE_SUFFIX)
RCXX_TARGET = rcc++$(EXE_SUFFIX)
else
RCC_TARGET = $(BINDIR)/rcc$(EXE_SUFFIX)
RCXX_TARGET = $(BINDIR)/rcc++$(EXE_SUFFIX)
endif

# RLD (Linker) - uses minimal common code
RLD_COMMON_SRCS = $(SRCDIR)/utils.c $(SRCDIR)/emit_ro.c $(SRCDIR)/archive.c $(SRCDIR)/build_manifest.c
RLD_COMMON_OBJS = $(RLD_COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RLD_SRCS = $(SRCDIR)/main_rld.c $(SRCDIR)/linker.c
RLD_OBJS = $(RLD_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
ifeq ($(OS),Windows_NT)
RLD_TARGET = rld$(EXE_SUFFIX)
else
RLD_TARGET = $(BINDIR)/rld$(EXE_SUFFIX)
endif

# Native v3 image validator used by the format audit target.  Keep this as a
# sibling checkout by default, while allowing CI and package builds to point
# at an installed validator.
ifeq ($(OS),Windows_NT)
RINVALIDATE ?= ../rinvalidate/rinvalidate.exe
else
RINVALIDATE ?= ../rinvalidate/rinvalidate
endif

# Aquamarine Shader Language compiler. RSH1 validation is shared with the
# public RinGPU checkout selected by RINGPU_ROOT.
AQC_SRCS = $(SRCDIR)/main_aqc.c $(SRCDIR)/aqc.c
AQC_OBJS = $(AQC_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o) $(OBJDIR)/ringpu_shader.o
ifeq ($(OS),Windows_NT)
AQC_TARGET = aqc$(EXE_SUFFIX)
else
AQC_TARGET = $(BINDIR)/aqc$(EXE_SUFFIX)
endif

# RAR (Archiver) - uses minimal common code
RAR_COMMON_SRCS = $(SRCDIR)/utils.c
RAR_COMMON_OBJS = $(RAR_COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RAR_SRCS = $(SRCDIR)/main_rar.c $(SRCDIR)/archive.c
RAR_OBJS = $(RAR_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
ifeq ($(OS),Windows_NT)
RAR_TARGET = rar$(EXE_SUFFIX)
else
RAR_TARGET = $(BINDIR)/rar$(EXE_SUFFIX)
endif

# Keep object/header dependencies in the build tree so a changed public
# header can never leave incompatible compiler objects mixed together.
-include $(wildcard $(OBJDIR)/*.d)

.PHONY: all clean build-rcc build-rcxx build-rld build-rar test-cxx test-cxx-cli test-cxx-language-core test-cxx-multiple-inheritance-virtual test-cxx-secondary-virtual-override test-cxx-virtual-base test-cxx-destructor-body test-cxx-array-destructor test-cxx-constexpr test-cxx-constexpr-aggregate test-cxx-enum-class test-cxx-constraints test-cxx-new-array test-cxx-language-linkage test-cxx-member-specifiers test-cxx-member-methods test-cxx-function-templates test-cxx-function-template-overloads test-cxx-function-template-references test-cxx-non-type-templates test-initializer-brace-elision test-initializer-mixed test-flexible-arrays test-floating-static-initializers test-floating-runtime-x64 test-floating-runtime-i686 test-numeric-literals test-vla-runtime test-vla-semantics test-static-locals test-block-extern test-tls-block-scope test-cxx-qualified-namespaces test-cxx-using test-cxx-overloads test-cxx-inline-aggregates test-cxx-parser-recovery test-cxx-exceptions test-cxx-object-exceptions test-tool-relative-includes test-preprocessor-continuation test-preprocessor-if test-preprocessor-operators test-preprocessor-va-opt test-atomic-builtins test-atomic-language test-x86-wide-scalar test-language-boundaries test-noreturn test-integer-literals test-integer-promotions test-integer-conversions test-function-calls test-inline-asm test-inline-asm-encoding test-inline-asm-ports test-intrin-header test-inline-asm-execute test-inline-asm-validation test-varargs test-scalar-comparisons test-aggregate-copy test-aggregate-returns test-aggregate-packed-abi test-compound-literals test-static-compound-address test-bootstrap-core test-bootstrap-link test-bootstrap-execute test-bootstrap-stage2 test-executable-imports test-pragma-pack test-bitfields test-cxx-bitfields test-compound-assignment test-switch-statement test-control-flow test-parser-recovery test-link test-archive-link test-static-assert test-manifest test-signing test-sanitize test-driver-policy test-weak-link test-comdat-link test-object-width test-special-sections test-direct-relocation test-format-validation test-global-initializers test-global-finalizers test-ir test-ir-lowering test-verified-backend test-verified-i686-floating-arithmetic test-verified-cxx-reference-local test-verified-cxx-reference-return test-verified-cxx-conditional-aggregate test-optimize test-generic test-initializer-overrides test-alignof test-alignas test-tls test-pic-plt test-pic-got test-pic-tls test-pic-direct-internal test-golden-artifacts test-cxx-lambda-invalid test-cxx-lambda-init-capture-invalid test-cxx-spaceship test-cxx-final test-cxx-override test-cxx-empty-base test-cxx-no-unique-address
.PHONY: print-host-cc test-c17 test-c-old-style test-c-multi-declarator test-restrict-qualifier test-determinism test-property-gate test-fuzz test-ci
.PHONY: test-cxx-range-for test-cxx-iterator-range-for test-cxx-selection-init test-cxx-exception-cleanup test-cxx-const-member-overload test-cxx-ref-qualified-overload test-cxx-ref-qualified-overload-invalid test-cxx-volatile-member-overload test-cxx-volatile-member-overload-invalid test-cxx-member-lifetime test-cxx-global-constructor
.PHONY: test-cxx-operator-arrow
.PHONY: test-verified-i686-floating-comparisons test-verified-i686-floating-operations
.PHONY: test-cxx-variable-templates
.PHONY: test-cxx-template-local-classes
.PHONY: test-cxx-requires-expression test-cxx-requires-type test-cxx-named-concepts test-cxx-alias-templates
.PHONY: test-assignment-constraints
.PHONY: test-cxx-inline-variables
.PHONY: test-cxx-inline-namespace test-cxx-nested-namespace test-cxx-namespace-alias test-cxx-friend-function test-cxx-nodiscard test-cxx-deprecated test-cxx-friend-class
.PHONY: test-cxx-designated-initializer
.PHONY: test-cxx-utf8-literals
.PHONY: test-compiler-builtins
.PHONY: test-cxx-nontrivial-object-exceptions test-cxx-cross-library-exceptions
.PHONY: test-cxx-cross-translation-unit-virtual
.PHONY: test-cxx-shared-virtual-base
.PHONY: test-cxx-shared-virtual-base-method test-cxx-virtual-base-conversion \
test-cxx-virtual-base-constructor test-cxx-virtual-base-constructor-order
.PHONY: test-cxx-lambda-function-pointer test-cxx-generic-lambda test-cxx-generic-lambda-stored-invalid test-cxx-template-template test-cxx-template-template-invalid test-cxx-template-template-dependent-invalid test-cxx-structured-bindings test-cxx-structured-bindings-invalid test-cxx-alignas test-cxx-alignas-invalid test-cxx-constinit test-cxx-constinit-invalid test-cxx-using-enum test-cxx-using-enum-invalid test-cxx-conditional-explicit test-multiple-inputs
.PHONY: test-cxx-default-destructor
.PHONY: test-cxx-pure-virtual
.PHONY: test-cxx-if-constexpr test-cxx-if-constexpr-template \
test-cxx-adl-multiple-namespaces test-cxx-using-overload-namespaces \
	test-cxx-template-two-phase-namespace test-cxx-template-two-phase-adl \
	test-cxx-template-two-phase-ordinary test-cxx-template-parameter-pack \
	test-cxx-class-type-pack
.PHONY: test-cxx-constexpr-pointer
.PHONY: test-cxx-constexpr-pointer-mutation
.PHONY: test-cxx-constexpr-pointer-aggregate
.PHONY: test-cxx-noexcept-expression test-cxx-noexcept-redeclarations \
	test-cxx-typeid test-cxx-typeid-dynamic
.PHONY: test-cxx-auto-return test-cxx-decltype test-cxx-decltype-auto \
	test-cxx-auto-local-refs test-cxx-auto-direct-list-invalid \
	test-cxx-decltype-auto-local \
	test-cxx-const-cast test-cxx-dynamic-cast
.PHONY: test-cxx-dynamic-cast-downcast test-cxx-dynamic-cast-runtime test-cxx-dynamic-cast-reference
.PHONY: test-cxx-default-member-initializer test-cxx-base-constructor-initializer test-cxx-delegating-constructor test-cxx-converting-constructor test-cxx-inherited-constructor
.PHONY: test-cxx-qualified-class-initialization test-cxx-static-member-tls
.PHONY: test-cxx-constructor-general
.PHONY: test-cxx-implicit-copy
.PHONY: test-cxx-protected-member
.PHONY: test-cxx-auto-non-type-template
.PHONY: test-cxx-numeric-separators test-cxx-user-defined-literals
.PHONY: test-string-embedded-nul
.PHONY: test-preprocessor-line test-preprocessor-date-time test-preprocessor-standard-macros test-preprocessor-has-include test-preprocessor-attributes test-preprocessor-cxx-features test-universal-character-identifiers
.PHONY: test-preprocessor-line-macro
.PHONY: test-preprocessor-include test-preprocessor-include-next
.PHONY: test-cxx-predefined-function-identifiers
.PHONY: test-cxx-class-template-deduction test-cxx-aggregate-paren-init
.PHONY: test-cxx-abbreviated-function-template test-cxx-trailing-requires \
	test-cxx-constrained-abbreviated test-cxx-constrained-class-template \
	test-cxx-raw-strings test-cxx-alternative-tokens \
	test-cxx20-unsupported-boundaries
.PHONY: test-debug-info
.PHONY: test-cxx-static-reference-temporaries \
	test-cxx-static-reference-temporaries-posix
.PHONY: test-cxx-static-reference-retry \
	test-cxx-static-reference-retry-posix
.PHONY: test-cxx-static-local-exception \
	test-cxx-static-local-exception-posix
.PHONY: test-cxx-static-reference-subobjects \
	test-cxx-static-reference-subobjects-posix
.PHONY: test-cxx-static-reference-conversions \
	test-cxx-static-reference-conversions-posix
.PHONY: test-cxx-member-pointer-data \
	test-cxx-member-pointer-reference-lifetime \
	test-cxx-member-pointer-functions \
	test-cxx-member-pointer-functions-posix \
	test-cxx-member-pointer-data-posix
.PHONY: test-verified-volatile
.PHONY: test-verified-cxx-temporary-cleanup
.PHONY: test-weak-attribute
.PHONY: test-cxx-multi-declarator
.PHONY: test-cxx-return-semantics
.PHONY: test-c-return-semantics

CXX_REGRESSION_TARGETS = \
	test-cxx-cli \
	test-compiler-builtins \
	test-cxx-predefined-function-identifiers \
	test-universal-character-identifiers \
	test-preprocessor-has-include \
	test-preprocessor-attributes \
	test-preprocessor-cxx-features \
	test-preprocessor-date-time \
	test-preprocessor-standard-macros \
	test-preprocessor-include \
	test-preprocessor-include-next \
	test-preprocessor-line-macro \
	test-multiple-inputs \
	test-cxx-language-core \
	test-cxx-multi-declarator \
	test-cxx-numeric-separators \
	test-cxx-user-defined-literals \
	test-cxx-enum-class \
	test-cxx-language-linkage \
	test-cxx-member-specifiers \
	test-cxx-member-methods \
	test-cxx-qualified-namespaces \
	test-cxx-using \
	test-cxx-overloads \
	test-cxx-const-member-overload \
	test-cxx-ref-qualified-overload \
	test-cxx-ref-qualified-overload-invalid \
	test-cxx-volatile-member-overload \
	test-cxx-volatile-member-overload-invalid \
	test-cxx-inline-aggregates \
	test-cxx-empty-base \
	test-cxx-no-unique-address \
	test-cxx-multiple-inheritance-virtual \
	test-cxx-secondary-virtual-override \
	test-cxx-virtual-base \
	test-cxx-shared-virtual-base \
	test-cxx-shared-virtual-base-method \
	test-cxx-virtual-base-conversion \
	test-cxx-virtual-base-constructor \
	test-cxx-virtual-base-constructor-order \
	test-cxx-destructor-body \
	test-cxx-default-destructor \
	test-cxx-member-lifetime \
	test-cxx-array-destructor \
	test-cxx-constructor-general \
	test-cxx-implicit-copy \
	test-cxx-protected-member \
	test-cxx-constructor-body \
	test-cxx-constructor-initializer-body \
	test-cxx-base-constructor-initializer \
	test-cxx-default-member-initializer \
	test-cxx-delegating-constructor \
	test-cxx-converting-constructor \
	test-cxx-inherited-constructor \
	test-cxx-static-members \
	test-cxx-static-data-members \
	test-cxx-static-member-tls \
	test-cxx-class-template-static-data \
	test-cxx-class-template-static-data-odr \
	test-cxx-static-locals \
	test-cxx-function-templates \
	test-cxx-template-local-classes \
	test-cxx-abbreviated-function-template \
	test-cxx-abbreviated-function-template-invalid \
	test-cxx-trailing-requires \
	test-cxx-constrained-abbreviated \
	test-cxx-constrained-class-template \
	test-cxx-raw-strings \
	test-cxx-alternative-tokens \
	test-cxx-variable-templates \
	test-cxx-function-template-overloads \
	test-cxx-return-semantics \
	test-cxx-function-template-references \
	test-cxx-static-reference-temporaries \
	test-cxx-static-reference-retry \
	test-cxx-static-local-exception \
	test-cxx-static-reference-subobjects \
	test-cxx-static-reference-conversions \
	test-cxx-member-pointer-data \
	test-cxx-member-pointer-functions \
	test-cxx-member-pointer-reference-lifetime \
	test-cxx-class-template-methods \
	test-cxx-class-template-specialization \
	test-cxx-class-template-dependent-base \
	test-cxx-class-template-specialization-ambiguous \
	test-cxx-class-template-specialization-partial-order-invalid \
	test-cxx-class-template-specialization-constraint-invalid \
	test-cxx-class-template-non-type \
	test-cxx-non-type-templates \
	test-cxx-auto-non-type-template \
	test-cxx-non-type-template-deduction \
	test-cxx-constraints \
	test-cxx20-unsupported-boundaries \
	test-cxx-named-concepts \
	test-cxx-alias-templates \
	test-cxx-operator-overload \
	test-cxx-member-operator-forms \
	test-cxx-assignment-operator \
	test-cxx-nonmember-operator \
	test-cxx-spaceship \
	test-cxx-final \
	test-cxx-override \
	test-cxx-conditional-explicit \
	test-cxx-class-template-deduction \
	test-cxx-pure-virtual \
	test-cxx-conversion-operator \
	test-cxx-lambda \
	test-cxx-lambda-invalid \
	test-cxx-lambda-init-capture-invalid \
	test-cxx-structured-bindings \
	test-cxx-structured-bindings-invalid \
	test-cxx-alignas \
	test-cxx-alignas-invalid \
	test-cxx-constinit \
	test-cxx-constinit-invalid \
	test-cxx-using-enum \
	test-cxx-using-enum-invalid \
	test-cxx-template-template \
	test-cxx-template-template-invalid \
	test-cxx-template-template-dependent-invalid \
	test-cxx-lambda-function-pointer \
	test-cxx-generic-lambda \
	test-cxx-generic-lambda-stored-invalid \
	test-cxx-range-for \
	test-cxx-iterator-range-for \
	test-cxx-operator-arrow \
	test-cxx-requires-expression \
	test-cxx-requires-type \
	test-cxx-inline-variables \
	test-cxx-inline-namespace \
	test-cxx-nested-namespace \
	test-cxx-namespace-alias \
	test-cxx-friend-function \
	test-cxx-nodiscard \
	test-cxx-deprecated \
	test-cxx-friend-class \
	test-cxx-selection-init \
	test-cxx-aggregate-paren-init \
	test-cxx-designated-initializer \
	test-cxx-utf8-literals \
	test-cxx-if-constexpr \
	test-cxx-if-constexpr-template \
	test-cxx-adl-multiple-namespaces \
	test-cxx-using-overload-namespaces \
	test-cxx-template-two-phase-namespace \
	test-cxx-template-two-phase-adl \
	test-cxx-template-two-phase-ordinary \
	test-cxx-template-parameter-pack \
	test-cxx-class-type-pack \
	test-cxx-qualified-class-initialization \
	test-cxx-constexpr \
	test-cxx-constexpr-aggregate \
	test-cxx-constexpr-pointer \
	test-cxx-constexpr-pointer-mutation \
	test-cxx-constexpr-pointer-aggregate \
	test-cxx-noexcept-expression \
	test-cxx-noexcept-redeclarations \
	test-cxx-typeid \
	test-cxx-typeid-dynamic \
	test-cxx-auto-return \
	test-cxx-decltype \
	test-cxx-decltype-auto \
	test-cxx-auto-local-refs \
	test-cxx-auto-direct-list-invalid \
	test-cxx-decltype-auto-local \
	test-cxx-exceptions \
	test-cxx-object-exceptions \
	test-cxx-cross-library-exceptions \
	test-cxx-cross-translation-unit-virtual \
	test-cxx-exception-cleanup \
	test-cxx-nontrivial-object-exceptions \
	test-cxx-const-cast \
	test-cxx-dynamic-cast \
	test-cxx-dynamic-cast-downcast \
	test-cxx-dynamic-cast-runtime \
	test-cxx-dynamic-cast-reference \
	test-cxx-bitfields \
	test-cxx-global-constructor

# The C17 profile is an explicit aggregate of the focused frontend, ABI, and
# native-execution tests.  Keeping these as prerequisites makes the
# conformance claim auditable rather than compile-only.
C17_REGRESSION_TARGETS = \
	test-preprocessor-continuation \
	test-preprocessor-include-next \
	test-preprocessor-if \
	test-preprocessor-operators \
	test-preprocessor-va-opt \
	test-language-boundaries \
	test-c-return-semantics \
	test-noreturn \
	test-compiler-builtins \
	test-integer-literals \
	test-integer-promotions \
	test-integer-conversions \
	test-function-calls \
	test-varargs \
	test-scalar-comparisons \
	test-aggregate-copy \
	test-aggregate-returns \
	test-aggregate-packed-abi \
	test-compound-literals \
	test-static-compound-address \
	test-flexible-arrays \
	test-floating-static-initializers \
	test-floating-runtime-x64 \
	test-floating-runtime-i686 \
	test-numeric-literals \
	test-vla-runtime \
	test-vla-semantics \
	test-restrict-qualifier \
	test-vla-declarations \
	test-vla-declarator-variants \
	test-static-locals \
	test-block-extern \
	test-tls \
	test-tls-block-scope \
	test-atomic-builtins \
	test-atomic-language \
	test-x86-wide-scalar \
	test-bitfields \
	test-compound-assignment \
	test-switch-statement \
	test-control-flow \
	test-parser-recovery \
	test-static-assert \
	test-generic \
	test-initializer-overrides \
	test-initializer-brace-elision \
	test-initializer-mixed \
	test-alignof \
	test-alignas \
	test-global-initializers \
	test-global-finalizers \
	test-pragma-pack \
	test-string-embedded-nul

C17_REGRESSION_TARGETS += test-c-old-style test-c-multi-declarator

# `-g` currently emits a relocatable DWARF line table.  Keep this gate in the
# C17 aggregate so the option cannot silently regress to a no-op.
C17_REGRESSION_TARGETS += test-debug-info

all: $(OBJDIR) $(BINDIR) $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET) $(RAR_TARGET) $(AQC_TARGET)

build-rcc: $(OBJDIR) $(RCC_TARGET)

build-rcxx: $(OBJDIR) $(RCXX_TARGET)

build-rld: $(OBJDIR) $(RLD_TARGET)

build-rar: $(OBJDIR) $(RAR_TARGET)

build-aqc: $(OBJDIR) $(AQC_TARGET)

$(OBJDIR):
	$(call MKDIR_P,$(OBJDIR))

$(BINDIR):
	$(call MKDIR_P,$(BINDIR))

$(RCC_TARGET): $(COMMON_OBJS) $(RCC_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(RCXX_TARGET): $(COMMON_OBJS) $(RCXX_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(RLD_TARGET): $(RLD_COMMON_OBJS) $(RLD_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(RAR_TARGET): $(RAR_COMMON_OBJS) $(RAR_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(AQC_TARGET): $(AQC_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/ringpu_shader.o: $(RINGPU_ROOT)/src/validation/shader.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(RINGPU_ROOT)/include -c -o $@ $<

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(RINGPU_ROOT)/include -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET) $(RAR_TARGET) $(AQC_TARGET)

# Test
print-host-cc:
	@echo $(CC)

test: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) --emit-unsigned-v3 -o $(TEST_OUT)/hello.rin tests/hello.c
	@echo "RCC test completed"

# Keep the production regression gate explicit.  The language aggregates cover
# their complete C17/C++20 prerequisite lists; this target adds the independent
# ABI, image, and assembler checks that cannot be reached through those lists.
# Sanitizer and AQC suites remain separate jobs because they use different host
# runtimes/toolchains.  Hardware validation is intentionally not implied here.
TEST_CI_TARGETS = \
	test-c17 \
	test-cxx \
	test-ir \
	test-ir-lowering \
	test-verified-backend \
	test-optimize \
	test-assignment-constraints \
	test-debug-info \
	test-aggregate-union-abi \
	test-aggregate-flexible-abi \
	test-aggregate-sse-abi \
	test-aggregate-nested-abi \
	test-inline-asm \
	test-inline-asm-execute \
	test-inline-asm-encoding \
	test-inline-asm-ports \
	test-intrin-header \
	test-inline-asm-validation \
	test-executable-imports \
	test-signing \
	test-comdat-link \
	test-pic-direct-internal \
	test-pic-tls \
	test-pic-got \
	test-pic-plt \
	test-link \
	test-archive \
	test-archive-link \
	test-format-validation \
	test-golden-artifacts \
	test-determinism \
	test-property-gate \
	test-fuzz \
	test-global-initializers \
	test-global-finalizers \
	test-manifest \
	test-driver-policy \
	test-weak-link \
	test-weak-attribute \
	test-object-width \
	test-special-sections \
	test-tls \
	test-direct-relocation \
	test-bootstrap-core \
	test-bootstrap-link \
	test-bootstrap-execute \
	test-bootstrap-stage2

test-ci: $(TEST_CI_TARGETS)
	@echo "RCC production C17/C++20, IR, ABI, image, and bootstrap regression gate completed"

test-c17: $(RCC_TARGET) $(C17_REGRESSION_TARGETS)
	@echo "RCC C17 conformance compile-and-run suite completed"

test-c-old-style: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/c-old-style)
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/c-old-style/x64.s tests/c_old_style.c
	$(CC) -no-pie -o $(TEST_OUT)/c-old-style/x64 \
		$(TEST_OUT)/c-old-style/x64.s
	$(TEST_OUT)/c-old-style/x64
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/c-old-style/x86.s tests/c_old_style.c
	$(CC) -m32 -c -o $(TEST_OUT)/c-old-style/x86.o \
		$(TEST_OUT)/c-old-style/x86.s
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/c-old-style/x86.ro tests/c_old_style.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/c-old-style/x64.ro tests/c_old_style.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c -o $(TEST_OUT)/c-old-style/invalid.ro tests/c_old_style_invalid.c,$(TEST_OUT)/c-old-style/invalid.log)
	$(GREP) -F -q "old-style parameter declaration names an unknown parameter" $(TEST_OUT)/c-old-style/invalid.log
	@echo "C17 old-style function declaration tests completed"

test-cxx-multi-declarator: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-multi-declarator)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-multi-declarator/x64.s \
		tests/cxx_multi_declarator.cpp
	$(CC) -c $(TEST_OUT)/cxx-multi-declarator/x64.s \
		-o $(TEST_OUT)/cxx-multi-declarator/x64.o
	$(OBJCOPY) --redefine-sym main=cxx_multi_declarator_main \
		$(TEST_OUT)/cxx-multi-declarator/x64.o
	$(CC) $(TEST_OUT)/cxx-multi-declarator/x64.o \
		tests/cxx_multi_declarator_host.c \
		-o $(TEST_OUT)/cxx-multi-declarator/x64
	$(TEST_OUT)/cxx-multi-declarator/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-multi-declarator/x86.s \
		tests/cxx_multi_declarator.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-multi-declarator/x86.o \
		$(TEST_OUT)/cxx-multi-declarator/x86.s
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-multi-declarator/x86.ro \
		tests/cxx_multi_declarator.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-multi-declarator/x64.ro \
		tests/cxx_multi_declarator.cpp
	@echo "C++ comma-separated declarator tests completed"

test-c-multi-declarator: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/c-multi-declarator)
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/c-multi-declarator/x64.s tests/c_multi_declarator.c
	$(CC) -no-pie -o $(TEST_OUT)/c-multi-declarator/x64 \
		$(TEST_OUT)/c-multi-declarator/x64.s
	$(TEST_OUT)/c-multi-declarator/x64
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/c-multi-declarator/x86.s tests/c_multi_declarator.c
	$(CC) -m32 -c -o $(TEST_OUT)/c-multi-declarator/x86.o \
		$(TEST_OUT)/c-multi-declarator/x86.s
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/c-multi-declarator/x86.ro tests/c_multi_declarator.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/c-multi-declarator/x64.ro tests/c_multi_declarator.c
	@echo "C17 comma-separated declarator tests completed"

test-debug-info: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/debug-info)
	$(RCC_TARGET) --target i686-unknown-rinos -g -c \
		-o $(TEST_OUT)/debug-info/x86-g.ro tests/debug_info.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -g -c \
		-o $(TEST_OUT)/debug-info/x64-g.ro tests/debug_info.c
	$(RCC_TARGET) --target i686-unknown-rinos -g -c \
		-o $(TEST_OUT)/debug-info/x86-aligned-g.ro tests/debug_info_aligned.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -g -c \
		-o $(TEST_OUT)/debug-info/x64-aligned-g.ro tests/debug_info_aligned.c
	$(RCXX_TARGET) --target x86_64-unknown-rinos -g -c \
		-o $(TEST_OUT)/debug-info/cxx-g.ro tests/hello.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++11 -g -c \
		-o $(TEST_OUT)/debug-info/cxx11-g.ro tests/hello.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++14 -g -c \
		-o $(TEST_OUT)/debug-info/cxx14-g.ro tests/hello.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -g -c \
		-o $(TEST_OUT)/debug-info/cxx17-g.ro tests/hello.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -g -c \
		-o $(TEST_OUT)/debug-info/cxx-member-x86-g.ro \
		tests/debug_info_cxx_member.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -g -c \
		-o $(TEST_OUT)/debug-info/cxx-member-x64-g.ro \
		tests/debug_info_cxx_member.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -g -c \
		-o $(TEST_OUT)/debug-info/cxx-enum-x86-g.ro \
		tests/debug_info_cxx_enum.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -g -c \
		-o $(TEST_OUT)/debug-info/cxx-enum-x64-g.ro \
		tests/debug_info_cxx_enum.cpp
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/debug-info/x86-no-g.ro tests/debug_info.c
	$(RCC_TARGET) --target i686-unknown-rinos -g -O0 -fverified-backend -c \
		-o $(TEST_OUT)/debug-info/verified-x86-g.ro \
		tests/verified_backend_debug.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -g -O0 -fverified-backend -c \
		-o $(TEST_OUT)/debug-info/verified-x64-g.ro \
		tests/verified_backend_debug.c
	$(RCC_TARGET) --target i686-unknown-rinos -g -O2 -fverified-backend -c \
		-o $(TEST_OUT)/debug-info/verified-opt-x86-g.ro \
		tests/verified_backend_debug.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -g -O2 -fverified-backend -c \
		-o $(TEST_OUT)/debug-info/verified-opt-x64-g.ro \
		tests/verified_backend_debug.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -g -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/debug-info/verified-cxx-opt-x86-g.ro \
		tests/verified_backend_debug.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -g -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/debug-info/verified-cxx-opt-x64-g.ro \
		tests/verified_backend_debug.cpp
	$(RCC_TARGET) --target i686-unknown-rinos -g -fverified-backend -c \
		-o $(TEST_OUT)/debug-info/verified-globals-x86-g.ro \
		tests/verified_backend_globals.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -g -fverified-backend -c \
		-o $(TEST_OUT)/debug-info/verified-globals-x64-g.ro \
		tests/verified_backend_globals.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/debug-info/verify \
		tests/debug_info_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-e debug_line_entry -o $(TEST_OUT)/debug-info/x86.rin \
		$(TEST_OUT)/debug-info/x86-g.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-e debug_line_entry -o $(TEST_OUT)/debug-info/x64.rin \
		$(TEST_OUT)/debug-info/x64-g.ro
	$(TEST_OUT)/debug-info/verify \
		$(TEST_OUT)/debug-info/x86-g.ro \
		$(TEST_OUT)/debug-info/x64-g.ro \
		$(TEST_OUT)/debug-info/cxx-g.ro \
		$(TEST_OUT)/debug-info/x86-no-g.ro \
		$(TEST_OUT)/debug-info/x86-aligned-g.ro \
		$(TEST_OUT)/debug-info/x64-aligned-g.ro \
		$(TEST_OUT)/debug-info/cxx11-g.ro \
		$(TEST_OUT)/debug-info/cxx14-g.ro \
		$(TEST_OUT)/debug-info/cxx17-g.ro \
		$(TEST_OUT)/debug-info/verified-x86-g.ro \
		$(TEST_OUT)/debug-info/verified-x64-g.ro \
		$(TEST_OUT)/debug-info/verified-globals-x86-g.ro \
		$(TEST_OUT)/debug-info/verified-globals-x64-g.ro \
		$(TEST_OUT)/debug-info/cxx-member-x86-g.ro \
		$(TEST_OUT)/debug-info/cxx-member-x64-g.ro \
		$(TEST_OUT)/debug-info/verified-opt-x86-g.ro \
		$(TEST_OUT)/debug-info/verified-opt-x64-g.ro \
		$(TEST_OUT)/debug-info/cxx-enum-x86-g.ro \
		$(TEST_OUT)/debug-info/cxx-enum-x64-g.ro \
		$(TEST_OUT)/debug-info/verified-cxx-opt-x86-g.ro \
		$(TEST_OUT)/debug-info/verified-cxx-opt-x64-g.ro \
		$(TEST_OUT)/debug-info/x86.rin \
		$(TEST_OUT)/debug-info/x64.rin
	@echo "Relocatable DWARF line/info/location-list/frame tests completed"

test-aqc: $(AQC_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(AQC_TARGET) -o $(TEST_OUT)/passthrough_vertex.rsh \
		$(RINOS_ROOT)/resources/shaders/passthrough_vertex.aq
	$(AQC_TARGET) -o $(TEST_OUT)/sample_fragment.rsh \
		$(RINOS_ROOT)/resources/shaders/sample_fragment.aq
	@echo "AQC test completed"

test-cxx: $(RCXX_TARGET) $(CXX_REGRESSION_TARGETS)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCXX_TARGET) --emit-unsigned-v3 -o $(TEST_OUT)/hello_cxx.rin tests/hello.cpp
	@echo "RCC++ test completed"

test-cxx20-unsupported-boundaries: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx20-unsupported-boundaries)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx20-unsupported-boundaries/x86.ro tests/cxx20_modules_coroutines_invalid.cpp,$(TEST_OUT)/cxx20-unsupported-boundaries/x86.log)
	$(GREP) -F -q "C++20 modules (module/import/export module) are not supported by RCC++" $(TEST_OUT)/cxx20-unsupported-boundaries/x86.log
	$(GREP) -F -q "C++20 coroutine keyword 'co_await' is not supported by RCC++" $(TEST_OUT)/cxx20-unsupported-boundaries/x86.log
	$(GREP) -F -q "C++20 coroutine keyword 'co_yield' is not supported by RCC++" $(TEST_OUT)/cxx20-unsupported-boundaries/x86.log
	$(GREP) -F -q "C++20 coroutine keyword 'co_return' is not supported by RCC++" $(TEST_OUT)/cxx20-unsupported-boundaries/x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx20-unsupported-boundaries/x64.ro tests/cxx20_modules_coroutines_invalid.cpp,$(TEST_OUT)/cxx20-unsupported-boundaries/x64.log)
	$(GREP) -F -q "C++20 modules (module/import/export module) are not supported by RCC++" $(TEST_OUT)/cxx20-unsupported-boundaries/x64.log
	$(GREP) -F -q "C++20 coroutine keyword 'co_return' is not supported by RCC++" $(TEST_OUT)/cxx20-unsupported-boundaries/x64.log
	@echo "C++20 module and coroutine unsupported-boundary diagnostics completed"

test-cxx-predefined-function-identifiers: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-predefined-function-identifiers)
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x86.ro \
		tests/predefined_function_identifiers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x64.ro \
		tests/predefined_function_identifiers.c
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x86.s \
		tests/predefined_function_identifiers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x64.s \
		tests/predefined_function_identifiers.c
	$(CC) -o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x64 \
		tests/predefined_function_c_run_test.c \
		$(TEST_OUT)/cxx-predefined-function-identifiers/c-x64.s
	$(TEST_OUT)/cxx-predefined-function-identifiers/c-x64
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/c-object-run-test \
		tests/predefined_function_object_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/cxx-predefined-function-identifiers/c-object-run-test \
		$(TEST_OUT)/cxx-predefined-function-identifiers/c-x86.ro \
		$(TEST_OUT)/cxx-predefined-function-identifiers/c-x64.ro
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x86.ro \
		tests/predefined_function_identifiers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64.ro \
		tests/predefined_function_identifiers.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x86.s \
		tests/predefined_function_identifiers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64.s \
		tests/predefined_function_identifiers.cpp
	$(CC) -o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64 \
		tests/predefined_function_cxx_run_test.c \
		$(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64.s
	$(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64
	$(TEST_OUT)/cxx-predefined-function-identifiers/c-object-run-test --cxx \
		$(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x86.ro \
		$(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64.ro
else
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x86.s \
		tests/predefined_function_identifiers.c
	$(CC) -m32 -o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x86 \
		tests/predefined_function_c_run_test.c \
		$(TEST_OUT)/cxx-predefined-function-identifiers/c-x86.s
	$(TEST_OUT)/cxx-predefined-function-identifiers/c-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x64.s \
		tests/predefined_function_identifiers.c
	$(CC) -o $(TEST_OUT)/cxx-predefined-function-identifiers/c-x64 \
		tests/predefined_function_c_run_test.c \
		$(TEST_OUT)/cxx-predefined-function-identifiers/c-x64.s
	$(TEST_OUT)/cxx-predefined-function-identifiers/c-x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x86.s \
		tests/predefined_function_identifiers.cpp
	$(CC) -m32 -o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x86 \
		tests/predefined_function_cxx_run_test.c \
		$(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x86.s
	$(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64.s \
		tests/predefined_function_identifiers.cpp
	$(CC) -o $(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64 \
		tests/predefined_function_cxx_run_test.c \
		$(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64.s
	$(TEST_OUT)/cxx-predefined-function-identifiers/cxx-x64
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/invalid-c.ro \
		tests/predefined_function_invalid.c,$(TEST_OUT)/cxx-predefined-function-identifiers/invalid-c.log)
	$(GREP) -q "__func__ is only valid within a function body" \
		$(TEST_OUT)/cxx-predefined-function-identifiers/invalid-c.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-predefined-function-identifiers/invalid-cxx.ro \
		tests/predefined_function_invalid.c,$(TEST_OUT)/cxx-predefined-function-identifiers/invalid-cxx.log)
	$(GREP) -q "__func__ is only valid within a function body" \
		$(TEST_OUT)/cxx-predefined-function-identifiers/invalid-cxx.log
	@echo "C/C++ predefined function identifier tests completed"

test-preprocessor-date-time: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/preprocessor-date-time)
	$(DATE_TIME_ENV) $(RCC_TARGET) -E tests/preprocessor_date_time.c > \
		$(TEST_OUT)/preprocessor-date-time/c.i
	$(GREP) -F -q 'const char rcc_preprocessor_date[] = "Jan  1 1970";' \
		$(TEST_OUT)/preprocessor-date-time/c.i
	$(GREP) -F -q 'const char rcc_preprocessor_time[] = "00:00:00";' \
		$(TEST_OUT)/preprocessor-date-time/c.i
	$(DATE_TIME_ENV) $(RCXX_TARGET) -std=c++20 -E \
		tests/preprocessor_date_time.cpp > \
		$(TEST_OUT)/preprocessor-date-time/cxx.i
	$(GREP) -F -q 'const char rcc_cpp_preprocessor_date[] = "Jan  1 1970";' \
		$(TEST_OUT)/preprocessor-date-time/cxx.i
	$(GREP) -F -q 'const char rcc_cpp_preprocessor_time[] = "00:00:00";' \
		$(TEST_OUT)/preprocessor-date-time/cxx.i
	$(DATE_TIME_ENV) $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-date-time/c-x86.ro \
		tests/preprocessor_date_time.c
	$(DATE_TIME_ENV) $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-date-time/c-x64.ro \
		tests/preprocessor_date_time.c
	$(DATE_TIME_ENV) $(RCXX_TARGET) --target i686-unknown-rinos \
		-std=c++20 -c -o $(TEST_OUT)/preprocessor-date-time/cxx-x86.ro \
		tests/preprocessor_date_time.cpp
	$(DATE_TIME_ENV) $(RCXX_TARGET) --target x86_64-unknown-rinos \
		-std=c++20 -c -o $(TEST_OUT)/preprocessor-date-time/cxx-x64.ro \
		tests/preprocessor_date_time.cpp
	$(DATE_TIME_INVALID)
	$(GREP) -F -q "invalid SOURCE_DATE_EPOCH value 'not-a-timestamp'" \
		$(TEST_OUT)/preprocessor-date-time/invalid.log
	@echo "C17/C++20 __DATE__/__TIME__ tests completed"

test-preprocessor-standard-macros: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/preprocessor-standard-macros)
	$(RCC_TARGET) -E tests/preprocessor_standard_macros.c > \
		$(TEST_OUT)/preprocessor-standard-macros/c-hosted.i
	$(GREP) -F -q 'int rcc_standard_hosted_value = 1;' \
		$(TEST_OUT)/preprocessor-standard-macros/c-hosted.i
	$(RCC_TARGET) -ffreestanding -E tests/preprocessor_freestanding_macros.c > \
		$(TEST_OUT)/preprocessor-standard-macros/c-freestanding.i
	$(GREP) -F -q 'int rcc_freestanding_hosted_value = 0;' \
		$(TEST_OUT)/preprocessor-standard-macros/c-freestanding.i
	$(RCXX_TARGET) -std=c++20 -E tests/preprocessor_standard_macros.cpp > \
		$(TEST_OUT)/preprocessor-standard-macros/cxx-hosted.i
	$(GREP) -F -q 'constexpr int rcc_cpp_standard_hosted_value = 1;' \
		$(TEST_OUT)/preprocessor-standard-macros/cxx-hosted.i
	$(RCXX_TARGET) -std=c++20 -ffreestanding -E \
		tests/preprocessor_freestanding_macros.cpp > \
		$(TEST_OUT)/preprocessor-standard-macros/cxx-freestanding.i
	$(GREP) -F -q 'constexpr int rcc_cpp_freestanding_hosted_value = 0;' \
		$(TEST_OUT)/preprocessor-standard-macros/cxx-freestanding.i
	$(RCC_TARGET) --target i686-unknown-rinos -ffreestanding -c \
		-o $(TEST_OUT)/preprocessor-standard-macros/c-x86.ro \
		tests/preprocessor_freestanding_macros.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -ffreestanding -c \
		-o $(TEST_OUT)/preprocessor-standard-macros/c-x64.ro \
		tests/preprocessor_freestanding_macros.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -ffreestanding -c \
		-o $(TEST_OUT)/preprocessor-standard-macros/cxx-x86.ro \
		tests/preprocessor_freestanding_macros.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -ffreestanding -c \
		-o $(TEST_OUT)/preprocessor-standard-macros/cxx-x64.ro \
		tests/preprocessor_freestanding_macros.cpp
	@echo "C17/C++20 __STDC_HOSTED__ tests completed"

test-universal-character-identifiers: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/universal-character-identifiers)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/universal-character-identifiers/c-x86.ro \
		tests/universal_character_identifiers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/universal-character-identifiers/c-x64.ro \
		tests/universal_character_identifiers.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/universal-character-identifiers/cxx-x86.ro \
		tests/universal_character_identifiers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/universal-character-identifiers/cxx-x64.ro \
		tests/universal_character_identifiers.cpp
	$(call EXPECT_FAILURE,$(RCC_TARGET) -c -o $(TEST_OUT)/universal-character-identifiers/invalid-c.ro tests/invalid_universal_character_name.c,$(TEST_OUT)/universal-character-identifiers/invalid-c.log)
	$(GREP) -q "universal character names are not supported by the RinOS byte-string ABI" \
		$(TEST_OUT)/universal-character-identifiers/invalid-c.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -std=c++20 -c -o $(TEST_OUT)/universal-character-identifiers/invalid-cxx.ro tests/invalid_universal_character_name.c,$(TEST_OUT)/universal-character-identifiers/invalid-cxx.log)
	$(GREP) -q "universal character names are not supported by the RinOS byte-string ABI" \
		$(TEST_OUT)/universal-character-identifiers/invalid-cxx.log
	@echo "C17/C++20 universal character identifier tests completed"

test-preprocessor-has-include: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/preprocessor-has-include)
	$(RCC_TARGET) -E tests/preprocessor_has_include.c > \
		$(TEST_OUT)/preprocessor-has-include/c.i
	$(GREP) -F -q 'int preprocessor_has_include_c(void)' \
		$(TEST_OUT)/preprocessor-has-include/c.i
	$(RCXX_TARGET) -std=c++20 -E tests/preprocessor_has_include.cpp > \
		$(TEST_OUT)/preprocessor-has-include/cxx.i
	$(GREP) -F -q 'int preprocessor_has_include_cxx()' \
		$(TEST_OUT)/preprocessor-has-include/cxx.i
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-has-include/c-x86.ro \
		tests/preprocessor_has_include.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-has-include/c-x64.ro \
		tests/preprocessor_has_include.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-has-include/cxx-x86.ro \
		tests/preprocessor_has_include.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-has-include/cxx-x64.ro \
		tests/preprocessor_has_include.cpp
	$(call EXPECT_FAILURE,$(RCC_TARGET) -c -o $(TEST_OUT)/preprocessor-has-include/invalid-c.ro tests/invalid_preprocessor_has_include.c,$(TEST_OUT)/preprocessor-has-include/invalid-c.log)
	$(GREP) -F -q 'invalid #if expression' \
		$(TEST_OUT)/preprocessor-has-include/invalid-c.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -std=c++20 -c -o $(TEST_OUT)/preprocessor-has-include/invalid-cxx.ro tests/invalid_preprocessor_has_include.c,$(TEST_OUT)/preprocessor-has-include/invalid-cxx.log)
	$(GREP) -F -q 'invalid #if expression' \
		$(TEST_OUT)/preprocessor-has-include/invalid-cxx.log
	@echo "C17/C++20 __has_include tests completed"

test-preprocessor-attributes: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/preprocessor-attributes)
	$(RCXX_TARGET) -std=c++20 -E tests/preprocessor_attributes.cpp > \
		$(TEST_OUT)/preprocessor-attributes/cxx.i
	$(GREP) -F -q 'int preprocessor_attribute_probe(int value)' \
		$(TEST_OUT)/preprocessor-attributes/cxx.i
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-attributes/cxx-x86.ro \
		tests/preprocessor_attributes.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-attributes/cxx-x64.ro \
		tests/preprocessor_attributes.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -std=c++20 -c -o $(TEST_OUT)/preprocessor-attributes/invalid-cxx.ro tests/invalid_preprocessor_attributes.cpp,$(TEST_OUT)/preprocessor-attributes/invalid-cxx.log)
	$(GREP) -F -q 'invalid #if expression' \
		$(TEST_OUT)/preprocessor-attributes/invalid-cxx.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -std=c++17 -c -o $(TEST_OUT)/preprocessor-attributes/invalid-standard-attribute.ro tests/invalid_cxx_standard_attributes.cpp,$(TEST_OUT)/preprocessor-attributes/invalid-standard-attribute.log)
	$(GREP) -F -q '[[likely]] and [[unlikely]] require C++20 or newer' \
		$(TEST_OUT)/preprocessor-attributes/invalid-standard-attribute.log
	@echo "C++20 __has_cpp_attribute tests completed"

test-preprocessor-cxx-features: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/preprocessor-cxx-features)
	$(RCXX_TARGET) -std=c++11 -E tests/preprocessor_cpp_features.cpp > \
		$(TEST_OUT)/preprocessor-cxx-features/cxx11.i
	$(RCXX_TARGET) -std=c++14 -E tests/preprocessor_cpp_features.cpp > \
		$(TEST_OUT)/preprocessor-cxx-features/cxx14.i
	$(RCXX_TARGET) -std=c++17 -E tests/preprocessor_cpp_features.cpp > \
		$(TEST_OUT)/preprocessor-cxx-features/cxx17.i
	$(RCXX_TARGET) -std=c++20 -E tests/preprocessor_cpp_features.cpp > \
		$(TEST_OUT)/preprocessor-cxx-features/cxx20.i
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++11 -c \
		-o $(TEST_OUT)/preprocessor-cxx-features/cxx11-x86.ro \
		tests/preprocessor_cpp_features.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++14 -c \
		-o $(TEST_OUT)/preprocessor-cxx-features/cxx14-x64.ro \
		tests/preprocessor_cpp_features.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/preprocessor-cxx-features/cxx17-x86.ro \
		tests/preprocessor_cpp_features.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-cxx-features/cxx20-x64.ro \
		tests/preprocessor_cpp_features.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -std=c++11 -c -o $(TEST_OUT)/preprocessor-cxx-features/invalid-cxx11.ro tests/cxx_standard_cpp14_invalid.cpp,$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx11.log)
	$(GREP) -F -q "structured bindings require C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx11.log
	$(GREP) -F -q "generic lambda parameters require C++14 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx11.log
	$(GREP) -F -q "lambda init-captures require C++14 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx11.log
	$(GREP) -F -q "fold expressions require C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx11.log
	$(GREP) -F -q "inline variables require C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx11.log
	$(GREP) -F -q "C++ designated initializers require C++20 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx11.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -std=c++14 -c -o $(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.ro tests/cxx_standard_cpp17_invalid.cpp,$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.log)
	$(GREP) -F -q "if constexpr requires C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.log
	$(GREP) -F -q "constexpr lambda specifiers require C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.log
	$(GREP) -F -q "fold expressions require C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.log
	$(GREP) -F -q "template<auto> parameters require C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.log
	$(GREP) -F -q "nested namespace definitions require C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.log
	$(GREP) -F -q "inline variables require C++17 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.log
	$(GREP) -F -q "C++ designated initializers require C++20 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx14.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -std=c++17 -c -o $(TEST_OUT)/preprocessor-cxx-features/invalid-cxx17.ro tests/cxx_standard_cpp20_invalid.cpp,$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx17.log)
	$(GREP) -F -q "requires C++20 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx17.log
	$(GREP) -F -q "C++ designated initializers require C++20 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx17.log
	$(GREP) -F -q "consteval lambda specifiers require C++20 or newer" \
		$(TEST_OUT)/preprocessor-cxx-features/invalid-cxx17.log
	@echo "C++ standard-version gates and feature-test macros completed"

test-golden-artifacts: $(RCC_TARGET) $(RCXX_TARGET)
	python3 scripts/check_golden.py --rcc $(RCC_TARGET) --rccxx $(RCXX_TARGET)

test-determinism: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/determinism)
	$(RCC_TARGET) --target i686-unknown-rinos -O2 -c \
		-o $(TEST_OUT)/determinism/c-x86-1.ro tests/hello.c
	$(RCC_TARGET) --target i686-unknown-rinos -O2 -c \
		-o $(TEST_OUT)/determinism/c-x86-2.ro tests/hello.c
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/c-x86-1.ro,$(TEST_OUT)/determinism/c-x86-2.ro)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -c \
		-o $(TEST_OUT)/determinism/c-x64-1.ro tests/hello.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -c \
		-o $(TEST_OUT)/determinism/c-x64-2.ro tests/hello.c
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/c-x64-1.ro,$(TEST_OUT)/determinism/c-x64-2.ro)
	$(RCC_TARGET) --target i686-unknown-rinos -O2 \
		--emit-unsigned-v3 -o $(TEST_OUT)/determinism/c-x86-1.rin \
		tests/hello.c
	$(RCC_TARGET) --target i686-unknown-rinos -O2 \
		--emit-unsigned-v3 -o $(TEST_OUT)/determinism/c-x86-2.rin \
		tests/hello.c
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/c-x86-1.rin,$(TEST_OUT)/determinism/c-x86-2.rin)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 \
		--emit-unsigned-v3 -o $(TEST_OUT)/determinism/c-x64-1.rin \
		tests/hello.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 \
		--emit-unsigned-v3 -o $(TEST_OUT)/determinism/c-x64-2.rin \
		tests/hello.c
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/c-x64-1.rin,$(TEST_OUT)/determinism/c-x64-2.rin)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -c \
		-o $(TEST_OUT)/determinism/cxx-x86-1.ro tests/hello.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -c \
		-o $(TEST_OUT)/determinism/cxx-x86-2.ro tests/hello.cpp
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/cxx-x86-1.ro,$(TEST_OUT)/determinism/cxx-x86-2.ro)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -c \
		-o $(TEST_OUT)/determinism/cxx-x64-1.ro tests/hello.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -c \
		-o $(TEST_OUT)/determinism/cxx-x64-2.ro tests/hello.cpp
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/cxx-x64-1.ro,$(TEST_OUT)/determinism/cxx-x64-2.ro)
	$(RCC_TARGET) --target i686-unknown-rinos -g -fverified-backend -c \
		-o $(TEST_OUT)/determinism/verified-x86-g-1.ro \
		tests/verified_backend_debug.c
	$(RCC_TARGET) --target i686-unknown-rinos -g -fverified-backend -c \
		-o $(TEST_OUT)/determinism/verified-x86-g-2.ro \
		tests/verified_backend_debug.c
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/verified-x86-g-1.ro,$(TEST_OUT)/determinism/verified-x86-g-2.ro)
	$(RCC_TARGET) --target x86_64-unknown-rinos -g -fverified-backend -c \
		-o $(TEST_OUT)/determinism/verified-x64-g-1.ro \
		tests/verified_backend_debug.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -g -fverified-backend -c \
		-o $(TEST_OUT)/determinism/verified-x64-g-2.ro \
		tests/verified_backend_debug.c
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/verified-x64-g-1.ro,$(TEST_OUT)/determinism/verified-x64-g-2.ro)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		--emit-unsigned-v3 -o $(TEST_OUT)/determinism/cxx-x86-1.rin \
		tests/hello.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		--emit-unsigned-v3 -o $(TEST_OUT)/determinism/cxx-x86-2.rin \
		tests/hello.cpp
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/cxx-x86-1.rin,$(TEST_OUT)/determinism/cxx-x86-2.rin)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		--emit-unsigned-v3 -o $(TEST_OUT)/determinism/cxx-x64-1.rin \
		tests/hello.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		--emit-unsigned-v3 -o $(TEST_OUT)/determinism/cxx-x64-2.rin \
		tests/hello.cpp
	$(call COMPARE_FILES,$(TEST_OUT)/determinism/cxx-x64-1.rin,$(TEST_OUT)/determinism/cxx-x64-2.rin)
	@echo "Dual-architecture C/C++ deterministic object and image tests completed"

test-property-gate: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/property-gate)
	$(RCC_TARGET) --target i686-unknown-rinos -O2 -c \
		-o $(TEST_OUT)/property-gate/c-x86-1.ro tests/property_valid.c
	$(RCC_TARGET) --target i686-unknown-rinos -O2 -c \
		-o $(TEST_OUT)/property-gate/c-x86-2.ro tests/property_valid.c
	$(call COMPARE_FILES,$(TEST_OUT)/property-gate/c-x86-1.ro,$(TEST_OUT)/property-gate/c-x86-2.ro)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -c \
		-o $(TEST_OUT)/property-gate/c-x64-1.ro tests/property_valid.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -c \
		-o $(TEST_OUT)/property-gate/c-x64-2.ro tests/property_valid.c
	$(call COMPARE_FILES,$(TEST_OUT)/property-gate/c-x64-1.ro,$(TEST_OUT)/property-gate/c-x64-2.ro)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -c \
		-o $(TEST_OUT)/property-gate/cxx-x86-1.ro tests/property_valid.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -c \
		-o $(TEST_OUT)/property-gate/cxx-x86-2.ro tests/property_valid.cpp
	$(call COMPARE_FILES,$(TEST_OUT)/property-gate/cxx-x86-1.ro,$(TEST_OUT)/property-gate/cxx-x86-2.ro)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -c \
		-o $(TEST_OUT)/property-gate/cxx-x64-1.ro tests/property_valid.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -c \
		-o $(TEST_OUT)/property-gate/cxx-x64-2.ro tests/property_valid.cpp
	$(call COMPARE_FILES,$(TEST_OUT)/property-gate/cxx-x64-1.ro,$(TEST_OUT)/property-gate/cxx-x64-2.ro)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/property-gate/invalid-c-x86.ro tests/property_invalid.c,$(TEST_OUT)/property-gate/invalid-c-x86.log)
	$(call CHECK_NONEMPTY,$(TEST_OUT)/property-gate/invalid-c-x86.log)
	$(GREP) -q "error:" $(TEST_OUT)/property-gate/invalid-c-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/property-gate/invalid-c-x64.ro tests/property_invalid.c,$(TEST_OUT)/property-gate/invalid-c-x64.log)
	$(call CHECK_NONEMPTY,$(TEST_OUT)/property-gate/invalid-c-x64.log)
	$(GREP) -q "error:" $(TEST_OUT)/property-gate/invalid-c-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/property-gate/invalid-cxx-x86.ro tests/property_invalid.cpp,$(TEST_OUT)/property-gate/invalid-cxx-x86.log)
	$(call CHECK_NONEMPTY,$(TEST_OUT)/property-gate/invalid-cxx-x86.log)
	$(GREP) -q "error:" $(TEST_OUT)/property-gate/invalid-cxx-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/property-gate/invalid-cxx-x64.ro tests/property_invalid.cpp,$(TEST_OUT)/property-gate/invalid-cxx-x64.log)
	$(call CHECK_NONEMPTY,$(TEST_OUT)/property-gate/invalid-cxx-x64.log)
	$(GREP) -q "error:" $(TEST_OUT)/property-gate/invalid-cxx-x64.log
	@echo "C/C++ dual-architecture property corpus gate completed"

test-fuzz: $(RCC_TARGET) $(RCXX_TARGET)
	python3 scripts/fuzz_compile.py --rcc $(RCC_TARGET) --rccxx $(RCXX_TARGET)

test-cxx-enum-class: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-enum-class)
ifeq ($(OS),Windows_NT)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-enum-class/enum-x86.ro \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-enum-class/enum-x64.ro \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-enum-class/enum-x86.s \
		tests/cxx_enum_class.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-enum-class/enum-x86.o \
		$(TEST_OUT)/cxx-enum-class/enum-x86.s
	objdump -f $(TEST_OUT)/cxx-enum-class/enum-x86.o > $(TEST_OUT)/cxx-enum-class/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-enum-class/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-enum-class/enum-x64.s \
		tests/cxx_enum_class.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-enum-class/enum-x64.o \
		$(TEST_OUT)/cxx-enum-class/enum-x64.s
	$(OBJCOPY) --redefine-sym main=rcc_test_main \
		$(TEST_OUT)/cxx-enum-class/enum-x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-enum-class/enum-x64-host \
		tests/cxx_main_host.c $(TEST_OUT)/cxx-enum-class/enum-x64.o
	$(TEST_OUT)/cxx-enum-class/enum-x64-host
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-enum-class/enum-x86.ro \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-enum-class/enum-x64.ro \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-enum-class/enum-x86.s \
		tests/cxx_enum_class.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-enum-class/enum-x86.o \
		$(TEST_OUT)/cxx-enum-class/enum-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-enum-class/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-enum-class/enum-x86 \
		$(TEST_OUT)/cxx-enum-class/start-x86.o \
		$(TEST_OUT)/cxx-enum-class/enum-x86.o
	$(TEST_OUT)/cxx-enum-class/enum-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-enum-class/enum-x64.s \
		tests/cxx_enum_class.cpp
	gcc -c -o $(TEST_OUT)/cxx-enum-class/enum-x64.o \
		$(TEST_OUT)/cxx-enum-class/enum-x64.s
	gcc -c -o $(TEST_OUT)/cxx-enum-class/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-enum-class/enum-x64 \
		$(TEST_OUT)/cxx-enum-class/start-x64.o \
		$(TEST_OUT)/cxx-enum-class/enum-x64.o
	$(TEST_OUT)/cxx-enum-class/enum-x64
endif
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		--emit-unsigned-v3 -o $(TEST_OUT)/cxx-enum-class/enum-x86.rin \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		--emit-unsigned-v3 -o $(TEST_OUT)/cxx-enum-class/enum-x64.rin \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -shared \
		--emit-unsigned-v3 -o $(TEST_OUT)/cxx-enum-class/enum-x86.rll \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -shared \
		--emit-unsigned-v3 -o $(TEST_OUT)/cxx-enum-class/enum-x64.rll \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -driver \
		--emit-unsigned-v3 -o $(TEST_OUT)/cxx-enum-class/enum-x86.drv \
		tests/cxx_enum_class.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -driver \
		--emit-unsigned-v3 -o $(TEST_OUT)/cxx-enum-class/enum-x64.drv \
		tests/cxx_enum_class.cpp
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-enum-class/enum-x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-enum-class/enum-x64.rin
	$(RINVALIDATE) --kind library --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-enum-class/enum-x86.rll
	$(RINVALIDATE) --kind library --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-enum-class/enum-x64.rll
	$(RINVALIDATE) --kind driver --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-enum-class/enum-x86.drv
	$(RINVALIDATE) --kind driver --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-enum-class/enum-x64.drv
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-enum-class/invalid-x86.ro tests/cxx_enum_class_invalid.cpp,$(TEST_OUT)/cxx-enum-class/invalid-x86.log)
	$(GREP) -q 'scoped enum' $(TEST_OUT)/cxx-enum-class/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-enum-class/invalid-x64.ro tests/cxx_enum_class_invalid.cpp,$(TEST_OUT)/cxx-enum-class/invalid-x64.log)
	$(GREP) -q 'scoped enum' $(TEST_OUT)/cxx-enum-class/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-enum-class/invalid-overflow-x86.ro tests/invalid_cxx_enum_constant_expression.cpp,$(TEST_OUT)/cxx-enum-class/invalid-overflow-x86.log)
	$(GREP) -F -q 'implicit enumerator value exceeds the supported 64-bit range' $(TEST_OUT)/cxx-enum-class/invalid-overflow-x86.log
	$(GREP) -F -q 'enumerator value exceeds the supported 64-bit range' $(TEST_OUT)/cxx-enum-class/invalid-overflow-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-enum-class/invalid-overflow-x64.ro tests/invalid_cxx_enum_constant_expression.cpp,$(TEST_OUT)/cxx-enum-class/invalid-overflow-x64.log)
	$(GREP) -F -q 'implicit enumerator value exceeds the supported 64-bit range' $(TEST_OUT)/cxx-enum-class/invalid-overflow-x64.log
	$(GREP) -F -q 'enumerator value exceeds the supported 64-bit range' $(TEST_OUT)/cxx-enum-class/invalid-overflow-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-enum-class/invalid-underlying-x86.ro tests/invalid_cxx_enum_underlying_range.cpp,$(TEST_OUT)/cxx-enum-class/invalid-underlying-x86.log)
	$(GREP) -F -q 'enumerator value is not representable in its fixed underlying type' $(TEST_OUT)/cxx-enum-class/invalid-underlying-x86.log
	$(GREP) -F -q 'implicit enumerator value is not representable in its fixed underlying type' $(TEST_OUT)/cxx-enum-class/invalid-underlying-x86.log
	$(GREP) -F -q 'implicit enumerator value exceeds the supported 64-bit range' $(TEST_OUT)/cxx-enum-class/invalid-underlying-x86.log
	$(GREP) -F -q 'no supported integer type can represent all enumerators' $(TEST_OUT)/cxx-enum-class/invalid-underlying-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-enum-class/invalid-underlying-x64.ro tests/invalid_cxx_enum_underlying_range.cpp,$(TEST_OUT)/cxx-enum-class/invalid-underlying-x64.log)
	$(GREP) -F -q 'enumerator value is not representable in its fixed underlying type' $(TEST_OUT)/cxx-enum-class/invalid-underlying-x64.log
	$(GREP) -F -q 'implicit enumerator value is not representable in its fixed underlying type' $(TEST_OUT)/cxx-enum-class/invalid-underlying-x64.log
	$(GREP) -F -q 'implicit enumerator value exceeds the supported 64-bit range' $(TEST_OUT)/cxx-enum-class/invalid-underlying-x64.log
	$(GREP) -F -q 'no supported integer type can represent all enumerators' $(TEST_OUT)/cxx-enum-class/invalid-underlying-x64.log
	@echo "RCC++ scoped enum test completed"

test-cxx-cli: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCXX_TARGET) --target x86_64-unknown-rinos -O3 -c -MMD \
		-MF $(TEST_OUT)/cxx_cli_options.d -nostdinc -Itests/include \
		-DRCC_CXX_CLI_VALUE=23 -DRCC_CXX_REMOVE_ME -URCC_CXX_REMOVE_ME \
		-o $(TEST_OUT)/cxx_cli_options.ro tests/cxx_cli_options.cpp
	$(call EXPECT_FAILURE,$(RCC_TARGET) -O4 -c -o $(TEST_OUT)/invalid-o-c.ro tests/hello.c,$(TEST_OUT)/invalid-o-c.log)
	$(GREP) -q 'expected -O0 through -O3' $(TEST_OUT)/invalid-o-c.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) -Wunknown -c -o $(TEST_OUT)/invalid-w-c.ro tests/hello.c,$(TEST_OUT)/invalid-w-c.log)
	$(GREP) -q 'unsupported warning option' $(TEST_OUT)/invalid-w-c.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) -funknown -c -o $(TEST_OUT)/invalid-f-c.ro tests/hello.c,$(TEST_OUT)/invalid-f-c.log)
	$(GREP) -q 'unsupported code-generation option' $(TEST_OUT)/invalid-f-c.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -Ofoo -c -o $(TEST_OUT)/invalid-o-cxx.ro tests/cxx_cli_options.cpp,$(TEST_OUT)/invalid-o-cxx.log)
	$(GREP) -q 'expected -O0 through -O3' $(TEST_OUT)/invalid-o-cxx.log
	@echo "RCC++ command-line compatibility test completed"

test-cxx-language-core: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-language-core)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/new-delete-x86.ro \
		tests/cxx_new_delete.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/new-delete-x64.ro \
		tests/cxx_new_delete.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/new-delete-x86.s \
		tests/cxx_new_delete.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/new-delete-x64.s \
		tests/cxx_new_delete.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/adl-x86.s \
		tests/cxx_adl.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/adl-x64.s \
		tests/cxx_adl.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.s \
		tests/cxx_virtual_dispatch.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.s \
		tests/cxx_virtual_dispatch.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/field-initializers-x86.s \
		tests/cxx_field_initializers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/field-initializers-x64.s \
		tests/cxx_field_initializers.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/inheritance-x86.s \
		tests/cxx_inheritance.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/inheritance-x64.s \
		tests/cxx_inheritance.cpp
ifeq ($(OS),Windows_NT)
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-language-core/new-delete-x86.o \
		$(TEST_OUT)/cxx-language-core/new-delete-x86.s
	objdump -f $(TEST_OUT)/cxx-language-core/new-delete-x86.o > \
		$(TEST_OUT)/cxx-language-core/new-delete-x86.arch
	$(GREP) -F -q "i386" $(TEST_OUT)/cxx-language-core/new-delete-x86.arch
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-language-core/adl-x86.o \
		$(TEST_OUT)/cxx-language-core/adl-x86.s
	objdump -f $(TEST_OUT)/cxx-language-core/adl-x86.o > \
		$(TEST_OUT)/cxx-language-core/adl-x86.arch
	$(GREP) -F -q "i386" $(TEST_OUT)/cxx-language-core/adl-x86.arch
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.o \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.s
	objdump -f $(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.o > \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.arch
	$(GREP) -F -q "i386" $(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.arch
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-language-core/field-initializers-x86.o \
		$(TEST_OUT)/cxx-language-core/field-initializers-x86.s
	objdump -f $(TEST_OUT)/cxx-language-core/field-initializers-x86.o > \
		$(TEST_OUT)/cxx-language-core/field-initializers-x86.arch
	$(GREP) -F -q "i386" $(TEST_OUT)/cxx-language-core/field-initializers-x86.arch
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-language-core/inheritance-x86.o \
		$(TEST_OUT)/cxx-language-core/inheritance-x86.s
	objdump -f $(TEST_OUT)/cxx-language-core/inheritance-x86.o > \
		$(TEST_OUT)/cxx-language-core/inheritance-x86.arch
	$(GREP) -F -q "i386" $(TEST_OUT)/cxx-language-core/inheritance-x86.arch
	$(CC) -c -o $(TEST_OUT)/cxx-language-core/new-delete-x64.o \
		$(TEST_OUT)/cxx-language-core/new-delete-x64.s
	$(OBJCOPY) --redefine-sym main=rcc_language_core_main \
		$(TEST_OUT)/cxx-language-core/new-delete-x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-language-core/new-delete-x64-host \
		tests/cxx_language_core_host.c $(TEST_OUT)/cxx-language-core/new-delete-x64.o
	$(TEST_OUT)/cxx-language-core/new-delete-x64-host
	$(CC) -c -o $(TEST_OUT)/cxx-language-core/adl-x64.o \
		$(TEST_OUT)/cxx-language-core/adl-x64.s
	$(OBJCOPY) --redefine-sym main=rcc_language_core_main \
		$(TEST_OUT)/cxx-language-core/adl-x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-language-core/adl-x64-host \
		tests/cxx_language_core_host.c $(TEST_OUT)/cxx-language-core/adl-x64.o
	$(TEST_OUT)/cxx-language-core/adl-x64-host
	$(CC) -c -o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.o \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.s
	$(OBJCOPY) --redefine-sym main=rcc_language_core_main \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x64-host \
		tests/cxx_language_core_host.c $(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.o
	$(TEST_OUT)/cxx-language-core/virtual-dispatch-x64-host
	$(CC) -c -o $(TEST_OUT)/cxx-language-core/field-initializers-x64.o \
		$(TEST_OUT)/cxx-language-core/field-initializers-x64.s
	$(OBJCOPY) --redefine-sym main=rcc_language_core_main \
		$(TEST_OUT)/cxx-language-core/field-initializers-x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-language-core/field-initializers-x64-host \
		tests/cxx_language_core_host.c $(TEST_OUT)/cxx-language-core/field-initializers-x64.o
	$(TEST_OUT)/cxx-language-core/field-initializers-x64-host
	$(CC) -c -o $(TEST_OUT)/cxx-language-core/inheritance-x64.o \
		$(TEST_OUT)/cxx-language-core/inheritance-x64.s
	$(OBJCOPY) --redefine-sym main=rcc_language_core_main \
		$(TEST_OUT)/cxx-language-core/inheritance-x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-language-core/inheritance-x64-host \
		tests/cxx_language_core_host.c $(TEST_OUT)/cxx-language-core/inheritance-x64.o
	$(TEST_OUT)/cxx-language-core/inheritance-x64-host
else
	bash tests/run_cxx_language_core.sh $(TEST_OUT)/cxx-language-core
endif
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/template-call-x86.ro \
		tests/cxx_template_call.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/template-call-x64.ro \
		tests/cxx_template_call.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/function-templates-x86.ro \
		tests/cxx_function_templates.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/function-templates-x64.ro \
		tests/cxx_function_templates.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/function-specifiers-x86.ro \
		tests/cxx_function_specifiers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/function-specifiers-x64.ro \
		tests/cxx_function_specifiers.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-language-core/invalid-array-new-x86.ro tests/cxx_new_array_invalid.cpp,$(TEST_OUT)/cxx-language-core/invalid-array-new-x86.log)
	$(GREP) -q 'array new has no lowerable constructor for its element initializers' \
		$(TEST_OUT)/cxx-language-core/invalid-array-new-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-language-core/invalid-array-new-x64.ro tests/cxx_new_array_invalid.cpp,$(TEST_OUT)/cxx-language-core/invalid-array-new-x64.log)
	$(GREP) -q 'array new has no lowerable constructor for its element initializers' \
		$(TEST_OUT)/cxx-language-core/invalid-array-new-x64.log
	@echo "RCC++ core language tests completed"

test-cxx-empty-base: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-empty-base)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-empty-base/x86.ro \
		tests/cxx_empty_base.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-empty-base/x64.ro \
		tests/cxx_empty_base.cpp
	@echo "C++ empty-base optimization and same-type overlap tests completed"

test-cxx-no-unique-address: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-no-unique-address)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-no-unique-address/x86.ro \
		tests/cxx_no_unique_address.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-no-unique-address/x64.ro \
		tests/cxx_no_unique_address.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-no-unique-address/invalid-x86.ro \
		tests/cxx_no_unique_address_invalid.cpp,$(TEST_OUT)/cxx-no-unique-address/invalid-x86.log)
	$(call CHECK_NONEMPTY,$(TEST_OUT)/cxx-no-unique-address/invalid-x86.log)
	$(GREP) -q "requires C[+][+]20 or newer" \
		$(TEST_OUT)/cxx-no-unique-address/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-no-unique-address/invalid-x64.ro \
		tests/cxx_no_unique_address_invalid.cpp,$(TEST_OUT)/cxx-no-unique-address/invalid-x64.log)
	$(call CHECK_NONEMPTY,$(TEST_OUT)/cxx-no-unique-address/invalid-x64.log)
	$(GREP) -q "requires C[+][+]20 or newer" \
		$(TEST_OUT)/cxx-no-unique-address/invalid-x64.log
	@echo "C++20 no_unique_address layout and version diagnostics completed"

test-cxx-multiple-inheritance-virtual: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-multiple-inheritance-virtual)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-multiple-inheritance-virtual,test,cxx_multiple_inheritance_virtual.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x86.s \
		tests/cxx_multiple_inheritance_virtual.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x86.o \
		$(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-multiple-inheritance-virtual/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x86 \
		$(TEST_OUT)/cxx-multiple-inheritance-virtual/start-x86.o \
		$(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x86.o
	$(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x64.s \
		tests/cxx_multiple_inheritance_virtual.cpp
	gcc -c -o $(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x64.o \
		$(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x64.s
	gcc -c -o $(TEST_OUT)/cxx-multiple-inheritance-virtual/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x64 \
		$(TEST_OUT)/cxx-multiple-inheritance-virtual/start-x64.o \
		$(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x64.o
	$(TEST_OUT)/cxx-multiple-inheritance-virtual/test-x64
endif
	@echo "C++ secondary virtual-base vptr test completed"

test-cxx-secondary-virtual-override: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-secondary-virtual-override)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-secondary-virtual-override,test,cxx_secondary_virtual_override.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-secondary-virtual-override/test-x86.s \
		tests/cxx_secondary_virtual_override.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-secondary-virtual-override/test-x86.o \
		$(TEST_OUT)/cxx-secondary-virtual-override/test-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-secondary-virtual-override/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-secondary-virtual-override/test-x86 \
		$(TEST_OUT)/cxx-secondary-virtual-override/start-x86.o \
		$(TEST_OUT)/cxx-secondary-virtual-override/test-x86.o
	$(TEST_OUT)/cxx-secondary-virtual-override/test-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-secondary-virtual-override/test-x64.s \
		tests/cxx_secondary_virtual_override.cpp
	gcc -c -o $(TEST_OUT)/cxx-secondary-virtual-override/test-x64.o \
		$(TEST_OUT)/cxx-secondary-virtual-override/test-x64.s
	gcc -c -o $(TEST_OUT)/cxx-secondary-virtual-override/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-secondary-virtual-override/test-x64 \
		$(TEST_OUT)/cxx-secondary-virtual-override/start-x64.o \
		$(TEST_OUT)/cxx-secondary-virtual-override/test-x64.o
	$(TEST_OUT)/cxx-secondary-virtual-override/test-x64
endif
	@echo "C++ secondary virtual override execution test completed"

test-cxx-virtual-base: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-virtual-base)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-virtual-base,test,cxx_virtual_base.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base/test-x86.s \
		tests/cxx_virtual_base.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-virtual-base/test-x86.o \
		$(TEST_OUT)/cxx-virtual-base/test-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-virtual-base/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base/test-x86 \
		$(TEST_OUT)/cxx-virtual-base/start-x86.o \
		$(TEST_OUT)/cxx-virtual-base/test-x86.o
	$(TEST_OUT)/cxx-virtual-base/test-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base/test-x64.s \
		tests/cxx_virtual_base.cpp
	gcc -c -o $(TEST_OUT)/cxx-virtual-base/test-x64.o \
		$(TEST_OUT)/cxx-virtual-base/test-x64.s
	gcc -c -o $(TEST_OUT)/cxx-virtual-base/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base/test-x64 \
		$(TEST_OUT)/cxx-virtual-base/start-x64.o \
		$(TEST_OUT)/cxx-virtual-base/test-x64.o
	$(TEST_OUT)/cxx-virtual-base/test-x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base/test-virtual-x86.s \
		tests/cxx_virtual_base_virtual.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-virtual-base/test-virtual-x86.o \
		$(TEST_OUT)/cxx-virtual-base/test-virtual-x86.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base/test-virtual-x86 \
		$(TEST_OUT)/cxx-virtual-base/start-x86.o \
		$(TEST_OUT)/cxx-virtual-base/test-virtual-x86.o
	$(TEST_OUT)/cxx-virtual-base/test-virtual-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base/test-virtual-x64.s \
		tests/cxx_virtual_base_virtual.cpp
	gcc -c -o $(TEST_OUT)/cxx-virtual-base/test-virtual-x64.o \
		$(TEST_OUT)/cxx-virtual-base/test-virtual-x64.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base/test-virtual-x64 \
		$(TEST_OUT)/cxx-virtual-base/start-x64.o \
		$(TEST_OUT)/cxx-virtual-base/test-virtual-x64.o
	$(TEST_OUT)/cxx-virtual-base/test-virtual-x64
endif
	@echo "C++ direct virtual-base layout and dispatch tests completed"

test-cxx-virtual-base-conversion: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-virtual-base-conversion)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-virtual-base-conversion,test,cxx_virtual_base_conversion.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base-conversion/x86.s \
		tests/cxx_virtual_base_conversion.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-virtual-base-conversion/x86.o \
		$(TEST_OUT)/cxx-virtual-base-conversion/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-virtual-base-conversion/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base-conversion/x86 \
		$(TEST_OUT)/cxx-virtual-base-conversion/start-x86.o \
		$(TEST_OUT)/cxx-virtual-base-conversion/x86.o
	$(TEST_OUT)/cxx-virtual-base-conversion/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base-conversion/x64.s \
		tests/cxx_virtual_base_conversion.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-virtual-base-conversion/x64.o \
		$(TEST_OUT)/cxx-virtual-base-conversion/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-virtual-base-conversion/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base-conversion/x64 \
		$(TEST_OUT)/cxx-virtual-base-conversion/start-x64.o \
		$(TEST_OUT)/cxx-virtual-base-conversion/x64.o
	$(TEST_OUT)/cxx-virtual-base-conversion/x64
endif
	@echo "C++ virtual-base conversion tests completed"

test-cxx-virtual-base-constructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-virtual-base-constructor)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-virtual-base-constructor,test,cxx_virtual_base_constructor.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base-constructor/x86.s \
		tests/cxx_virtual_base_constructor.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-virtual-base-constructor/x86.o \
		$(TEST_OUT)/cxx-virtual-base-constructor/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-virtual-base-constructor/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base-constructor/x86 \
		$(TEST_OUT)/cxx-virtual-base-constructor/start-x86.o \
		$(TEST_OUT)/cxx-virtual-base-constructor/x86.o
	$(TEST_OUT)/cxx-virtual-base-constructor/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base-constructor/x64.s \
		tests/cxx_virtual_base_constructor.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-virtual-base-constructor/x64.o \
		$(TEST_OUT)/cxx-virtual-base-constructor/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-virtual-base-constructor/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base-constructor/x64 \
		$(TEST_OUT)/cxx-virtual-base-constructor/start-x64.o \
		$(TEST_OUT)/cxx-virtual-base-constructor/x64.o
	$(TEST_OUT)/cxx-virtual-base-constructor/x64
endif
	@echo "C++ virtual-base constructor tests completed"

test-cxx-constructor-general: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constructor-general)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-constructor-general,test,cxx_constructor_general.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constructor-general/x86.s \
		tests/cxx_constructor_general.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constructor-general/x86.o \
		$(TEST_OUT)/cxx-constructor-general/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constructor-general/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constructor-general/x86 \
		$(TEST_OUT)/cxx-constructor-general/start-x86.o \
		$(TEST_OUT)/cxx-constructor-general/x86.o
	$(TEST_OUT)/cxx-constructor-general/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constructor-general/x64.s \
		tests/cxx_constructor_general.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-constructor-general/x64.o \
		$(TEST_OUT)/cxx-constructor-general/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-constructor-general/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constructor-general/x64 \
		$(TEST_OUT)/cxx-constructor-general/start-x64.o \
		$(TEST_OUT)/cxx-constructor-general/x64.o
	$(TEST_OUT)/cxx-constructor-general/x64
endif
	@echo "C++ general constructor-body tests completed"

test-cxx-implicit-copy: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-implicit-copy)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-implicit-copy,test,cxx_implicit_copy.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-implicit-copy/x86.s \
		tests/cxx_implicit_copy.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-implicit-copy/x86.o \
		$(TEST_OUT)/cxx-implicit-copy/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-implicit-copy/start-x86.o \
		tests/cxx_new_delete_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-implicit-copy/x86 \
		$(TEST_OUT)/cxx-implicit-copy/start-x86.o \
		$(TEST_OUT)/cxx-implicit-copy/x86.o
	$(TEST_OUT)/cxx-implicit-copy/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-implicit-copy/x64.s \
		tests/cxx_implicit_copy.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-implicit-copy/x64.o \
		$(TEST_OUT)/cxx-implicit-copy/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-implicit-copy/start-x64.o \
		tests/cxx_new_delete_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-implicit-copy/x64 \
		$(TEST_OUT)/cxx-implicit-copy/start-x64.o \
		$(TEST_OUT)/cxx-implicit-copy/x64.o
	$(TEST_OUT)/cxx-implicit-copy/x64
endif
	@echo "C++ implicit copy-construction tests completed"

test-cxx-protected-member: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-protected-member)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-protected-member,test,cxx_protected_member.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-protected-member/x86.s \
		tests/cxx_protected_member.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-protected-member/x86.o \
		$(TEST_OUT)/cxx-protected-member/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-protected-member/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-protected-member/x86 \
		$(TEST_OUT)/cxx-protected-member/start-x86.o \
		$(TEST_OUT)/cxx-protected-member/x86.o
	$(TEST_OUT)/cxx-protected-member/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-protected-member/x64.s \
		tests/cxx_protected_member.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-protected-member/x64.o \
		$(TEST_OUT)/cxx-protected-member/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-protected-member/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-protected-member/x64 \
		$(TEST_OUT)/cxx-protected-member/start-x64.o \
		$(TEST_OUT)/cxx-protected-member/x64.o
	$(TEST_OUT)/cxx-protected-member/x64
endif
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-protected-member/rejected-x86.ro tests/cxx_protected_member_rejected.cpp,$(TEST_OUT)/cxx-protected-member/rejected-x86.log)
	$(GREP) -q "member 'counter' is not accessible" \
		$(TEST_OUT)/cxx-protected-member/rejected-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-protected-member/rejected-x64.ro tests/cxx_protected_member_rejected.cpp,$(TEST_OUT)/cxx-protected-member/rejected-x64.log)
	$(GREP) -q "member 'counter' is not accessible" \
		$(TEST_OUT)/cxx-protected-member/rejected-x64.log
	@echo "C++ protected-member access tests completed"

test-cxx-virtual-base-constructor-order: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-virtual-base-constructor-order)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-virtual-base-constructor-order,test,cxx_virtual_base_constructor_order.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base-constructor-order/x86.s \
		tests/cxx_virtual_base_constructor_order.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-virtual-base-constructor-order/x86.o \
		$(TEST_OUT)/cxx-virtual-base-constructor-order/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-virtual-base-constructor-order/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base-constructor-order/x86 \
		$(TEST_OUT)/cxx-virtual-base-constructor-order/start-x86.o \
		$(TEST_OUT)/cxx-virtual-base-constructor-order/x86.o
	$(TEST_OUT)/cxx-virtual-base-constructor-order/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-virtual-base-constructor-order/x64.s \
		tests/cxx_virtual_base_constructor_order.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-virtual-base-constructor-order/x64.o \
		$(TEST_OUT)/cxx-virtual-base-constructor-order/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-virtual-base-constructor-order/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-virtual-base-constructor-order/x64 \
		$(TEST_OUT)/cxx-virtual-base-constructor-order/start-x64.o \
		$(TEST_OUT)/cxx-virtual-base-constructor-order/x64.o
	$(TEST_OUT)/cxx-virtual-base-constructor-order/x64
endif
	@echo "C++ virtual-base construction-order tests completed"

test-cxx-shared-virtual-base: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-shared-virtual-base)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-shared-virtual-base,test,cxx_shared_virtual_base.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-shared-virtual-base/test-x86.s \
		tests/cxx_shared_virtual_base.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-shared-virtual-base/test-x86.o \
		$(TEST_OUT)/cxx-shared-virtual-base/test-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-shared-virtual-base/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-shared-virtual-base/test-x86 \
		$(TEST_OUT)/cxx-shared-virtual-base/start-x86.o \
		$(TEST_OUT)/cxx-shared-virtual-base/test-x86.o
	$(TEST_OUT)/cxx-shared-virtual-base/test-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-shared-virtual-base/test-x64.s \
		tests/cxx_shared_virtual_base.cpp
	gcc -c -o $(TEST_OUT)/cxx-shared-virtual-base/test-x64.o \
		$(TEST_OUT)/cxx-shared-virtual-base/test-x64.s
	gcc -c -o $(TEST_OUT)/cxx-shared-virtual-base/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-shared-virtual-base/test-x64 \
		$(TEST_OUT)/cxx-shared-virtual-base/start-x64.o \
		$(TEST_OUT)/cxx-shared-virtual-base/test-x64.o
	$(TEST_OUT)/cxx-shared-virtual-base/test-x64
endif
	@echo "C++ shared virtual-base diamond test completed"

test-cxx-shared-virtual-base-method: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-shared-virtual-base-method)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-shared-virtual-base-method,test,cxx_shared_virtual_base_method.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-shared-virtual-base-method/test-x86.s \
		tests/cxx_shared_virtual_base_method.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-shared-virtual-base-method/test-x86.o \
		$(TEST_OUT)/cxx-shared-virtual-base-method/test-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-shared-virtual-base-method/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-shared-virtual-base-method/test-x86 \
		$(TEST_OUT)/cxx-shared-virtual-base-method/start-x86.o \
		$(TEST_OUT)/cxx-shared-virtual-base-method/test-x86.o
	$(TEST_OUT)/cxx-shared-virtual-base-method/test-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-shared-virtual-base-method/test-x64.s \
		tests/cxx_shared_virtual_base_method.cpp
	gcc -c -o $(TEST_OUT)/cxx-shared-virtual-base-method/test-x64.o \
		$(TEST_OUT)/cxx-shared-virtual-base-method/test-x64.s
	gcc -c -o $(TEST_OUT)/cxx-shared-virtual-base-method/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-shared-virtual-base-method/test-x64 \
		$(TEST_OUT)/cxx-shared-virtual-base-method/start-x64.o \
		$(TEST_OUT)/cxx-shared-virtual-base-method/test-x64.o
	$(TEST_OUT)/cxx-shared-virtual-base-method/test-x64
endif
	@echo "C++ shared virtual-base member dispatch test completed"

test-cxx-destructor-body: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-destructor-body)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-destructor-body,test,cxx_destructor_body.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-destructor-body/x86.s \
		tests/cxx_destructor_body.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-destructor-body/x86.o \
		$(TEST_OUT)/cxx-destructor-body/x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-destructor-body/start-x86.o \
		tests/cxx_new_delete_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-destructor-body/x86 \
		$(TEST_OUT)/cxx-destructor-body/start-x86.o \
		$(TEST_OUT)/cxx-destructor-body/x86.o
	$(TEST_OUT)/cxx-destructor-body/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-destructor-body/x64.s \
		tests/cxx_destructor_body.cpp
	gcc -c -o $(TEST_OUT)/cxx-destructor-body/x64.o \
		$(TEST_OUT)/cxx-destructor-body/x64.s
	gcc -c -o $(TEST_OUT)/cxx-destructor-body/start-x64.o \
		tests/cxx_new_delete_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-destructor-body/x64 \
		$(TEST_OUT)/cxx-destructor-body/start-x64.o \
		$(TEST_OUT)/cxx-destructor-body/x64.o
	$(TEST_OUT)/cxx-destructor-body/x64
endif
	@echo "C++ explicit destructor body and lifetime tests completed"

test-cxx-default-destructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-default-destructor)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-default-destructor,test,cxx_default_destructor.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-default-destructor/x86.s \
		tests/cxx_default_destructor.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-default-destructor/x86.o \
		$(TEST_OUT)/cxx-default-destructor/x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-default-destructor/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-default-destructor/x86 \
		$(TEST_OUT)/cxx-default-destructor/start-x86.o \
		$(TEST_OUT)/cxx-default-destructor/x86.o
	$(TEST_OUT)/cxx-default-destructor/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-default-destructor/x64.s \
		tests/cxx_default_destructor.cpp
	gcc -c -o $(TEST_OUT)/cxx-default-destructor/x64.o \
		$(TEST_OUT)/cxx-default-destructor/x64.s
	gcc -c -o $(TEST_OUT)/cxx-default-destructor/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-default-destructor/x64 \
		$(TEST_OUT)/cxx-default-destructor/start-x64.o \
		$(TEST_OUT)/cxx-default-destructor/x64.o
	$(TEST_OUT)/cxx-default-destructor/x64
endif
	@echo "C++ default-constructor destructor lifetime tests completed"

test-cxx-member-lifetime: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-lifetime)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-member-lifetime,test,cxx_member_lifetime.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-lifetime/x86.s \
		tests/cxx_member_lifetime.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-member-lifetime/x86.o \
		$(TEST_OUT)/cxx-member-lifetime/x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-member-lifetime/start-x86.o \
		tests/cxx_new_delete_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-member-lifetime/x86 \
		$(TEST_OUT)/cxx-member-lifetime/start-x86.o \
		$(TEST_OUT)/cxx-member-lifetime/x86.o
	$(TEST_OUT)/cxx-member-lifetime/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-lifetime/x64.s \
		tests/cxx_member_lifetime.cpp
	gcc -c -o $(TEST_OUT)/cxx-member-lifetime/x64.o \
		$(TEST_OUT)/cxx-member-lifetime/x64.s
	gcc -c -o $(TEST_OUT)/cxx-member-lifetime/start-x64.o \
		tests/cxx_new_delete_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-member-lifetime/x64 \
		$(TEST_OUT)/cxx-member-lifetime/start-x64.o \
		$(TEST_OUT)/cxx-member-lifetime/x64.o
	$(TEST_OUT)/cxx-member-lifetime/x64
endif
	@echo "C++ nested member construction and destruction tests completed"

test-cxx-array-destructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-array-destructor)
ifeq ($(OS),Windows_NT)
	$(call CXX_WINDOWS_MAIN,cxx-array-destructor,test,cxx_array_destructor.cpp)
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-array-destructor/x86.s \
		tests/cxx_array_destructor.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-array-destructor/x86.o \
		$(TEST_OUT)/cxx-array-destructor/x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-array-destructor/start-x86.o \
		tests/cxx_array_destructor_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-array-destructor/x86 \
		$(TEST_OUT)/cxx-array-destructor/start-x86.o \
		$(TEST_OUT)/cxx-array-destructor/x86.o
	$(TEST_OUT)/cxx-array-destructor/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-array-destructor/x64.s \
		tests/cxx_array_destructor.cpp
	gcc -c -o $(TEST_OUT)/cxx-array-destructor/x64.o \
		$(TEST_OUT)/cxx-array-destructor/x64.s
	gcc -c -o $(TEST_OUT)/cxx-array-destructor/start-x64.o \
		tests/cxx_array_destructor_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-array-destructor/x64 \
		$(TEST_OUT)/cxx-array-destructor/start-x64.o \
		$(TEST_OUT)/cxx-array-destructor/x64.o
	$(TEST_OUT)/cxx-array-destructor/x64
endif
	@echo "C++ array destructor cookie and reverse-lifetime tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-constexpr: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr)
	$(call CXX_WINDOWS_CONSTEXPR_TEST,cxx-constexpr,cxx_constexpr.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constexpr/invalid-x86.ro tests/cxx_constexpr_invalid.cpp,$(TEST_OUT)/cxx-constexpr/invalid-x86.log)
	$(GREP) -F -q "constexpr variable requires an initializer" $(TEST_OUT)/cxx-constexpr/invalid-x86.log
	$(GREP) -F -q "constexpr variable initializer is not a supported constant expression" $(TEST_OUT)/cxx-constexpr/invalid-x86.log
	$(GREP) -F -q "consteval call is not a constant expression" $(TEST_OUT)/cxx-constexpr/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constexpr/invalid-x64.ro tests/cxx_constexpr_invalid.cpp,$(TEST_OUT)/cxx-constexpr/invalid-x64.log)
	$(GREP) -F -q "constexpr variable requires an initializer" $(TEST_OUT)/cxx-constexpr/invalid-x64.log
	$(GREP) -F -q "constexpr variable initializer is not a supported constant expression" $(TEST_OUT)/cxx-constexpr/invalid-x64.log
	$(GREP) -F -q "consteval call is not a constant expression" $(TEST_OUT)/cxx-constexpr/invalid-x64.log
	@echo "RCC++ scalar constexpr folding tests completed"
else
test-cxx-constexpr: test-cxx-constexpr-posix
endif

test-cxx-constexpr-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr/x86.s tests/cxx_constexpr.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr/x64.s tests/cxx_constexpr.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-constexpr/x86.o \
		$(TEST_OUT)/cxx-constexpr/x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-constexpr/host-x86.o \
		tests/cxx_constexpr_host.c
	objcopy --redefine-sym main=rcc_cxx_constexpr_main \
		$(TEST_OUT)/cxx-constexpr/x86.o
	gcc -m32 -no-pie -o $(TEST_OUT)/cxx-constexpr/x86 \
		$(TEST_OUT)/cxx-constexpr/host-x86.o \
		$(TEST_OUT)/cxx-constexpr/x86.o
	$(TEST_OUT)/cxx-constexpr/x86
	gcc -c -o $(TEST_OUT)/cxx-constexpr/x64.o \
		$(TEST_OUT)/cxx-constexpr/x64.s
	gcc -c -o $(TEST_OUT)/cxx-constexpr/host-x64.o \
		tests/cxx_constexpr_host.c
	objcopy --redefine-sym main=rcc_cxx_constexpr_main \
		$(TEST_OUT)/cxx-constexpr/x64.o
	gcc -no-pie -o $(TEST_OUT)/cxx-constexpr/x64 \
		$(TEST_OUT)/cxx-constexpr/host-x64.o \
		$(TEST_OUT)/cxx-constexpr/x64.o
	$(TEST_OUT)/cxx-constexpr/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constexpr/invalid-x86.ro \
		tests/cxx_constexpr_invalid.cpp \
		>$(TEST_OUT)/cxx-constexpr/invalid-x86.log 2>&1
	$(GREP) -q "constexpr variable requires an initializer" \
		$(TEST_OUT)/cxx-constexpr/invalid-x86.log
	$(GREP) -q "constexpr variable initializer is not a supported constant expression" \
		$(TEST_OUT)/cxx-constexpr/invalid-x86.log
	$(GREP) -q "consteval call is not a constant expression" \
		$(TEST_OUT)/cxx-constexpr/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constexpr/invalid-x64.ro \
		tests/cxx_constexpr_invalid.cpp \
		>$(TEST_OUT)/cxx-constexpr/invalid-x64.log 2>&1
	$(GREP) -q "constexpr variable requires an initializer" \
		$(TEST_OUT)/cxx-constexpr/invalid-x64.log
	$(GREP) -q "constexpr variable initializer is not a supported constant expression" \
		$(TEST_OUT)/cxx-constexpr/invalid-x64.log
	$(GREP) -q "consteval call is not a constant expression" \
		$(TEST_OUT)/cxx-constexpr/invalid-x64.log
	@echo "RCC++ scalar constexpr folding tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-constexpr-aggregate: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr-aggregate)
	$(call CXX_WINDOWS_CONSTEXPR_TEST,cxx-constexpr-aggregate,cxx_constexpr_aggregate.cpp)
	@echo "RCC++ aggregate constexpr tests completed"
else
test-cxx-constexpr-aggregate: test-cxx-constexpr-aggregate-posix
endif

test-cxx-constexpr-aggregate-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr-aggregate)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr-aggregate/x86.s \
		tests/cxx_constexpr_aggregate.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr-aggregate/x64.s \
		tests/cxx_constexpr_aggregate.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-constexpr-aggregate/x86.o \
		$(TEST_OUT)/cxx-constexpr-aggregate/x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-constexpr-aggregate/host-x86.o \
		tests/cxx_constexpr_host.c
	objcopy --redefine-sym main=rcc_cxx_constexpr_main \
		$(TEST_OUT)/cxx-constexpr-aggregate/x86.o
	gcc -m32 -no-pie -o $(TEST_OUT)/cxx-constexpr-aggregate/x86 \
		$(TEST_OUT)/cxx-constexpr-aggregate/host-x86.o \
		$(TEST_OUT)/cxx-constexpr-aggregate/x86.o
	$(TEST_OUT)/cxx-constexpr-aggregate/x86
	gcc -c -o $(TEST_OUT)/cxx-constexpr-aggregate/x64.o \
		$(TEST_OUT)/cxx-constexpr-aggregate/x64.s
	gcc -c -o $(TEST_OUT)/cxx-constexpr-aggregate/host-x64.o \
		tests/cxx_constexpr_host.c
	objcopy --redefine-sym main=rcc_cxx_constexpr_main \
		$(TEST_OUT)/cxx-constexpr-aggregate/x64.o
	gcc -no-pie -o $(TEST_OUT)/cxx-constexpr-aggregate/x64 \
		$(TEST_OUT)/cxx-constexpr-aggregate/host-x64.o \
		$(TEST_OUT)/cxx-constexpr-aggregate/x64.o
	$(TEST_OUT)/cxx-constexpr-aggregate/x64
	@echo "RCC++ aggregate constexpr tests completed"

test-pic-direct-internal: $(RCC_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/pic-direct-internal)
	$(RCC_TARGET) --target i686-unknown-rinos -fPIC --emit-unsigned-v3 \
		-o $(TEST_OUT)/pic-direct-internal/x86.rin \
		tests/pic_direct_internal.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -fPIE --emit-unsigned-v3 \
		-o $(TEST_OUT)/pic-direct-internal/x64.rin \
		tests/pic_direct_internal.c
	$(RCC_TARGET) --target i686-unknown-rinos -fPIC -shared --emit-unsigned-v3 \
		-o $(TEST_OUT)/pic-direct-internal/x86.rll \
		tests/pic_direct_internal.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -fPIC -shared --emit-unsigned-v3 \
		-o $(TEST_OUT)/pic-direct-internal/x64.rll \
		tests/pic_direct_internal.c
	$(RCC_TARGET) --target i686-unknown-rinos -fPIC --emit-unsigned-v3 \
		-driver -o $(TEST_OUT)/pic-direct-internal/x86.drv \
		tests/pic_direct_internal.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -fPIE --emit-unsigned-v3 \
		-driver -o $(TEST_OUT)/pic-direct-internal/x64.drv \
		tests/pic_direct_internal.c
	$(RINVALIDATE) --allow-unsigned --kind executable --arch x86 \
		$(TEST_OUT)/pic-direct-internal/x86.rin
	$(RINVALIDATE) --allow-unsigned --kind executable --arch x86_64 \
		$(TEST_OUT)/pic-direct-internal/x64.rin
	$(RINVALIDATE) --allow-unsigned --kind library --arch x86 \
		$(TEST_OUT)/pic-direct-internal/x86.rll
	$(RINVALIDATE) --allow-unsigned --kind library --arch x86_64 \
		$(TEST_OUT)/pic-direct-internal/x64.rll
	$(RINVALIDATE) --allow-unsigned --kind driver --arch x86 \
		$(TEST_OUT)/pic-direct-internal/x86.drv
	$(RINVALIDATE) --allow-unsigned --kind driver --arch x86_64 \
		$(TEST_OUT)/pic-direct-internal/x64.drv
	@echo "Direct PIC/PIE RIN/RLL/NDRV GOT materialization tests completed"

test-pic-tls: $(RCC_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/pic-tls)
	$(RCC_TARGET) --target i686-unknown-rinos -fPIC --emit-unsigned-v3 \
		-o $(TEST_OUT)/pic-tls/x86.rin tests/tls.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -fPIE --emit-unsigned-v3 \
		-o $(TEST_OUT)/pic-tls/x64.rin tests/tls.c
	$(RCC_TARGET) --target i686-unknown-rinos -fPIC -c \
		-o $(TEST_OUT)/pic-tls/x86.ro tests/tls.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -fPIE -c \
		-o $(TEST_OUT)/pic-tls/x64.ro tests/tls.c
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/pic-tls/linked-x86.rin \
		$(TEST_OUT)/pic-tls/x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/pic-tls/linked-x64.rin \
		$(TEST_OUT)/pic-tls/x64.ro
	$(RINVALIDATE) --allow-unsigned --kind executable --arch x86 \
		$(TEST_OUT)/pic-tls/x86.rin
	$(RINVALIDATE) --allow-unsigned --kind executable --arch x86_64 \
		$(TEST_OUT)/pic-tls/x64.rin
	$(RINVALIDATE) --allow-unsigned --kind executable --arch x86 \
		$(TEST_OUT)/pic-tls/linked-x86.rin
	$(RINVALIDATE) --allow-unsigned --kind executable --arch x86_64 \
		$(TEST_OUT)/pic-tls/linked-x64.rin
	@echo "PIC/PIE local-exec TLS relocation tests completed"

test-cxx-new-array: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-new-array)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-new-array/x86.ro tests/cxx_new_array.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-new-array/x64.ro tests/cxx_new_array.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-new-array/constructor-x86.ro \
		tests/cxx_new_array_constructor.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-new-array/constructor-x64.ro \
		tests/cxx_new_array_constructor.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-new-array/constructor-x64.s \
		tests/cxx_new_array_constructor.cpp
	$(CC) -o $(TEST_OUT)/cxx-new-array/constructor-run-test \
		tests/cxx_new_array_constructor_run_test.c \
		$(TEST_OUT)/cxx-new-array/constructor-x64.s
	$(TEST_OUT)/cxx-new-array/constructor-run-test
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-new-array/invalid.ro tests/cxx_new_array_parenthesized_rejected.cpp,$(TEST_OUT)/cxx-new-array/invalid.log)
	$(GREP) -F -q "array new element initializers require braces" $(TEST_OUT)/cxx-new-array/invalid.log
else
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-new-array/invalid.ro \
		tests/cxx_new_array_parenthesized_rejected.cpp \
		>$(TEST_OUT)/cxx-new-array/invalid.log 2>&1; then \
		echo "parenthesized array-new initializer unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "array new element initializers require braces" \
		$(TEST_OUT)/cxx-new-array/invalid.log
endif

	@echo "RCC++ scalar and constructor array-new initializer tests completed"

test-cxx-language-linkage: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-language-linkage)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-linkage/x86.ro \
		tests/cxx_language_linkage.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-linkage/x64.ro \
		tests/cxx_language_linkage.cpp
	$(call CHECK_TEXT,_Z21call_language_linkagei,$(TEST_OUT)/cxx-language-linkage/x86.ro)
	$(call CHECK_TEXT,_Z18cpp_linkage_importi,$(TEST_OUT)/cxx-language-linkage/x64.ro)
	$(call CHECK_TEXT,_Z19cpp_linkage_counter,$(TEST_OUT)/cxx-language-linkage/x64.ro)
	$(call CHECK_TEXT,linkage_import,$(TEST_OUT)/cxx-language-linkage/x86.ro)
	$(call CHECK_TEXT,linkage_aggregate_member,$(TEST_OUT)/cxx-language-linkage/x86.ro)
	$(call CHECK_TEXT,second_linkage_import,$(TEST_OUT)/cxx-language-linkage/x86.ro)
	$(call CHECK_TEXT,c_linkage_counter,$(TEST_OUT)/cxx-language-linkage/x64.ro)
	$(call CHECK_TEXT,scoped_linkage_function,$(TEST_OUT)/cxx-language-linkage/x86.ro)
	$(call CHECK_TEXT,scoped_c_counter,$(TEST_OUT)/cxx-language-linkage/x64.ro)
	$(call CHECK_TEXT,_ZN14scoped_linkage18scoped_cpp_counterE,$(TEST_OUT)/cxx-language-linkage/x64.ro)
	$(call CHECK_TEXT,_ZN14scoped_linkage19call_scoped_linkageEi,$(TEST_OUT)/cxx-language-linkage/x64.ro)
	$(call CHECK_TEXT,_ZN14scoped_linkage17scoped_cpp_importEi,$(TEST_OUT)/cxx-language-linkage/x64.ro)
	$(call CHECK_TEXT,_Z29call_qualified_scoped_linkagei,$(TEST_OUT)/cxx-language-linkage/x64.ro)
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -Command "if (Select-String -Quiet -SimpleMatch '_Z14linkage_importi' '$(TEST_OUT)/cxx-language-linkage/x64.ro') { exit 1 }; if (Select-String -Quiet -SimpleMatch '_ZN14scoped_linkage23scoped_linkage_functionEi' '$(TEST_OUT)/cxx-language-linkage/x64.ro') { exit 1 }; if (Select-String -Quiet -SimpleMatch '_ZN14scoped_linkage16scoped_c_counterE' '$(TEST_OUT)/cxx-language-linkage/x64.ro') { exit 1 }"
else
	! strings $(TEST_OUT)/cxx-language-linkage/x64.ro | $(GREP) -x -q '_Z14linkage_importi'
	! strings $(TEST_OUT)/cxx-language-linkage/x64.ro | $(GREP) -x -q '_ZN14scoped_linkage23scoped_linkage_functionEi'
	! strings $(TEST_OUT)/cxx-language-linkage/x64.ro | $(GREP) -x -q '_ZN14scoped_linkage16scoped_c_counterE'
endif
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-language-linkage,cxx_language_linkage.cpp)
	@echo "RCC++ C/C++ language-linkage tests completed"

test-cxx-member-specifiers: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-specifiers)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-member-specifiers/x86.ro \
		tests/cxx_member_specifiers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-member-specifiers/x64.ro \
		tests/cxx_member_specifiers.cpp
	@echo "RCC++ class member specifier tests completed"

test-cxx-function-templates: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-function-templates)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-function-templates/x86.ro \
		tests/cxx_function_templates.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-function-templates/x64.ro \
		tests/cxx_function_templates.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-function-templates/x86.s \
		tests/cxx_function_templates.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-function-templates/x86.o \
		$(TEST_OUT)/cxx-function-templates/x86.s
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-function-templates/x64.s \
		tests/cxx_function_templates.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-function-templates/x64.o \
		$(TEST_OUT)/cxx-function-templates/x64.s
	$(call CHECK_BINARY_STRING,_ZN8identityEi,$(TEST_OUT)/cxx-function-templates/x86.ro)
	$(call CHECK_BINARY_STRING,_ZN8identityEl,$(TEST_OUT)/cxx-function-templates/x86.ro)
	$(call CHECK_BINARY_STRING,_ZN6detail16pointer_identityEPi,$(TEST_OUT)/cxx-function-templates/x86.ro)
	$(call CHECK_BINARY_STRING,_ZN6detail15default_deducedEIlEi,$(TEST_OUT)/cxx-function-templates/x86.ro)
	$(call CHECK_BINARY_STRING,_ZN6detail18type_only_templateEIiEv,$(TEST_OUT)/cxx-function-templates/x86.ro)
	$(call CHECK_BINARY_STRING,_ZN6detail18type_only_templateEIlEv,$(TEST_OUT)/cxx-function-templates/x86.ro)
	@echo "RCC++ function template syntax tests completed"

test-cxx-template-local-classes: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-local-classes)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-local-classes/x86.ro \
		tests/cxx_template_local_classes.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-local-classes/x64.ro \
		tests/cxx_template_local_classes.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-local-classes/x64.s \
		tests/cxx_template_local_classes.cpp
	$(call CHECK_TEXT_ABSENT,should_not_be_lowered,$(TEST_OUT)/cxx-template-local-classes/x64.s)
	$(CC) -no-pie -o $(TEST_OUT)/cxx-template-local-classes/run \
		$(TEST_OUT)/cxx-template-local-classes/x64.s
	$(TEST_OUT)/cxx-template-local-classes/run
	@echo "RCC++ function-template local class specialization tests completed"

test-cxx-variable-templates-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-variable-templates)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-variable-templates/x86.s \
		tests/cxx_variable_templates.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-variable-templates/x86.o \
		$(TEST_OUT)/cxx-variable-templates/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-variable-templates/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-variable-templates/x86 \
		$(TEST_OUT)/cxx-variable-templates/start-x86.o \
		$(TEST_OUT)/cxx-variable-templates/x86.o
	$(TEST_OUT)/cxx-variable-templates/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-variable-templates/x64.s \
		tests/cxx_variable_templates.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-variable-templates/x64.o \
		$(TEST_OUT)/cxx-variable-templates/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-variable-templates/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-variable-templates/x64 \
		$(TEST_OUT)/cxx-variable-templates/start-x64.o \
		$(TEST_OUT)/cxx-variable-templates/x64.o
	$(TEST_OUT)/cxx-variable-templates/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++11 -c \
		-o $(TEST_OUT)/cxx-variable-templates/invalid-x86.ro \
		tests/cxx_variable_templates_invalid.cpp \
		>$(TEST_OUT)/cxx-variable-templates/invalid-x86.log 2>&1; then \
		echo "C++11 variable template unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "variable templates require C++14 or newer" \
		$(TEST_OUT)/cxx-variable-templates/invalid-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++11 -c \
		-o $(TEST_OUT)/cxx-variable-templates/invalid-x64.ro \
		tests/cxx_variable_templates_invalid.cpp \
		>$(TEST_OUT)/cxx-variable-templates/invalid-x64.log 2>&1; then \
		echo "C++11 variable template unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "variable templates require C++14 or newer" \
		$(TEST_OUT)/cxx-variable-templates/invalid-x64.log
	@echo "RCC++ variable-template specialization tests completed"

test-cxx-function-template-overloads-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-function-template-overloads)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-function-template-overloads/x86.s \
		tests/cxx_function_template_overloads.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-function-template-overloads/x86.o \
		$(TEST_OUT)/cxx-function-template-overloads/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-function-template-overloads/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-function-template-overloads/x86 \
		$(TEST_OUT)/cxx-function-template-overloads/start-x86.o \
		$(TEST_OUT)/cxx-function-template-overloads/x86.o
	$(TEST_OUT)/cxx-function-template-overloads/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-function-template-overloads/x64.s \
		tests/cxx_function_template_overloads.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-function-template-overloads/x64.o \
		$(TEST_OUT)/cxx-function-template-overloads/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-function-template-overloads/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-function-template-overloads/x64 \
		$(TEST_OUT)/cxx-function-template-overloads/start-x64.o \
		$(TEST_OUT)/cxx-function-template-overloads/x64.o
	$(TEST_OUT)/cxx-function-template-overloads/x64
	@set +e; $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x86.ro \
		tests/cxx_function_template_overloads_partial_order_invalid.cpp \
		>$(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x86.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	$(GREP) -q "ambiguous function template overload for 'select_template'" \
		$(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x86.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x64.ro \
		tests/cxx_function_template_overloads_partial_order_invalid.cpp \
		>$(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x64.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	$(GREP) -q "ambiguous function template overload for 'select_template'" \
		$(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x64.log
	@echo "RCC++ function-template overload and expression-deduction tests completed"

test-cxx-function-template-references-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-function-template-references)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-function-template-references/x86.s \
		tests/cxx_function_template_references.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-function-template-references/x86.o \
		$(TEST_OUT)/cxx-function-template-references/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-function-template-references/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-function-template-references/x86 \
		$(TEST_OUT)/cxx-function-template-references/start-x86.o \
		$(TEST_OUT)/cxx-function-template-references/x86.o
	$(TEST_OUT)/cxx-function-template-references/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-function-template-references/x64.s \
		tests/cxx_function_template_references.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-function-template-references/x64.o \
		$(TEST_OUT)/cxx-function-template-references/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-function-template-references/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-function-template-references/x64 \
		$(TEST_OUT)/cxx-function-template-references/start-x64.o \
		$(TEST_OUT)/cxx-function-template-references/x64.o
	$(TEST_OUT)/cxx-function-template-references/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-function-template-references/invalid.ro \
		tests/cxx_function_template_references_invalid.cpp \
		>$(TEST_OUT)/cxx-function-template-references/invalid.log 2>&1
	$(GREP) -q "no matching function template overload for 'read_rvalue'" \
		$(TEST_OUT)/cxx-function-template-references/invalid.log
	@echo "RCC++ function-template reference and function-pointer deduction tests completed"

test-cxx-static-reference-temporaries-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-reference-temporaries)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-DRCC_STATIC_REFERENCE_TLS_TEST -S \
		-o $(TEST_OUT)/cxx-static-reference-temporaries/x86.s \
		tests/cxx_static_reference_temporaries.cpp
	$(GREP) -F -q "__cxa_thread_atexit" \
		$(TEST_OUT)/cxx-static-reference-temporaries/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-temporaries/x86.o \
		$(TEST_OUT)/cxx-static-reference-temporaries/x86.s
	objdump -f $(TEST_OUT)/cxx-static-reference-temporaries/x86.o \
		> $(TEST_OUT)/cxx-static-reference-temporaries/x86-arch.log
	$(GREP) -F -q "elf32-i386" \
		$(TEST_OUT)/cxx-static-reference-temporaries/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-DRCC_STATIC_REFERENCE_TLS_TEST -S \
		-o $(TEST_OUT)/cxx-static-reference-temporaries/x64.s \
		tests/cxx_static_reference_temporaries.cpp
	$(GREP) -F -q "__cxa_thread_atexit" \
		$(TEST_OUT)/cxx-static-reference-temporaries/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-temporaries/x64.o \
		$(TEST_OUT)/cxx-static-reference-temporaries/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main \
		$(TEST_OUT)/cxx-static-reference-temporaries/x64.o
	$(CC) $(CFLAGS) -DRCC_STATIC_REFERENCE_TLS_TEST -pthread -no-pie \
		-o $(TEST_OUT)/cxx-static-reference-temporaries/x64-host \
		tests/cxx_static_reference_host.c \
		$(TEST_OUT)/cxx-static-reference-temporaries/x64.o -lstdc++
	$(TEST_OUT)/cxx-static-reference-temporaries/x64-host

test-cxx-static-reference-retry-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-reference-retry)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-reference-retry/x86.s \
		tests/cxx_static_reference_retry.cpp
	$(GREP) -F -q "__cxa_guard_abort" \
		$(TEST_OUT)/cxx-static-reference-retry/x86.s
	$(GREP) -F -q "rin_cpp_exception_register_current_cleanup" \
		$(TEST_OUT)/cxx-static-reference-retry/x86.s
	$(GREP) -F -q "rin_cpp_exception_unregister_current_cleanup" \
		$(TEST_OUT)/cxx-static-reference-retry/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-retry/x86.o \
		$(TEST_OUT)/cxx-static-reference-retry/x86.s
	objdump -f $(TEST_OUT)/cxx-static-reference-retry/x86.o \
		> $(TEST_OUT)/cxx-static-reference-retry/x86-arch.log
	$(GREP) -F -q "elf32-i386" \
		$(TEST_OUT)/cxx-static-reference-retry/x86-arch.log
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-retry/x86-start.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-retry/x86-guard.o \
		tests/cxx_static_reference_guard_runtime.c
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-static-reference-retry/x86 \
		$(TEST_OUT)/cxx-static-reference-retry/x86-start.o \
		$(TEST_OUT)/cxx-static-reference-retry/x86-guard.o \
		$(TEST_OUT)/cxx-static-reference-retry/x86.o
	$(TEST_OUT)/cxx-static-reference-retry/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-reference-retry/x64.s \
		tests/cxx_static_reference_retry.cpp
	$(GREP) -F -q "__cxa_guard_abort" \
		$(TEST_OUT)/cxx-static-reference-retry/x64.s
	$(GREP) -F -q "rin_cpp_exception_register_current_cleanup" \
		$(TEST_OUT)/cxx-static-reference-retry/x64.s
	$(GREP) -F -q "rin_cpp_exception_unregister_current_cleanup" \
		$(TEST_OUT)/cxx-static-reference-retry/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-retry/x64.o \
		$(TEST_OUT)/cxx-static-reference-retry/x64.s
	objdump -f $(TEST_OUT)/cxx-static-reference-retry/x64.o \
		> $(TEST_OUT)/cxx-static-reference-retry/x64-arch.log
	$(GREP) -F -q "elf64-x86-64" \
		$(TEST_OUT)/cxx-static-reference-retry/x64-arch.log
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-retry/x64-start.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-retry/x64-guard.o \
		tests/cxx_static_reference_guard_runtime.c
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-static-reference-retry/x64 \
		$(TEST_OUT)/cxx-static-reference-retry/x64-start.o \
		$(TEST_OUT)/cxx-static-reference-retry/x64-guard.o \
		$(TEST_OUT)/cxx-static-reference-retry/x64.o
	$(TEST_OUT)/cxx-static-reference-retry/x64
	@echo "C++ static reference guard abort and retry execution passed for i686 and AMD64"

test-cxx-static-local-exception-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-local-exception)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-local-exception/x86.s \
		tests/cxx_static_local_exception.cpp
	$(GREP) -F -q "__cxa_guard_abort" \
		$(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "__cxa_atexit" \
		$(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "__cxa_thread_atexit" \
		$(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "rin_cpp_exception_register_current_cleanup" \
		$(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "rin_cpp_exception_unregister_current_cleanup" \
		$(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "__rcc_static_3_value@NTPOFF" \
		$(TEST_OUT)/cxx-static-local-exception/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-local-exception/x86.o \
		$(TEST_OUT)/cxx-static-local-exception/x86.s
	objdump -r $(TEST_OUT)/cxx-static-local-exception/x86.o \
		> $(TEST_OUT)/cxx-static-local-exception/x86-reloc.log
	$(GREP) -E -q "R_386_TLS_LE.*__rcc_static_3_value" \
		$(TEST_OUT)/cxx-static-local-exception/x86-reloc.log
	objdump -f $(TEST_OUT)/cxx-static-local-exception/x86.o \
		> $(TEST_OUT)/cxx-static-local-exception/x86-arch.log
	$(GREP) -F -q "elf32-i386" \
		$(TEST_OUT)/cxx-static-local-exception/x86-arch.log
	$(CC) -m32 -c \
		-o $(TEST_OUT)/cxx-static-local-exception/x86-start.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -c \
		-o $(TEST_OUT)/cxx-static-local-exception/x86-runtime.o \
		-fno-stack-protector tests/cxx_static_local_exception_runtime.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-S \
		-o $(TEST_OUT)/cxx-static-local-exception/x86-run.s \
		tests/cxx_static_local_exception.cpp
	$(CC) -m32 -c \
		-o $(TEST_OUT)/cxx-static-local-exception/x86-run.o \
		$(TEST_OUT)/cxx-static-local-exception/x86-run.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-static-local-exception/x86 \
		$(TEST_OUT)/cxx-static-local-exception/x86-start.o \
		$(TEST_OUT)/cxx-static-local-exception/x86-runtime.o \
		$(TEST_OUT)/cxx-static-local-exception/x86-run.o
	$(TEST_OUT)/cxx-static-local-exception/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-local-exception/x64.s \
		tests/cxx_static_local_exception.cpp
	$(GREP) -F -q "__cxa_guard_abort" \
		$(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "__cxa_atexit" \
		$(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "__cxa_thread_atexit" \
		$(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "rin_cpp_exception_register_current_cleanup" \
		$(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "rin_cpp_exception_unregister_current_cleanup" \
		$(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "__rcc_static_3_value@TPOFF" \
		$(TEST_OUT)/cxx-static-local-exception/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-static-local-exception/x64.o \
		$(TEST_OUT)/cxx-static-local-exception/x64.s
	objdump -r $(TEST_OUT)/cxx-static-local-exception/x64.o \
		> $(TEST_OUT)/cxx-static-local-exception/x64-reloc.log
	$(GREP) -E -q "R_X86_64_TPOFF32.*__rcc_static_3_value" \
		$(TEST_OUT)/cxx-static-local-exception/x64-reloc.log
	objdump -f $(TEST_OUT)/cxx-static-local-exception/x64.o \
		> $(TEST_OUT)/cxx-static-local-exception/x64-arch.log
	$(GREP) -F -q "elf64-x86-64" \
		$(TEST_OUT)/cxx-static-local-exception/x64-arch.log
	$(CC) -c \
		-o $(TEST_OUT)/cxx-static-local-exception/x64-start.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -c \
		-o $(TEST_OUT)/cxx-static-local-exception/x64-runtime.o \
		-fno-stack-protector tests/cxx_static_local_exception_runtime.c
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-S \
		-o $(TEST_OUT)/cxx-static-local-exception/x64-run.s \
		tests/cxx_static_local_exception.cpp
	$(CC) -c \
		-o $(TEST_OUT)/cxx-static-local-exception/x64-run.o \
		$(TEST_OUT)/cxx-static-local-exception/x64-run.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-static-local-exception/x64 \
		$(TEST_OUT)/cxx-static-local-exception/x64-start.o \
		$(TEST_OUT)/cxx-static-local-exception/x64-runtime.o \
		$(TEST_OUT)/cxx-static-local-exception/x64-run.o
	$(TEST_OUT)/cxx-static-local-exception/x64
	@echo "C++ static exception retry/finalization and local-exec TLS runtime/relocations passed for i686 and AMD64"

test-cxx-static-reference-subobjects-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-reference-subobjects)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-reference-subobjects/x86.s \
		tests/cxx_static_reference_subobjects.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-subobjects/x86.o \
		$(TEST_OUT)/cxx-static-reference-subobjects/x86.s
	objdump -f $(TEST_OUT)/cxx-static-reference-subobjects/x86.o \
		> $(TEST_OUT)/cxx-static-reference-subobjects/x86-arch.log
	$(GREP) -F -q "elf32-i386" \
		$(TEST_OUT)/cxx-static-reference-subobjects/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-reference-subobjects/x64.s \
		tests/cxx_static_reference_subobjects.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-subobjects/x64.o \
		$(TEST_OUT)/cxx-static-reference-subobjects/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main \
		$(TEST_OUT)/cxx-static-reference-subobjects/x64.o
	$(CC) $(CFLAGS) -no-pie \
		-o $(TEST_OUT)/cxx-static-reference-subobjects/x64-host \
		tests/cxx_static_reference_host.c \
		$(TEST_OUT)/cxx-static-reference-subobjects/x64.o
	$(TEST_OUT)/cxx-static-reference-subobjects/x64-host
	@echo "C++ static reference subobject lifetime tests passed for i686 and AMD64"

test-cxx-static-reference-conversions-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-reference-conversions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-reference-conversions/x86.s \
		tests/cxx_static_reference_conversions.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-conversions/x86.o \
		$(TEST_OUT)/cxx-static-reference-conversions/x86.s
	objdump -f $(TEST_OUT)/cxx-static-reference-conversions/x86.o \
		> $(TEST_OUT)/cxx-static-reference-conversions/x86-arch.log
	$(GREP) -F -q "elf32-i386" \
		$(TEST_OUT)/cxx-static-reference-conversions/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-reference-conversions/x64.s \
		tests/cxx_static_reference_conversions.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-conversions/x64.o \
		$(TEST_OUT)/cxx-static-reference-conversions/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main \
		$(TEST_OUT)/cxx-static-reference-conversions/x64.o
	$(CC) $(CFLAGS) -no-pie \
		-o $(TEST_OUT)/cxx-static-reference-conversions/x64-host \
		tests/cxx_static_reference_host.c \
		$(TEST_OUT)/cxx-static-reference-conversions/x64.o
	$(TEST_OUT)/cxx-static-reference-conversions/x64-host
	@echo "C++ static reference conversion lifetime tests passed for i686 and AMD64"

test-cxx-member-pointer-data-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-pointer-data)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-data/x86.s \
		tests/cxx_member_pointer_data.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-pointer-data/x86.o \
		$(TEST_OUT)/cxx-member-pointer-data/x86.s
	objdump -f $(TEST_OUT)/cxx-member-pointer-data/x86.o \
		> $(TEST_OUT)/cxx-member-pointer-data/x86-arch.log
	$(GREP) -F -q "elf32-i386" \
		$(TEST_OUT)/cxx-member-pointer-data/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-data/x64.s \
		tests/cxx_member_pointer_data.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-member-pointer-data/x64.o \
		$(TEST_OUT)/cxx-member-pointer-data/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main \
		$(TEST_OUT)/cxx-member-pointer-data/x64.o
	$(CC) $(CFLAGS) -no-pie \
		-o $(TEST_OUT)/cxx-member-pointer-data/x64-host \
		tests/cxx_static_reference_host.c \
		$(TEST_OUT)/cxx-member-pointer-data/x64.o
	$(TEST_OUT)/cxx-member-pointer-data/x64-host
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-member-pointer-data/ir-x86.ro \
		tests/cxx_member_pointer_ir.cpp \
		> $(TEST_OUT)/cxx-member-pointer-data/ir-x86.log
	$(GREP) -F -q "Verified backend: 7 function(s) emitted" \
		$(TEST_OUT)/cxx-member-pointer-data/ir-x86.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-member-pointer-data/ir-x64.ro \
		tests/cxx_member_pointer_ir.cpp \
		> $(TEST_OUT)/cxx-member-pointer-data/ir-x64.log
	$(GREP) -F -q "Verified backend: 7 function(s) emitted" \
		$(TEST_OUT)/cxx-member-pointer-data/ir-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.ro tests/cxx_member_pointer_data_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log)
	$(GREP) -F -q "data-member pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log
	$(GREP) -F -q "member-pointer application requires one public base subobject path" $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log
	$(GREP) -F -q "invalid pointer-to-member conversion in initialization" $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log
	$(GREP) -F -q "assignment requires modifiable lvalue" $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.ro tests/cxx_member_pointer_data_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log)
	$(GREP) -F -q "data-member pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log
	$(GREP) -F -q "member-pointer application requires one public base subobject path" $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log
	$(GREP) -F -q "invalid pointer-to-member conversion in initialization" $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log
	$(GREP) -F -q "assignment requires modifiable lvalue" $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log
	@echo "C++ data member-pointer operations and static subobject lifetime tests passed"

test-cxx-member-pointer-functions-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-pointer-functions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-functions/x86.s \
		tests/cxx_member_pointer_functions.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-pointer-functions/x86.o \
		$(TEST_OUT)/cxx-member-pointer-functions/x86.s
	objdump -f $(TEST_OUT)/cxx-member-pointer-functions/x86.o \
		> $(TEST_OUT)/cxx-member-pointer-functions/x86-arch.log
	$(GREP) -F -q "elf32-i386" \
		$(TEST_OUT)/cxx-member-pointer-functions/x86-arch.log
	$(GREP) -F -q "_Z10invoke_dotR19MemberFunctionOwnerMS_FiiEi" \
		$(TEST_OUT)/cxx-member-pointer-functions/x86.s
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-functions/x64.s \
		tests/cxx_member_pointer_functions.cpp
	g++ -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-functions/gcc-x64.s \
		tests/cxx_member_pointer_functions.cpp
	$(GREP) -F -q "_Z22invoke_double_overloadR19MemberFunctionOwnerMS_FidEd" \
		$(TEST_OUT)/cxx-member-pointer-functions/gcc-x64.s
	$(GREP) -F -q "_Z12invoke_arrowP19MemberFunctionOwnerMS_FiiEi" \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z12invoke_constRK19MemberFunctionOwnerMS_KFiiEi" \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z15invoke_volatileRV19MemberFunctionOwnerMS_VFiiEi" \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z9invoke_cvRVK19MemberFunctionOwnerMS_VKFiiEi" \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z22invoke_double_overloadR19MemberFunctionOwnerMS_FidEd" \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z23invoke_inherited_memberR30InheritedMemberFunctionDerivedM27InheritedMemberFunctionBaseFiiEi" \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-member-pointer-functions/x64.o \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.o
	$(CC) $(CFLAGS) -no-pie \
		-o $(TEST_OUT)/cxx-member-pointer-functions/x64-host \
		tests/cxx_language_core_host.c \
		$(TEST_OUT)/cxx-member-pointer-functions/x64.o
	$(TEST_OUT)/cxx-member-pointer-functions/x64-host
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86.ro \
		tests/cxx_member_pointer_functions_ir.cpp \
		> $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86.log 2>&1
	$(GREP) -F -q "Verified backend: 9 function(s) emitted" \
		$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64.ro \
		tests/cxx_member_pointer_functions_ir.cpp \
		> $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64.log 2>&1
	$(GREP) -F -q "Verified backend: 9 function(s) emitted" \
		$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86-o2.ro \
		tests/cxx_member_pointer_functions_ir.cpp \
		> $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86-o2.log 2>&1
	$(GREP) -F -q "Verified backend: 9 function(s) emitted" \
		$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86-o2.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64-o2.ro \
		tests/cxx_member_pointer_functions_ir.cpp \
		> $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64-o2.log 2>&1
	$(GREP) -F -q "Verified backend: 9 function(s) emitted" \
		$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64-o2.log,0)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/invalid-x86.ro tests/cxx_member_pointer_functions_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/invalid-x86.log)
	$(GREP) -F -q "invalid pointer-to-member conversion in initialization" $(TEST_OUT)/cxx-member-pointer-functions/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/invalid-x64.ro tests/cxx_member_pointer_functions_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/invalid-x64.log)
	$(GREP) -F -q "invalid pointer-to-member conversion in initialization" $(TEST_OUT)/cxx-member-pointer-functions/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x86.ro tests/cxx_member_pointer_functions_inherited_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x86.log)
	$(GREP) -F -q "member-pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x64.ro tests/cxx_member_pointer_functions_inherited_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x64.log)
	$(GREP) -F -q "member-pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x86.ro tests/cxx_member_pointer_functions_overload_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x86.log)
	$(GREP) -F -q "no matching member-function overload 'apply' for member-pointer target" $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x86.log
	$(GREP) -F -q "overloaded member-function address requires a member-pointer target type" $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x64.ro tests/cxx_member_pointer_functions_overload_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x64.log)
	$(GREP) -F -q "no matching member-function overload 'apply' for member-pointer target" $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x64.log
	$(GREP) -F -q "overloaded member-function address requires a member-pointer target type" $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x64.log
	@echo "C++ non-virtual member-function pointer calls passed for i686 and AMD64"

test-cxx-constraints-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constraints)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constraints/x86.s \
		tests/cxx_constraints.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constraints/x86.o \
		$(TEST_OUT)/cxx-constraints/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constraints/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constraints/x86 \
		$(TEST_OUT)/cxx-constraints/start-x86.o \
		$(TEST_OUT)/cxx-constraints/x86.o
	$(TEST_OUT)/cxx-constraints/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constraints/x64.s \
		tests/cxx_constraints.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-constraints/x64.o \
		$(TEST_OUT)/cxx-constraints/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-constraints/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constraints/x64 \
		$(TEST_OUT)/cxx-constraints/start-x64.o \
		$(TEST_OUT)/cxx-constraints/x64.o
	$(TEST_OUT)/cxx-constraints/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constraints/invalid-x86.ro \
		tests/cxx_constraints_invalid.cpp \
		>$(TEST_OUT)/cxx-constraints/invalid-x86.log 2>&1; then \
		echo "invalid constrained template unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-constraints/invalid-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constraints/invalid-x64.ro \
		tests/cxx_constraints_invalid.cpp \
		>$(TEST_OUT)/cxx-constraints/invalid-x64.log 2>&1; then \
		echo "invalid constrained template unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-constraints/invalid-x64.log
	@echo "RCC++ integral template constraint tests completed"

test-cxx-named-concepts-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-named-concepts)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-named-concepts/x86.s \
		tests/cxx_named_concepts.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-named-concepts/x86.o \
		$(TEST_OUT)/cxx-named-concepts/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-named-concepts/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-named-concepts/x86 \
		$(TEST_OUT)/cxx-named-concepts/start-x86.o \
		$(TEST_OUT)/cxx-named-concepts/x86.o
	$(TEST_OUT)/cxx-named-concepts/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-named-concepts/x64.s \
		tests/cxx_named_concepts.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-named-concepts/x64.o \
		$(TEST_OUT)/cxx-named-concepts/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-named-concepts/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-named-concepts/x64 \
		$(TEST_OUT)/cxx-named-concepts/start-x64.o \
		$(TEST_OUT)/cxx-named-concepts/x64.o
	$(TEST_OUT)/cxx-named-concepts/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-named-concepts/invalid-x86.ro \
		tests/cxx_named_concepts_invalid.cpp \
		>$(TEST_OUT)/cxx-named-concepts/invalid-x86.log 2>&1; then \
		echo "parameter-pack named concept unexpectedly compiled on i686"; exit 1; \
	fi
	$(GREP) -q "named concepts do not support parameter packs" \
		$(TEST_OUT)/cxx-named-concepts/invalid-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-named-concepts/invalid-x64.ro \
		tests/cxx_named_concepts_invalid.cpp \
		>$(TEST_OUT)/cxx-named-concepts/invalid-x64.log 2>&1; then \
		echo "parameter-pack named concept unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "named concepts do not support parameter packs" \
		$(TEST_OUT)/cxx-named-concepts/invalid-x64.log
	@echo "RCC++ bounded named concept tests completed"

test-cxx-alias-templates-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-alias-templates)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-alias-templates/x86.s \
		tests/cxx_alias_templates.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-alias-templates/x86.o \
		$(TEST_OUT)/cxx-alias-templates/x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-alias-templates/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-alias-templates/x86 \
		$(TEST_OUT)/cxx-alias-templates/start-x86.o \
		$(TEST_OUT)/cxx-alias-templates/x86.o
	$(TEST_OUT)/cxx-alias-templates/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-alias-templates/x64.s \
		tests/cxx_alias_templates.cpp
	gcc -c -o $(TEST_OUT)/cxx-alias-templates/x64.o \
		$(TEST_OUT)/cxx-alias-templates/x64.s
	gcc -c -o $(TEST_OUT)/cxx-alias-templates/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-alias-templates/x64 \
		$(TEST_OUT)/cxx-alias-templates/start-x64.o \
		$(TEST_OUT)/cxx-alias-templates/x64.o
	$(TEST_OUT)/cxx-alias-templates/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-alias-templates/invalid-x86.ro \
		tests/cxx_alias_templates_invalid.cpp \
		>$(TEST_OUT)/cxx-alias-templates/invalid-x86.log 2>&1
	$(GREP) -q "alias template parameter packs are not supported" \
		$(TEST_OUT)/cxx-alias-templates/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-alias-templates/invalid-x64.ro \
		tests/cxx_alias_templates_invalid.cpp \
		>$(TEST_OUT)/cxx-alias-templates/invalid-x64.log 2>&1
	$(GREP) -q "alias template parameter packs are not supported" \
		$(TEST_OUT)/cxx-alias-templates/invalid-x64.log
	@echo "RCC++ bounded alias template tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-variable-templates: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-variable-templates)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-variable-templates,cxx_variable_templates.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++11 -c -o $(TEST_OUT)/cxx-variable-templates/invalid-x86.ro tests/cxx_variable_templates_invalid.cpp,$(TEST_OUT)/cxx-variable-templates/invalid-x86.log)
	$(GREP) -F -q "variable templates require C++14 or newer" $(TEST_OUT)/cxx-variable-templates/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++11 -c -o $(TEST_OUT)/cxx-variable-templates/invalid-x64.ro tests/cxx_variable_templates_invalid.cpp,$(TEST_OUT)/cxx-variable-templates/invalid-x64.log)
	$(GREP) -F -q "variable templates require C++14 or newer" $(TEST_OUT)/cxx-variable-templates/invalid-x64.log

test-cxx-function-template-overloads: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-function-template-overloads)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-function-template-overloads,cxx_function_template_overloads.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x86.ro tests/cxx_function_template_overloads_partial_order_invalid.cpp,$(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x86.log)
	$(GREP) -F -q "ambiguous function template overload for 'select_template'" $(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x64.ro tests/cxx_function_template_overloads_partial_order_invalid.cpp,$(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x64.log)
	$(GREP) -F -q "ambiguous function template overload for 'select_template'" $(TEST_OUT)/cxx-function-template-overloads/partial-order-invalid-x64.log

test-cxx-function-template-references: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-function-template-references)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-function-template-references,cxx_function_template_references.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-function-template-references/non-template-invalid-x86.ro tests/cxx_reference_binding_invalid.cpp,$(TEST_OUT)/cxx-function-template-references/non-template-invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-function-template-references/non-template-invalid-x64.ro tests/cxx_reference_binding_invalid.cpp,$(TEST_OUT)/cxx-function-template-references/non-template-invalid-x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-function-template-references/invalid.ro tests/cxx_function_template_references_invalid.cpp,$(TEST_OUT)/cxx-function-template-references/invalid.log)
	$(GREP) -F -q "no matching function template overload for 'read_rvalue'" $(TEST_OUT)/cxx-function-template-references/invalid.log

test-cxx-return-semantics: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-return-semantics)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-return-semantics,cxx_return_semantics.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-return-semantics/invalid-x86.ro tests/cxx_return_semantics_invalid.cpp,$(TEST_OUT)/cxx-return-semantics/invalid-x86.log)
	$(call CHECK_COUNT,invalid C++ reference binding in return,$(TEST_OUT)/cxx-return-semantics/invalid-x86.log,2)
	$(call CHECK_COUNT,incompatible C++ return type,$(TEST_OUT)/cxx-return-semantics/invalid-x86.log,1)
	$(call CHECK_COUNT,return expression in a C++ void function must have void type,$(TEST_OUT)/cxx-return-semantics/invalid-x86.log,1)
	$(call CHECK_COUNT,return statement in a non-void C++ function requires a value,$(TEST_OUT)/cxx-return-semantics/invalid-x86.log,1)
	$(call ASSERT_ABSENT,$(TEST_OUT)/cxx-return-semantics/invalid-x86.ro)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-return-semantics/invalid-x64.ro tests/cxx_return_semantics_invalid.cpp,$(TEST_OUT)/cxx-return-semantics/invalid-x64.log)
	$(call CHECK_COUNT,invalid C++ reference binding in return,$(TEST_OUT)/cxx-return-semantics/invalid-x64.log,2)
	$(call CHECK_COUNT,incompatible C++ return type,$(TEST_OUT)/cxx-return-semantics/invalid-x64.log,1)
	$(call CHECK_COUNT,return expression in a C++ void function must have void type,$(TEST_OUT)/cxx-return-semantics/invalid-x64.log,1)
	$(call CHECK_COUNT,return statement in a non-void C++ function requires a value,$(TEST_OUT)/cxx-return-semantics/invalid-x64.log,1)
	$(call ASSERT_ABSENT,$(TEST_OUT)/cxx-return-semantics/invalid-x64.ro)

test-c-return-semantics: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/c-return-semantics)
	$(call C_WINDOWS_ENTRY_TEST,c-return-semantics,c_return_semantics.c)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c -o $(TEST_OUT)/c-return-semantics/invalid-x86.ro tests/c_return_semantics_invalid.c,$(TEST_OUT)/c-return-semantics/invalid-x86.log)
	$(call CHECK_COUNT,return expression in a void function is not allowed,$(TEST_OUT)/c-return-semantics/invalid-x86.log,1)
	$(call CHECK_COUNT,return statement in a non-void function requires a value,$(TEST_OUT)/c-return-semantics/invalid-x86.log,1)
	$(call ASSERT_ABSENT,$(TEST_OUT)/c-return-semantics/invalid-x86.ro)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c -o $(TEST_OUT)/c-return-semantics/invalid-x64.ro tests/c_return_semantics_invalid.c,$(TEST_OUT)/c-return-semantics/invalid-x64.log)
	$(call CHECK_COUNT,return expression in a void function is not allowed,$(TEST_OUT)/c-return-semantics/invalid-x64.log,1)
	$(call CHECK_COUNT,return statement in a non-void function requires a value,$(TEST_OUT)/c-return-semantics/invalid-x64.log,1)
	$(call ASSERT_ABSENT,$(TEST_OUT)/c-return-semantics/invalid-x64.ro)
	@echo "C17 return statement constraint tests completed"

test-cxx-static-reference-temporaries: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-reference-temporaries)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-reference-temporaries/x86.s tests/cxx_static_reference_temporaries.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-temporaries/x86.o $(TEST_OUT)/cxx-static-reference-temporaries/x86.s
	objdump -f $(TEST_OUT)/cxx-static-reference-temporaries/x86.o > $(TEST_OUT)/cxx-static-reference-temporaries/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-static-reference-temporaries/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-reference-temporaries/x64.s tests/cxx_static_reference_temporaries.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-temporaries/x64.o $(TEST_OUT)/cxx-static-reference-temporaries/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/cxx-static-reference-temporaries/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-static-reference-temporaries/x64-host tests/cxx_static_reference_host.c $(TEST_OUT)/cxx-static-reference-temporaries/x64.o
	$(TEST_OUT)/cxx-static-reference-temporaries/x64-host

test-cxx-static-reference-retry: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-reference-retry)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-reference-retry/x86.s tests/cxx_static_reference_retry.cpp
	$(GREP) -F -q "__cxa_guard_abort" $(TEST_OUT)/cxx-static-reference-retry/x86.s
	$(GREP) -F -q "rin_cpp_exception_register_current_cleanup" $(TEST_OUT)/cxx-static-reference-retry/x86.s
	$(GREP) -F -q "rin_cpp_exception_unregister_current_cleanup" $(TEST_OUT)/cxx-static-reference-retry/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-retry/x86.o $(TEST_OUT)/cxx-static-reference-retry/x86.s
	objdump -f $(TEST_OUT)/cxx-static-reference-retry/x86.o > $(TEST_OUT)/cxx-static-reference-retry/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-static-reference-retry/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-reference-retry/x64.s tests/cxx_static_reference_retry.cpp
	$(GREP) -F -q "__cxa_guard_abort" $(TEST_OUT)/cxx-static-reference-retry/x64.s
	$(GREP) -F -q "rin_cpp_exception_register_current_cleanup" $(TEST_OUT)/cxx-static-reference-retry/x64.s
	$(GREP) -F -q "rin_cpp_exception_unregister_current_cleanup" $(TEST_OUT)/cxx-static-reference-retry/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-retry/x64.o $(TEST_OUT)/cxx-static-reference-retry/x64.s
	objdump -f $(TEST_OUT)/cxx-static-reference-retry/x64.o > $(TEST_OUT)/cxx-static-reference-retry/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-static-reference-retry/x64-arch.log
	$(CC) $(CFLAGS) -c -o $(TEST_OUT)/cxx-static-reference-retry/guard-runtime.o tests/cxx_static_reference_guard_runtime.c

test-cxx-static-local-exception: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-local-exception)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-local-exception/x86.s tests/cxx_static_local_exception.cpp
	$(GREP) -F -q "__cxa_guard_abort" $(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "__cxa_atexit" $(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "__cxa_thread_atexit" $(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "rin_cpp_exception_register_current_cleanup" $(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "rin_cpp_exception_unregister_current_cleanup" $(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q ".section .tdata" $(TEST_OUT)/cxx-static-local-exception/x86.s
	$(GREP) -F -q "@NTPOFF" $(TEST_OUT)/cxx-static-local-exception/x86.s
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-local-exception/x64.s tests/cxx_static_local_exception.cpp
	$(GREP) -F -q "__cxa_guard_abort" $(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "__cxa_atexit" $(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "__cxa_thread_atexit" $(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "rin_cpp_exception_register_current_cleanup" $(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "rin_cpp_exception_unregister_current_cleanup" $(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q ".section .tdata" $(TEST_OUT)/cxx-static-local-exception/x64.s
	$(GREP) -F -q "@TPOFF" $(TEST_OUT)/cxx-static-local-exception/x64.s

test-cxx-static-reference-subobjects: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-reference-subobjects)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-reference-subobjects/x86.s tests/cxx_static_reference_subobjects.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-subobjects/x86.o $(TEST_OUT)/cxx-static-reference-subobjects/x86.s
	objdump -f $(TEST_OUT)/cxx-static-reference-subobjects/x86.o > $(TEST_OUT)/cxx-static-reference-subobjects/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-static-reference-subobjects/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-reference-subobjects/x64.s tests/cxx_static_reference_subobjects.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-subobjects/x64.o $(TEST_OUT)/cxx-static-reference-subobjects/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/cxx-static-reference-subobjects/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-static-reference-subobjects/x64-host tests/cxx_static_reference_host.c $(TEST_OUT)/cxx-static-reference-subobjects/x64.o
	$(TEST_OUT)/cxx-static-reference-subobjects/x64-host

test-cxx-static-reference-conversions: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-reference-conversions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-reference-conversions/x86.s tests/cxx_static_reference_conversions.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-reference-conversions/x86.o $(TEST_OUT)/cxx-static-reference-conversions/x86.s
	objdump -f $(TEST_OUT)/cxx-static-reference-conversions/x86.o > $(TEST_OUT)/cxx-static-reference-conversions/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-static-reference-conversions/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-static-reference-conversions/x64.s tests/cxx_static_reference_conversions.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-static-reference-conversions/x64.o $(TEST_OUT)/cxx-static-reference-conversions/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/cxx-static-reference-conversions/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-static-reference-conversions/x64-host tests/cxx_static_reference_host.c $(TEST_OUT)/cxx-static-reference-conversions/x64.o
	$(TEST_OUT)/cxx-static-reference-conversions/x64-host

test-cxx-member-pointer-reference-lifetime: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-pointer-reference-lifetime)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-reference-lifetime/x86.s \
		tests/cxx_member_pointer_reference_lifetime.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-pointer-reference-lifetime/x86.o \
		$(TEST_OUT)/cxx-member-pointer-reference-lifetime/x86.s
	objdump -f $(TEST_OUT)/cxx-member-pointer-reference-lifetime/x86.o \
		> $(TEST_OUT)/cxx-member-pointer-reference-lifetime/x86-arch.log
	$(GREP) -F -q "pe-i386" \
		$(TEST_OUT)/cxx-member-pointer-reference-lifetime/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-reference-lifetime/x64.s \
		tests/cxx_member_pointer_reference_lifetime.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-member-pointer-reference-lifetime/x64.o \
		$(TEST_OUT)/cxx-member-pointer-reference-lifetime/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main \
		$(TEST_OUT)/cxx-member-pointer-reference-lifetime/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-member-pointer-reference-lifetime/x64-host \
		tests/cxx_static_reference_host.c \
		$(TEST_OUT)/cxx-member-pointer-reference-lifetime/x64.o
	$(TEST_OUT)/cxx-member-pointer-reference-lifetime/x64-host
	@echo "C++ pointer-to-member static reference lifetime tests completed"

test-cxx-member-pointer-data: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-pointer-data)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-data/x86.s \
		tests/cxx_member_pointer_data.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-pointer-data/x86.o \
		$(TEST_OUT)/cxx-member-pointer-data/x86.s
	objdump -f $(TEST_OUT)/cxx-member-pointer-data/x86.o \
		> $(TEST_OUT)/cxx-member-pointer-data/x86-arch.log
	$(GREP) -F -q "pe-i386" \
		$(TEST_OUT)/cxx-member-pointer-data/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-pointer-data/x64.s \
		tests/cxx_member_pointer_data.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-member-pointer-data/x64.o \
		$(TEST_OUT)/cxx-member-pointer-data/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main \
		$(TEST_OUT)/cxx-member-pointer-data/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-member-pointer-data/lifetime-host \
		tests/cxx_static_reference_host.c \
		$(TEST_OUT)/cxx-member-pointer-data/x64.o
	$(TEST_OUT)/cxx-member-pointer-data/lifetime-host
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend \
		-v -c -o $(TEST_OUT)/cxx-member-pointer-data/ir-x86.ro \
		tests/cxx_member_pointer_ir.cpp \
		> $(TEST_OUT)/cxx-member-pointer-data/ir-x86.log
	$(GREP) -F -q "Verified backend: 7 function(s) emitted" \
		$(TEST_OUT)/cxx-member-pointer-data/ir-x86.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -fverified-backend \
		-v -c -o $(TEST_OUT)/cxx-member-pointer-data/ir-x64.ro \
		tests/cxx_member_pointer_ir.cpp \
		> $(TEST_OUT)/cxx-member-pointer-data/ir-x64.log
	$(GREP) -F -q "Verified backend: 7 function(s) emitted" \
		$(TEST_OUT)/cxx-member-pointer-data/ir-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.ro tests/cxx_member_pointer_data_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log)
	$(GREP) -F -q "data-member pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log
	$(GREP) -F -q "member-pointer application requires one public base subobject path" $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log
	$(GREP) -F -q "invalid pointer-to-member conversion in initialization" $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log
	$(GREP) -F -q "assignment requires modifiable lvalue" $(TEST_OUT)/cxx-member-pointer-data/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.ro tests/cxx_member_pointer_data_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log)
	$(GREP) -F -q "data-member pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log
	$(GREP) -F -q "member-pointer application requires one public base subobject path" $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log
	$(GREP) -F -q "invalid pointer-to-member conversion in initialization" $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log
	$(GREP) -F -q "assignment requires modifiable lvalue" $(TEST_OUT)/cxx-member-pointer-data/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-data/protected-designator-x86.ro tests/cxx_member_pointer_data_protected_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-data/protected-designator-x86.log)
	$(GREP) -F -q "data-member pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-data/protected-designator-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-data/protected-designator-x64.ro tests/cxx_member_pointer_data_protected_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-data/protected-designator-x64.log)
	$(GREP) -F -q "data-member pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-data/protected-designator-x64.log

test-cxx-member-pointer-functions: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-pointer-functions)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-member-pointer-functions,cxx_member_pointer_functions.cpp)
	g++ -std=c++20 -S -o $(TEST_OUT)/cxx-member-pointer-functions/gcc-x64.s tests/cxx_member_pointer_functions.cpp
	$(GREP) -F -q "_Z22invoke_double_overloadR19MemberFunctionOwnerMS_FidEd" $(TEST_OUT)/cxx-member-pointer-functions/gcc-x64.s
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86.ro tests/cxx_member_pointer_functions_ir.cpp > $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86.log 2>&1
	$(GREP) -F -q "Verified backend: 9 function(s) emitted" $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64.ro tests/cxx_member_pointer_functions_ir.cpp > $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64.log 2>&1
	$(GREP) -F -q "Verified backend: 9 function(s) emitted" $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -v -c -o $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86-o2.ro tests/cxx_member_pointer_functions_ir.cpp > $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86-o2.log 2>&1
	$(GREP) -F -q "Verified backend: 9 function(s) emitted" $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x86-o2.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -fverified-backend -v -c -o $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64-o2.ro tests/cxx_member_pointer_functions_ir.cpp > $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64-o2.log 2>&1
	$(GREP) -F -q "Verified backend: 9 function(s) emitted" $(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/cxx-member-pointer-functions/verified-ir-x64-o2.log,0)
	$(GREP) -F -q "_Z10invoke_dotR19MemberFunctionOwnerMS_FiiEi" $(TEST_OUT)/cxx-member-pointer-functions/x86.s
	$(GREP) -F -q "_Z12invoke_arrowP19MemberFunctionOwnerMS_FiiEi" $(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z12invoke_constRK19MemberFunctionOwnerMS_KFiiEi" $(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z15invoke_volatileRV19MemberFunctionOwnerMS_VFiiEi" $(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z9invoke_cvRVK19MemberFunctionOwnerMS_VKFiiEi" $(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z22invoke_double_overloadR19MemberFunctionOwnerMS_FidEd" $(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(GREP) -F -q "_Z23invoke_inherited_memberR30InheritedMemberFunctionDerivedM27InheritedMemberFunctionBaseFiiEi" $(TEST_OUT)/cxx-member-pointer-functions/x64.s
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/invalid-x86.ro tests/cxx_member_pointer_functions_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/invalid-x86.log)
	$(GREP) -F -q "invalid pointer-to-member conversion in initialization" $(TEST_OUT)/cxx-member-pointer-functions/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/invalid-x64.ro tests/cxx_member_pointer_functions_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/invalid-x64.log)
	$(GREP) -F -q "invalid pointer-to-member conversion in initialization" $(TEST_OUT)/cxx-member-pointer-functions/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x86.ro tests/cxx_member_pointer_functions_inherited_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x86.log)
	$(GREP) -F -q "member-pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x64.ro tests/cxx_member_pointer_functions_inherited_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x64.log)
	$(GREP) -F -q "member-pointer formation is not accessible in this context" $(TEST_OUT)/cxx-member-pointer-functions/inherited-invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x86.ro tests/cxx_member_pointer_functions_overload_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x86.log)
	$(GREP) -F -q "no matching member-function overload 'apply' for member-pointer target" $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x86.log
	$(GREP) -F -q "overloaded member-function address requires a member-pointer target type" $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x64.ro tests/cxx_member_pointer_functions_overload_invalid.cpp,$(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x64.log)
	$(GREP) -F -q "no matching member-function overload 'apply' for member-pointer target" $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x64.log
	$(GREP) -F -q "overloaded member-function address requires a member-pointer target type" $(TEST_OUT)/cxx-member-pointer-functions/overload-invalid-x64.log
	@echo "C++ non-virtual member-function pointer calls passed for i686 and AMD64"

test-cxx-constraints: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constraints)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-constraints,cxx_constraints.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constraints/invalid-x86.ro tests/cxx_constraints_invalid.cpp,$(TEST_OUT)/cxx-constraints/invalid-x86.log)
	$(GREP) -F -q "template constraints are not satisfied" $(TEST_OUT)/cxx-constraints/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constraints/invalid-x64.ro tests/cxx_constraints_invalid.cpp,$(TEST_OUT)/cxx-constraints/invalid-x64.log)
	$(GREP) -F -q "template constraints are not satisfied" $(TEST_OUT)/cxx-constraints/invalid-x64.log

test-cxx-named-concepts: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-named-concepts)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-named-concepts,cxx_named_concepts.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-named-concepts/invalid-x86.ro tests/cxx_named_concepts_invalid.cpp,$(TEST_OUT)/cxx-named-concepts/invalid-x86.log)
	$(GREP) -F -q "named concepts do not support parameter packs" $(TEST_OUT)/cxx-named-concepts/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-named-concepts/invalid-x64.ro tests/cxx_named_concepts_invalid.cpp,$(TEST_OUT)/cxx-named-concepts/invalid-x64.log)
	$(GREP) -F -q "named concepts do not support parameter packs" $(TEST_OUT)/cxx-named-concepts/invalid-x64.log

test-cxx-alias-templates: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-alias-templates)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-alias-templates,cxx_alias_templates.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-alias-templates/invalid-x86.ro tests/cxx_alias_templates_invalid.cpp,$(TEST_OUT)/cxx-alias-templates/invalid-x86.log)
	$(GREP) -F -q "alias template parameter packs are not supported" $(TEST_OUT)/cxx-alias-templates/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-alias-templates/invalid-x64.ro tests/cxx_alias_templates_invalid.cpp,$(TEST_OUT)/cxx-alias-templates/invalid-x64.log)
	$(GREP) -F -q "alias template parameter packs are not supported" $(TEST_OUT)/cxx-alias-templates/invalid-x64.log
else
test-cxx-variable-templates: test-cxx-variable-templates-posix
test-cxx-function-template-overloads: test-cxx-function-template-overloads-posix
test-cxx-function-template-references: test-cxx-function-template-references-posix
test-cxx-static-reference-temporaries: test-cxx-static-reference-temporaries-posix
test-cxx-static-reference-retry: test-cxx-static-reference-retry-posix
test-cxx-static-local-exception: test-cxx-static-local-exception-posix
test-cxx-static-reference-subobjects: test-cxx-static-reference-subobjects-posix
test-cxx-static-reference-conversions: test-cxx-static-reference-conversions-posix
test-cxx-member-pointer-data: test-cxx-member-pointer-data-posix
test-cxx-member-pointer-functions: test-cxx-member-pointer-functions-posix
test-cxx-constraints: test-cxx-constraints-posix
test-cxx-named-concepts: test-cxx-named-concepts-posix
test-cxx-alias-templates: test-cxx-alias-templates-posix
endif

test-cxx-class-template-methods-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-methods)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-methods/x86.s \
		tests/cxx_class_template_methods.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-methods/x86.o \
		$(TEST_OUT)/cxx-class-template-methods/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-methods/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-methods/x86 \
		$(TEST_OUT)/cxx-class-template-methods/start-x86.o \
		$(TEST_OUT)/cxx-class-template-methods/x86.o
	$(TEST_OUT)/cxx-class-template-methods/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-methods/x64.s \
		tests/cxx_class_template_methods.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-methods/x64.o \
		$(TEST_OUT)/cxx-class-template-methods/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-methods/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-methods/x64 \
		$(TEST_OUT)/cxx-class-template-methods/start-x64.o \
		$(TEST_OUT)/cxx-class-template-methods/x64.o
	$(TEST_OUT)/cxx-class-template-methods/x64
	@echo "RCC++ substituted class-template member and constructor test completed"

test-cxx-class-template-specialization-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-specialization/x86.s \
		tests/cxx_class_template_specialization.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-specialization/x86.o \
		$(TEST_OUT)/cxx-class-template-specialization/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-specialization/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-specialization/x86 \
		$(TEST_OUT)/cxx-class-template-specialization/start-x86.o \
		$(TEST_OUT)/cxx-class-template-specialization/x86.o
	$(TEST_OUT)/cxx-class-template-specialization/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-specialization/x64.s \
		tests/cxx_class_template_specialization.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-specialization/x64.o \
		$(TEST_OUT)/cxx-class-template-specialization/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-specialization/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-specialization/x64 \
		$(TEST_OUT)/cxx-class-template-specialization/start-x64.o \
		$(TEST_OUT)/cxx-class-template-specialization/x64.o
	$(TEST_OUT)/cxx-class-template-specialization/x64
	@echo "RCC++ ordered class-template specialization test completed"

ifeq ($(OS),Windows_NT)
test-cxx-class-template-methods: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-methods)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-class-template-methods,cxx_class_template_methods.cpp)

test-cxx-class-template-specialization: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-class-template-specialization,cxx_class_template_specialization.cpp)

test-cxx-class-template-dependent-base: $(RCXX_TARGET) test-cxx-class-template-dependent-base-lookup-invalid
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-dependent-base)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-class-template-dependent-base/x86-freestanding.s tests/cxx_class_template_dependent_base.cpp
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/cxx-class-template-dependent-base/x86-freestanding,$(TEST_OUT)/cxx-class-template-dependent-base/x86-freestanding.s)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-class-template-dependent-base,cxx_class_template_dependent_base.cpp)
	$(call CXX_I686_WIDE_MEMBER_INITIALIZER_CHECK,cxx-class-template-dependent-base)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dependent-dmi-parameter)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-dependent-dmi-parameter,cxx_dependent_dmi_parameter.cpp)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/cxx-dependent-dmi-parameter/x86-freestanding,$(TEST_OUT)/cxx-dependent-dmi-parameter/x86.s)
else
test-cxx-class-template-methods: test-cxx-class-template-methods-posix
test-cxx-class-template-specialization: test-cxx-class-template-specialization-posix
test-cxx-class-template-dependent-base: $(RCXX_TARGET) test-cxx-class-template-dependent-base-lookup-invalid
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-dependent-base)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-class-template-dependent-base/x86-freestanding.s tests/cxx_class_template_dependent_base.cpp
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/cxx-class-template-dependent-base/x86-freestanding,$(TEST_OUT)/cxx-class-template-dependent-base/x86-freestanding.s)
	$(call CXX_POSIX_ENTRY_TEST,cxx-class-template-dependent-base,cxx_class_template_dependent_base.cpp)
	$(call CXX_I686_WIDE_MEMBER_INITIALIZER_CHECK,cxx-class-template-dependent-base)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dependent-dmi-parameter)
	$(call CXX_POSIX_ENTRY_TEST,cxx-dependent-dmi-parameter,cxx_dependent_dmi_parameter.cpp)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/cxx-dependent-dmi-parameter/x86-freestanding,$(TEST_OUT)/cxx-dependent-dmi-parameter/x86.s)
endif

test-cxx-class-template-dependent-base-lookup-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-dependent-base)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x86.ro tests/cxx_class_template_dependent_base_lookup_invalid.cpp,$(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x86.log)
	$(GREP) -q "ambiguous" $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x86.log
	$(GREP) -q "member 'collision' is ambiguous" $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x86.log
	$(GREP) -q "ambiguous member lookup for 'collision'" $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x86.log
	$(GREP) -q "not accessible" $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x64.ro tests/cxx_class_template_dependent_base_lookup_invalid.cpp,$(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x64.log)
	$(GREP) -q "ambiguous" $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x64.log
	$(GREP) -q "member 'collision' is ambiguous" $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x64.log
	$(GREP) -q "ambiguous member lookup for 'collision'" $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x64.log
	$(GREP) -q "not accessible" $(TEST_OUT)/cxx-class-template-dependent-base/lookup-invalid-x64.log

test-cxx-class-template-specialization-ambiguous-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization-ambiguous)
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.ro \
		tests/cxx_class_template_specialization_ambiguous.cpp \
		>$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.log 2>&1; then \
		echo "ambiguous class template specialization unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "ambiguous class template partial specialization" \
		$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.ro \
		tests/cxx_class_template_specialization_ambiguous.cpp \
		>$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.log 2>&1; then \
		echo "ambiguous class template specialization unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "ambiguous class template partial specialization" \
		$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.log
	@echo "RCC++ ambiguous class-template specialization diagnostic completed"

test-cxx-class-template-specialization-partial-order-invalid-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid)
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x86.ro \
		tests/cxx_class_template_specialization_partial_order_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x86.log 2>&1; then \
		echo "incomparable partial specializations unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "ambiguous class template partial specialization" \
		$(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x64.ro \
		tests/cxx_class_template_specialization_partial_order_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x64.log 2>&1; then \
		echo "incomparable partial specializations unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "ambiguous class template partial specialization" \
		$(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x64.log
	@echo "RCC++ incomparable partial-specialization diagnostic completed"

test-cxx-class-template-specialization-constraint-invalid-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization-constraint-invalid)
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x86.ro \
		tests/cxx_class_template_specialization_constraint_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x86.log 2>&1; then \
		echo "unsupported partial specialization constraint unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "constraint could not be evaluated" \
		$(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x64.ro \
		tests/cxx_class_template_specialization_constraint_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x64.log 2>&1; then \
		echo "unsupported partial specialization constraint unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "constraint could not be evaluated" \
		$(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x64.log
	@echo "RCC++ unsupported partial-specialization constraint diagnostic completed"

ifeq ($(OS),Windows_NT)
test-cxx-class-template-specialization-ambiguous: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization-ambiguous)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.ro tests/cxx_class_template_specialization_ambiguous.cpp,$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.log)
	$(GREP) -F -q "ambiguous class template partial specialization" $(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.ro tests/cxx_class_template_specialization_ambiguous.cpp,$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.log)
	$(GREP) -F -q "ambiguous class template partial specialization" $(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.log
	@echo "RCC++ ambiguous class-template specialization diagnostic completed"

test-cxx-class-template-specialization-partial-order-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x86.ro tests/cxx_class_template_specialization_partial_order_invalid.cpp,$(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x86.log)
	$(GREP) -F -q "ambiguous class template partial specialization" $(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x64.ro tests/cxx_class_template_specialization_partial_order_invalid.cpp,$(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x64.log)
	$(GREP) -F -q "ambiguous class template partial specialization" $(TEST_OUT)/cxx-class-template-specialization-partial-order-invalid/x64.log
	@echo "RCC++ incomparable partial-specialization diagnostic completed"

test-cxx-class-template-specialization-constraint-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization-constraint-invalid)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x86.ro tests/cxx_class_template_specialization_constraint_invalid.cpp,$(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x86.log)
	$(GREP) -F -q "constraint could not be evaluated" $(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x64.ro tests/cxx_class_template_specialization_constraint_invalid.cpp,$(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x64.log)
	$(GREP) -F -q "constraint could not be evaluated" $(TEST_OUT)/cxx-class-template-specialization-constraint-invalid/x64.log
	@echo "RCC++ unsupported partial-specialization constraint diagnostic completed"
else
test-cxx-class-template-specialization-ambiguous: test-cxx-class-template-specialization-ambiguous-posix
test-cxx-class-template-specialization-partial-order-invalid: test-cxx-class-template-specialization-partial-order-invalid-posix
test-cxx-class-template-specialization-constraint-invalid: test-cxx-class-template-specialization-constraint-invalid-posix
endif

test-cxx-class-template-non-type-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-non-type)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-non-type/x86.s \
		tests/cxx_class_template_non_type.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-non-type/x86.o \
		$(TEST_OUT)/cxx-class-template-non-type/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-non-type/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-non-type/x86 \
		$(TEST_OUT)/cxx-class-template-non-type/start-x86.o \
		$(TEST_OUT)/cxx-class-template-non-type/x86.o
	$(TEST_OUT)/cxx-class-template-non-type/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-non-type/x64.s \
		tests/cxx_class_template_non_type.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-non-type/x64.o \
		$(TEST_OUT)/cxx-class-template-non-type/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-non-type/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-non-type/x64 \
		$(TEST_OUT)/cxx-class-template-non-type/start-x64.o \
		$(TEST_OUT)/cxx-class-template-non-type/x64.o
	$(TEST_OUT)/cxx-class-template-non-type/x64
	@echo "RCC++ class non-type template test completed"

test-cxx-operator-overload-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-operator-overload)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-operator-overload/x86.s \
		tests/cxx_operator_overload.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-operator-overload/x86.o \
		$(TEST_OUT)/cxx-operator-overload/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-operator-overload/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-operator-overload/x86 \
		$(TEST_OUT)/cxx-operator-overload/start-x86.o \
		$(TEST_OUT)/cxx-operator-overload/x86.o
	$(TEST_OUT)/cxx-operator-overload/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-operator-overload/x64.s \
		tests/cxx_operator_overload.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-operator-overload/x64.o \
		$(TEST_OUT)/cxx-operator-overload/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-operator-overload/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-operator-overload/x64 \
		$(TEST_OUT)/cxx-operator-overload/start-x64.o \
		$(TEST_OUT)/cxx-operator-overload/x64.o
	$(TEST_OUT)/cxx-operator-overload/x64
	@echo "RCC++ member operator overload test completed"

test-cxx-member-operator-forms-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-operator-forms)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-operator-forms/x86.s \
		tests/cxx_member_operator_forms.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-operator-forms/x86.o \
		$(TEST_OUT)/cxx-member-operator-forms/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-operator-forms/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-member-operator-forms/x86 \
		$(TEST_OUT)/cxx-member-operator-forms/start-x86.o \
		$(TEST_OUT)/cxx-member-operator-forms/x86.o
	$(TEST_OUT)/cxx-member-operator-forms/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-operator-forms/x64.s \
		tests/cxx_member_operator_forms.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-member-operator-forms/x64.o \
		$(TEST_OUT)/cxx-member-operator-forms/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-member-operator-forms/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-member-operator-forms/x64 \
		$(TEST_OUT)/cxx-member-operator-forms/start-x64.o \
		$(TEST_OUT)/cxx-member-operator-forms/x64.o
	$(TEST_OUT)/cxx-member-operator-forms/x64
	@echo "RCC++ unary, subscript, and call operator tests completed"

test-cxx-assignment-operator-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-assignment-operator)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-assignment-operator/x86.s \
		tests/cxx_assignment_operator.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-assignment-operator/x86.o \
		$(TEST_OUT)/cxx-assignment-operator/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-assignment-operator/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-assignment-operator/x86 \
		$(TEST_OUT)/cxx-assignment-operator/start-x86.o \
		$(TEST_OUT)/cxx-assignment-operator/x86.o
	$(TEST_OUT)/cxx-assignment-operator/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-assignment-operator/x64.s \
		tests/cxx_assignment_operator.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-assignment-operator/x64.o \
		$(TEST_OUT)/cxx-assignment-operator/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-assignment-operator/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-assignment-operator/x64 \
		$(TEST_OUT)/cxx-assignment-operator/start-x64.o \
		$(TEST_OUT)/cxx-assignment-operator/x64.o
	$(TEST_OUT)/cxx-assignment-operator/x64
	@echo "RCC++ assignment operator tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-class-template-non-type: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-non-type)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-class-template-non-type,cxx_class_template_non_type.cpp)

test-cxx-operator-overload: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-operator-overload)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-operator-overload,cxx_operator_overload.cpp)

test-cxx-member-operator-forms: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-operator-forms)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-member-operator-forms,cxx_member_operator_forms.cpp)

test-cxx-assignment-operator: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-assignment-operator)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-assignment-operator,cxx_assignment_operator.cpp)
else
test-cxx-class-template-non-type: test-cxx-class-template-non-type-posix
test-cxx-operator-overload: test-cxx-operator-overload-posix
test-cxx-member-operator-forms: test-cxx-member-operator-forms-posix
test-cxx-assignment-operator: test-cxx-assignment-operator-posix
endif

test-cxx-lambda-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-lambda)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-lambda/x86.s tests/cxx_lambda.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-lambda/x86.o \
		$(TEST_OUT)/cxx-lambda/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-lambda/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-lambda/x86 \
		$(TEST_OUT)/cxx-lambda/start-x86.o $(TEST_OUT)/cxx-lambda/x86.o
	$(TEST_OUT)/cxx-lambda/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-lambda/x64.s tests/cxx_lambda.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-lambda/x64.o \
		$(TEST_OUT)/cxx-lambda/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-lambda/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-lambda/x64 \
		$(TEST_OUT)/cxx-lambda/start-x64.o $(TEST_OUT)/cxx-lambda/x64.o
	$(TEST_OUT)/cxx-lambda/x64
	@echo "RCC++ lambda capture tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-lambda: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-lambda)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-lambda,cxx_lambda.cpp)
else
test-cxx-lambda: test-cxx-lambda-posix
endif

test-cxx-lambda-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-lambda-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-lambda-invalid/x86.ro tests/cxx_lambda_invalid.cpp,$(TEST_OUT)/cxx-lambda-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-lambda-invalid/x64.ro tests/cxx_lambda_invalid.cpp,$(TEST_OUT)/cxx-lambda-invalid/x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-lambda-invalid/x86.ro \
		tests/cxx_lambda_invalid.cpp \
		>$(TEST_OUT)/cxx-lambda-invalid/x86.log 2>&1; then \
		echo "non-mutable lambda capture unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-lambda-invalid/x64.ro \
		tests/cxx_lambda_invalid.cpp \
		>$(TEST_OUT)/cxx-lambda-invalid/x64.log 2>&1; then \
		echo "non-mutable lambda capture unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "assignment requires modifiable lvalue" \
		$(TEST_OUT)/cxx-lambda-invalid/x86.log
	$(GREP) -q "assignment requires modifiable lvalue" \
		$(TEST_OUT)/cxx-lambda-invalid/x64.log
	@echo "RCC++ non-mutable lambda capture diagnostics completed"

test-cxx-lambda-init-capture-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-lambda-init-capture-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-lambda-init-capture-invalid/x86.ro tests/cxx_lambda_init_capture_invalid.cpp,$(TEST_OUT)/cxx-lambda-init-capture-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-lambda-init-capture-invalid/x64.ro tests/cxx_lambda_init_capture_invalid.cpp,$(TEST_OUT)/cxx-lambda-init-capture-invalid/x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-lambda-init-capture-invalid/x86.ro \
		tests/cxx_lambda_init_capture_invalid.cpp \
		>$(TEST_OUT)/cxx-lambda-init-capture-invalid/x86.log 2>&1; then \
		echo "reference lambda init-capture unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-lambda-init-capture-invalid/x64.ro \
		tests/cxx_lambda_init_capture_invalid.cpp \
		>$(TEST_OUT)/cxx-lambda-init-capture-invalid/x64.log 2>&1; then \
		echo "reference lambda init-capture unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "lambda init-capture cannot initialize a reference or this capture" \
		$(TEST_OUT)/cxx-lambda-init-capture-invalid/x86.log
	$(GREP) -q "lambda init-capture cannot initialize a reference or this capture" \
		$(TEST_OUT)/cxx-lambda-init-capture-invalid/x64.log
	@echo "RCC++ reference lambda init-capture diagnostics completed"

test-cxx-structured-bindings-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-structured-bindings)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-structured-bindings/x86.s \
		tests/cxx_structured_bindings.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-structured-bindings/x86.o \
		$(TEST_OUT)/cxx-structured-bindings/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-structured-bindings/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-structured-bindings/x86 \
		$(TEST_OUT)/cxx-structured-bindings/start-x86.o \
		$(TEST_OUT)/cxx-structured-bindings/x86.o
	$(TEST_OUT)/cxx-structured-bindings/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-structured-bindings/x64.s \
		tests/cxx_structured_bindings.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-structured-bindings/x64.o \
		$(TEST_OUT)/cxx-structured-bindings/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-structured-bindings/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-structured-bindings/x64 \
		$(TEST_OUT)/cxx-structured-bindings/start-x64.o \
		$(TEST_OUT)/cxx-structured-bindings/x64.o
	$(TEST_OUT)/cxx-structured-bindings/x64
	@echo "RCC++ structured binding tests completed"

test-cxx-structured-bindings-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-structured-bindings-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-structured-bindings-invalid/x86.ro tests/cxx_structured_bindings_invalid.cpp,$(TEST_OUT)/cxx-structured-bindings-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-structured-bindings-invalid/x64.ro tests/cxx_structured_bindings_invalid.cpp,$(TEST_OUT)/cxx-structured-bindings-invalid/x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-structured-bindings-invalid/x86.ro \
		tests/cxx_structured_bindings_invalid.cpp \
		>$(TEST_OUT)/cxx-structured-bindings-invalid/x86.log 2>&1; then \
		echo "invalid structured binding field count unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-structured-bindings-invalid/x64.ro \
		tests/cxx_structured_bindings_invalid.cpp \
		>$(TEST_OUT)/cxx-structured-bindings-invalid/x64.log 2>&1; then \
		echo "invalid structured binding field count unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "structured binding count does not match aggregate fields" \
		$(TEST_OUT)/cxx-structured-bindings-invalid/x86.log
	$(GREP) -q "structured binding count does not match aggregate fields" \
		$(TEST_OUT)/cxx-structured-bindings-invalid/x64.log
	$(GREP) -q "direct-list structured binding requires one initializer expression" \
		$(TEST_OUT)/cxx-structured-bindings-invalid/x86.log
	$(GREP) -q "direct-list structured binding requires one initializer expression" \
		$(TEST_OUT)/cxx-structured-bindings-invalid/x64.log
	@echo "RCC++ structured binding diagnostics completed"

test-cxx-alignas-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-alignas)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-alignas/x86.s tests/cxx_alignas.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-alignas/x86.o \
		$(TEST_OUT)/cxx-alignas/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-alignas/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-alignas/x86 \
		$(TEST_OUT)/cxx-alignas/start-x86.o $(TEST_OUT)/cxx-alignas/x86.o
	$(TEST_OUT)/cxx-alignas/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-alignas/x64.s tests/cxx_alignas.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-alignas/x64.o \
		$(TEST_OUT)/cxx-alignas/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-alignas/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-alignas/x64 \
		$(TEST_OUT)/cxx-alignas/start-x64.o $(TEST_OUT)/cxx-alignas/x64.o
	$(TEST_OUT)/cxx-alignas/x64
	@echo "RCC++ alignas/alignof tests completed"

test-cxx-alignas-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-alignas-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-alignas-invalid/x86.ro tests/cxx_alignas_invalid.cpp,$(TEST_OUT)/cxx-alignas-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-alignas-invalid/x64.ro tests/cxx_alignas_invalid.cpp,$(TEST_OUT)/cxx-alignas-invalid/x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-alignas-invalid/x86.ro \
		tests/cxx_alignas_invalid.cpp \
		>$(TEST_OUT)/cxx-alignas-invalid/x86.log 2>&1; then \
		echo "invalid C++ alignas unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-alignas-invalid/x64.ro \
		tests/cxx_alignas_invalid.cpp \
		>$(TEST_OUT)/cxx-alignas-invalid/x64.log 2>&1; then \
		echo "invalid C++ alignas unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "_Alignas alignment must be a power of two" \
		$(TEST_OUT)/cxx-alignas-invalid/x86.log
	$(GREP) -q "_Alignas alignment must be a power of two" \
		$(TEST_OUT)/cxx-alignas-invalid/x64.log
	@echo "RCC++ alignas diagnostics completed"

test-cxx-constinit-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constinit)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constinit/x86.s tests/cxx_constinit.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constinit/x86.o \
		$(TEST_OUT)/cxx-constinit/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constinit/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constinit/x86 \
		$(TEST_OUT)/cxx-constinit/start-x86.o \
		$(TEST_OUT)/cxx-constinit/x86.o
	$(TEST_OUT)/cxx-constinit/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constinit/x64.s tests/cxx_constinit.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-constinit/x64.o \
		$(TEST_OUT)/cxx-constinit/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-constinit/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constinit/x64 \
		$(TEST_OUT)/cxx-constinit/start-x64.o \
		$(TEST_OUT)/cxx-constinit/x64.o
	$(TEST_OUT)/cxx-constinit/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constinit/tls-x86.ro tests/cxx_constinit_tls.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constinit/tls-x64.ro tests/cxx_constinit_tls.cpp
	@echo "RCC++ constinit execution and thread-local object tests completed"

test-cxx-constinit-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constinit-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constinit-invalid/x86.ro tests/cxx_constinit_invalid.cpp,$(TEST_OUT)/cxx-constinit-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constinit-invalid/x64.ro tests/cxx_constinit_invalid.cpp,$(TEST_OUT)/cxx-constinit-invalid/x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constinit-invalid/function-x86.ro tests/cxx_constinit_function_invalid.cpp,$(TEST_OUT)/cxx-constinit-invalid/function-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constinit-invalid/function-x64.ro tests/cxx_constinit_function_invalid.cpp,$(TEST_OUT)/cxx-constinit-invalid/function-x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constinit-invalid/x86.ro \
		tests/cxx_constinit_invalid.cpp \
		>$(TEST_OUT)/cxx-constinit-invalid/x86.log 2>&1; then \
		echo "invalid constinit semantics unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constinit-invalid/x64.ro \
		tests/cxx_constinit_invalid.cpp \
		>$(TEST_OUT)/cxx-constinit-invalid/x64.log 2>&1; then \
		echo "invalid constinit semantics unexpectedly compiled on AMD64"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constinit-invalid/function-x86.ro \
		tests/cxx_constinit_function_invalid.cpp \
		>$(TEST_OUT)/cxx-constinit-invalid/function-x86.log 2>&1; then \
		echo "constinit function unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constinit-invalid/function-x64.ro \
		tests/cxx_constinit_function_invalid.cpp \
		>$(TEST_OUT)/cxx-constinit-invalid/function-x64.log 2>&1; then \
		echo "constinit function unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "constinit variable initializer is not a supported constant expression" \
		$(TEST_OUT)/cxx-constinit-invalid/x86.log
	$(GREP) -q "constinit variable requires static or thread storage duration" \
		$(TEST_OUT)/cxx-constinit-invalid/x86.log
	$(GREP) -q "constinit cannot be combined with constexpr" \
		$(TEST_OUT)/cxx-constinit-invalid/x86.log
	$(GREP) -q "constinit variable initializer is not a supported constant expression" \
		$(TEST_OUT)/cxx-constinit-invalid/x64.log
	$(GREP) -q "constinit variable requires static or thread storage duration" \
		$(TEST_OUT)/cxx-constinit-invalid/x64.log
	$(GREP) -q "constinit cannot be combined with constexpr" \
		$(TEST_OUT)/cxx-constinit-invalid/x64.log
	$(GREP) -q "constinit declaration must declare a variable" \
		$(TEST_OUT)/cxx-constinit-invalid/function-x86.log
	$(GREP) -q "constinit declaration must declare a variable" \
		$(TEST_OUT)/cxx-constinit-invalid/function-x64.log
	@echo "RCC++ constinit diagnostics completed"

test-cxx-using-enum-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-using-enum)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-using-enum/x86.s tests/cxx_using_enum.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-using-enum/x86.o \
		$(TEST_OUT)/cxx-using-enum/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-using-enum/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-using-enum/x86 \
		$(TEST_OUT)/cxx-using-enum/start-x86.o \
		$(TEST_OUT)/cxx-using-enum/x86.o
	$(TEST_OUT)/cxx-using-enum/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-using-enum/x64.s tests/cxx_using_enum.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-using-enum/x64.o \
		$(TEST_OUT)/cxx-using-enum/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-using-enum/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-using-enum/x64 \
		$(TEST_OUT)/cxx-using-enum/start-x64.o \
		$(TEST_OUT)/cxx-using-enum/x64.o
	$(TEST_OUT)/cxx-using-enum/x64
	@echo "RCC++ using enum execution tests completed"

test-cxx-using-enum-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-using-enum-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-using-enum-invalid/x86.ro tests/cxx_using_enum_invalid.cpp,$(TEST_OUT)/cxx-using-enum-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-using-enum-invalid/x64.ro tests/cxx_using_enum_invalid.cpp,$(TEST_OUT)/cxx-using-enum-invalid/x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-using-enum-invalid/x86.ro \
		tests/cxx_using_enum_invalid.cpp \
		>$(TEST_OUT)/cxx-using-enum-invalid/x86.log 2>&1; then \
		echo "conflicting using enum unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-using-enum-invalid/x64.ro \
		tests/cxx_using_enum_invalid.cpp \
		>$(TEST_OUT)/cxx-using-enum-invalid/x64.log 2>&1; then \
		echo "conflicting using enum unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "using enum introduces a conflicting enumerator 'shared'" \
		$(TEST_OUT)/cxx-using-enum-invalid/x86.log
	$(GREP) -q "using enum introduces a conflicting enumerator 'shared'" \
		$(TEST_OUT)/cxx-using-enum-invalid/x64.log
	@echo "RCC++ using enum diagnostics completed"

test-cxx-template-template-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-template)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-template/x86.s \
		tests/cxx_template_template.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-template/x86.o \
		$(TEST_OUT)/cxx-template-template/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-template/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-template/x86 \
		$(TEST_OUT)/cxx-template-template/start-x86.o \
		$(TEST_OUT)/cxx-template-template/x86.o
	$(TEST_OUT)/cxx-template-template/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-template/x64.s \
		tests/cxx_template_template.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-template-template/x64.o \
		$(TEST_OUT)/cxx-template-template/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-template-template/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-template/x64 \
		$(TEST_OUT)/cxx-template-template/start-x64.o \
		$(TEST_OUT)/cxx-template-template/x64.o
	$(TEST_OUT)/cxx-template-template/x64
	@echo "RCC++ template-template parameter tests completed"

test-cxx-template-template-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-template-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-template-template-invalid/x86.ro tests/cxx_template_template_invalid.cpp,$(TEST_OUT)/cxx-template-template-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-template-template-invalid/x64.ro tests/cxx_template_template_invalid.cpp,$(TEST_OUT)/cxx-template-template-invalid/x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-template-invalid/x86.ro \
		tests/cxx_template_template_invalid.cpp \
		>$(TEST_OUT)/cxx-template-template-invalid/x86.log 2>&1; then \
		echo "mismatched template-template argument unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-template-invalid/x64.ro \
		tests/cxx_template_template_invalid.cpp \
		>$(TEST_OUT)/cxx-template-template-invalid/x64.log 2>&1; then \
		echo "mismatched template-template argument unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "template-template argument does not match its parameter list" \
		$(TEST_OUT)/cxx-template-template-invalid/x86.log
	$(GREP) -q "template-template argument does not match its parameter list" \
		$(TEST_OUT)/cxx-template-template-invalid/x64.log
	@echo "RCC++ template-template parameter diagnostics completed"

test-cxx-template-template-dependent-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-template-dependent-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-template-template-dependent-invalid/x86.ro tests/cxx_template_template_dependent_invalid.cpp,$(TEST_OUT)/cxx-template-template-dependent-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-template-template-dependent-invalid/x64.ro tests/cxx_template_template_dependent_invalid.cpp,$(TEST_OUT)/cxx-template-template-dependent-invalid/x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-template-dependent-invalid/x86.ro \
		tests/cxx_template_template_dependent_invalid.cpp \
		>$(TEST_OUT)/cxx-template-template-dependent-invalid/x86.log 2>&1; then \
		echo "unresolved dependent template-template value unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-template-dependent-invalid/x64.ro \
		tests/cxx_template_template_dependent_invalid.cpp \
		>$(TEST_OUT)/cxx-template-template-dependent-invalid/x64.log 2>&1; then \
		echo "unresolved dependent template-template value unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "dependent template-template non-type argument must be an integer constant expression" \
		$(TEST_OUT)/cxx-template-template-dependent-invalid/x86.log
	$(GREP) -q "dependent template-template non-type argument must be an integer constant expression" \
		$(TEST_OUT)/cxx-template-template-dependent-invalid/x64.log
	$(GREP) -q "template-template argument does not match its parameter list" \
		$(TEST_OUT)/cxx-template-template-dependent-invalid/x86.log
	$(GREP) -q "template-template argument does not match its parameter list" \
		$(TEST_OUT)/cxx-template-template-dependent-invalid/x64.log
	@echo "RCC++ dependent template-template value diagnostics completed"

test-cxx-conversion-operator-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-conversion-operator)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-conversion-operator/x86.s \
		tests/cxx_conversion_operator.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-conversion-operator/x86.o \
		$(TEST_OUT)/cxx-conversion-operator/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-conversion-operator/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-conversion-operator/x86 \
		$(TEST_OUT)/cxx-conversion-operator/start-x86.o \
		$(TEST_OUT)/cxx-conversion-operator/x86.o
	$(TEST_OUT)/cxx-conversion-operator/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-conversion-operator/x64.s \
		tests/cxx_conversion_operator.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-conversion-operator/x64.o \
		$(TEST_OUT)/cxx-conversion-operator/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-conversion-operator/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-conversion-operator/x64 \
		$(TEST_OUT)/cxx-conversion-operator/start-x64.o \
		$(TEST_OUT)/cxx-conversion-operator/x64.o
	$(TEST_OUT)/cxx-conversion-operator/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-conversion-operator/x86.ro \
		tests/cxx_conversion_operator.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-conversion-operator/x64.ro \
		tests/cxx_conversion_operator.cpp
	@echo "RCC++ user-defined conversion operator tests completed"

test-cxx-nonmember-operator-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-nonmember-operator)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-nonmember-operator/x86.s \
		tests/cxx_nonmember_operator.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nonmember-operator/x86.o \
		$(TEST_OUT)/cxx-nonmember-operator/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nonmember-operator/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-nonmember-operator/x86 \
		$(TEST_OUT)/cxx-nonmember-operator/start-x86.o \
		$(TEST_OUT)/cxx-nonmember-operator/x86.o
	$(TEST_OUT)/cxx-nonmember-operator/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-nonmember-operator/x64.s \
		tests/cxx_nonmember_operator.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-nonmember-operator/x64.o \
		$(TEST_OUT)/cxx-nonmember-operator/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-nonmember-operator/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-nonmember-operator/x64 \
		$(TEST_OUT)/cxx-nonmember-operator/start-x64.o \
		$(TEST_OUT)/cxx-nonmember-operator/x64.o
	$(TEST_OUT)/cxx-nonmember-operator/x64
	@echo "RCC++ non-member operator tests completed"

test-cxx-spaceship-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-spaceship)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-spaceship/x86.s tests/cxx_spaceship.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-spaceship/x86.o \
		$(TEST_OUT)/cxx-spaceship/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-spaceship/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-spaceship/x86 \
		$(TEST_OUT)/cxx-spaceship/start-x86.o \
		$(TEST_OUT)/cxx-spaceship/x86.o
	$(TEST_OUT)/cxx-spaceship/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-spaceship/x64.s tests/cxx_spaceship.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-spaceship/x64.o \
		$(TEST_OUT)/cxx-spaceship/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-spaceship/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-spaceship/x64 \
		$(TEST_OUT)/cxx-spaceship/start-x64.o \
		$(TEST_OUT)/cxx-spaceship/x64.o
	$(TEST_OUT)/cxx-spaceship/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-spaceship/x86.ro tests/cxx_spaceship.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-spaceship/x64.ro tests/cxx_spaceship.cpp
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-spaceship/invalid-x86.ro \
		tests/cxx_spaceship_invalid.cpp \
		>$(TEST_OUT)/cxx-spaceship/invalid-x86.log 2>&1; then \
		echo "built-in <=> unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "RinOS C++20 built-in <=> requires integral, enum, or compatible pointer operands" \
		$(TEST_OUT)/cxx-spaceship/invalid-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-spaceship/invalid-x64.ro \
		tests/cxx_spaceship_invalid.cpp \
		>$(TEST_OUT)/cxx-spaceship/invalid-x64.log 2>&1; then \
		echo "built-in <=> unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "RinOS C++20 built-in <=> requires integral, enum, or compatible pointer operands" \
		$(TEST_OUT)/cxx-spaceship/invalid-x64.log
	$(GREP) -q "C++20 comparison rewriting requires an integer-returning operator<=> in the bounded RCC++ profile" \
		$(TEST_OUT)/cxx-spaceship/invalid-x86.log
	$(GREP) -q "C++20 comparison rewriting requires an integer-returning operator<=> in the bounded RCC++ profile" \
		$(TEST_OUT)/cxx-spaceship/invalid-x64.log
	@echo "C++20 user-defined spaceship operator tests completed"

test-cxx-final-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-final)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-final/x86.s tests/cxx_final.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-final/x86.o \
		$(TEST_OUT)/cxx-final/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-final/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-final/x86 \
		$(TEST_OUT)/cxx-final/start-x86.o $(TEST_OUT)/cxx-final/x86.o
	$(TEST_OUT)/cxx-final/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-final/x64.s tests/cxx_final.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-final/x64.o \
		$(TEST_OUT)/cxx-final/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-final/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-final/x64 \
		$(TEST_OUT)/cxx-final/start-x64.o $(TEST_OUT)/cxx-final/x64.o
	$(TEST_OUT)/cxx-final/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-final/invalid-x86.ro tests/cxx_final_invalid.cpp \
		>$(TEST_OUT)/cxx-final/invalid-x86.log 2>&1; then \
		echo "derivation from final class unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "cannot derive from final class 'FinalBase'" \
		$(TEST_OUT)/cxx-final/invalid-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-final/invalid-x64.ro tests/cxx_final_invalid.cpp \
		>$(TEST_OUT)/cxx-final/invalid-x64.log 2>&1; then \
		echo "derivation from final class unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "cannot derive from final class 'FinalBase'" \
		$(TEST_OUT)/cxx-final/invalid-x64.log
	@echo "C++ final class semantics tests completed"

test-cxx-override-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-override)
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-override/invalid-x86.ro \
		tests/cxx_override_invalid.cpp \
		>$(TEST_OUT)/cxx-override/invalid-x86.log 2>&1; then \
		echo "invalid override declarations unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "marked override but does not override a base class method" \
		$(TEST_OUT)/cxx-override/invalid-x86.log
	$(GREP) -q "cannot override final method 'final_value'" \
		$(TEST_OUT)/cxx-override/invalid-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-override/invalid-x64.ro \
		tests/cxx_override_invalid.cpp \
		>$(TEST_OUT)/cxx-override/invalid-x64.log 2>&1; then \
		echo "invalid override declarations unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "marked override but does not override a base class method" \
		$(TEST_OUT)/cxx-override/invalid-x64.log
	$(GREP) -q "cannot override final method 'final_value'" \
		$(TEST_OUT)/cxx-override/invalid-x64.log
	@echo "C++ override and final method semantics tests completed"

test-cxx-conditional-explicit-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-conditional-explicit)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-conditional-explicit/x86.s \
		tests/cxx_conditional_explicit.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-conditional-explicit/x86.o \
		$(TEST_OUT)/cxx-conditional-explicit/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-conditional-explicit/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-conditional-explicit/x86 \
		$(TEST_OUT)/cxx-conditional-explicit/start-x86.o \
		$(TEST_OUT)/cxx-conditional-explicit/x86.o
	$(TEST_OUT)/cxx-conditional-explicit/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-conditional-explicit/x64.s \
		tests/cxx_conditional_explicit.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-conditional-explicit/x64.o \
		$(TEST_OUT)/cxx-conditional-explicit/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-conditional-explicit/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-conditional-explicit/x64 \
		$(TEST_OUT)/cxx-conditional-explicit/start-x64.o \
		$(TEST_OUT)/cxx-conditional-explicit/x64.o
	$(TEST_OUT)/cxx-conditional-explicit/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-conditional-explicit/old-x86.ro \
		tests/cxx_conditional_explicit_invalid.cpp \
		>$(TEST_OUT)/cxx-conditional-explicit/old-x86.log 2>&1; then \
		echo "conditional explicit unexpectedly compiled before C++20 on i686"; exit 1; \
	fi
	$(GREP) -q "conditional explicit specifiers require C++20 or newer" \
		$(TEST_OUT)/cxx-conditional-explicit/old-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-conditional-explicit/old-x64.ro \
		tests/cxx_conditional_explicit_invalid.cpp \
		>$(TEST_OUT)/cxx-conditional-explicit/old-x64.log 2>&1; then \
		echo "conditional explicit unexpectedly compiled before C++20 on AMD64"; exit 1; \
	fi
	$(GREP) -q "conditional explicit specifiers require C++20 or newer" \
		$(TEST_OUT)/cxx-conditional-explicit/old-x64.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-conditional-explicit/nonconstant-x86.ro \
		tests/cxx_conditional_explicit_invalid.cpp \
		>$(TEST_OUT)/cxx-conditional-explicit/nonconstant-x86.log 2>&1; then \
		echo "non-constant conditional explicit unexpectedly compiled on i686"; exit 1; \
	fi
	$(GREP) -q "conditional explicit specifier requires an integral constant expression" \
		$(TEST_OUT)/cxx-conditional-explicit/nonconstant-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-conditional-explicit/nonconstant-x64.ro \
		tests/cxx_conditional_explicit_invalid.cpp \
		>$(TEST_OUT)/cxx-conditional-explicit/nonconstant-x64.log 2>&1; then \
		echo "non-constant conditional explicit unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "conditional explicit specifier requires an integral constant expression" \
		$(TEST_OUT)/cxx-conditional-explicit/nonconstant-x64.log
	@echo "C++20 conditional explicit tests completed"

test-cxx-class-template-deduction-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-deduction)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -S \
		-o $(TEST_OUT)/cxx-class-template-deduction/x86.s \
		tests/cxx_class_template_deduction.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-deduction/x86.o \
		$(TEST_OUT)/cxx-class-template-deduction/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-deduction/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-deduction/x86 \
		$(TEST_OUT)/cxx-class-template-deduction/start-x86.o \
		$(TEST_OUT)/cxx-class-template-deduction/x86.o
	$(TEST_OUT)/cxx-class-template-deduction/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-deduction/x64.s \
		tests/cxx_class_template_deduction.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-deduction/x64.o \
		$(TEST_OUT)/cxx-class-template-deduction/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-deduction/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-deduction/x64 \
		$(TEST_OUT)/cxx-class-template-deduction/start-x64.o \
		$(TEST_OUT)/cxx-class-template-deduction/x64.o
	$(TEST_OUT)/cxx-class-template-deduction/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++14 -c \
		-o $(TEST_OUT)/cxx-class-template-deduction/old-x86.ro \
		tests/cxx_class_template_deduction_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-deduction/old-x86.log 2>&1; then \
		echo "class template argument deduction unexpectedly compiled before C++17 on i686"; exit 1; \
	fi
	$(GREP) -q "class template argument deduction requires C++17 or newer" \
		$(TEST_OUT)/cxx-class-template-deduction/old-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++14 -c \
		-o $(TEST_OUT)/cxx-class-template-deduction/old-x64.ro \
		tests/cxx_class_template_deduction_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-deduction/old-x64.log 2>&1; then \
		echo "class template argument deduction unexpectedly compiled before C++17 on AMD64"; exit 1; \
	fi
	$(GREP) -q "class template argument deduction requires C++17 or newer" \
		$(TEST_OUT)/cxx-class-template-deduction/old-x64.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++14 -c \
		-o $(TEST_OUT)/cxx-class-template-deduction/guide-old-x86.ro \
		tests/cxx_class_template_deduction_guide_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-deduction/guide-old-x86.log 2>&1; then \
		echo "deduction guide unexpectedly compiled before C++17 on i686"; exit 1; \
	fi
	$(GREP) -q "deduction guides require C++17 or newer" \
		$(TEST_OUT)/cxx-class-template-deduction/guide-old-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++14 -c \
		-o $(TEST_OUT)/cxx-class-template-deduction/guide-old-x64.ro \
		tests/cxx_class_template_deduction_guide_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-deduction/guide-old-x64.log 2>&1; then \
		echo "deduction guide unexpectedly compiled before C++17 on AMD64"; exit 1; \
	fi
	$(GREP) -q "deduction guides require C++17 or newer" \
		$(TEST_OUT)/cxx-class-template-deduction/guide-old-x64.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x86.ro \
		tests/cxx_class_template_aggregate_deduction_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x86.log 2>&1; then \
		echo "pre-C++20 aggregate paren CTAD unexpectedly compiled on i686"; exit 1; \
	fi
	$(GREP) -q "aggregate class template argument deduction requires braced initialization before C++20" \
		$(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x64.ro \
		tests/cxx_class_template_aggregate_deduction_invalid.cpp \
		>$(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x64.log 2>&1; then \
		echo "pre-C++20 aggregate paren CTAD unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "aggregate class template argument deduction requires braced initialization before C++20" \
		$(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x64.log
	@echo "C++17 class template argument deduction tests completed"

test-cxx-pure-virtual-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-pure-virtual)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-pure-virtual/x86.s tests/cxx_pure_virtual.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-pure-virtual/x86.o \
		$(TEST_OUT)/cxx-pure-virtual/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-pure-virtual/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-pure-virtual/x86 \
		$(TEST_OUT)/cxx-pure-virtual/start-x86.o \
		$(TEST_OUT)/cxx-pure-virtual/x86.o
	$(TEST_OUT)/cxx-pure-virtual/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-pure-virtual/x64.s tests/cxx_pure_virtual.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-pure-virtual/x64.o \
		$(TEST_OUT)/cxx-pure-virtual/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-pure-virtual/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-pure-virtual/x64 \
		$(TEST_OUT)/cxx-pure-virtual/start-x64.o \
		$(TEST_OUT)/cxx-pure-virtual/x64.o
	$(TEST_OUT)/cxx-pure-virtual/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-pure-virtual/invalid-x86.ro \
		tests/cxx_abstract_rejected.cpp \
		>$(TEST_OUT)/cxx-pure-virtual/invalid-x86.log 2>&1; then \
		echo "abstract class instantiation unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "cannot instantiate abstract class 'AbstractValue'" \
		$(TEST_OUT)/cxx-pure-virtual/invalid-x86.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-pure-virtual/invalid-new-x86.ro \
		tests/cxx_abstract_new_rejected.cpp \
		>$(TEST_OUT)/cxx-pure-virtual/invalid-new-x86.log 2>&1; then \
		echo "new abstract class unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "cannot allocate abstract class 'AbstractValue'" \
		$(TEST_OUT)/cxx-pure-virtual/invalid-new-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-pure-virtual/invalid-x64.ro \
		tests/cxx_abstract_rejected.cpp \
		>$(TEST_OUT)/cxx-pure-virtual/invalid-x64.log 2>&1; then \
		echo "abstract class instantiation unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "cannot instantiate abstract class 'AbstractValue'" \
		$(TEST_OUT)/cxx-pure-virtual/invalid-x64.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-pure-virtual/invalid-new-x64.ro \
		tests/cxx_abstract_new_rejected.cpp \
		>$(TEST_OUT)/cxx-pure-virtual/invalid-new-x64.log 2>&1; then \
		echo "new abstract class unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "cannot allocate abstract class 'AbstractValue'" \
		$(TEST_OUT)/cxx-pure-virtual/invalid-new-x64.log
	@echo "C++ pure virtual and abstract class tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-conversion-operator: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-conversion-operator)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-conversion-operator,cxx_conversion_operator.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/cxx-conversion-operator/x86.ro tests/cxx_conversion_operator.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/cxx-conversion-operator/x64.ro tests/cxx_conversion_operator.cpp

test-cxx-nonmember-operator: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-nonmember-operator)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-nonmember-operator,cxx_nonmember_operator.cpp)

test-cxx-spaceship: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-spaceship)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-spaceship,cxx_spaceship.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-spaceship/invalid-x86.ro tests/cxx_spaceship_invalid.cpp,$(TEST_OUT)/cxx-spaceship/invalid-x86.log)
	$(GREP) -F -q "RinOS C++20 built-in <=> requires integral, enum, or compatible pointer operands" $(TEST_OUT)/cxx-spaceship/invalid-x86.log
	$(GREP) -F -q "C++20 comparison rewriting requires an integer-returning operator<=> in the bounded RCC++ profile" $(TEST_OUT)/cxx-spaceship/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-spaceship/invalid-x64.ro tests/cxx_spaceship_invalid.cpp,$(TEST_OUT)/cxx-spaceship/invalid-x64.log)
	$(GREP) -F -q "RinOS C++20 built-in <=> requires integral, enum, or compatible pointer operands" $(TEST_OUT)/cxx-spaceship/invalid-x64.log
	$(GREP) -F -q "C++20 comparison rewriting requires an integer-returning operator<=> in the bounded RCC++ profile" $(TEST_OUT)/cxx-spaceship/invalid-x64.log

test-cxx-final: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-final)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-final,cxx_final.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-final/invalid-x86.ro tests/cxx_final_invalid.cpp,$(TEST_OUT)/cxx-final/invalid-x86.log)
	$(GREP) -F -q "cannot derive from final class 'FinalBase'" $(TEST_OUT)/cxx-final/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-final/invalid-x64.ro tests/cxx_final_invalid.cpp,$(TEST_OUT)/cxx-final/invalid-x64.log)
	$(GREP) -F -q "cannot derive from final class 'FinalBase'" $(TEST_OUT)/cxx-final/invalid-x64.log

test-cxx-override: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-override)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-override/invalid-x86.ro tests/cxx_override_invalid.cpp,$(TEST_OUT)/cxx-override/invalid-x86.log)
	$(GREP) -F -q "marked override but does not override a base class method" $(TEST_OUT)/cxx-override/invalid-x86.log
	$(GREP) -F -q "cannot override final method 'final_value'" $(TEST_OUT)/cxx-override/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-override/invalid-x64.ro tests/cxx_override_invalid.cpp,$(TEST_OUT)/cxx-override/invalid-x64.log)
	$(GREP) -F -q "marked override but does not override a base class method" $(TEST_OUT)/cxx-override/invalid-x64.log
	$(GREP) -F -q "cannot override final method 'final_value'" $(TEST_OUT)/cxx-override/invalid-x64.log

test-cxx-conditional-explicit: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-conditional-explicit)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-conditional-explicit,cxx_conditional_explicit.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-conditional-explicit/old-x86.ro tests/cxx_conditional_explicit_invalid.cpp,$(TEST_OUT)/cxx-conditional-explicit/old-x86.log)
	$(GREP) -F -q "conditional explicit specifiers require C++20 or newer" $(TEST_OUT)/cxx-conditional-explicit/old-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-conditional-explicit/old-x64.ro tests/cxx_conditional_explicit_invalid.cpp,$(TEST_OUT)/cxx-conditional-explicit/old-x64.log)
	$(GREP) -F -q "conditional explicit specifiers require C++20 or newer" $(TEST_OUT)/cxx-conditional-explicit/old-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-conditional-explicit/nonconstant-x86.ro tests/cxx_conditional_explicit_invalid.cpp,$(TEST_OUT)/cxx-conditional-explicit/nonconstant-x86.log)
	$(GREP) -F -q "conditional explicit specifier requires an integral constant expression" $(TEST_OUT)/cxx-conditional-explicit/nonconstant-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-conditional-explicit/nonconstant-x64.ro tests/cxx_conditional_explicit_invalid.cpp,$(TEST_OUT)/cxx-conditional-explicit/nonconstant-x64.log)
	$(GREP) -F -q "conditional explicit specifier requires an integral constant expression" $(TEST_OUT)/cxx-conditional-explicit/nonconstant-x64.log

test-cxx-class-template-deduction: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-deduction)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-class-template-deduction,cxx_class_template_deduction.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++14 -c -o $(TEST_OUT)/cxx-class-template-deduction/old-x86.ro tests/cxx_class_template_deduction_invalid.cpp,$(TEST_OUT)/cxx-class-template-deduction/old-x86.log)
	$(GREP) -F -q "class template argument deduction requires C++17 or newer" $(TEST_OUT)/cxx-class-template-deduction/old-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++14 -c -o $(TEST_OUT)/cxx-class-template-deduction/old-x64.ro tests/cxx_class_template_deduction_invalid.cpp,$(TEST_OUT)/cxx-class-template-deduction/old-x64.log)
	$(GREP) -F -q "class template argument deduction requires C++17 or newer" $(TEST_OUT)/cxx-class-template-deduction/old-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++14 -c -o $(TEST_OUT)/cxx-class-template-deduction/guide-old-x86.ro tests/cxx_class_template_deduction_guide_invalid.cpp,$(TEST_OUT)/cxx-class-template-deduction/guide-old-x86.log)
	$(GREP) -F -q "deduction guides require C++17 or newer" $(TEST_OUT)/cxx-class-template-deduction/guide-old-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++14 -c -o $(TEST_OUT)/cxx-class-template-deduction/guide-old-x64.ro tests/cxx_class_template_deduction_guide_invalid.cpp,$(TEST_OUT)/cxx-class-template-deduction/guide-old-x64.log)
	$(GREP) -F -q "deduction guides require C++17 or newer" $(TEST_OUT)/cxx-class-template-deduction/guide-old-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x86.ro tests/cxx_class_template_aggregate_deduction_invalid.cpp,$(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x86.log)
	$(GREP) -F -q "aggregate class template argument deduction requires braced initialization before C++20" $(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x64.ro tests/cxx_class_template_aggregate_deduction_invalid.cpp,$(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x64.log)
	$(GREP) -F -q "aggregate class template argument deduction requires braced initialization before C++20" $(TEST_OUT)/cxx-class-template-deduction/aggregate-old-x64.log

test-cxx-pure-virtual: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-pure-virtual)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-pure-virtual,cxx_pure_virtual.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-pure-virtual/invalid-x86.ro tests/cxx_abstract_rejected.cpp,$(TEST_OUT)/cxx-pure-virtual/invalid-x86.log)
	$(GREP) -F -q "cannot instantiate abstract class 'AbstractValue'" $(TEST_OUT)/cxx-pure-virtual/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-pure-virtual/invalid-new-x86.ro tests/cxx_abstract_new_rejected.cpp,$(TEST_OUT)/cxx-pure-virtual/invalid-new-x86.log)
	$(GREP) -F -q "cannot allocate abstract class 'AbstractValue'" $(TEST_OUT)/cxx-pure-virtual/invalid-new-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-pure-virtual/invalid-x64.ro tests/cxx_abstract_rejected.cpp,$(TEST_OUT)/cxx-pure-virtual/invalid-x64.log)
	$(GREP) -F -q "cannot instantiate abstract class 'AbstractValue'" $(TEST_OUT)/cxx-pure-virtual/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-pure-virtual/invalid-new-x64.ro tests/cxx_abstract_new_rejected.cpp,$(TEST_OUT)/cxx-pure-virtual/invalid-new-x64.log)
	$(GREP) -F -q "cannot allocate abstract class 'AbstractValue'" $(TEST_OUT)/cxx-pure-virtual/invalid-new-x64.log

test-cxx-structured-bindings: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-structured-bindings)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-structured-bindings,cxx_structured_bindings.cpp)

test-cxx-alignas: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-alignas)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-alignas,cxx_alignas.cpp)

test-cxx-constinit: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constinit)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-constinit,cxx_constinit.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constinit/tls-x86.ro tests/cxx_constinit_tls.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constinit/tls-x64.ro tests/cxx_constinit_tls.cpp

test-cxx-using-enum: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-using-enum)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-using-enum,cxx_using_enum.cpp)

test-cxx-template-template: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-template)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-template-template,cxx_template_template.cpp)

test-cxx-range-for: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-range-for)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-range-for/x86.s tests/cxx_function_pointer_probe.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-range-for/x86.o $(TEST_OUT)/cxx-range-for/x86.s
	objdump -f $(TEST_OUT)/cxx-range-for/x86.o > $(TEST_OUT)/cxx-range-for/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-range-for/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-range-for/x64.s tests/cxx_function_pointer_probe.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-range-for/x64.o $(TEST_OUT)/cxx-range-for/x64.s
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-range-for/x64-host tests/cxx_range_for_run_test.c $(TEST_OUT)/cxx-range-for/x64.o
	$(TEST_OUT)/cxx-range-for/x64-host
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-range-for/invalid.ro tests/cxx_range_for_invalid.cpp,$(TEST_OUT)/cxx-range-for/invalid.log)
	$(GREP) -F -q "const auto&& range variable cannot bind to an array lvalue" $(TEST_OUT)/cxx-range-for/invalid.log

test-cxx-iterator-range-for: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-iterator-range-for)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-iterator-range-for,cxx_iterator_range_for.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-iterator-range-for/invalid-x86.ro tests/cxx_iterator_range_for_invalid.cpp,$(TEST_OUT)/cxx-iterator-range-for/invalid-x86.log)
	$(GREP) -F -q "requires one public non-overloaded begin() and end() member" $(TEST_OUT)/cxx-iterator-range-for/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-iterator-range-for/invalid-x64.ro tests/cxx_iterator_range_for_invalid.cpp,$(TEST_OUT)/cxx-iterator-range-for/invalid-x64.log)
	$(GREP) -F -q "requires one public non-overloaded begin() and end() member" $(TEST_OUT)/cxx-iterator-range-for/invalid-x64.log

test-cxx-operator-arrow: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-operator-arrow)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-operator-arrow,cxx_operator_arrow.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-operator-arrow/invalid-x86.ro tests/cxx_operator_arrow_invalid.cpp,$(TEST_OUT)/cxx-operator-arrow/invalid-x86.log)
	$(GREP) -F -q "operator-> must return a pointer in the bounded RCC++ profile" $(TEST_OUT)/cxx-operator-arrow/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-operator-arrow/invalid-x64.ro tests/cxx_operator_arrow_invalid.cpp,$(TEST_OUT)/cxx-operator-arrow/invalid-x64.log)
	$(GREP) -F -q "operator-> must return a pointer in the bounded RCC++ profile" $(TEST_OUT)/cxx-operator-arrow/invalid-x64.log

test-cxx-requires-expression: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-requires-expression)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-requires-expression,cxx_requires_expression.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-requires-expression/invalid-x86.ro tests/cxx_requires_expression_invalid.cpp,$(TEST_OUT)/cxx-requires-expression/invalid-x86.log)
	$(GREP) -F -q "requires-expression parameters cannot have defaults" $(TEST_OUT)/cxx-requires-expression/invalid-x86.log
	$(GREP) -F -q "unsupported C++20 requires-expression return constraint" $(TEST_OUT)/cxx-requires-expression/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-requires-expression/invalid-x64.ro tests/cxx_requires_expression_invalid.cpp,$(TEST_OUT)/cxx-requires-expression/invalid-x64.log)
	$(GREP) -F -q "requires-expression parameters cannot have defaults" $(TEST_OUT)/cxx-requires-expression/invalid-x64.log
	$(GREP) -F -q "unsupported C++20 requires-expression return constraint" $(TEST_OUT)/cxx-requires-expression/invalid-x64.log

test-cxx-requires-type: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-requires-type)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-requires-type,cxx_requires_type.cpp)

test-cxx-inline-variables: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inline-variables)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-inline-variables,cxx_inline_variable.cpp)

test-cxx-inline-namespace: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inline-namespace)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-inline-namespace,cxx_inline_namespace.cpp)

test-cxx-nested-namespace: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-nested-namespace)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-nested-namespace,cxx_nested_namespace.cpp)

test-cxx-namespace-alias: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-namespace-alias)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-namespace-alias,cxx_namespace_alias.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-namespace-alias/invalid-x86.ro tests/cxx_namespace_alias_invalid.cpp,$(TEST_OUT)/cxx-namespace-alias/invalid-x86.log)
	$(GREP) -F -q "unknown namespace alias target" $(TEST_OUT)/cxx-namespace-alias/invalid-x86.log
	$(GREP) -F -q "namespace alias 'api' conflicts" $(TEST_OUT)/cxx-namespace-alias/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-namespace-alias/invalid-x64.ro tests/cxx_namespace_alias_invalid.cpp,$(TEST_OUT)/cxx-namespace-alias/invalid-x64.log)
	$(GREP) -F -q "unknown namespace alias target" $(TEST_OUT)/cxx-namespace-alias/invalid-x64.log
	$(GREP) -F -q "namespace alias 'api' conflicts" $(TEST_OUT)/cxx-namespace-alias/invalid-x64.log

test-cxx-friend-function: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-friend-function)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-friend-function,cxx_friend_function.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-friend-function/invalid-x86.ro tests/cxx_friend_class_invalid.cpp,$(TEST_OUT)/cxx-friend-function/invalid-x86.log)
	$(GREP) -F -q "member 'value' is not accessible" $(TEST_OUT)/cxx-friend-function/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-friend-function/invalid-x64.ro tests/cxx_friend_class_invalid.cpp,$(TEST_OUT)/cxx-friend-function/invalid-x64.log)
	$(GREP) -F -q "member 'value' is not accessible" $(TEST_OUT)/cxx-friend-function/invalid-x64.log

test-cxx-nodiscard: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-nodiscard)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-nodiscard/warnings-x86.s tests/cxx_nodiscard.cpp >$(TEST_OUT)/cxx-nodiscard/x86.log 2>&1
	$(call CHECK_COUNT,ignoring return value of nodiscard function,$(TEST_OUT)/cxx-nodiscard/x86.log,2)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-nodiscard/warnings-x64.s tests/cxx_nodiscard.cpp >$(TEST_OUT)/cxx-nodiscard/x64.log 2>&1
	$(call CHECK_COUNT,ignoring return value of nodiscard function,$(TEST_OUT)/cxx-nodiscard/x64.log,2)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-nodiscard,cxx_nodiscard.cpp)

test-cxx-deprecated: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-deprecated)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-deprecated/warnings-x86.s tests/cxx_deprecated.cpp >$(TEST_OUT)/cxx-deprecated/x86.log 2>&1
	$(call CHECK_COUNT,use of deprecated,$(TEST_OUT)/cxx-deprecated/x86.log,4)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-deprecated/warnings-x64.s tests/cxx_deprecated.cpp >$(TEST_OUT)/cxx-deprecated/x64.log 2>&1
	$(call CHECK_COUNT,use of deprecated,$(TEST_OUT)/cxx-deprecated/x64.log,4)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-deprecated,cxx_deprecated.cpp)

test-cxx-friend-class: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-friend-class)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-friend-class,cxx_friend_class.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-friend-class/invalid-x86.ro tests/cxx_friend_class_invalid.cpp,$(TEST_OUT)/cxx-friend-class/invalid-x86.log)
	$(GREP) -F -q "member 'value' is not accessible" $(TEST_OUT)/cxx-friend-class/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-friend-class/invalid-x64.ro tests/cxx_friend_class_invalid.cpp,$(TEST_OUT)/cxx-friend-class/invalid-x64.log)
	$(GREP) -F -q "member 'value' is not accessible" $(TEST_OUT)/cxx-friend-class/invalid-x64.log

test-cxx-selection-init: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-selection-init)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-selection-init,cxx_selection_init.cpp)

test-cxx-aggregate-paren-init: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-aggregate-paren-init)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-aggregate-paren-init,cxx_aggregate_paren_init.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/invalid-x86.ro tests/cxx_aggregate_paren_init_invalid.cpp,$(TEST_OUT)/cxx-aggregate-paren-init/invalid-x86.log)
	$(GREP) -F -q "too many initializers for aggregate" $(TEST_OUT)/cxx-aggregate-paren-init/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/invalid-x64.ro tests/cxx_aggregate_paren_init_invalid.cpp,$(TEST_OUT)/cxx-aggregate-paren-init/invalid-x64.log)
	$(GREP) -F -q "too many initializers for aggregate" $(TEST_OUT)/cxx-aggregate-paren-init/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x86.ro tests/cxx_aggregate_paren_init.cpp,$(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x86.log)
	$(GREP) -F -q "C++20 aggregate parenthesized initialization requires C++20 or newer" $(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x64.ro tests/cxx_aggregate_paren_init.cpp,$(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x64.log)
	$(GREP) -F -q "C++20 aggregate parenthesized initialization requires C++20 or newer" $(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++14 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx14-x86.ro tests/cxx_aggregate_bases_pre17_invalid.cpp,$(TEST_OUT)/cxx-aggregate-paren-init/base-cxx14-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++14 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx14-x64.ro tests/cxx_aggregate_bases_pre17_invalid.cpp,$(TEST_OUT)/cxx-aggregate-paren-init/base-cxx14-x64.log)
	$(call CHECK_COUNT,no safely lowerable constructor accepts the C++ initializer,$(TEST_OUT)/cxx-aggregate-paren-init/base-cxx14-x86.log,1)
	$(call CHECK_COUNT,no safely lowerable constructor accepts the C++ initializer,$(TEST_OUT)/cxx-aggregate-paren-init/base-cxx14-x64.log,1)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/paren-base-invalid-x86.ro tests/cxx_aggregate_paren_base_invalid.cpp,$(TEST_OUT)/cxx-aggregate-paren-init/paren-base-invalid-x86.log)
	$(call CHECK_COUNT,incompatible aggregate copy initialization,$(TEST_OUT)/cxx-aggregate-paren-init/paren-base-invalid-x86.log,2)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/paren-base-invalid-x64.ro tests/cxx_aggregate_paren_base_invalid.cpp,$(TEST_OUT)/cxx-aggregate-paren-init/paren-base-invalid-x64.log)
	$(call CHECK_COUNT,incompatible aggregate copy initialization,$(TEST_OUT)/cxx-aggregate-paren-init/paren-base-invalid-x64.log,2)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -S -o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x86.s tests/cxx_aggregate_bases.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x86.o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x86.s
	objdump -f $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x86.o > $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x86.arch
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x86.arch
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -S -o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x64.s tests/cxx_aggregate_bases.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x64.o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x64-host tests/cxx_language_core_host.c $(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x64.o
	$(TEST_OUT)/cxx-aggregate-paren-init/base-cxx17-x64-host
else
test-cxx-conversion-operator: test-cxx-conversion-operator-posix
test-cxx-nonmember-operator: test-cxx-nonmember-operator-posix
test-cxx-spaceship: test-cxx-spaceship-posix
test-cxx-final: test-cxx-final-posix
test-cxx-override: test-cxx-override-posix
test-cxx-conditional-explicit: test-cxx-conditional-explicit-posix
test-cxx-class-template-deduction: test-cxx-class-template-deduction-posix
test-cxx-pure-virtual: test-cxx-pure-virtual-posix
test-cxx-structured-bindings: test-cxx-structured-bindings-posix
test-cxx-alignas: test-cxx-alignas-posix
test-cxx-constinit: test-cxx-constinit-posix
test-cxx-using-enum: test-cxx-using-enum-posix
test-cxx-template-template: test-cxx-template-template-posix
test-cxx-range-for: test-cxx-range-for-posix
test-cxx-iterator-range-for: test-cxx-iterator-range-for-posix
test-cxx-operator-arrow: test-cxx-operator-arrow-posix
test-cxx-requires-expression: test-cxx-requires-expression-posix
test-cxx-requires-type: test-cxx-requires-type-posix
test-cxx-inline-variables: test-cxx-inline-variables-posix
test-cxx-inline-namespace: test-cxx-inline-namespace-posix
test-cxx-nested-namespace: test-cxx-nested-namespace-posix
test-cxx-namespace-alias: test-cxx-namespace-alias-posix
test-cxx-friend-function: test-cxx-friend-function-posix
test-cxx-nodiscard: test-cxx-nodiscard-posix
test-cxx-deprecated: test-cxx-deprecated-posix
test-cxx-friend-class: test-cxx-friend-class-posix
test-cxx-selection-init: test-cxx-selection-init-posix
test-cxx-aggregate-paren-init: test-cxx-aggregate-paren-init-posix
endif

ifeq ($(OS),Windows_NT)
test-cxx-designated-initializer: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-designated-initializer)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-designated-initializer/x86.s tests/cxx_designated_initializer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-designated-initializer/x86.o $(TEST_OUT)/cxx-designated-initializer/x86.s
	objdump -f $(TEST_OUT)/cxx-designated-initializer/x86.o > $(TEST_OUT)/cxx-designated-initializer/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-designated-initializer/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-designated-initializer/x64.s tests/cxx_designated_initializer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-designated-initializer/x64.o $(TEST_OUT)/cxx-designated-initializer/x64.s
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-designated-initializer/x64-host tests/cxx_designated_initializer_run_test.c $(TEST_OUT)/cxx-designated-initializer/x64.o
	$(TEST_OUT)/cxx-designated-initializer/x64-host
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-designated-initializer/invalid-order-x86.ro tests/cxx_designated_initializer_invalid.cpp,$(TEST_OUT)/cxx-designated-initializer/invalid-order-x86.log)
	$(GREP) -F -q "declaration order" $(TEST_OUT)/cxx-designated-initializer/invalid-order-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-designated-initializer/invalid-order-x64.ro tests/cxx_designated_initializer_invalid.cpp,$(TEST_OUT)/cxx-designated-initializer/invalid-order-x64.log)
	$(GREP) -F -q "declaration order" $(TEST_OUT)/cxx-designated-initializer/invalid-order-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x86.ro tests/cxx_designated_initializer_mixed_invalid.cpp,$(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x86.log)
	$(GREP) -F -q "cannot mix designated and positional" $(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x64.ro tests/cxx_designated_initializer_mixed_invalid.cpp,$(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x64.log)
	$(GREP) -F -q "cannot mix designated and positional" $(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-designated-initializer/invalid-nested-x86.ro tests/cxx_designated_initializer_nested_invalid.cpp,$(TEST_OUT)/cxx-designated-initializer/invalid-nested-x86.log)
	$(GREP) -F -q "nested designators" $(TEST_OUT)/cxx-designated-initializer/invalid-nested-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-designated-initializer/invalid-nested-x64.ro tests/cxx_designated_initializer_nested_invalid.cpp,$(TEST_OUT)/cxx-designated-initializer/invalid-nested-x64.log)
	$(GREP) -F -q "nested designators" $(TEST_OUT)/cxx-designated-initializer/invalid-nested-x64.log

test-cxx-utf8-literals: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-utf8-literals)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-utf8-literals/x86.s tests/cxx_utf8_literals.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-utf8-literals/x86.o $(TEST_OUT)/cxx-utf8-literals/x86.s
	objdump -f $(TEST_OUT)/cxx-utf8-literals/x86.o > $(TEST_OUT)/cxx-utf8-literals/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-utf8-literals/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-utf8-literals/x64.s tests/cxx_utf8_literals.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-utf8-literals/x64.o $(TEST_OUT)/cxx-utf8-literals/x64.s
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-utf8-literals/x64-host tests/cxx_utf8_literals_run_test.c $(TEST_OUT)/cxx-utf8-literals/x64.o
	$(TEST_OUT)/cxx-utf8-literals/x64-host
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-utf8-literals/invalid-x86.ro tests/cxx_prefixed_literal_invalid.cpp,$(TEST_OUT)/cxx-utf8-literals/invalid-x86.log)
	$(GREP) -F -q "wide, UTF-16, and UTF-32 literals are not supported" $(TEST_OUT)/cxx-utf8-literals/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-utf8-literals/invalid-x64.ro tests/cxx_prefixed_literal_invalid.cpp,$(TEST_OUT)/cxx-utf8-literals/invalid-x64.log)
	$(GREP) -F -q "wide, UTF-16, and UTF-32 literals are not supported" $(TEST_OUT)/cxx-utf8-literals/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-utf8-literals/invalid-mixed.ro tests/cxx_mixed_literal_invalid.cpp,$(TEST_OUT)/cxx-utf8-literals/invalid-mixed.log)
	$(GREP) -F -q "adjacent ordinary and UTF-8 string literals cannot be concatenated" $(TEST_OUT)/cxx-utf8-literals/invalid-mixed.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-utf8-literals/invalid-array-x86.ro tests/cxx_mismatched_literal_array_invalid.cpp,$(TEST_OUT)/cxx-utf8-literals/invalid-array-x86.log)
	$(GREP) -F -q "character array initializer encoding does not match the element type" $(TEST_OUT)/cxx-utf8-literals/invalid-array-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-utf8-literals/invalid-array-x64.ro tests/cxx_mismatched_literal_array_invalid.cpp,$(TEST_OUT)/cxx-utf8-literals/invalid-array-x64.log)
	$(GREP) -F -q "character array initializer encoding does not match the element type" $(TEST_OUT)/cxx-utf8-literals/invalid-array-x64.log
else
test-cxx-designated-initializer: test-cxx-designated-initializer-posix
test-cxx-utf8-literals: test-cxx-utf8-literals-posix
endif

test-cxx-non-type-templates: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-non-type-templates)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-non-type-templates/x86.ro \
		tests/cxx_non_type_templates.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-non-type-templates/x64.ro \
		tests/cxx_non_type_templates.cpp
	$(call CHECK_BINARY_STRING,_ZN12add_constantEILi3EEi,$(TEST_OUT)/cxx-non-type-templates/x86.ro)
	$(call CHECK_BINARY_STRING,_ZN12add_constantEILin2EEi,$(TEST_OUT)/cxx-non-type-templates/x86.ro)
	$(call CHECK_BINARY_STRING,_ZN20add_default_constantEILi4EEi,$(TEST_OUT)/cxx-non-type-templates/x64.ro)
	$(call CHECK_BINARY_STRING,_ZN22add_default_from_valueEILi3ELi4EEi,$(TEST_OUT)/cxx-non-type-templates/x86.ro)
	@echo "RCC++ non-type integer template tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-auto-non-type-template: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-auto-non-type-template)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-auto-non-type-template,cxx_auto_non_type_template.cpp)
	@echo "C++ auto non-type template tests completed"
else
test-cxx-auto-non-type-template: test-cxx-auto-non-type-template-posix
endif

test-cxx-auto-non-type-template-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-auto-non-type-template)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-auto-non-type-template/x86.s \
		tests/cxx_auto_non_type_template.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-auto-non-type-template/x86.o \
		$(TEST_OUT)/cxx-auto-non-type-template/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-auto-non-type-template/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-auto-non-type-template/x86 \
		$(TEST_OUT)/cxx-auto-non-type-template/start-x86.o \
		$(TEST_OUT)/cxx-auto-non-type-template/x86.o
	$(TEST_OUT)/cxx-auto-non-type-template/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-auto-non-type-template/x64.s \
		tests/cxx_auto_non_type_template.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-auto-non-type-template/x64.o \
		$(TEST_OUT)/cxx-auto-non-type-template/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-auto-non-type-template/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-auto-non-type-template/x64 \
		$(TEST_OUT)/cxx-auto-non-type-template/start-x64.o \
		$(TEST_OUT)/cxx-auto-non-type-template/x64.o
	$(TEST_OUT)/cxx-auto-non-type-template/x64
	@echo "C++ auto non-type template tests completed"

test-cxx-range-for-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-range-for)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-range-for/x86.s \
		tests/cxx_function_pointer_probe.cpp
	$(GREP) -F -x -q '.globl _Z12call_pointerPFiiEi' \
		$(TEST_OUT)/cxx-range-for/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-range-for/x86.o \
		$(TEST_OUT)/cxx-range-for/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-range-for/x86 \
		tests/cxx_range_for_run_test.c \
		$(TEST_OUT)/cxx-range-for/x86.o
	$(TEST_OUT)/cxx-range-for/x86
	@echo "C++ braced range-for i686 compile completed"
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-range-for/x64.s \
		tests/cxx_function_pointer_probe.cpp
	$(GREP) -F -x -q '.globl _Z12call_pointerPFiiEi' \
		$(TEST_OUT)/cxx-range-for/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-range-for/x64.o \
		$(TEST_OUT)/cxx-range-for/x64.s
	$(CC) -o $(TEST_OUT)/cxx-range-for/x64 \
		tests/cxx_range_for_run_test.c \
		$(TEST_OUT)/cxx-range-for/x64.o
	$(TEST_OUT)/cxx-range-for/x64
	@echo "C++ braced range-for AMD64 compile completed"
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-range-for/invalid.ro \
		tests/cxx_range_for_invalid.cpp \
		>$(TEST_OUT)/cxx-range-for/invalid.log 2>&1; then \
		echo "const auto&& range-for unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "const auto&& range variable cannot bind to an array lvalue" \
		$(TEST_OUT)/cxx-range-for/invalid.log
	@echo "C++ array-lvalue range-for tests completed"

test-cxx-iterator-range-for-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-iterator-range-for)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-iterator-range-for/x86.s \
		tests/cxx_iterator_range_for.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-iterator-range-for/x86.o \
		$(TEST_OUT)/cxx-iterator-range-for/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-iterator-range-for/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-iterator-range-for/x86 \
		$(TEST_OUT)/cxx-iterator-range-for/start-x86.o \
		$(TEST_OUT)/cxx-iterator-range-for/x86.o
	$(TEST_OUT)/cxx-iterator-range-for/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-iterator-range-for/x64.s \
		tests/cxx_iterator_range_for.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-iterator-range-for/x64.o \
		$(TEST_OUT)/cxx-iterator-range-for/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-iterator-range-for/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-iterator-range-for/x64 \
		$(TEST_OUT)/cxx-iterator-range-for/start-x64.o \
		$(TEST_OUT)/cxx-iterator-range-for/x64.o
	$(TEST_OUT)/cxx-iterator-range-for/x64
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-iterator-range-for/invalid.ro \
		tests/cxx_iterator_range_for_invalid.cpp \
		>$(TEST_OUT)/cxx-iterator-range-for/invalid.log 2>&1; then \
		echo "unsupported iterator range-for unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "requires one public non-overloaded begin() and end() member" \
		$(TEST_OUT)/cxx-iterator-range-for/invalid.log
	@echo "C++ bounded iterator range-for tests completed"

test-cxx-operator-arrow-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-operator-arrow)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-operator-arrow/x86.s \
		tests/cxx_operator_arrow.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-operator-arrow/x86.o \
		$(TEST_OUT)/cxx-operator-arrow/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-operator-arrow/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-operator-arrow/x86 \
		$(TEST_OUT)/cxx-operator-arrow/start-x86.o \
		$(TEST_OUT)/cxx-operator-arrow/x86.o
	$(TEST_OUT)/cxx-operator-arrow/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-operator-arrow/x64.s \
		tests/cxx_operator_arrow.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-operator-arrow/x64.o \
		$(TEST_OUT)/cxx-operator-arrow/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-operator-arrow/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-operator-arrow/x64 \
		$(TEST_OUT)/cxx-operator-arrow/start-x64.o \
		$(TEST_OUT)/cxx-operator-arrow/x64.o
	$(TEST_OUT)/cxx-operator-arrow/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-operator-arrow/invalid-x86.ro \
		tests/cxx_operator_arrow_invalid.cpp \
		>$(TEST_OUT)/cxx-operator-arrow/invalid-x86.log 2>&1; then \
		echo "invalid operator-> return type unexpectedly compiled"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-operator-arrow/invalid-x64.ro \
		tests/cxx_operator_arrow_invalid.cpp \
		>$(TEST_OUT)/cxx-operator-arrow/invalid-x64.log 2>&1; then \
		echo "invalid operator-> return type unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "operator-> must return a pointer in the bounded RCC++ profile" \
		$(TEST_OUT)/cxx-operator-arrow/invalid-x86.log
	$(GREP) -q "operator-> must return a pointer in the bounded RCC++ profile" \
		$(TEST_OUT)/cxx-operator-arrow/invalid-x64.log
	@echo "C++ overloaded operator-> tests completed"

test-cxx-requires-expression-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-requires-expression)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-requires-expression/x86.s \
		tests/cxx_requires_expression.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-requires-expression/x86.o \
		$(TEST_OUT)/cxx-requires-expression/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-requires-expression/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-requires-expression/x86 \
		$(TEST_OUT)/cxx-requires-expression/start-x86.o \
		$(TEST_OUT)/cxx-requires-expression/x86.o
	$(TEST_OUT)/cxx-requires-expression/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-requires-expression/x64.s \
		tests/cxx_requires_expression.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-requires-expression/x64.o \
		$(TEST_OUT)/cxx-requires-expression/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-requires-expression/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-requires-expression/x64 \
		$(TEST_OUT)/cxx-requires-expression/start-x64.o \
		$(TEST_OUT)/cxx-requires-expression/x64.o
	$(TEST_OUT)/cxx-requires-expression/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-requires-expression/invalid-x86.ro \
		tests/cxx_requires_expression_invalid.cpp \
		>$(TEST_OUT)/cxx-requires-expression/invalid-x86.log 2>&1; then \
		echo "parameter-list requires-expression unexpectedly compiled"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-requires-expression/invalid-x64.ro \
		tests/cxx_requires_expression_invalid.cpp \
		>$(TEST_OUT)/cxx-requires-expression/invalid-x64.log 2>&1; then \
		echo "parameter-list requires-expression unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "requires-expression parameters cannot have defaults" \
		$(TEST_OUT)/cxx-requires-expression/invalid-x86.log
	$(GREP) -q "requires-expression parameters cannot have defaults" \
		$(TEST_OUT)/cxx-requires-expression/invalid-x64.log
	$(GREP) -q "unsupported C++20 requires-expression return constraint" \
		$(TEST_OUT)/cxx-requires-expression/invalid-x86.log
	$(GREP) -q "unsupported C++20 requires-expression return constraint" \
		$(TEST_OUT)/cxx-requires-expression/invalid-x64.log
	@echo "C++20 bounded requires-expression and return-constraint tests completed"

test-cxx-requires-type-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-requires-type)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-requires-type/x86.s \
		tests/cxx_requires_type.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-requires-type/x86.o \
		$(TEST_OUT)/cxx-requires-type/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-requires-type/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-requires-type/x86 \
		$(TEST_OUT)/cxx-requires-type/start-x86.o \
		$(TEST_OUT)/cxx-requires-type/x86.o
	$(TEST_OUT)/cxx-requires-type/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-requires-type/x64.s \
		tests/cxx_requires_type.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-requires-type/x64.o \
		$(TEST_OUT)/cxx-requires-type/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-requires-type/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-requires-type/x64 \
		$(TEST_OUT)/cxx-requires-type/start-x64.o \
		$(TEST_OUT)/cxx-requires-type/x64.o
	$(TEST_OUT)/cxx-requires-type/x64
	@echo "C++20 requires-expression type requirement tests completed"

test-cxx-inline-variables-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inline-variables)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-inline-variables/x86.s \
		tests/cxx_inline_variable.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-inline-variables/x86.o \
		$(TEST_OUT)/cxx-inline-variables/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-inline-variables/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-inline-variables/x86 \
		$(TEST_OUT)/cxx-inline-variables/start-x86.o \
		$(TEST_OUT)/cxx-inline-variables/x86.o
	$(TEST_OUT)/cxx-inline-variables/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-inline-variables/x64.s \
		tests/cxx_inline_variable.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-inline-variables/x64.o \
		$(TEST_OUT)/cxx-inline-variables/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-inline-variables/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-inline-variables/x64 \
		$(TEST_OUT)/cxx-inline-variables/start-x64.o \
		$(TEST_OUT)/cxx-inline-variables/x64.o
	$(TEST_OUT)/cxx-inline-variables/x64
	@echo "C++ inline variable tests completed"

test-cxx-inline-namespace-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inline-namespace)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-inline-namespace/x86.s \
		tests/cxx_inline_namespace.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-inline-namespace/x86.o \
		$(TEST_OUT)/cxx-inline-namespace/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-inline-namespace/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-inline-namespace/x86 \
		$(TEST_OUT)/cxx-inline-namespace/start-x86.o \
		$(TEST_OUT)/cxx-inline-namespace/x86.o
	$(TEST_OUT)/cxx-inline-namespace/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-inline-namespace/x64.s \
		tests/cxx_inline_namespace.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-inline-namespace/x64.o \
		$(TEST_OUT)/cxx-inline-namespace/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-inline-namespace/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-inline-namespace/x64 \
		$(TEST_OUT)/cxx-inline-namespace/start-x64.o \
		$(TEST_OUT)/cxx-inline-namespace/x64.o
	$(TEST_OUT)/cxx-inline-namespace/x64
	@echo "C++ inline namespace tests completed"

test-cxx-nested-namespace-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-nested-namespace)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-nested-namespace/x86.s \
		tests/cxx_nested_namespace.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nested-namespace/x86.o \
		$(TEST_OUT)/cxx-nested-namespace/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nested-namespace/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-nested-namespace/x86 \
		$(TEST_OUT)/cxx-nested-namespace/start-x86.o \
		$(TEST_OUT)/cxx-nested-namespace/x86.o
	$(TEST_OUT)/cxx-nested-namespace/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-nested-namespace/x64.s \
		tests/cxx_nested_namespace.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-nested-namespace/x64.o \
		$(TEST_OUT)/cxx-nested-namespace/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-nested-namespace/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-nested-namespace/x64 \
		$(TEST_OUT)/cxx-nested-namespace/start-x64.o \
		$(TEST_OUT)/cxx-nested-namespace/x64.o
	$(TEST_OUT)/cxx-nested-namespace/x64
	@echo "C++ nested namespace definition tests completed"

test-cxx-namespace-alias-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-namespace-alias)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-namespace-alias/x86.s \
		tests/cxx_namespace_alias.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-namespace-alias/x86.o \
		$(TEST_OUT)/cxx-namespace-alias/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-namespace-alias/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-namespace-alias/x86 \
		$(TEST_OUT)/cxx-namespace-alias/start-x86.o \
		$(TEST_OUT)/cxx-namespace-alias/x86.o
	$(TEST_OUT)/cxx-namespace-alias/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-namespace-alias/x64.s \
		tests/cxx_namespace_alias.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-namespace-alias/x64.o \
		$(TEST_OUT)/cxx-namespace-alias/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-namespace-alias/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-namespace-alias/x64 \
		$(TEST_OUT)/cxx-namespace-alias/start-x64.o \
		$(TEST_OUT)/cxx-namespace-alias/x64.o
	$(TEST_OUT)/cxx-namespace-alias/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-namespace-alias/invalid-x86.ro \
		tests/cxx_namespace_alias_invalid.cpp \
		>$(TEST_OUT)/cxx-namespace-alias/invalid-x86.log 2>&1
	$(GREP) -q "unknown namespace alias target" \
		$(TEST_OUT)/cxx-namespace-alias/invalid-x86.log
	$(GREP) -q "namespace alias 'api' conflicts" \
		$(TEST_OUT)/cxx-namespace-alias/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-namespace-alias/invalid-x64.ro \
		tests/cxx_namespace_alias_invalid.cpp \
		>$(TEST_OUT)/cxx-namespace-alias/invalid-x64.log 2>&1
	$(GREP) -q "unknown namespace alias target" \
		$(TEST_OUT)/cxx-namespace-alias/invalid-x64.log
	$(GREP) -q "namespace alias 'api' conflicts" \
		$(TEST_OUT)/cxx-namespace-alias/invalid-x64.log
	@echo "C++ namespace alias tests completed"

test-cxx-friend-function-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-friend-function)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-friend-function/x86.s \
		tests/cxx_friend_function.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-friend-function/x86.o \
		$(TEST_OUT)/cxx-friend-function/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-friend-function/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-friend-function/x86 \
		$(TEST_OUT)/cxx-friend-function/start-x86.o \
		$(TEST_OUT)/cxx-friend-function/x86.o
	$(TEST_OUT)/cxx-friend-function/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-friend-function/x64.s \
		tests/cxx_friend_function.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-friend-function/x64.o \
		$(TEST_OUT)/cxx-friend-function/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-friend-function/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-friend-function/x64 \
		$(TEST_OUT)/cxx-friend-function/start-x64.o \
		$(TEST_OUT)/cxx-friend-function/x64.o
	$(TEST_OUT)/cxx-friend-function/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-friend-function/invalid-x86.ro \
		tests/cxx_friend_class_invalid.cpp \
		>$(TEST_OUT)/cxx-friend-function/invalid-x86.log 2>&1
	$(GREP) -q "member 'value' is not accessible" \
		$(TEST_OUT)/cxx-friend-function/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-friend-function/invalid-x64.ro \
		tests/cxx_friend_class_invalid.cpp \
		>$(TEST_OUT)/cxx-friend-function/invalid-x64.log 2>&1
	$(GREP) -q "member 'value' is not accessible" \
		$(TEST_OUT)/cxx-friend-function/invalid-x64.log
	@echo "C++ friend function and class access tests completed"

test-cxx-nodiscard-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-nodiscard)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-nodiscard/x86.s \
		tests/cxx_nodiscard.cpp \
		>$(TEST_OUT)/cxx-nodiscard/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-nodiscard/x64.s \
		tests/cxx_nodiscard.cpp \
		>$(TEST_OUT)/cxx-nodiscard/x64.log 2>&1
	test "$$($(GREP) -c "ignoring return value of nodiscard function" \
		$(TEST_OUT)/cxx-nodiscard/x86.log)" -eq 2
	test "$$($(GREP) -c "ignoring return value of nodiscard function" \
		$(TEST_OUT)/cxx-nodiscard/x64.log)" -eq 2
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nodiscard/x86.o \
		$(TEST_OUT)/cxx-nodiscard/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nodiscard/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-nodiscard/x86 \
		$(TEST_OUT)/cxx-nodiscard/start-x86.o \
		$(TEST_OUT)/cxx-nodiscard/x86.o
	$(TEST_OUT)/cxx-nodiscard/x86
	$(CC) -c -o $(TEST_OUT)/cxx-nodiscard/x64.o \
		$(TEST_OUT)/cxx-nodiscard/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-nodiscard/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-nodiscard/x64 \
		$(TEST_OUT)/cxx-nodiscard/start-x64.o \
		$(TEST_OUT)/cxx-nodiscard/x64.o
	$(TEST_OUT)/cxx-nodiscard/x64
	@echo "C++ nodiscard attribute tests completed"

test-cxx-deprecated-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-deprecated)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-deprecated/x86.s \
		tests/cxx_deprecated.cpp \
		>$(TEST_OUT)/cxx-deprecated/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-deprecated/x64.s \
		tests/cxx_deprecated.cpp \
		>$(TEST_OUT)/cxx-deprecated/x64.log 2>&1
	test "$$($(GREP) -c "use of deprecated" \
		$(TEST_OUT)/cxx-deprecated/x86.log)" -eq 4
	test "$$($(GREP) -c "use of deprecated" \
		$(TEST_OUT)/cxx-deprecated/x64.log)" -eq 4
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-deprecated/x86.o \
		$(TEST_OUT)/cxx-deprecated/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-deprecated/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-deprecated/x86 \
		$(TEST_OUT)/cxx-deprecated/start-x86.o \
		$(TEST_OUT)/cxx-deprecated/x86.o
	$(TEST_OUT)/cxx-deprecated/x86
	$(CC) -c -o $(TEST_OUT)/cxx-deprecated/x64.o \
		$(TEST_OUT)/cxx-deprecated/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-deprecated/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-deprecated/x64 \
		$(TEST_OUT)/cxx-deprecated/start-x64.o \
		$(TEST_OUT)/cxx-deprecated/x64.o
	$(TEST_OUT)/cxx-deprecated/x64
	@echo "C++ deprecated attribute tests completed"

test-cxx-friend-class-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-friend-class)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-friend-class/x86.s \
		tests/cxx_friend_class.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-friend-class/x64.s \
		tests/cxx_friend_class.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-friend-class/x86.o \
		$(TEST_OUT)/cxx-friend-class/x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-friend-class/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-friend-class/x86 \
		$(TEST_OUT)/cxx-friend-class/start-x86.o \
		$(TEST_OUT)/cxx-friend-class/x86.o
	$(TEST_OUT)/cxx-friend-class/x86
	gcc -c -o $(TEST_OUT)/cxx-friend-class/x64.o \
		$(TEST_OUT)/cxx-friend-class/x64.s
	gcc -c -o $(TEST_OUT)/cxx-friend-class/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-friend-class/x64 \
		$(TEST_OUT)/cxx-friend-class/start-x64.o \
		$(TEST_OUT)/cxx-friend-class/x64.o
	$(TEST_OUT)/cxx-friend-class/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-friend-class/invalid-x86.ro \
		tests/cxx_friend_class_invalid.cpp \
		>$(TEST_OUT)/cxx-friend-class/invalid-x86.log 2>&1
	$(GREP) -q "member 'value' is not accessible" \
		$(TEST_OUT)/cxx-friend-class/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-friend-class/invalid-x64.ro \
		tests/cxx_friend_class_invalid.cpp \
		>$(TEST_OUT)/cxx-friend-class/invalid-x64.log 2>&1
	$(GREP) -q "member 'value' is not accessible" \
		$(TEST_OUT)/cxx-friend-class/invalid-x64.log
	@echo "C++ friend class access tests completed"

test-cxx-selection-init-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-selection-init)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-selection-init/x86.s \
		tests/cxx_selection_init.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-selection-init/x86.o \
		$(TEST_OUT)/cxx-selection-init/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-selection-init/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-selection-init/x86 \
		$(TEST_OUT)/cxx-selection-init/start-x86.o \
		$(TEST_OUT)/cxx-selection-init/x86.o
	$(TEST_OUT)/cxx-selection-init/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-selection-init/x64.s \
		tests/cxx_selection_init.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-selection-init/x64.o \
		$(TEST_OUT)/cxx-selection-init/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-selection-init/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-selection-init/x64 \
		$(TEST_OUT)/cxx-selection-init/start-x64.o \
		$(TEST_OUT)/cxx-selection-init/x64.o
	$(TEST_OUT)/cxx-selection-init/x64
	@echo "C++ selection-statement initializer tests completed"

test-cxx-aggregate-paren-init-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-aggregate-paren-init)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-aggregate-paren-init/x86.s \
		tests/cxx_aggregate_paren_init.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/x86.o \
		$(TEST_OUT)/cxx-aggregate-paren-init/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-aggregate-paren-init/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-aggregate-paren-init/x86 \
		$(TEST_OUT)/cxx-aggregate-paren-init/start-x86.o \
		$(TEST_OUT)/cxx-aggregate-paren-init/x86.o
	$(TEST_OUT)/cxx-aggregate-paren-init/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-aggregate-paren-init/x64.s \
		tests/cxx_aggregate_paren_init.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-aggregate-paren-init/x64.o \
		$(TEST_OUT)/cxx-aggregate-paren-init/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-aggregate-paren-init/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-aggregate-paren-init/x64 \
		$(TEST_OUT)/cxx-aggregate-paren-init/start-x64.o \
		$(TEST_OUT)/cxx-aggregate-paren-init/x64.o
	$(TEST_OUT)/cxx-aggregate-paren-init/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-aggregate-paren-init/invalid-x86.ro \
		tests/cxx_aggregate_paren_init_invalid.cpp \
		>$(TEST_OUT)/cxx-aggregate-paren-init/invalid-x86.log 2>&1
	$(GREP) -q "too many initializers for aggregate" \
		$(TEST_OUT)/cxx-aggregate-paren-init/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-aggregate-paren-init/invalid-x64.ro \
		tests/cxx_aggregate_paren_init_invalid.cpp \
		>$(TEST_OUT)/cxx-aggregate-paren-init/invalid-x64.log 2>&1
	$(GREP) -q "too many initializers for aggregate" \
		$(TEST_OUT)/cxx-aggregate-paren-init/invalid-x64.log
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x86.ro \
		tests/cxx_aggregate_paren_init.cpp \
		>$(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x86.log 2>&1
	$(GREP) -q "C++20 aggregate parenthesized initialization requires C++20 or newer" \
		$(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x64.ro \
		tests/cxx_aggregate_paren_init.cpp \
		>$(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x64.log 2>&1
	$(GREP) -q "C++20 aggregate parenthesized initialization requires C++20 or newer" \
		$(TEST_OUT)/cxx-aggregate-paren-init/cxx17-x64.log
	@echo "C++20 aggregate parenthesized initialization tests completed"

test-cxx-designated-initializer-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-designated-initializer)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-designated-initializer/x86.s \
		tests/cxx_designated_initializer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-designated-initializer/x86.o \
		$(TEST_OUT)/cxx-designated-initializer/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-designated-initializer/x86 \
		tests/cxx_designated_initializer_run_test.c \
		$(TEST_OUT)/cxx-designated-initializer/x86.o
	$(TEST_OUT)/cxx-designated-initializer/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-designated-initializer/x64.s \
		tests/cxx_designated_initializer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-designated-initializer/x64.o \
		$(TEST_OUT)/cxx-designated-initializer/x64.s
	$(CC) -o $(TEST_OUT)/cxx-designated-initializer/x64 \
		tests/cxx_designated_initializer_run_test.c \
		$(TEST_OUT)/cxx-designated-initializer/x64.o
	$(TEST_OUT)/cxx-designated-initializer/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-designated-initializer/invalid-order-x86.ro \
		tests/cxx_designated_initializer_invalid.cpp \
		>$(TEST_OUT)/cxx-designated-initializer/invalid-order-x86.log 2>&1; then \
		echo "C++ designated initializer order case unexpectedly compiled"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-designated-initializer/invalid-order-x64.ro \
		tests/cxx_designated_initializer_invalid.cpp \
		>$(TEST_OUT)/cxx-designated-initializer/invalid-order-x64.log 2>&1; then \
		echo "C++ designated initializer order case unexpectedly compiled"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x86.ro \
		tests/cxx_designated_initializer_mixed_invalid.cpp \
		>$(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x86.log 2>&1; then \
		echo "C++ designated initializer mixed case unexpectedly compiled"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x64.ro \
		tests/cxx_designated_initializer_mixed_invalid.cpp \
		>$(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x64.log 2>&1; then \
		echo "C++ designated initializer mixed case unexpectedly compiled"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-designated-initializer/invalid-nested-x86.ro \
		tests/cxx_designated_initializer_nested_invalid.cpp \
		>$(TEST_OUT)/cxx-designated-initializer/invalid-nested-x86.log 2>&1; then \
		echo "C++ designated initializer nested case unexpectedly compiled"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-designated-initializer/invalid-nested-x64.ro \
		tests/cxx_designated_initializer_nested_invalid.cpp \
		>$(TEST_OUT)/cxx-designated-initializer/invalid-nested-x64.log 2>&1; then \
		echo "C++ designated initializer nested case unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "declaration order" \
		$(TEST_OUT)/cxx-designated-initializer/invalid-order-x86.log
	$(GREP) -q "cannot mix designated and positional" \
		$(TEST_OUT)/cxx-designated-initializer/invalid-mixed-x64.log
	$(GREP) -q "nested designators" \
		$(TEST_OUT)/cxx-designated-initializer/invalid-nested-x86.log
	@echo "C++20 designated initializer tests completed"

test-cxx-utf8-literals-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-utf8-literals)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-utf8-literals/x86.s \
		tests/cxx_utf8_literals.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-utf8-literals/x86.o \
		$(TEST_OUT)/cxx-utf8-literals/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-utf8-literals/x86 \
		tests/cxx_utf8_literals_run_test.c \
		$(TEST_OUT)/cxx-utf8-literals/x86.o
	$(TEST_OUT)/cxx-utf8-literals/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-utf8-literals/x64.s \
		tests/cxx_utf8_literals.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-utf8-literals/x64.o \
		$(TEST_OUT)/cxx-utf8-literals/x64.s
	$(CC) -o $(TEST_OUT)/cxx-utf8-literals/x64 \
		tests/cxx_utf8_literals_run_test.c \
		$(TEST_OUT)/cxx-utf8-literals/x64.o
	$(TEST_OUT)/cxx-utf8-literals/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-utf8-literals/invalid-x86.ro \
		tests/cxx_prefixed_literal_invalid.cpp \
		>$(TEST_OUT)/cxx-utf8-literals/invalid-x86.log 2>&1; then \
		echo "unsupported prefixed literal case unexpectedly compiled"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-utf8-literals/invalid-x64.ro \
		tests/cxx_prefixed_literal_invalid.cpp \
		>$(TEST_OUT)/cxx-utf8-literals/invalid-x64.log 2>&1; then \
		echo "unsupported prefixed literal case unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "wide, UTF-16, and UTF-32 literals are not supported" \
		$(TEST_OUT)/cxx-utf8-literals/invalid-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-utf8-literals/invalid-mixed.ro \
		tests/cxx_mixed_literal_invalid.cpp \
		>$(TEST_OUT)/cxx-utf8-literals/invalid-mixed.log 2>&1; then \
		echo "mixed-encoding string literal case unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "adjacent ordinary and UTF-8 string literals cannot be concatenated" \
		$(TEST_OUT)/cxx-utf8-literals/invalid-mixed.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-utf8-literals/invalid-array-x86.ro \
		tests/cxx_mismatched_literal_array_invalid.cpp \
		>$(TEST_OUT)/cxx-utf8-literals/invalid-array-x86.log 2>&1; then \
		echo "mismatched character array encoding unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "character array initializer encoding does not match the element type" \
		$(TEST_OUT)/cxx-utf8-literals/invalid-array-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-utf8-literals/invalid-array-x64.ro \
		tests/cxx_mismatched_literal_array_invalid.cpp \
		>$(TEST_OUT)/cxx-utf8-literals/invalid-array-x64.log 2>&1; then \
		echo "mismatched character array encoding unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "character array initializer encoding does not match the element type" \
		$(TEST_OUT)/cxx-utf8-literals/invalid-array-x64.log
	@echo "C++ UTF-8 literal tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-lambda-function-pointer: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-lambda-function-pointer)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-lambda-function-pointer/x86.s tests/cxx_lambda_pointer_probe.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-lambda-function-pointer/x86.o $(TEST_OUT)/cxx-lambda-function-pointer/x86.s
	objdump -f $(TEST_OUT)/cxx-lambda-function-pointer/x86.o > $(TEST_OUT)/cxx-lambda-function-pointer/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-lambda-function-pointer/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-lambda-function-pointer/x64.s tests/cxx_lambda_pointer_probe.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-lambda-function-pointer/x64.o $(TEST_OUT)/cxx-lambda-function-pointer/x64.s
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-lambda-function-pointer/x64-host tests/cxx_lambda_pointer_run_test.c $(TEST_OUT)/cxx-lambda-function-pointer/x64.o
	$(TEST_OUT)/cxx-lambda-function-pointer/x64-host

test-cxx-generic-lambda: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-generic-lambda)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-generic-lambda,cxx_generic_lambda.cpp)
	$(call MKDIR_P,$(TEST_OUT)/cxx-generic-lambda-explicit)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-generic-lambda-explicit,cxx_lambda_explicit_template.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x86.ro tests/cxx_lambda_explicit_template_invalid.cpp,$(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x86.log)
	$(GREP) -F -q "cannot deduce generic lambda non-type parameter pack value" $(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x64.ro tests/cxx_lambda_explicit_template_invalid.cpp,$(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x64.log)
	$(GREP) -F -q "cannot deduce generic lambda non-type parameter pack value" $(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x64.log
else
test-cxx-lambda-function-pointer: test-cxx-lambda-function-pointer-posix
test-cxx-generic-lambda: test-cxx-generic-lambda-posix
endif

test-cxx-lambda-function-pointer-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-lambda-function-pointer)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-lambda-function-pointer/x86.s \
		tests/cxx_lambda_pointer_probe.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-lambda-function-pointer/x86.o \
		$(TEST_OUT)/cxx-lambda-function-pointer/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-lambda-function-pointer/x86 \
		tests/cxx_lambda_pointer_run_test.c \
		$(TEST_OUT)/cxx-lambda-function-pointer/x86.o
	$(TEST_OUT)/cxx-lambda-function-pointer/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-lambda-function-pointer/x64.s \
		tests/cxx_lambda_pointer_probe.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-lambda-function-pointer/x64.o \
		$(TEST_OUT)/cxx-lambda-function-pointer/x64.s
	$(CC) -o $(TEST_OUT)/cxx-lambda-function-pointer/x64 \
		tests/cxx_lambda_pointer_run_test.c \
		$(TEST_OUT)/cxx-lambda-function-pointer/x64.o
	$(TEST_OUT)/cxx-lambda-function-pointer/x64
	@echo "C++ captureless lambda function-pointer tests completed"

test-cxx-generic-lambda-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-generic-lambda)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-generic-lambda/x86.s \
		tests/cxx_generic_lambda.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-generic-lambda/x86.o \
		$(TEST_OUT)/cxx-generic-lambda/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-generic-lambda/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-generic-lambda/x86 \
		$(TEST_OUT)/cxx-generic-lambda/start-x86.o \
		$(TEST_OUT)/cxx-generic-lambda/x86.o
	$(TEST_OUT)/cxx-generic-lambda/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-generic-lambda/x64.s \
		tests/cxx_generic_lambda.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-generic-lambda/x64.o \
		$(TEST_OUT)/cxx-generic-lambda/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-generic-lambda/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-generic-lambda/x64 \
		$(TEST_OUT)/cxx-generic-lambda/start-x64.o \
		$(TEST_OUT)/cxx-generic-lambda/x64.o
	$(TEST_OUT)/cxx-generic-lambda/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-generic-lambda/explicit-x86.s \
		tests/cxx_lambda_explicit_template.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-generic-lambda/explicit-x86.o \
		$(TEST_OUT)/cxx-generic-lambda/explicit-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-generic-lambda/explicit-start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-generic-lambda/explicit-x86 \
		$(TEST_OUT)/cxx-generic-lambda/explicit-start-x86.o \
		$(TEST_OUT)/cxx-generic-lambda/explicit-x86.o
	$(TEST_OUT)/cxx-generic-lambda/explicit-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-generic-lambda/explicit-x64.s \
		tests/cxx_lambda_explicit_template.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-generic-lambda/explicit-x64.o \
		$(TEST_OUT)/cxx-generic-lambda/explicit-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-generic-lambda/explicit-start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-generic-lambda/explicit-x64 \
		$(TEST_OUT)/cxx-generic-lambda/explicit-start-x64.o \
		$(TEST_OUT)/cxx-generic-lambda/explicit-x64.o
	$(TEST_OUT)/cxx-generic-lambda/explicit-x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x86.ro \
		tests/cxx_lambda_explicit_template_invalid.cpp \
		>$(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x86.log 2>&1
	$(GREP) -q "cannot deduce generic lambda non-type parameter pack value" \
		$(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x64.ro \
		tests/cxx_lambda_explicit_template_invalid.cpp \
		>$(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x64.log 2>&1
	$(GREP) -q "cannot deduce generic lambda non-type parameter pack value" \
		$(TEST_OUT)/cxx-generic-lambda/invalid-explicit-x64.log
	@echo "C++ generic lambda deduction tests completed"

test-cxx-generic-lambda-stored-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-generic-lambda-stored-invalid)
ifeq ($(OS),Windows_NT)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-generic-lambda-stored-invalid/x86.ro tests/cxx_generic_lambda_stored_invalid.cpp,$(TEST_OUT)/cxx-generic-lambda-stored-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-generic-lambda-stored-invalid/x64.ro tests/cxx_generic_lambda_stored_invalid.cpp,$(TEST_OUT)/cxx-generic-lambda-stored-invalid/x64.log)
else
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-generic-lambda-stored-invalid/x86.ro \
		tests/cxx_generic_lambda_stored_invalid.cpp \
		>$(TEST_OUT)/cxx-generic-lambda-stored-invalid/x86.log 2>&1; then \
		echo "stored generic lambda unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-generic-lambda-stored-invalid/x64.ro \
		tests/cxx_generic_lambda_stored_invalid.cpp \
		>$(TEST_OUT)/cxx-generic-lambda-stored-invalid/x64.log 2>&1; then \
		echo "stored generic lambda unexpectedly compiled on AMD64"; exit 1; \
	fi
endif
	$(GREP) -q "stored generic lambda must be directly invoked" \
		$(TEST_OUT)/cxx-generic-lambda-stored-invalid/x86.log
	$(GREP) -q "stored generic lambda must be directly invoked" \
		$(TEST_OUT)/cxx-generic-lambda-stored-invalid/x64.log
	@echo "C++ stored generic lambda diagnostics completed"

test-multiple-inputs: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/multiple-inputs/c)
	$(call MKDIR_P,$(TEST_OUT)/multiple-inputs/cxx)
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -Command "Copy-Item -LiteralPath 'tests/hello.c' -Destination '$(TEST_OUT)/multiple-inputs/c/first.c' -Force"
	powershell -NoProfile -Command "Copy-Item -LiteralPath 'tests/aggregate_copy.c' -Destination '$(TEST_OUT)/multiple-inputs/c/second.c' -Force"
	powershell -NoProfile -Command "Set-Location '$(abspath $(TEST_OUT)/multiple-inputs/c)'; & '$(abspath $(RCC_TARGET))' --target x86_64-unknown-rinos -c -MMD first.c second.c"
	if not exist $(TEST_OUT)\multiple-inputs\c\first.ro exit /b 1
	if not exist $(TEST_OUT)\multiple-inputs\c\second.ro exit /b 1
	if not exist $(TEST_OUT)\multiple-inputs\c\first.d exit /b 1
	if not exist $(TEST_OUT)\multiple-inputs\c\second.d exit /b 1
	powershell -NoProfile -Command "Set-Location '$(abspath $(TEST_OUT)/multiple-inputs/c)'; & '$(abspath $(RCC_TARGET))' --target x86_64-unknown-rinos -S first.c second.c"
	if not exist $(TEST_OUT)\multiple-inputs\c\first.s exit /b 1
	if not exist $(TEST_OUT)\multiple-inputs\c\second.s exit /b 1
	powershell -NoProfile -Command "Set-Location '$(abspath $(TEST_OUT)/multiple-inputs/c)'; & '$(abspath $(RCC_TARGET))' --target x86_64-unknown-rinos -E first.c second.c > combined.i"
	$(GREP) -F -q "int main" $(TEST_OUT)/multiple-inputs/c/combined.i
	$(GREP) -F -q "struct Pair" $(TEST_OUT)/multiple-inputs/c/combined.i
	powershell -NoProfile -Command "Copy-Item -LiteralPath 'tests/cxx_function_templates.cpp' -Destination '$(TEST_OUT)/multiple-inputs/cxx/first.cpp' -Force"
	powershell -NoProfile -Command "Copy-Item -LiteralPath 'tests/cxx_lambda.cpp' -Destination '$(TEST_OUT)/multiple-inputs/cxx/second.cpp' -Force"
	powershell -NoProfile -Command "Set-Location '$(abspath $(TEST_OUT)/multiple-inputs/cxx)'; & '$(abspath $(RCXX_TARGET))' --target x86_64-unknown-rinos -std=c++20 -c first.cpp second.cpp"
	if not exist $(TEST_OUT)\multiple-inputs\cxx\first.ro exit /b 1
	if not exist $(TEST_OUT)\multiple-inputs\cxx\second.ro exit /b 1
else
	cp tests/hello.c $(TEST_OUT)/multiple-inputs/c/first.c
	cp tests/aggregate_copy.c $(TEST_OUT)/multiple-inputs/c/second.c
	(cd $(TEST_OUT)/multiple-inputs/c && $(abspath $(RCC_TARGET)) \
		--target x86_64-unknown-rinos -c -MMD first.c second.c)
	test -f $(TEST_OUT)/multiple-inputs/c/first.ro
	test -f $(TEST_OUT)/multiple-inputs/c/second.ro
	test -f $(TEST_OUT)/multiple-inputs/c/first.d
	test -f $(TEST_OUT)/multiple-inputs/c/second.d
	(cd $(TEST_OUT)/multiple-inputs/c && $(abspath $(RCC_TARGET)) \
		--target x86_64-unknown-rinos -S first.c second.c)
	test -f $(TEST_OUT)/multiple-inputs/c/first.s
	test -f $(TEST_OUT)/multiple-inputs/c/second.s
	(cd $(TEST_OUT)/multiple-inputs/c && $(abspath $(RCC_TARGET)) \
		--target x86_64-unknown-rinos -E first.c second.c > combined.i)
	$(GREP) -q "int main" $(TEST_OUT)/multiple-inputs/c/combined.i
	$(GREP) -q "struct Pair" $(TEST_OUT)/multiple-inputs/c/combined.i
	cp tests/cxx_function_templates.cpp $(TEST_OUT)/multiple-inputs/cxx/first.cpp
	cp tests/cxx_lambda.cpp $(TEST_OUT)/multiple-inputs/cxx/second.cpp
	(cd $(TEST_OUT)/multiple-inputs/cxx && $(abspath $(RCXX_TARGET)) \
		--target x86_64-unknown-rinos -std=c++20 -c first.cpp second.cpp)
	test -f $(TEST_OUT)/multiple-inputs/cxx/first.ro
	test -f $(TEST_OUT)/multiple-inputs/cxx/second.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/multiple-inputs/one.ro $(TEST_OUT)/multiple-inputs/c/first.c $(TEST_OUT)/multiple-inputs/c/second.c,$(TEST_OUT)/multiple-inputs/invalid-o-c.log)
	$(GREP) -q "[-]o cannot name one output for multiple input files" \
		$(TEST_OUT)/multiple-inputs/invalid-o-c.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/multiple-inputs/one-cxx.ro $(TEST_OUT)/multiple-inputs/cxx/first.cpp $(TEST_OUT)/multiple-inputs/cxx/second.cpp,$(TEST_OUT)/multiple-inputs/invalid-o-cxx.log)
	$(GREP) -q "[-]o cannot name one output for multiple input files" \
		$(TEST_OUT)/multiple-inputs/invalid-o-cxx.log
	@echo "RCC/RCC++ multiple-input compilation tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-if-constexpr: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-if-constexpr)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-if-constexpr,cxx_if_constexpr.cpp,cxx_if_constexpr_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-if-constexpr/nonconstant.ro tests/cxx_if_constexpr_nonconstant.cpp,$(TEST_OUT)/cxx-if-constexpr/nonconstant.log)
	$(GREP) -F -q "if constexpr condition is not a constant expression" $(TEST_OUT)/cxx-if-constexpr/nonconstant.log
	@echo "C++ if constexpr selection and diagnostics tests completed"
else
test-cxx-if-constexpr: test-cxx-if-constexpr-posix
endif

test-cxx-if-constexpr-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-if-constexpr)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-if-constexpr/x86.s \
		tests/cxx_if_constexpr.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-if-constexpr/x86.o \
		$(TEST_OUT)/cxx-if-constexpr/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-if-constexpr/x86 \
		tests/cxx_if_constexpr_run_test.c \
		$(TEST_OUT)/cxx-if-constexpr/x86.o
	$(TEST_OUT)/cxx-if-constexpr/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-if-constexpr/x64.s \
		tests/cxx_if_constexpr.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-if-constexpr/x64.o \
		$(TEST_OUT)/cxx-if-constexpr/x64.s
	$(CC) -o $(TEST_OUT)/cxx-if-constexpr/x64 \
		tests/cxx_if_constexpr_run_test.c \
		$(TEST_OUT)/cxx-if-constexpr/x64.o
	$(TEST_OUT)/cxx-if-constexpr/x64
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-if-constexpr/nonconstant.ro \
		tests/cxx_if_constexpr_nonconstant.cpp \
		>$(TEST_OUT)/cxx-if-constexpr/nonconstant.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	$(GREP) -q "if constexpr condition is not a constant expression" \
		$(TEST_OUT)/cxx-if-constexpr/nonconstant.log
	@echo "C++ if constexpr selection and diagnostics tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-if-constexpr-template: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-if-constexpr-template)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-if-constexpr-template,cxx_if_constexpr_template.cpp)
	@echo "C++ dependent if constexpr template tests completed"
else
test-cxx-if-constexpr-template: test-cxx-if-constexpr-template-posix
endif

test-cxx-if-constexpr-template-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-if-constexpr-template)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-if-constexpr-template/x86.s \
		tests/cxx_if_constexpr_template.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-if-constexpr-template/x86.o \
		$(TEST_OUT)/cxx-if-constexpr-template/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-if-constexpr-template/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-if-constexpr-template/x86 \
		$(TEST_OUT)/cxx-if-constexpr-template/start-x86.o \
		$(TEST_OUT)/cxx-if-constexpr-template/x86.o
	$(TEST_OUT)/cxx-if-constexpr-template/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-if-constexpr-template/x64.s \
		tests/cxx_if_constexpr_template.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-if-constexpr-template/x64.o \
		$(TEST_OUT)/cxx-if-constexpr-template/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-if-constexpr-template/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-if-constexpr-template/x64 \
		$(TEST_OUT)/cxx-if-constexpr-template/start-x64.o \
		$(TEST_OUT)/cxx-if-constexpr-template/x64.o
	$(TEST_OUT)/cxx-if-constexpr-template/x64
	@echo "C++ dependent if constexpr template tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-adl-multiple-namespaces: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-adl-multiple-namespaces)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-adl-multiple-namespaces,cxx_adl_multiple_namespaces.cpp)
	@echo "C++ multiple-namespace ADL tests completed"
else
test-cxx-adl-multiple-namespaces: test-cxx-adl-multiple-namespaces-posix
endif

test-cxx-adl-multiple-namespaces-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-adl-multiple-namespaces)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-adl-multiple-namespaces/x86.s \
		tests/cxx_adl_multiple_namespaces.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-adl-multiple-namespaces/x86.o \
		$(TEST_OUT)/cxx-adl-multiple-namespaces/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-adl-multiple-namespaces/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-adl-multiple-namespaces/x86 \
		$(TEST_OUT)/cxx-adl-multiple-namespaces/start-x86.o \
		$(TEST_OUT)/cxx-adl-multiple-namespaces/x86.o
	$(TEST_OUT)/cxx-adl-multiple-namespaces/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-adl-multiple-namespaces/x64.s \
		tests/cxx_adl_multiple_namespaces.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-adl-multiple-namespaces/x64.o \
		$(TEST_OUT)/cxx-adl-multiple-namespaces/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-adl-multiple-namespaces/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-adl-multiple-namespaces/x64 \
		$(TEST_OUT)/cxx-adl-multiple-namespaces/start-x64.o \
		$(TEST_OUT)/cxx-adl-multiple-namespaces/x64.o
	$(TEST_OUT)/cxx-adl-multiple-namespaces/x64
	@echo "C++ multiple-namespace ADL tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-using-overload-namespaces: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-using-overload-namespaces)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-using-overload-namespaces,cxx_using_overload_namespaces.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x86.ro tests/cxx_using_overload_ambiguous.cpp,$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x86.log)
	$(GREP) -F -q "ambiguous overload for 'choose'" $(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.ro tests/cxx_using_overload_ambiguous.cpp,$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.log)
	$(GREP) -F -q "ambiguous overload for 'choose'" $(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.log
	@echo "C++ using-namespace overload tests completed"
else
test-cxx-using-overload-namespaces: test-cxx-using-overload-namespaces-posix
endif

test-cxx-using-overload-namespaces-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-using-overload-namespaces)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-using-overload-namespaces/x86.s \
		tests/cxx_using_overload_namespaces.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-using-overload-namespaces/x86.o \
		$(TEST_OUT)/cxx-using-overload-namespaces/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-using-overload-namespaces/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-using-overload-namespaces/x86 \
		$(TEST_OUT)/cxx-using-overload-namespaces/start-x86.o \
		$(TEST_OUT)/cxx-using-overload-namespaces/x86.o
	$(TEST_OUT)/cxx-using-overload-namespaces/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-using-overload-namespaces/x64.s \
		tests/cxx_using_overload_namespaces.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-using-overload-namespaces/x64.o \
		$(TEST_OUT)/cxx-using-overload-namespaces/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-using-overload-namespaces/start-x64.o \
		 tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-using-overload-namespaces/x64 \
		$(TEST_OUT)/cxx-using-overload-namespaces/start-x64.o \
		$(TEST_OUT)/cxx-using-overload-namespaces/x64.o
	$(TEST_OUT)/cxx-using-overload-namespaces/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x86.ro \
		tests/cxx_using_overload_ambiguous.cpp \
		>$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x86.log 2>&1
	$(GREP) -q "ambiguous overload for 'choose'" \
		$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.ro \
		tests/cxx_using_overload_ambiguous.cpp \
		>$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.log 2>&1
	$(GREP) -q "ambiguous overload for 'choose'" \
		$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.log
	@echo "C++ using-namespace overload tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-template-two-phase-namespace: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-two-phase-namespace)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-template-two-phase-namespace,cxx_template_two_phase_namespace.cpp)
	@echo "C++ template defining-namespace lookup tests completed"
else
test-cxx-template-two-phase-namespace: test-cxx-template-two-phase-namespace-posix
endif

test-cxx-template-two-phase-namespace-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-two-phase-namespace)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-two-phase-namespace/x86.s \
		tests/cxx_template_two_phase_namespace.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-two-phase-namespace/x86.o \
		$(TEST_OUT)/cxx-template-two-phase-namespace/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-two-phase-namespace/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-two-phase-namespace/x86 \
		$(TEST_OUT)/cxx-template-two-phase-namespace/start-x86.o \
		$(TEST_OUT)/cxx-template-two-phase-namespace/x86.o
	$(TEST_OUT)/cxx-template-two-phase-namespace/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-two-phase-namespace/x64.s \
		tests/cxx_template_two_phase_namespace.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-template-two-phase-namespace/x64.o \
		$(TEST_OUT)/cxx-template-two-phase-namespace/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-template-two-phase-namespace/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-two-phase-namespace/x64 \
		$(TEST_OUT)/cxx-template-two-phase-namespace/start-x64.o \
		$(TEST_OUT)/cxx-template-two-phase-namespace/x64.o
	$(TEST_OUT)/cxx-template-two-phase-namespace/x64
	@echo "C++ template defining-namespace lookup tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-template-two-phase-adl: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-two-phase-adl)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-template-two-phase-adl,cxx_template_two_phase_adl.cpp)
	@echo "C++ template instantiation-time ADL tests completed"
else
test-cxx-template-two-phase-adl: test-cxx-template-two-phase-adl-posix
endif

test-cxx-template-two-phase-adl-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-two-phase-adl)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-two-phase-adl/x86.s \
		tests/cxx_template_two_phase_adl.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-two-phase-adl/x86.o \
		$(TEST_OUT)/cxx-template-two-phase-adl/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-two-phase-adl/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-two-phase-adl/x86 \
		$(TEST_OUT)/cxx-template-two-phase-adl/start-x86.o \
		$(TEST_OUT)/cxx-template-two-phase-adl/x86.o
	$(TEST_OUT)/cxx-template-two-phase-adl/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-two-phase-adl/x64.s \
		tests/cxx_template_two_phase_adl.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-template-two-phase-adl/x64.o \
		$(TEST_OUT)/cxx-template-two-phase-adl/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-template-two-phase-adl/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-two-phase-adl/x64 \
		$(TEST_OUT)/cxx-template-two-phase-adl/start-x64.o \
		$(TEST_OUT)/cxx-template-two-phase-adl/x64.o
	$(TEST_OUT)/cxx-template-two-phase-adl/x64
	@echo "C++ template instantiation-time ADL tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-template-two-phase-ordinary: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-two-phase-ordinary)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-template-two-phase-ordinary,cxx_template_two_phase_ordinary.cpp)
	@echo "C++ template definition-time ordinary lookup tests completed"
else
test-cxx-template-two-phase-ordinary: test-cxx-template-two-phase-ordinary-posix
endif

test-cxx-template-two-phase-ordinary-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-two-phase-ordinary)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-two-phase-ordinary/x86.s \
		tests/cxx_template_two_phase_ordinary.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-two-phase-ordinary/x86.o \
		$(TEST_OUT)/cxx-template-two-phase-ordinary/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-two-phase-ordinary/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-two-phase-ordinary/x86 \
		$(TEST_OUT)/cxx-template-two-phase-ordinary/start-x86.o \
		$(TEST_OUT)/cxx-template-two-phase-ordinary/x86.o
	$(TEST_OUT)/cxx-template-two-phase-ordinary/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-two-phase-ordinary/x64.s \
		tests/cxx_template_two_phase_ordinary.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-template-two-phase-ordinary/x64.o \
		$(TEST_OUT)/cxx-template-two-phase-ordinary/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-template-two-phase-ordinary/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-two-phase-ordinary/x64 \
		$(TEST_OUT)/cxx-template-two-phase-ordinary/start-x64.o \
		$(TEST_OUT)/cxx-template-two-phase-ordinary/x64.o
	$(TEST_OUT)/cxx-template-two-phase-ordinary/x64
	@echo "C++ template definition-time ordinary lookup tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-template-parameter-pack: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-parameter-pack)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-template-parameter-pack/x86.ro tests/cxx_template_parameter_pack.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-template-parameter-pack/x64.ro tests/cxx_template_parameter_pack.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -c -o $(TEST_OUT)/cxx-template-parameter-pack/verified-x86.ro tests/cxx_template_parameter_pack.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -fverified-backend -c -o $(TEST_OUT)/cxx-template-parameter-pack/verified-x64.ro tests/cxx_template_parameter_pack.cpp
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-template-parameter-pack,cxx_template_parameter_pack.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-template-parameter-pack/invalid-x86.ro tests/cxx_template_parameter_pack_invalid.cpp,$(TEST_OUT)/cxx-template-parameter-pack/invalid-x86.log)
	$(GREP) -F -q "only a single class-template parameter pack is supported" $(TEST_OUT)/cxx-template-parameter-pack/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.ro tests/cxx_template_parameter_pack_invalid.cpp,$(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.log)
	$(GREP) -F -q "only a single class-template parameter pack is supported" $(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.log
	@echo "C++ type parameter pack arity tests completed"
else
test-cxx-template-parameter-pack: test-cxx-template-parameter-pack-posix
endif

test-cxx-template-parameter-pack-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-template-parameter-pack)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-parameter-pack/x86.ro \
		tests/cxx_template_parameter_pack.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-parameter-pack/x64.ro \
		tests/cxx_template_parameter_pack.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-template-parameter-pack/verified-x86.ro \
		tests/cxx_template_parameter_pack.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-template-parameter-pack/verified-x64.ro \
		tests/cxx_template_parameter_pack.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-parameter-pack/x86.s \
		tests/cxx_template_parameter_pack.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-parameter-pack/x86.o \
		$(TEST_OUT)/cxx-template-parameter-pack/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-template-parameter-pack/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-parameter-pack/x86 \
		$(TEST_OUT)/cxx-template-parameter-pack/start-x86.o \
		$(TEST_OUT)/cxx-template-parameter-pack/x86.o
	$(TEST_OUT)/cxx-template-parameter-pack/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-template-parameter-pack/x64.s \
		tests/cxx_template_parameter_pack.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-template-parameter-pack/x64.o \
		$(TEST_OUT)/cxx-template-parameter-pack/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-template-parameter-pack/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-template-parameter-pack/x64 \
		$(TEST_OUT)/cxx-template-parameter-pack/start-x64.o \
		$(TEST_OUT)/cxx-template-parameter-pack/x64.o
	$(TEST_OUT)/cxx-template-parameter-pack/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-parameter-pack/invalid-x86.ro \
		tests/cxx_template_parameter_pack_invalid.cpp \
		>$(TEST_OUT)/cxx-template-parameter-pack/invalid-x86.log 2>&1
	$(GREP) -q "only a single class-template parameter pack is supported" \
		$(TEST_OUT)/cxx-template-parameter-pack/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.ro \
		tests/cxx_template_parameter_pack_invalid.cpp \
		>$(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.log 2>&1
	$(GREP) -q "only a single class-template parameter pack is supported" \
		$(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.log
	@echo "C++ type parameter pack arity tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-class-type-pack: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-type-pack)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-class-type-pack,cxx_class_type_pack.cpp)
	@echo "C++ class type parameter-pack tests completed"
else
test-cxx-class-type-pack: test-cxx-class-type-pack-posix
endif

test-cxx-class-type-pack-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-type-pack)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-type-pack/x86.s \
		tests/cxx_class_type_pack.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-type-pack/x86.o \
		$(TEST_OUT)/cxx-class-type-pack/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-type-pack/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-type-pack/x86 \
		$(TEST_OUT)/cxx-class-type-pack/start-x86.o \
		$(TEST_OUT)/cxx-class-type-pack/x86.o
	$(TEST_OUT)/cxx-class-type-pack/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-type-pack/x64.s \
		tests/cxx_class_type_pack.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-class-type-pack/x64.o \
		$(TEST_OUT)/cxx-class-type-pack/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-class-type-pack/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-type-pack/x64 \
		$(TEST_OUT)/cxx-class-type-pack/start-x64.o \
		$(TEST_OUT)/cxx-class-type-pack/x64.o
	$(TEST_OUT)/cxx-class-type-pack/x64
	@echo "C++ class type parameter-pack tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-qualified-class-initialization: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-qualified-class-initialization)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-qualified-class-initialization,cxx_qualified_class_initialization.cpp)
	@echo "C++ qualified class initialization tests completed"
else
test-cxx-qualified-class-initialization: test-cxx-qualified-class-initialization-posix
endif

test-cxx-qualified-class-initialization-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-qualified-class-initialization)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-qualified-class-initialization/x86.s \
		tests/cxx_qualified_class_initialization.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-qualified-class-initialization/x86.o \
		$(TEST_OUT)/cxx-qualified-class-initialization/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-qualified-class-initialization/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-qualified-class-initialization/x86 \
		$(TEST_OUT)/cxx-qualified-class-initialization/start-x86.o \
		$(TEST_OUT)/cxx-qualified-class-initialization/x86.o
	$(TEST_OUT)/cxx-qualified-class-initialization/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-qualified-class-initialization/x64.s \
		tests/cxx_qualified_class_initialization.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-qualified-class-initialization/x64.o \
		$(TEST_OUT)/cxx-qualified-class-initialization/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-qualified-class-initialization/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-qualified-class-initialization/x64 \
		$(TEST_OUT)/cxx-qualified-class-initialization/start-x64.o \
		$(TEST_OUT)/cxx-qualified-class-initialization/x64.o
	$(TEST_OUT)/cxx-qualified-class-initialization/x64
	@echo "C++ qualified class initialization tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-constexpr-pointer: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr-pointer)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-constexpr-pointer,cxx_constexpr_pointer.cpp,cxx_constexpr_pointer_run_test.c)
	@echo "C++ constexpr pointer tests completed"
else
test-cxx-constexpr-pointer: test-cxx-constexpr-pointer-posix
endif

test-cxx-constexpr-pointer-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr-pointer)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr-pointer/x86.s \
		tests/cxx_constexpr_pointer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constexpr-pointer/x86.o \
		$(TEST_OUT)/cxx-constexpr-pointer/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-constexpr-pointer/x86 \
		tests/cxx_constexpr_pointer_run_test.c \
		$(TEST_OUT)/cxx-constexpr-pointer/x86.o
	$(TEST_OUT)/cxx-constexpr-pointer/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr-pointer/x64.s \
		tests/cxx_constexpr_pointer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-constexpr-pointer/x64.o \
		$(TEST_OUT)/cxx-constexpr-pointer/x64.s
	$(CC) -o $(TEST_OUT)/cxx-constexpr-pointer/x64 \
		tests/cxx_constexpr_pointer_run_test.c \
		$(TEST_OUT)/cxx-constexpr-pointer/x64.o
	$(TEST_OUT)/cxx-constexpr-pointer/x64
	@echo "C++ constexpr pointer tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-constexpr-pointer-mutation: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr-pointer-mutation)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-constexpr-pointer-mutation,cxx_constexpr_pointer_mutation.cpp,cxx_constexpr_pointer_mutation_run_test.c)
	@echo "C++ constexpr local aggregate pointer mutation tests completed"
else
test-cxx-constexpr-pointer-mutation: test-cxx-constexpr-pointer-mutation-posix
endif

test-cxx-constexpr-pointer-mutation-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr-pointer-mutation)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr-pointer-mutation/x86.s \
		tests/cxx_constexpr_pointer_mutation.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constexpr-pointer-mutation/x86.o \
		$(TEST_OUT)/cxx-constexpr-pointer-mutation/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-constexpr-pointer-mutation/x86 \
		tests/cxx_constexpr_pointer_mutation_run_test.c \
		$(TEST_OUT)/cxx-constexpr-pointer-mutation/x86.o
	$(TEST_OUT)/cxx-constexpr-pointer-mutation/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr-pointer-mutation/x64.s \
		tests/cxx_constexpr_pointer_mutation.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-constexpr-pointer-mutation/x64.o \
		$(TEST_OUT)/cxx-constexpr-pointer-mutation/x64.s
	$(CC) -o $(TEST_OUT)/cxx-constexpr-pointer-mutation/x64 \
		tests/cxx_constexpr_pointer_mutation_run_test.c \
		$(TEST_OUT)/cxx-constexpr-pointer-mutation/x64.o
	$(TEST_OUT)/cxx-constexpr-pointer-mutation/x64
	@echo "C++ constexpr local aggregate pointer mutation tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-constexpr-pointer-aggregate: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr-pointer-aggregate)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-constexpr-pointer-aggregate,cxx_constexpr_pointer_aggregate.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/verified-x86.ro tests/cxx_constexpr_pointer_aggregate.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/verified-x64.ro tests/cxx_constexpr_pointer_aggregate.cpp
	@echo "C++ constexpr aggregate pointer provenance tests completed"
else
test-cxx-constexpr-pointer-aggregate: test-cxx-constexpr-pointer-aggregate-posix
endif

test-cxx-constexpr-pointer-aggregate-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constexpr-pointer-aggregate)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/x86.s \
		tests/cxx_constexpr_pointer_aggregate.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/x86.o \
		$(TEST_OUT)/cxx-constexpr-pointer-aggregate/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/x86 \
		$(TEST_OUT)/cxx-constexpr-pointer-aggregate/start-x86.o \
		$(TEST_OUT)/cxx-constexpr-pointer-aggregate/x86.o
	$(TEST_OUT)/cxx-constexpr-pointer-aggregate/x86
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/verified-x86.ro \
		tests/cxx_constexpr_pointer_aggregate.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/x64.s \
		tests/cxx_constexpr_pointer_aggregate.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/x64.o \
		$(TEST_OUT)/cxx-constexpr-pointer-aggregate/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/x64 \
		$(TEST_OUT)/cxx-constexpr-pointer-aggregate/start-x64.o \
		$(TEST_OUT)/cxx-constexpr-pointer-aggregate/x64.o
	$(TEST_OUT)/cxx-constexpr-pointer-aggregate/x64
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-constexpr-pointer-aggregate/verified-x64.ro \
		tests/cxx_constexpr_pointer_aggregate.cpp
	@echo "C++ constexpr aggregate pointer provenance tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-noexcept-expression: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-noexcept-expression)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-noexcept-expression,cxx_noexcept_expression.cpp,cxx_noexcept_expression_run_test.c)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/cxx-noexcept-expression/x86.ro tests/cxx_noexcept_expression.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/cxx-noexcept-expression/x64.ro tests/cxx_noexcept_expression.cpp
	@echo "C++ noexcept expression tests completed"
else
test-cxx-noexcept-expression: test-cxx-noexcept-expression-posix
endif

test-cxx-noexcept-expression-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-noexcept-expression)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-noexcept-expression/x86.s \
		tests/cxx_noexcept_expression.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-noexcept-expression/x86.o \
		$(TEST_OUT)/cxx-noexcept-expression/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-noexcept-expression/x86 \
		tests/cxx_noexcept_expression_run_test.c \
		$(TEST_OUT)/cxx-noexcept-expression/x86.o
	$(TEST_OUT)/cxx-noexcept-expression/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-noexcept-expression/x64.s \
		tests/cxx_noexcept_expression.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-noexcept-expression/x64.o \
		$(TEST_OUT)/cxx-noexcept-expression/x64.s
	$(CC) -o $(TEST_OUT)/cxx-noexcept-expression/x64 \
		tests/cxx_noexcept_expression_run_test.c \
		$(TEST_OUT)/cxx-noexcept-expression/x64.o
	$(TEST_OUT)/cxx-noexcept-expression/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-noexcept-expression/x86.ro \
		tests/cxx_noexcept_expression.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-noexcept-expression/x64.ro \
		tests/cxx_noexcept_expression.cpp
	@echo "C++ noexcept expression tests completed"

test-cxx-noexcept-redeclarations: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-noexcept-redeclarations)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-noexcept-redeclarations/match-x86.ro \
		tests/cxx_noexcept_redeclaration_match.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-noexcept-redeclarations/match-x64.ro \
		tests/cxx_noexcept_redeclaration_match.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-throwing-x86.ro tests/cxx_noexcept_redeclaration_mismatch_to_throwing.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-throwing-x86.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-throwing-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-throwing-x64.ro tests/cxx_noexcept_redeclaration_mismatch_to_throwing.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-throwing-x64.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-throwing-x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-nonthrowing-x86.ro tests/cxx_noexcept_redeclaration_mismatch_to_nonthrowing.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-nonthrowing-x86.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-nonthrowing-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-nonthrowing-x64.ro tests/cxx_noexcept_redeclaration_mismatch_to_nonthrowing.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-nonthrowing-x64.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-to-nonthrowing-x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-expression-x86.ro tests/cxx_noexcept_redeclaration_mismatch_expression.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-expression-x86.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-expression-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-expression-x64.ro tests/cxx_noexcept_redeclaration_mismatch_expression.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-expression-x64.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/mismatch-expression-x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-throwing-x86.ro tests/cxx_noexcept_member_redeclaration_mismatch_to_throwing.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-throwing-x86.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-throwing-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-throwing-x64.ro tests/cxx_noexcept_member_redeclaration_mismatch_to_throwing.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-throwing-x64.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-throwing-x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-nonthrowing-x86.ro tests/cxx_noexcept_member_redeclaration_mismatch_to_nonthrowing.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-nonthrowing-x86.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-nonthrowing-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-nonthrowing-x64.ro tests/cxx_noexcept_member_redeclaration_mismatch_to_nonthrowing.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-nonthrowing-x64.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-to-nonthrowing-x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-expression-x86.ro tests/cxx_noexcept_member_redeclaration_mismatch_expression.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-expression-x86.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-expression-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-expression-x64.ro tests/cxx_noexcept_member_redeclaration_mismatch_expression.cpp,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-expression-x64.log)
	$(call CHECK_TEXT,different exception specification,$(TEST_OUT)/cxx-noexcept-redeclarations/member-mismatch-expression-x64.log)
	@echo "C++ function redeclaration exception-specification tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-typeid: $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid)
	$(call MKDIR_P,$(TEST_OUT)/cxx-char-type-identity)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-deep)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-named-types)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-composite)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-local-scope)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeinfo-api)
	$(call CXX_WINDOWS_ENTRY_TWO_TU_TEST,cxx-typeid,cxx_typeid.cpp,cxx_typeid_peer.cpp)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-char-type-identity,cxx_char_type_identity.cpp)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-typeid-deep,cxx_typeid_deep.cpp)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-typeid-named-types,cxx_typeid_named_types.cpp)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-typeid-composite,cxx_typeid_composite.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-typeid-local-scope/x86.s tests/cxx_typeid_local_scope.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-typeid-local-scope/x86-peer.s tests/cxx_typeid_local_scope_peer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid-local-scope/x86.o $(TEST_OUT)/cxx-typeid-local-scope/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid-local-scope/x86-peer.o $(TEST_OUT)/cxx-typeid-local-scope/x86-peer.s
	objdump -f $(TEST_OUT)/cxx-typeid-local-scope/x86.o > $(TEST_OUT)/cxx-typeid-local-scope/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-typeid-local-scope/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-typeid-local-scope/x64.s tests/cxx_typeid_local_scope.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.s tests/cxx_typeid_local_scope_peer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-typeid-local-scope/x64.o $(TEST_OUT)/cxx-typeid-local-scope/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.o $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/cxx-typeid-local-scope/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-typeid-local-scope/x64-host tests/cxx_language_core_host.c $(TEST_OUT)/cxx-typeid-local-scope/x64.o $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.o
	$(TEST_OUT)/cxx-typeid-local-scope/x64-host
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-typeinfo-api,cxx_typeinfo_api.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid/x86.ro tests/cxx_typeid.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid/x86-peer.ro tests/cxx_typeid_peer.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid/x64.ro tests/cxx_typeid.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid/x64-peer.ro tests/cxx_typeid_peer.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid-named-types/x86.ro tests/cxx_typeid_named_types.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid-named-types/x64.ro tests/cxx_typeid_named_types.cpp
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 -e main -o $(TEST_OUT)/cxx-typeid/x86.rin $(TEST_OUT)/cxx-typeid/x86.ro $(TEST_OUT)/cxx-typeid/x86-peer.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 -e main -o $(TEST_OUT)/cxx-typeid/x64.rin $(TEST_OUT)/cxx-typeid/x64.ro $(TEST_OUT)/cxx-typeid/x64-peer.ro
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 -e main -o $(TEST_OUT)/cxx-typeid-named-types/x86.rin $(TEST_OUT)/cxx-typeid-named-types/x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 -e main -o $(TEST_OUT)/cxx-typeid-named-types/x64.rin $(TEST_OUT)/cxx-typeid-named-types/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned $(TEST_OUT)/cxx-typeid/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned $(TEST_OUT)/cxx-typeid/x64.rin
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid-local-scope/x86.ro tests/cxx_typeid_local_scope.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid-local-scope/x86-peer.ro tests/cxx_typeid_local_scope_peer.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid-local-scope/x64.ro tests/cxx_typeid_local_scope.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.ro tests/cxx_typeid_local_scope_peer.cpp
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 -e main -o $(TEST_OUT)/cxx-typeid-local-scope/x86.rin $(TEST_OUT)/cxx-typeid-local-scope/x86.ro $(TEST_OUT)/cxx-typeid-local-scope/x86-peer.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 -e main -o $(TEST_OUT)/cxx-typeid-local-scope/x64.rin $(TEST_OUT)/cxx-typeid-local-scope/x64.ro $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned $(TEST_OUT)/cxx-typeid-local-scope/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned $(TEST_OUT)/cxx-typeid-local-scope/x64.rin
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned $(TEST_OUT)/cxx-typeid-named-types/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned $(TEST_OUT)/cxx-typeid-named-types/x64.rin
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid/invalid-x86.ro tests/cxx_typeid_polymorphic_invalid.cpp,$(TEST_OUT)/cxx-typeid/invalid-x86.log)
	$(GREP) -F -q "typeid of a polymorphic expression requires a glvalue" $(TEST_OUT)/cxx-typeid/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid/invalid-x64.ro tests/cxx_typeid_polymorphic_invalid.cpp,$(TEST_OUT)/cxx-typeid/invalid-x64.log)
	$(GREP) -F -q "typeid of a polymorphic expression requires a glvalue" $(TEST_OUT)/cxx-typeid/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid/hash-invalid-x86.ro tests/cxx_typeid_hash_invalid.cpp,$(TEST_OUT)/cxx-typeid/hash-invalid-x86.log)
	$(GREP) -F -q "type_info::hash_code() takes no arguments" $(TEST_OUT)/cxx-typeid/hash-invalid-x86.log
	$(GREP) -F -q "type_info::name() takes no arguments" $(TEST_OUT)/cxx-typeid/hash-invalid-x86.log
	$(GREP) -F -q "comparison requires arithmetic or pointer operands" $(TEST_OUT)/cxx-typeid/hash-invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid/hash-invalid-x64.ro tests/cxx_typeid_hash_invalid.cpp,$(TEST_OUT)/cxx-typeid/hash-invalid-x64.log)
	$(GREP) -F -q "type_info::hash_code() takes no arguments" $(TEST_OUT)/cxx-typeid/hash-invalid-x64.log
	$(GREP) -F -q "type_info::name() takes no arguments" $(TEST_OUT)/cxx-typeid/hash-invalid-x64.log
	$(GREP) -F -q "comparison requires arithmetic or pointer operands" $(TEST_OUT)/cxx-typeid/hash-invalid-x64.log
	@echo "C++ static typeid identity tests completed"
else
test-cxx-typeid: test-cxx-typeid-posix
endif

test-cxx-typeid-posix: $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid)
	$(call MKDIR_P,$(TEST_OUT)/cxx-char-type-identity)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-deep)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-named-types)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-composite)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-local-scope)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeinfo-api)
	$(call CXX_POSIX_ENTRY_TEST,cxx-char-type-identity,cxx_char_type_identity.cpp)
	$(call CXX_POSIX_ENTRY_TEST,cxx-typeid-deep,cxx_typeid_deep.cpp)
	$(call CXX_POSIX_ENTRY_TEST,cxx-typeid-named-types,cxx_typeid_named_types.cpp)
	$(call CXX_POSIX_ENTRY_TEST,cxx-typeid-composite,cxx_typeid_composite.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid-local-scope/x86.s \
		tests/cxx_typeid_local_scope.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid-local-scope/x86-peer.s \
		tests/cxx_typeid_local_scope_peer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid-local-scope/x86.o \
		$(TEST_OUT)/cxx-typeid-local-scope/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid-local-scope/x86-peer.o \
		$(TEST_OUT)/cxx-typeid-local-scope/x86-peer.s
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid-local-scope/x64.s \
		tests/cxx_typeid_local_scope.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.s \
		tests/cxx_typeid_local_scope_peer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-typeid-local-scope/x64.o \
		$(TEST_OUT)/cxx-typeid-local-scope/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.o \
		$(TEST_OUT)/cxx-typeid-local-scope/x64-peer.s
	$(CC) -no-pie -o $(TEST_OUT)/cxx-typeid-local-scope/x64 \
		$(TEST_OUT)/cxx-typeid-local-scope/x64.o \
		$(TEST_OUT)/cxx-typeid-local-scope/x64-peer.o
	$(TEST_OUT)/cxx-typeid-local-scope/x64
	$(call CXX_POSIX_ENTRY_TEST,cxx-typeinfo-api,cxx_typeinfo_api.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid/x86.s tests/cxx_typeid.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid/x86-peer.s tests/cxx_typeid_peer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid/x86.o \
		$(TEST_OUT)/cxx-typeid/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid/x86-peer.o \
		$(TEST_OUT)/cxx-typeid/x86-peer.s
	$(CC) -m32 -no-pie -o $(TEST_OUT)/cxx-typeid/x86 \
		$(TEST_OUT)/cxx-typeid/x86.o $(TEST_OUT)/cxx-typeid/x86-peer.o
	$(TEST_OUT)/cxx-typeid/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid/x64.s tests/cxx_typeid.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid/x64-peer.s tests/cxx_typeid_peer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-typeid/x64.o \
		$(TEST_OUT)/cxx-typeid/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-typeid/x64-peer.o \
		$(TEST_OUT)/cxx-typeid/x64-peer.s
	$(CC) -no-pie -o $(TEST_OUT)/cxx-typeid/x64 \
		$(TEST_OUT)/cxx-typeid/x64.o $(TEST_OUT)/cxx-typeid/x64-peer.o
	$(TEST_OUT)/cxx-typeid/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid/x86.ro tests/cxx_typeid.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid/x86-peer.ro tests/cxx_typeid_peer.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid/x64.ro tests/cxx_typeid.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid/x64-peer.ro tests/cxx_typeid_peer.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid-named-types/x86.ro \
		tests/cxx_typeid_named_types.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid-named-types/x64.ro \
		tests/cxx_typeid_named_types.cpp
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-e main -o $(TEST_OUT)/cxx-typeid/x86.rin \
		$(TEST_OUT)/cxx-typeid/x86.ro $(TEST_OUT)/cxx-typeid/x86-peer.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-e main -o $(TEST_OUT)/cxx-typeid/x64.rin \
		$(TEST_OUT)/cxx-typeid/x64.ro $(TEST_OUT)/cxx-typeid/x64-peer.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-typeid/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-typeid/x64.rin
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid-local-scope/x86.ro \
		tests/cxx_typeid_local_scope.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid-local-scope/x86-peer.ro \
		tests/cxx_typeid_local_scope_peer.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid-local-scope/x64.ro \
		tests/cxx_typeid_local_scope.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid-local-scope/x64-peer.ro \
		tests/cxx_typeid_local_scope_peer.cpp
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-e main -o $(TEST_OUT)/cxx-typeid-local-scope/x86.rin \
		$(TEST_OUT)/cxx-typeid-local-scope/x86.ro \
		$(TEST_OUT)/cxx-typeid-local-scope/x86-peer.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-e main -o $(TEST_OUT)/cxx-typeid-local-scope/x64.rin \
		$(TEST_OUT)/cxx-typeid-local-scope/x64.ro \
		$(TEST_OUT)/cxx-typeid-local-scope/x64-peer.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-typeid-local-scope/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-typeid-local-scope/x64.rin
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-e main -o $(TEST_OUT)/cxx-typeid-named-types/x86.rin \
		$(TEST_OUT)/cxx-typeid-named-types/x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-e main -o $(TEST_OUT)/cxx-typeid-named-types/x64.rin \
		$(TEST_OUT)/cxx-typeid-named-types/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-typeid-named-types/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-typeid-named-types/x64.rin
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid/invalid-x86.ro \
		tests/cxx_typeid_polymorphic_invalid.cpp \
		>$(TEST_OUT)/cxx-typeid/invalid-x86.log 2>&1
	$(GREP) -q "typeid of a polymorphic expression requires a glvalue" \
		$(TEST_OUT)/cxx-typeid/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid/invalid-x64.ro \
		tests/cxx_typeid_polymorphic_invalid.cpp \
		>$(TEST_OUT)/cxx-typeid/invalid-x64.log 2>&1
	$(GREP) -q "typeid of a polymorphic expression requires a glvalue" \
		$(TEST_OUT)/cxx-typeid/invalid-x64.log
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid/hash-invalid-x86.ro \
		tests/cxx_typeid_hash_invalid.cpp \
		>$(TEST_OUT)/cxx-typeid/hash-invalid-x86.log 2>&1
	$(GREP) -q "type_info::hash_code() takes no arguments" \
		$(TEST_OUT)/cxx-typeid/hash-invalid-x86.log
	$(GREP) -q "type_info::name() takes no arguments" \
		$(TEST_OUT)/cxx-typeid/hash-invalid-x86.log
	$(GREP) -q "comparison requires arithmetic or pointer operands" \
		$(TEST_OUT)/cxx-typeid/hash-invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid/hash-invalid-x64.ro \
		tests/cxx_typeid_hash_invalid.cpp \
		>$(TEST_OUT)/cxx-typeid/hash-invalid-x64.log 2>&1
	$(GREP) -q "type_info::hash_code() takes no arguments" \
		$(TEST_OUT)/cxx-typeid/hash-invalid-x64.log
	$(GREP) -q "type_info::name() takes no arguments" \
		$(TEST_OUT)/cxx-typeid/hash-invalid-x64.log
	$(GREP) -q "comparison requires arithmetic or pointer operands" \
		$(TEST_OUT)/cxx-typeid/hash-invalid-x64.log
	@echo "C++ static typeid identity tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-typeid-dynamic: $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-dynamic)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-typeid-dynamic/x86.s tests/cxx_typeid_dynamic.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid-dynamic/x86.o $(TEST_OUT)/cxx-typeid-dynamic/x86.s
	objdump -f $(TEST_OUT)/cxx-typeid-dynamic/x86.o > $(TEST_OUT)/cxx-typeid-dynamic/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-typeid-dynamic/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-typeid-dynamic/x64.s tests/cxx_typeid_dynamic.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-typeid-dynamic/x64.o $(TEST_OUT)/cxx-typeid-dynamic/x64.s
	objdump -f $(TEST_OUT)/cxx-typeid-dynamic/x64.o > $(TEST_OUT)/cxx-typeid-dynamic/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-typeid-dynamic/x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid-dynamic/x86.ro tests/cxx_typeid_dynamic.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-typeid-dynamic/x64.ro tests/cxx_typeid_dynamic.cpp
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 --dep rincrt.rll --import setjmp=rincrt.rll@function --import rin_cpp_exception_install=rincrt.rll@function --import rin_cpp_exception_leave=rincrt.rll@function --import rin_cpp_exception_throw=rincrt.rll@function --import rin_cpp_exception_rethrow_frame=rincrt.rll@function --import rin_cpp_exception_release_frame=rincrt.rll@function -e main -o $(TEST_OUT)/cxx-typeid-dynamic/x86.rin $(TEST_OUT)/cxx-typeid-dynamic/x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 --dep rincrt.rll --import setjmp=rincrt.rll@function --import rin_cpp_exception_install=rincrt.rll@function --import rin_cpp_exception_leave=rincrt.rll@function --import rin_cpp_exception_throw=rincrt.rll@function --import rin_cpp_exception_rethrow_frame=rincrt.rll@function --import rin_cpp_exception_release_frame=rincrt.rll@function -e main -o $(TEST_OUT)/cxx-typeid-dynamic/x64.rin $(TEST_OUT)/cxx-typeid-dynamic/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned $(TEST_OUT)/cxx-typeid-dynamic/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned $(TEST_OUT)/cxx-typeid-dynamic/x64.rin
	@echo "C++ dynamic polymorphic typeid object and image tests completed; host execution requires RinOS exception runtime"
else
test-cxx-typeid-dynamic: test-cxx-typeid-dynamic-posix
endif

test-cxx-typeid-dynamic-posix: $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/cxx-typeid-dynamic)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid-dynamic/x86.s \
		tests/cxx_typeid_dynamic.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid-dynamic/x86.o \
		$(TEST_OUT)/cxx-typeid-dynamic/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-typeid-dynamic/start-x86.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-typeid-dynamic/x86 \
		$(TEST_OUT)/cxx-typeid-dynamic/start-x86.o \
		$(TEST_OUT)/cxx-typeid-dynamic/x86.o
	$(TEST_OUT)/cxx-typeid-dynamic/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-typeid-dynamic/x64.s \
		tests/cxx_typeid_dynamic.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-typeid-dynamic/x64.o \
		$(TEST_OUT)/cxx-typeid-dynamic/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-typeid-dynamic/start-x64.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-typeid-dynamic/x64 \
		$(TEST_OUT)/cxx-typeid-dynamic/start-x64.o \
		$(TEST_OUT)/cxx-typeid-dynamic/x64.o
	$(TEST_OUT)/cxx-typeid-dynamic/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid-dynamic/x86.ro \
		tests/cxx_typeid_dynamic.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-typeid-dynamic/x64.ro \
		tests/cxx_typeid_dynamic.cpp
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		--dep rincrt.rll \
		--import setjmp=rincrt.rll@function \
		--import rin_cpp_exception_install=rincrt.rll@function \
		--import rin_cpp_exception_leave=rincrt.rll@function \
		--import rin_cpp_exception_throw=rincrt.rll@function \
		--import rin_cpp_exception_rethrow_frame=rincrt.rll@function \
		--import rin_cpp_exception_release_frame=rincrt.rll@function \
		-e main -o $(TEST_OUT)/cxx-typeid-dynamic/x86.rin \
		$(TEST_OUT)/cxx-typeid-dynamic/x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		--dep rincrt.rll \
		--import setjmp=rincrt.rll@function \
		--import rin_cpp_exception_install=rincrt.rll@function \
		--import rin_cpp_exception_leave=rincrt.rll@function \
		--import rin_cpp_exception_throw=rincrt.rll@function \
		--import rin_cpp_exception_rethrow_frame=rincrt.rll@function \
		--import rin_cpp_exception_release_frame=rincrt.rll@function \
		-e main -o $(TEST_OUT)/cxx-typeid-dynamic/x64.rin \
		$(TEST_OUT)/cxx-typeid-dynamic/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-typeid-dynamic/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-typeid-dynamic/x64.rin
	@echo "C++ dynamic polymorphic typeid tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-auto-return: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-auto-return)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-auto-return,cxx_template_identity.cpp,cxx_template_identity_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-auto-return/invalid.ro tests/cxx_auto_return_invalid.cpp,$(TEST_OUT)/cxx-auto-return/invalid.log)
	$(GREP) -F -q "inconsistent deduction for auto return type" $(TEST_OUT)/cxx-auto-return/invalid.log
	$(GREP) -F -q "auto return type requires a function definition" $(TEST_OUT)/cxx-auto-return/invalid.log
	@echo "C++ auto return deduction tests completed"
else
test-cxx-auto-return: test-cxx-auto-return-posix
endif

test-cxx-auto-return-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-auto-return)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-auto-return/x86.s \
		tests/cxx_template_identity.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-auto-return/x86.o \
		$(TEST_OUT)/cxx-auto-return/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-auto-return/x86 \
		tests/cxx_template_identity_run_test.c \
		$(TEST_OUT)/cxx-auto-return/x86.o
	$(TEST_OUT)/cxx-auto-return/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-auto-return/x64.s \
		tests/cxx_template_identity.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-auto-return/x64.o \
		$(TEST_OUT)/cxx-auto-return/x64.s
	$(CC) -o $(TEST_OUT)/cxx-auto-return/x64 \
		tests/cxx_template_identity_run_test.c \
		$(TEST_OUT)/cxx-auto-return/x64.o
	$(TEST_OUT)/cxx-auto-return/x64
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-auto-return/invalid.ro \
		tests/cxx_auto_return_invalid.cpp \
		>$(TEST_OUT)/cxx-auto-return/invalid.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	$(GREP) -q "inconsistent deduction for auto return type" \
		$(TEST_OUT)/cxx-auto-return/invalid.log
	$(GREP) -q "auto return type requires a function definition" \
		$(TEST_OUT)/cxx-auto-return/invalid.log
	@echo "C++ auto return deduction tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-decltype: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-decltype)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-decltype,cxx_decltype.cpp,cxx_decltype_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-decltype/invalid.ro tests/cxx_decltype_invalid.cpp,$(TEST_OUT)/cxx-decltype/invalid.log)
	$(GREP) -F -q "unsupported operator in decltype expression" $(TEST_OUT)/cxx-decltype/invalid.log
	@echo "C++ decltype tests completed"
else
test-cxx-decltype: test-cxx-decltype-posix
endif

test-cxx-decltype-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-decltype)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-decltype/x86.s tests/cxx_decltype.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-decltype/x86.o \
		$(TEST_OUT)/cxx-decltype/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-decltype/x86 \
		tests/cxx_decltype_run_test.c $(TEST_OUT)/cxx-decltype/x86.o
	$(TEST_OUT)/cxx-decltype/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-decltype/x64.s tests/cxx_decltype.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-decltype/x64.o \
		$(TEST_OUT)/cxx-decltype/x64.s
	$(CC) -o $(TEST_OUT)/cxx-decltype/x64 \
		tests/cxx_decltype_run_test.c $(TEST_OUT)/cxx-decltype/x64.o
	$(TEST_OUT)/cxx-decltype/x64
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-decltype/invalid.ro \
		tests/cxx_decltype_invalid.cpp \
		>$(TEST_OUT)/cxx-decltype/invalid.log 2>&1; then \
		echo "unsupported decltype assignment unexpectedly compiled"; exit 1; \
	fi

ifeq ($(OS),Windows_NT)
test-cxx-decltype-auto: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-decltype-auto)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-decltype-auto,cxx_decltype_auto.cpp,cxx_decltype_auto_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-decltype-auto/invalid.ro tests/cxx_decltype_auto_invalid.cpp,$(TEST_OUT)/cxx-decltype-auto/invalid.log)
	$(GREP) -F -q "auto return type requires a function definition" $(TEST_OUT)/cxx-decltype-auto/invalid.log
	@echo "C++ decltype(auto) tests completed"
else
test-cxx-decltype-auto: test-cxx-decltype-auto-posix
endif

test-cxx-decltype-auto-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-decltype-auto)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-decltype-auto/x86.s tests/cxx_decltype_auto.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-decltype-auto/x86.o \
		$(TEST_OUT)/cxx-decltype-auto/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-decltype-auto/x86 \
		tests/cxx_decltype_auto_run_test.c \
		$(TEST_OUT)/cxx-decltype-auto/x86.o
	$(TEST_OUT)/cxx-decltype-auto/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-decltype-auto/x64.s tests/cxx_decltype_auto.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-decltype-auto/x64.o \
		$(TEST_OUT)/cxx-decltype-auto/x64.s
	$(CC) -o $(TEST_OUT)/cxx-decltype-auto/x64 \
		tests/cxx_decltype_auto_run_test.c \
		$(TEST_OUT)/cxx-decltype-auto/x64.o
	$(TEST_OUT)/cxx-decltype-auto/x64
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-decltype-auto/invalid.ro \
		tests/cxx_decltype_auto_invalid.cpp \
		>$(TEST_OUT)/cxx-decltype-auto/invalid.log 2>&1; then \
		echo "decltype(auto) declaration unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "auto return type requires a function definition" \
		$(TEST_OUT)/cxx-decltype-auto/invalid.log
	@echo "C++ decltype(auto) tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-auto-local-refs: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-auto-local-refs)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-auto-local-refs,cxx_auto_local_refs.cpp,cxx_auto_local_refs_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-auto-local-refs/invalid.ro tests/cxx_auto_local_refs_invalid.cpp,$(TEST_OUT)/cxx-auto-local-refs/invalid.log)
	$(GREP) -F -q "auto& initializer must be an lvalue" $(TEST_OUT)/cxx-auto-local-refs/invalid.log
	$(GREP) -F -q "auto* initializer must be a pointer or array" $(TEST_OUT)/cxx-auto-local-refs/invalid.log
	@echo "C++ local auto reference tests completed"
else
test-cxx-auto-local-refs: test-cxx-auto-local-refs-posix
endif

test-cxx-auto-local-refs-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-auto-local-refs)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-auto-local-refs/x86.s \
		tests/cxx_auto_local_refs.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-auto-local-refs/x86.o \
		$(TEST_OUT)/cxx-auto-local-refs/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-auto-local-refs/x86 \
		tests/cxx_auto_local_refs_run_test.c \
		$(TEST_OUT)/cxx-auto-local-refs/x86.o
	$(TEST_OUT)/cxx-auto-local-refs/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-auto-local-refs/x64.s \
		tests/cxx_auto_local_refs.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-auto-local-refs/x64.o \
		$(TEST_OUT)/cxx-auto-local-refs/x64.s
	$(CC) -o $(TEST_OUT)/cxx-auto-local-refs/x64 \
		tests/cxx_auto_local_refs_run_test.c \
		$(TEST_OUT)/cxx-auto-local-refs/x64.o
	$(TEST_OUT)/cxx-auto-local-refs/x64
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-auto-local-refs/invalid.ro \
		tests/cxx_auto_local_refs_invalid.cpp \
		>$(TEST_OUT)/cxx-auto-local-refs/invalid.log 2>&1; then \
		echo "auto& temporary unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "auto& initializer must be an lvalue" \
		$(TEST_OUT)/cxx-auto-local-refs/invalid.log
	$(GREP) -q "auto\* initializer must be a pointer or array" \
		$(TEST_OUT)/cxx-auto-local-refs/invalid.log
	@echo "C++ local auto reference tests completed"
	$(GREP) -q "unsupported operator in decltype expression" \
		$(TEST_OUT)/cxx-decltype/invalid.log
	@echo "C++ decltype tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-auto-direct-list-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-auto-direct-list-invalid)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-auto-direct-list-invalid/x86.ro tests/cxx_auto_direct_list_invalid.cpp,$(TEST_OUT)/cxx-auto-direct-list-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-auto-direct-list-invalid/x64.ro tests/cxx_auto_direct_list_invalid.cpp,$(TEST_OUT)/cxx-auto-direct-list-invalid/x64.log)
	$(GREP) -F -q "direct-list auto initialization requires one initializer expression" $(TEST_OUT)/cxx-auto-direct-list-invalid/x86.log
	$(GREP) -F -q "direct-list auto initialization requires one initializer expression" $(TEST_OUT)/cxx-auto-direct-list-invalid/x64.log
	@echo "C++ direct-list auto diagnostics completed"
else
test-cxx-auto-direct-list-invalid: test-cxx-auto-direct-list-invalid-posix
endif

test-cxx-auto-direct-list-invalid-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-auto-direct-list-invalid)
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-auto-direct-list-invalid/x86.ro \
		tests/cxx_auto_direct_list_invalid.cpp \
		>$(TEST_OUT)/cxx-auto-direct-list-invalid/x86.log 2>&1; then \
		echo "multi-element direct-list auto initialization unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-auto-direct-list-invalid/x64.ro \
		tests/cxx_auto_direct_list_invalid.cpp \
		>$(TEST_OUT)/cxx-auto-direct-list-invalid/x64.log 2>&1; then \
		echo "multi-element direct-list auto initialization unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "direct-list auto initialization requires one initializer expression" \
		$(TEST_OUT)/cxx-auto-direct-list-invalid/x86.log
	$(GREP) -q "direct-list auto initialization requires one initializer expression" \
		$(TEST_OUT)/cxx-auto-direct-list-invalid/x64.log
	@echo "C++ direct-list auto diagnostics completed"

ifeq ($(OS),Windows_NT)
test-cxx-decltype-auto-local: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-decltype-auto-local)
	$(call CXX_WINDOWS_C_RUN_TEST,cxx-decltype-auto-local,cxx_decltype_auto_local.cpp,cxx_decltype_auto_local_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-decltype-auto-local/invalid-x86.ro tests/cxx_decltype_auto_local_invalid.cpp,$(TEST_OUT)/cxx-decltype-auto-local/invalid-x86.log)
	$(GREP) -F -q "decltype(auto) variable requires an expression initializer" $(TEST_OUT)/cxx-decltype-auto-local/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-decltype-auto-local/invalid-x64.ro tests/cxx_decltype_auto_local_invalid.cpp,$(TEST_OUT)/cxx-decltype-auto-local/invalid-x64.log)
	$(GREP) -F -q "decltype(auto) variable requires an expression initializer" $(TEST_OUT)/cxx-decltype-auto-local/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-decltype-auto-local/missing-x86.ro tests/cxx_decltype_auto_local_missing.cpp,$(TEST_OUT)/cxx-decltype-auto-local/missing-x86.log)
	$(GREP) -F -q "decltype(auto) variable requires an initializer" $(TEST_OUT)/cxx-decltype-auto-local/missing-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-decltype-auto-local/missing-x64.ro tests/cxx_decltype_auto_local_missing.cpp,$(TEST_OUT)/cxx-decltype-auto-local/missing-x64.log)
	$(GREP) -F -q "decltype(auto) variable requires an initializer" $(TEST_OUT)/cxx-decltype-auto-local/missing-x64.log
	@echo "C++ local decltype(auto) tests completed"
else
test-cxx-decltype-auto-local: test-cxx-decltype-auto-local-posix
endif

test-cxx-decltype-auto-local-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-decltype-auto-local)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-decltype-auto-local/x86.s \
		tests/cxx_decltype_auto_local.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-decltype-auto-local/x86.o \
		$(TEST_OUT)/cxx-decltype-auto-local/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-decltype-auto-local/x86 \
		tests/cxx_decltype_auto_local_run_test.c \
		$(TEST_OUT)/cxx-decltype-auto-local/x86.o
	$(TEST_OUT)/cxx-decltype-auto-local/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-decltype-auto-local/x64.s \
		tests/cxx_decltype_auto_local.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-decltype-auto-local/x64.o \
		$(TEST_OUT)/cxx-decltype-auto-local/x64.s
	$(CC) -o $(TEST_OUT)/cxx-decltype-auto-local/x64 \
		tests/cxx_decltype_auto_local_run_test.c \
		$(TEST_OUT)/cxx-decltype-auto-local/x64.o
	$(TEST_OUT)/cxx-decltype-auto-local/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-decltype-auto-local/invalid-x86.ro \
		tests/cxx_decltype_auto_local_invalid.cpp \
		>$(TEST_OUT)/cxx-decltype-auto-local/invalid-x86.log 2>&1; then \
		echo "invalid decltype(auto) local declarations unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-decltype-auto-local/invalid-x64.ro \
		tests/cxx_decltype_auto_local_invalid.cpp \
		>$(TEST_OUT)/cxx-decltype-auto-local/invalid-x64.log 2>&1; then \
		echo "invalid decltype(auto) local declarations unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "decltype(auto) variable requires an expression initializer" \
		$(TEST_OUT)/cxx-decltype-auto-local/invalid-x86.log
	$(GREP) -q "decltype(auto) variable requires an expression initializer" \
		$(TEST_OUT)/cxx-decltype-auto-local/invalid-x64.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-decltype-auto-local/missing-x86.ro \
		tests/cxx_decltype_auto_local_missing.cpp \
		>$(TEST_OUT)/cxx-decltype-auto-local/missing-x86.log 2>&1; then \
		echo "uninitialized decltype(auto) unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-decltype-auto-local/missing-x64.ro \
		tests/cxx_decltype_auto_local_missing.cpp \
		>$(TEST_OUT)/cxx-decltype-auto-local/missing-x64.log 2>&1; then \
		echo "uninitialized decltype(auto) unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "decltype(auto) variable requires an initializer" \
		$(TEST_OUT)/cxx-decltype-auto-local/missing-x86.log
	$(GREP) -q "decltype(auto) variable requires an initializer" \
		$(TEST_OUT)/cxx-decltype-auto-local/missing-x64.log
	@echo "C++ local decltype(auto) tests completed"

test-cxx-non-type-template-deduction-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-non-type-template-deduction)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-non-type-template-deduction/x86.s \
		tests/cxx_non_type_templates.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-non-type-template-deduction/x86.o \
		$(TEST_OUT)/cxx-non-type-template-deduction/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-non-type-template-deduction/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-non-type-template-deduction/x86 \
		$(TEST_OUT)/cxx-non-type-template-deduction/start-x86.o \
		$(TEST_OUT)/cxx-non-type-template-deduction/x86.o
	$(TEST_OUT)/cxx-non-type-template-deduction/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-non-type-template-deduction/x64.s \
		tests/cxx_non_type_templates.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-non-type-template-deduction/x64.o \
		$(TEST_OUT)/cxx-non-type-template-deduction/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-non-type-template-deduction/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-non-type-template-deduction/x64 \
		$(TEST_OUT)/cxx-non-type-template-deduction/start-x64.o \
		$(TEST_OUT)/cxx-non-type-template-deduction/x64.o
	$(TEST_OUT)/cxx-non-type-template-deduction/x64
	@echo "RCC++ non-type array-bound deduction tests completed"

test-cxx-abbreviated-function-template-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-abbreviated-function-template)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-abbreviated-function-template/x86.s \
		tests/cxx-abbreviated-function-template.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-abbreviated-function-template/x86.o \
		$(TEST_OUT)/cxx-abbreviated-function-template/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-abbreviated-function-template/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-abbreviated-function-template/x86 \
		$(TEST_OUT)/cxx-abbreviated-function-template/start-x86.o \
		$(TEST_OUT)/cxx-abbreviated-function-template/x86.o
	$(TEST_OUT)/cxx-abbreviated-function-template/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-abbreviated-function-template/x64.s \
		tests/cxx-abbreviated-function-template.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-abbreviated-function-template/x64.o \
		$(TEST_OUT)/cxx-abbreviated-function-template/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-abbreviated-function-template/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-abbreviated-function-template/x64 \
		$(TEST_OUT)/cxx-abbreviated-function-template/start-x64.o \
		$(TEST_OUT)/cxx-abbreviated-function-template/x64.o
	$(TEST_OUT)/cxx-abbreviated-function-template/x64
	@echo "C++20 abbreviated function-template tests completed"

test-cxx-abbreviated-function-template-invalid-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-abbreviated-function-template-invalid)
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-abbreviated-function-template-invalid/x86.ro \
		tests/cxx-abbreviated-function-template-invalid.cpp \
		>$(TEST_OUT)/cxx-abbreviated-function-template-invalid/x86.log 2>&1; then \
		echo "C++20 abbreviated function template unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-abbreviated-function-template-invalid/x64.ro \
		tests/cxx-abbreviated-function-template-invalid.cpp \
		>$(TEST_OUT)/cxx-abbreviated-function-template-invalid/x64.log 2>&1; then \
		echo "C++20 abbreviated function template unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "abbreviated function templates require C++20 or newer" \
		$(TEST_OUT)/cxx-abbreviated-function-template-invalid/x86.log
	$(GREP) -q "abbreviated function templates require C++20 or newer" \
		$(TEST_OUT)/cxx-abbreviated-function-template-invalid/x64.log
	@echo "C++20 abbreviated function-template diagnostics completed"

ifeq ($(OS),Windows_NT)
test-cxx-non-type-template-deduction: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-non-type-template-deduction)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-non-type-template-deduction,cxx_non_type_templates.cpp)

test-cxx-abbreviated-function-template: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-abbreviated-function-template)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-abbreviated-function-template,cxx-abbreviated-function-template.cpp)

test-cxx-abbreviated-function-template-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-abbreviated-function-template-invalid)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-abbreviated-function-template-invalid/x86.ro tests/cxx-abbreviated-function-template-invalid.cpp,$(TEST_OUT)/cxx-abbreviated-function-template-invalid/x86.log)
	$(GREP) -F -q "abbreviated function templates require C++20 or newer" $(TEST_OUT)/cxx-abbreviated-function-template-invalid/x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-abbreviated-function-template-invalid/x64.ro tests/cxx-abbreviated-function-template-invalid.cpp,$(TEST_OUT)/cxx-abbreviated-function-template-invalid/x64.log)
	$(GREP) -F -q "abbreviated function templates require C++20 or newer" $(TEST_OUT)/cxx-abbreviated-function-template-invalid/x64.log
	@echo "C++20 abbreviated function-template diagnostics completed"
else
test-cxx-non-type-template-deduction: test-cxx-non-type-template-deduction-posix
test-cxx-abbreviated-function-template: test-cxx-abbreviated-function-template-posix
test-cxx-abbreviated-function-template-invalid: test-cxx-abbreviated-function-template-invalid-posix
endif

test-cxx-trailing-requires-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-trailing-requires)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-trailing-requires/x86.s \
		tests/cxx-abbreviated-function-template.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-trailing-requires/x86.o \
		$(TEST_OUT)/cxx-trailing-requires/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-trailing-requires/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-trailing-requires/x86 \
		$(TEST_OUT)/cxx-trailing-requires/start-x86.o \
		$(TEST_OUT)/cxx-trailing-requires/x86.o
	$(TEST_OUT)/cxx-trailing-requires/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-trailing-requires/x64.s \
		tests/cxx-abbreviated-function-template.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-trailing-requires/x64.o \
		$(TEST_OUT)/cxx-trailing-requires/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-trailing-requires/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-trailing-requires/x64 \
		$(TEST_OUT)/cxx-trailing-requires/start-x64.o \
		$(TEST_OUT)/cxx-trailing-requires/x64.o
	$(TEST_OUT)/cxx-trailing-requires/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-trailing-requires/invalid-x86.ro \
		tests/cxx-trailing-requires-invalid.cpp \
		>$(TEST_OUT)/cxx-trailing-requires/invalid-x86.log 2>&1; then \
		echo "unsatisfied trailing requires-clause unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-trailing-requires/invalid-x64.ro \
		tests/cxx-trailing-requires-invalid.cpp \
		>$(TEST_OUT)/cxx-trailing-requires/invalid-x64.log 2>&1; then \
		echo "unsatisfied trailing requires-clause unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-trailing-requires/invalid-x86.log
	$(GREP) -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-trailing-requires/invalid-x64.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-trailing-requires/old-x86.ro \
		tests/cxx-trailing-requires-invalid.cpp \
		>$(TEST_OUT)/cxx-trailing-requires/old-x86.log 2>&1; then \
		echo "trailing requires-clause unexpectedly compiled as C++17 on i686"; exit 1; \
	fi
	$(GREP) -q "requires-expressions and requires-clauses require C++20 or newer" \
		$(TEST_OUT)/cxx-trailing-requires/old-x86.log
	@echo "C++20 trailing requires-clause tests completed"

test-cxx-constrained-abbreviated-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constrained-abbreviated)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constrained-abbreviated/x86.s \
		tests/cxx-constrained-abbreviated.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constrained-abbreviated/x86.o \
		$(TEST_OUT)/cxx-constrained-abbreviated/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constrained-abbreviated/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constrained-abbreviated/x86 \
		$(TEST_OUT)/cxx-constrained-abbreviated/start-x86.o \
		$(TEST_OUT)/cxx-constrained-abbreviated/x86.o
	$(TEST_OUT)/cxx-constrained-abbreviated/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constrained-abbreviated/x64.s \
		tests/cxx-constrained-abbreviated.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-constrained-abbreviated/x64.o \
		$(TEST_OUT)/cxx-constrained-abbreviated/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-constrained-abbreviated/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constrained-abbreviated/x64 \
		$(TEST_OUT)/cxx-constrained-abbreviated/start-x64.o \
		$(TEST_OUT)/cxx-constrained-abbreviated/x64.o
	$(TEST_OUT)/cxx-constrained-abbreviated/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constrained-abbreviated/invalid-x86.ro \
		tests/cxx-constrained-abbreviated-invalid.cpp \
		>$(TEST_OUT)/cxx-constrained-abbreviated/invalid-x86.log 2>&1; then \
		echo "invalid constrained abbreviated template unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constrained-abbreviated/invalid-x64.ro \
		tests/cxx-constrained-abbreviated-invalid.cpp \
		>$(TEST_OUT)/cxx-constrained-abbreviated/invalid-x64.log 2>&1; then \
		echo "invalid constrained abbreviated template unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-constrained-abbreviated/invalid-x86.log
	$(GREP) -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-constrained-abbreviated/invalid-x64.log
	$(GREP) -q "requires a known named concept" \
		$(TEST_OUT)/cxx-constrained-abbreviated/invalid-x86.log
	$(GREP) -q "requires a known named concept" \
		$(TEST_OUT)/cxx-constrained-abbreviated/invalid-x64.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-constrained-abbreviated/old-x86.ro \
		tests/cxx-constrained-abbreviated-invalid.cpp \
		>$(TEST_OUT)/cxx-constrained-abbreviated/old-x86.log 2>&1; then \
		echo "constrained abbreviated template unexpectedly compiled as C++17"; exit 1; \
	fi
	$(GREP) -q "abbreviated function templates require C++20 or newer" \
		$(TEST_OUT)/cxx-constrained-abbreviated/old-x86.log
	@echo "C++20 constrained abbreviated-template tests completed"

test-cxx-constrained-class-template-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constrained-class-template)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constrained-class-template/x86.s \
		tests/cxx-constrained-class-template.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constrained-class-template/x86.o \
		$(TEST_OUT)/cxx-constrained-class-template/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-constrained-class-template/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constrained-class-template/x86 \
		$(TEST_OUT)/cxx-constrained-class-template/start-x86.o \
		$(TEST_OUT)/cxx-constrained-class-template/x86.o
	$(TEST_OUT)/cxx-constrained-class-template/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constrained-class-template/x64.s \
		tests/cxx-constrained-class-template.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-constrained-class-template/x64.o \
		$(TEST_OUT)/cxx-constrained-class-template/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-constrained-class-template/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-constrained-class-template/x64 \
		$(TEST_OUT)/cxx-constrained-class-template/start-x64.o \
		$(TEST_OUT)/cxx-constrained-class-template/x64.o
	$(TEST_OUT)/cxx-constrained-class-template/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constrained-class-template/invalid-x86.ro \
		tests/cxx-constrained-class-template-invalid.cpp \
		>$(TEST_OUT)/cxx-constrained-class-template/invalid-x86.log 2>&1; then \
		echo "invalid constrained class template unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constrained-class-template/invalid-x64.ro \
		tests/cxx-constrained-class-template-invalid.cpp \
		>$(TEST_OUT)/cxx-constrained-class-template/invalid-x64.log 2>&1; then \
		echo "invalid constrained class template unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-constrained-class-template/invalid-x86.log
	$(GREP) -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-constrained-class-template/invalid-x64.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c \
		-o $(TEST_OUT)/cxx-constrained-class-template/old-x86.ro \
		tests/cxx-constrained-class-template-invalid.cpp \
		>$(TEST_OUT)/cxx-constrained-class-template/old-x86.log 2>&1; then \
		echo "constrained class template unexpectedly compiled as C++17"; exit 1; \
	fi
	$(GREP) -q "requires-expressions and requires-clauses require C++20 or newer" \
		$(TEST_OUT)/cxx-constrained-class-template/old-x86.log
	@echo "C++20 constrained class-template tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-trailing-requires: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-trailing-requires)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-trailing-requires,cxx-abbreviated-function-template.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-trailing-requires/invalid-x86.ro tests/cxx-trailing-requires-invalid.cpp,$(TEST_OUT)/cxx-trailing-requires/invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-trailing-requires/invalid-x64.ro tests/cxx-trailing-requires-invalid.cpp,$(TEST_OUT)/cxx-trailing-requires/invalid-x64.log)
	$(GREP) -F -q "template constraints are not satisfied" $(TEST_OUT)/cxx-trailing-requires/invalid-x86.log
	$(GREP) -F -q "template constraints are not satisfied" $(TEST_OUT)/cxx-trailing-requires/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-trailing-requires/old-x86.ro tests/cxx-trailing-requires-invalid.cpp,$(TEST_OUT)/cxx-trailing-requires/old-x86.log)
	$(GREP) -F -q "requires-expressions and requires-clauses require C++20 or newer" $(TEST_OUT)/cxx-trailing-requires/old-x86.log

test-cxx-constrained-abbreviated: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constrained-abbreviated)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-constrained-abbreviated,cxx-constrained-abbreviated.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constrained-abbreviated/invalid-x86.ro tests/cxx-constrained-abbreviated-invalid.cpp,$(TEST_OUT)/cxx-constrained-abbreviated/invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constrained-abbreviated/invalid-x64.ro tests/cxx-constrained-abbreviated-invalid.cpp,$(TEST_OUT)/cxx-constrained-abbreviated/invalid-x64.log)
	$(GREP) -F -q "template constraints are not satisfied" $(TEST_OUT)/cxx-constrained-abbreviated/invalid-x86.log
	$(GREP) -F -q "template constraints are not satisfied" $(TEST_OUT)/cxx-constrained-abbreviated/invalid-x64.log
	$(GREP) -F -q "requires a known named concept" $(TEST_OUT)/cxx-constrained-abbreviated/invalid-x86.log
	$(GREP) -F -q "requires a known named concept" $(TEST_OUT)/cxx-constrained-abbreviated/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-constrained-abbreviated/old-x86.ro tests/cxx-constrained-abbreviated-invalid.cpp,$(TEST_OUT)/cxx-constrained-abbreviated/old-x86.log)
	$(GREP) -F -q "abbreviated function templates require C++20 or newer" $(TEST_OUT)/cxx-constrained-abbreviated/old-x86.log

test-cxx-constrained-class-template: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constrained-class-template)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-constrained-class-template,cxx-constrained-class-template.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constrained-class-template/invalid-x86.ro tests/cxx-constrained-class-template-invalid.cpp,$(TEST_OUT)/cxx-constrained-class-template/invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-constrained-class-template/invalid-x64.ro tests/cxx-constrained-class-template-invalid.cpp,$(TEST_OUT)/cxx-constrained-class-template/invalid-x64.log)
	$(GREP) -F -q "template constraints are not satisfied" $(TEST_OUT)/cxx-constrained-class-template/invalid-x86.log
	$(GREP) -F -q "template constraints are not satisfied" $(TEST_OUT)/cxx-constrained-class-template/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++17 -c -o $(TEST_OUT)/cxx-constrained-class-template/old-x86.ro tests/cxx-constrained-class-template-invalid.cpp,$(TEST_OUT)/cxx-constrained-class-template/old-x86.log)
	$(GREP) -F -q "requires-expressions and requires-clauses require C++20 or newer" $(TEST_OUT)/cxx-constrained-class-template/old-x86.log
else
test-cxx-trailing-requires: test-cxx-trailing-requires-posix
test-cxx-constrained-abbreviated: test-cxx-constrained-abbreviated-posix
test-cxx-constrained-class-template: test-cxx-constrained-class-template-posix
endif

test-cxx-raw-strings-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-raw-strings)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-raw-strings/x86.s \
		tests/cxx_raw_strings.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-raw-strings/x86.o \
		$(TEST_OUT)/cxx-raw-strings/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-raw-strings/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-raw-strings/x86 \
		$(TEST_OUT)/cxx-raw-strings/start-x86.o \
		$(TEST_OUT)/cxx-raw-strings/x86.o
	$(TEST_OUT)/cxx-raw-strings/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-raw-strings/x64.s \
		tests/cxx_raw_strings.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-raw-strings/x64.o \
		$(TEST_OUT)/cxx-raw-strings/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-raw-strings/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-raw-strings/x64 \
		$(TEST_OUT)/cxx-raw-strings/start-x64.o \
		$(TEST_OUT)/cxx-raw-strings/x64.o
	$(TEST_OUT)/cxx-raw-strings/x64
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-raw-strings/invalid-x86.ro \
		tests/cxx_raw_strings_invalid.cpp \
		>$(TEST_OUT)/cxx-raw-strings/invalid-x86.log 2>&1; then \
		echo "invalid raw string source unexpectedly compiled on i686"; exit 1; \
	fi
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-raw-strings/invalid-x64.ro \
		tests/cxx_raw_strings_invalid.cpp \
		>$(TEST_OUT)/cxx-raw-strings/invalid-x64.log 2>&1; then \
		echo "invalid raw string source unexpectedly compiled on AMD64"; exit 1; \
	fi
	$(GREP) -q "wide, UTF-16, and UTF-32 literals are not supported" \
		$(TEST_OUT)/cxx-raw-strings/invalid-x86.log
	$(GREP) -q "unterminated raw string literal" \
		$(TEST_OUT)/cxx-raw-strings/invalid-x86.log
	@echo "C++11 raw string literal tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-raw-strings: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-raw-strings)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-raw-strings,cxx_raw_strings.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-raw-strings/invalid-x86.ro tests/cxx_raw_strings_invalid.cpp,$(TEST_OUT)/cxx-raw-strings/invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-raw-strings/invalid-x64.ro tests/cxx_raw_strings_invalid.cpp,$(TEST_OUT)/cxx-raw-strings/invalid-x64.log)
	$(GREP) -F -q "wide, UTF-16, and UTF-32 literals are not supported" $(TEST_OUT)/cxx-raw-strings/invalid-x86.log
	$(GREP) -F -q "unterminated raw string literal" $(TEST_OUT)/cxx-raw-strings/invalid-x86.log
else
test-cxx-raw-strings: test-cxx-raw-strings-posix
endif

test-cxx-alternative-tokens-posix: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-alternative-tokens)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-alternative-tokens/cxx-x86.s \
		tests/cxx_alternative_tokens.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-alternative-tokens/cxx-x86.o \
		$(TEST_OUT)/cxx-alternative-tokens/cxx-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-alternative-tokens/cxx-start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-alternative-tokens/cxx-x86 \
		$(TEST_OUT)/cxx-alternative-tokens/cxx-start-x86.o \
		$(TEST_OUT)/cxx-alternative-tokens/cxx-x86.o
	$(TEST_OUT)/cxx-alternative-tokens/cxx-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-alternative-tokens/cxx-x64.s \
		tests/cxx_alternative_tokens.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-alternative-tokens/cxx-x64.o \
		$(TEST_OUT)/cxx-alternative-tokens/cxx-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-alternative-tokens/cxx-start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-alternative-tokens/cxx-x64 \
		$(TEST_OUT)/cxx-alternative-tokens/cxx-start-x64.o \
		$(TEST_OUT)/cxx-alternative-tokens/cxx-x64.o
	$(TEST_OUT)/cxx-alternative-tokens/cxx-x64
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/cxx-alternative-tokens/c-x86.s \
		tests/c_alternative_token_identifiers.c
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-alternative-tokens/c-x86.o \
		$(TEST_OUT)/cxx-alternative-tokens/c-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-alternative-tokens/c-start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-alternative-tokens/c-x86 \
		$(TEST_OUT)/cxx-alternative-tokens/c-start-x86.o \
		$(TEST_OUT)/cxx-alternative-tokens/c-x86.o
	$(TEST_OUT)/cxx-alternative-tokens/c-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/cxx-alternative-tokens/c-x64.s \
		tests/c_alternative_token_identifiers.c
	$(CC) -c -o $(TEST_OUT)/cxx-alternative-tokens/c-x64.o \
		$(TEST_OUT)/cxx-alternative-tokens/c-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-alternative-tokens/c-start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-alternative-tokens/c-x64 \
		$(TEST_OUT)/cxx-alternative-tokens/c-start-x64.o \
		$(TEST_OUT)/cxx-alternative-tokens/c-x64.o
	$(TEST_OUT)/cxx-alternative-tokens/c-x64
	@echo "C++ alternative operator-token and C identifier tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-alternative-tokens: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-alternative-tokens)
	$(call MKDIR_P,$(TEST_OUT)/cxx-alternative-tokens-c)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-alternative-tokens,cxx_alternative_tokens.cpp)
	$(call C_WINDOWS_ENTRY_TEST,cxx-alternative-tokens-c,c_alternative_token_identifiers.c)
else
test-cxx-alternative-tokens: test-cxx-alternative-tokens-posix
endif

test-initializer-brace-elision: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/initializer-brace-elision)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/initializer-brace-elision/x86.ro \
		tests/initializer_brace_elision.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/initializer-brace-elision/x64.ro \
		tests/initializer_brace_elision.c
	@echo "RCC C17 brace-elided initializer tests completed"

test-initializer-mixed: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/initializer-mixed)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/initializer-mixed/x86.ro \
		tests/initializer_mixed.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/initializer-mixed/x64.ro \
		tests/initializer_mixed.c
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/initializer-mixed/verified-x86.ro \
		tests/initializer_mixed.c \
		>$(TEST_OUT)/initializer-mixed/verified-x86.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' \
		$(TEST_OUT)/initializer-mixed/verified-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/initializer-mixed/verified-x64.ro \
		tests/initializer_mixed.c \
		>$(TEST_OUT)/initializer-mixed/verified-x64.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' \
		$(TEST_OUT)/initializer-mixed/verified-x64.log
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/initializer_mixed_run_test \
		tests/initializer_mixed_run_test.c src/emit_ro.c \
		src/utils.c
	$(TEST_OUT)/initializer_mixed_run_test \
		$(TEST_OUT)/initializer-mixed/x86.ro \
		$(TEST_OUT)/initializer-mixed/x64.ro \
		$(TEST_OUT)/initializer-mixed/verified-x86.ro \
		$(TEST_OUT)/initializer-mixed/verified-x64.ro
	@echo "C17 legacy/verified mixed designator and string-row initializer tests completed"

test-flexible-arrays: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/flexible-arrays)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/x86.ro tests/flexible_array.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/x64.ro tests/flexible_array.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/flexible-arrays/x86.s tests/flexible_array.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/flexible-arrays/run-test-x86 \
		$(TEST_OUT)/flexible-arrays/x86.s
	$(TEST_OUT)/flexible-arrays/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/flexible-arrays/x64.s tests/flexible_array.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/flexible-arrays/run-test-x64 \
		$(TEST_OUT)/flexible-arrays/x64.s
	$(TEST_OUT)/flexible-arrays/run-test-x64
else
	$(CC) $(VERIFIED_BACKEND_X86_HOST_CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/flexible-arrays/run-test-x86 \
		tests/flexible_array_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/flexible-arrays/run-test-x64 \
		tests/flexible_array_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/flexible-arrays/run-test-x86 \
		$(TEST_OUT)/flexible-arrays/x86.ro
	$(TEST_OUT)/flexible-arrays/run-test-x64 \
		$(TEST_OUT)/flexible-arrays/x64.ro

endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/invalid-x86.ro \
		tests/invalid_flexible_array.c,$(TEST_OUT)/flexible-arrays/invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/invalid-x64.ro \
		tests/invalid_flexible_array.c,$(TEST_OUT)/flexible-arrays/invalid-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/invalid-initializer-x86.ro \
		tests/invalid_flexible_initializer.c,$(TEST_OUT)/flexible-arrays/invalid-initializer-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/invalid-initializer-x64.ro \
		tests/invalid_flexible_initializer.c,$(TEST_OUT)/flexible-arrays/invalid-initializer-x64.log)
	$(GREP) -q "flexible array member requires another named member" \
		$(TEST_OUT)/flexible-arrays/invalid-x86.log
	$(GREP) -q "flexible array member must be the last member" \
		$(TEST_OUT)/flexible-arrays/invalid-x86.log
	$(GREP) -q "flexible array member is not allowed in a union" \
		$(TEST_OUT)/flexible-arrays/invalid-x86.log
	$(GREP) -q "flexible array member cannot be initialized" \
		$(TEST_OUT)/flexible-arrays/invalid-initializer-x86.log
	@echo "Dual-architecture C17 flexible array member tests completed"

test-floating-static-initializers: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/floating-static-initializers)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/floating-static-initializers/x86.ro \
		tests/floating_static_initializers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/floating-static-initializers/x64.ro \
		tests/floating_static_initializers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/floating-static-initializers/x64.s \
		tests/floating_static_initializers.c
	$(call CHECK_TEXT,0x00$(comma) 0x00$(comma) 0xe0$(comma) 0x3f,$(TEST_OUT)/floating-static-initializers/x64.s)
	$(call CHECK_TEXT,0x00$(comma) 0x00$(comma) 0xf8$(comma) 0xbf,$(TEST_OUT)/floating-static-initializers/x64.s)
	@echo "RCC C17 floating static/TLS initializer tests completed"

test-numeric-literals: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/numeric-literals)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/numeric-literals/x86.s tests/numeric_literals.c
ifeq ($(OS),Windows_NT)
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/numeric-literals/x86 \
		$(TEST_OUT)/numeric-literals/x86.s
	$(TEST_OUT)/numeric-literals/x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/numeric-literals/x64.s tests/numeric_literals.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/numeric-literals/x64 \
		$(TEST_OUT)/numeric-literals/x64.s
	$(TEST_OUT)/numeric-literals/x64
else
	$(CC) -m32 -c -o $(TEST_OUT)/numeric-literals/x86.o \
		$(TEST_OUT)/numeric-literals/x86.s
	$(CC) -m32 -no-pie -o $(TEST_OUT)/numeric-literals/x86 \
		$(TEST_OUT)/numeric-literals/x86.o
	$(TEST_OUT)/numeric-literals/x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/numeric-literals/x64.s tests/numeric_literals.c
	$(CC) -c -o $(TEST_OUT)/numeric-literals/x64.o \
		$(TEST_OUT)/numeric-literals/x64.s
	$(CC) -no-pie -o $(TEST_OUT)/numeric-literals/x64 \
		$(TEST_OUT)/numeric-literals/x64.o
	$(TEST_OUT)/numeric-literals/x64

endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/numeric-literals/invalid-x86.ro \
		tests/invalid_universal_character_name.c,$(TEST_OUT)/numeric-literals/invalid-x86.log)
	$(GREP) -F -q "universal character names are not supported by the RinOS byte-string ABI" \
		$(TEST_OUT)/numeric-literals/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/numeric-literals/invalid-x64.ro \
		tests/invalid_universal_character_name.c,$(TEST_OUT)/numeric-literals/invalid-x64.log)
	$(GREP) -F -q "universal character names are not supported by the RinOS byte-string ABI" \
		$(TEST_OUT)/numeric-literals/invalid-x64.log
	@echo "C17 decimal and hexadecimal floating literal tests completed"

test-floating-runtime-x64: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/floating-runtime-x64)
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/floating-runtime-x64/runtime.s \
		tests/floating_runtime_x64.c
	$(CC) -c -o $(TEST_OUT)/floating-runtime-x64/runtime.o \
		$(TEST_OUT)/floating-runtime-x64/runtime.s
	$(CC) -c -o $(TEST_OUT)/floating-runtime-x64/runtime-host.o \
		tests/floating_runtime_host.c
	$(OBJCOPY) --redefine-sym main=_rcc_generated_main \
		$(TEST_OUT)/floating-runtime-x64/runtime.o
	$(CC) -no-pie -o $(TEST_OUT)/floating-runtime-x64/runtime.exe \
		$(TEST_OUT)/floating-runtime-x64/runtime-host.o \
		$(TEST_OUT)/floating-runtime-x64/runtime.o
	$(TEST_OUT)/floating-runtime-x64/runtime.exe
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/floating-runtime-x64/float-abi.s \
		tests/floating_abi_x64.c
	$(CC) -c -o $(TEST_OUT)/floating-runtime-x64/float-abi.o \
		$(TEST_OUT)/floating-runtime-x64/float-abi.s
	$(CC) -c -o $(TEST_OUT)/floating-runtime-x64/float-abi-host.o \
		tests/floating_abi_host.c
	$(OBJCOPY) --redefine-sym _rcc_float_entry=_rcc_entry \
		$(TEST_OUT)/floating-runtime-x64/float-abi.o
	$(CC) -no-pie -o $(TEST_OUT)/floating-runtime-x64/float-abi.exe \
		$(TEST_OUT)/floating-runtime-x64/float-abi-host.o \
		$(TEST_OUT)/floating-runtime-x64/float-abi.o
	$(TEST_OUT)/floating-runtime-x64/float-abi.exe
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/floating-runtime-x64/double-abi.s \
		tests/floating_abi_double_x64.c
	$(CC) -c -o $(TEST_OUT)/floating-runtime-x64/double-abi.o \
		$(TEST_OUT)/floating-runtime-x64/double-abi.s
	$(CC) -c -o $(TEST_OUT)/floating-runtime-x64/double-abi-host.o \
		tests/floating_abi_double_host.c
	$(OBJCOPY) --redefine-sym _rcc_double_entry=_rcc_entry \
		$(TEST_OUT)/floating-runtime-x64/double-abi.o
	$(CC) -no-pie -o $(TEST_OUT)/floating-runtime-x64/double-abi.exe \
		$(TEST_OUT)/floating-runtime-x64/double-abi-host.o \
		$(TEST_OUT)/floating-runtime-x64/double-abi.o
	$(TEST_OUT)/floating-runtime-x64/double-abi.exe
	@echo "RCC x86-64 floating runtime and scalar ABI tests completed"

ifeq ($(OS),Windows_NT)
test-floating-runtime-i686: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/floating-runtime-i686)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/floating-runtime-i686/runtime.s \
		tests/floating_runtime_i686.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/floating-runtime-i686/runtime.exe \
		$(TEST_OUT)/floating-runtime-i686/runtime.s
	$(TEST_OUT)/floating-runtime-i686/runtime.exe
	@echo "RCC i686 floating runtime and scalar ABI tests completed"
else
test-floating-runtime-i686: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/floating-runtime-i686)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/floating-runtime-i686/runtime.s \
		tests/floating_runtime_i686.c
	$(CC) -m32 -c -o $(TEST_OUT)/floating-runtime-i686/runtime.o \
		$(TEST_OUT)/floating-runtime-i686/runtime.s
	$(CC) -m32 -c -o $(TEST_OUT)/floating-runtime-i686/start.o \
		tests/floating_runtime_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/floating-runtime-i686/runtime.exe \
		$(TEST_OUT)/floating-runtime-i686/start.o \
		$(TEST_OUT)/floating-runtime-i686/runtime.o
	$(TEST_OUT)/floating-runtime-i686/runtime.exe
	@echo "RCC i686 floating runtime and scalar ABI tests completed"
endif

test-restrict-qualifier: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/restrict-qualifier)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/restrict-qualifier/valid-x86.ro \
		tests/restrict_qualifier.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/restrict-qualifier/valid-x64.ro \
		tests/restrict_qualifier.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c -o $(TEST_OUT)/restrict-qualifier/invalid-x86.ro tests/invalid_restrict_qualifier.c,$(TEST_OUT)/restrict-qualifier/invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c -o $(TEST_OUT)/restrict-qualifier/invalid-x64.ro tests/invalid_restrict_qualifier.c,$(TEST_OUT)/restrict-qualifier/invalid-x64.log)
	$(GREP) -q 'restrict qualifier is only valid on pointer types' \
		$(TEST_OUT)/restrict-qualifier/invalid-x86.log
	$(GREP) -q 'restrict-qualified pointer must point to an object or incomplete type' \
		$(TEST_OUT)/restrict-qualifier/invalid-x86.log
	$(GREP) -q 'restrict qualifier is only valid on pointer types' \
		$(TEST_OUT)/restrict-qualifier/invalid-x64.log
	$(GREP) -q 'restrict-qualified pointer must point to an object or incomplete type' \
		$(TEST_OUT)/restrict-qualifier/invalid-x64.log
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/restrict-qualifier/invalid-nested-x86.ro \
		tests/invalid_nested_pointer_qualifier.c \
		>$(TEST_OUT)/restrict-qualifier/invalid-nested-x86.log 2>&1
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/restrict-qualifier/invalid-nested-x64.ro \
		tests/invalid_nested_pointer_qualifier.c \
		>$(TEST_OUT)/restrict-qualifier/invalid-nested-x64.log 2>&1
	$(call CHECK_COUNT,incompatible return type,$(TEST_OUT)/restrict-qualifier/invalid-nested-x86.log,2)
	$(call CHECK_COUNT,incompatible return type,$(TEST_OUT)/restrict-qualifier/invalid-nested-x64.log,2)
	@echo "C17 restrict qualifier tests completed"

ifeq ($(OS),Windows_NT)
test-vla-runtime: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-runtime)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/vla-runtime/x86.s tests/vla_runtime.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/vla-runtime/x64.s tests/vla_runtime.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/vla-runtime/x86.exe \
		$(TEST_OUT)/vla-runtime/x86.s
	$(TEST_OUT)/vla-runtime/x86.exe
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/vla-runtime/x64.exe \
		$(TEST_OUT)/vla-runtime/x64.s
	$(TEST_OUT)/vla-runtime/x64.exe
	@echo "C17 VLA runtime tests completed"
else
test-vla-runtime: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-runtime)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/vla-runtime/x86.s tests/vla_runtime.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/vla-runtime/x64.s tests/vla_runtime.c
	$(CC) -m32 -c -o $(TEST_OUT)/vla-runtime/x86.o \
		$(TEST_OUT)/vla-runtime/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/vla-runtime/x86-start.o \
		tests/vla_runtime_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/vla-runtime/x86 \
		$(TEST_OUT)/vla-runtime/x86-start.o $(TEST_OUT)/vla-runtime/x86.o
	$(TEST_OUT)/vla-runtime/x86
	$(CC) -c -o $(TEST_OUT)/vla-runtime/x64.o \
		$(TEST_OUT)/vla-runtime/x64.s
	$(CC) -c -o $(TEST_OUT)/vla-runtime/x64-start.o \
		tests/vla_runtime_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/vla-runtime/x64 \
		$(TEST_OUT)/vla-runtime/x64-start.o $(TEST_OUT)/vla-runtime/x64.o
	$(TEST_OUT)/vla-runtime/x64
	@echo "C17 VLA runtime tests completed"
endif

ifeq ($(OS),Windows_NT)
test-vla-semantics: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-semantics)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-x86.ro tests/invalid_vla_goto.c,$(TEST_OUT)/vla-semantics/invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-x64.ro tests/invalid_vla_goto.c,$(TEST_OUT)/vla-semantics/invalid-x64.log)
	$(GREP) -F -q "goto enters a variable-length array scope" $(TEST_OUT)/vla-semantics/invalid-x86.log
	$(GREP) -F -q "goto enters a variable-length array scope" $(TEST_OUT)/vla-semantics/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-array-x86.ro tests/invalid_array_parameter_qualifiers.c,$(TEST_OUT)/vla-semantics/invalid-array-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-array-x64.ro tests/invalid_array_parameter_qualifiers.c,$(TEST_OUT)/vla-semantics/invalid-array-x64.log)
	$(GREP) -F -q "array parameter qualifiers are only valid" $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	$(GREP) -F -q "array parameter qualifiers are only valid" $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	$(GREP) -F -q "static array parameter requires a bound expression" $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	$(GREP) -F -q "static array parameter requires a bound expression" $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	$(GREP) -F -q "unspecified variable-length array is only valid" $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	$(GREP) -F -q "unspecified variable-length array is only valid" $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	@echo "Dual-architecture VLA goto semantic tests completed"
else
test-vla-semantics: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-semantics)
	if $(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-x86.ro tests/invalid_vla_goto.c >$(TEST_OUT)/vla-semantics/invalid-x86.log 2>&1; then exit 1; fi
	if $(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-x64.ro tests/invalid_vla_goto.c >$(TEST_OUT)/vla-semantics/invalid-x64.log 2>&1; then exit 1; fi
	$(GREP) -q 'goto enters a variable-length array scope' $(TEST_OUT)/vla-semantics/invalid-x86.log
	$(GREP) -q 'goto enters a variable-length array scope' $(TEST_OUT)/vla-semantics/invalid-x64.log
	if $(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-array-x86.ro tests/invalid_array_parameter_qualifiers.c >$(TEST_OUT)/vla-semantics/invalid-array-x86.log 2>&1; then exit 1; fi
	if $(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-array-x64.ro tests/invalid_array_parameter_qualifiers.c >$(TEST_OUT)/vla-semantics/invalid-array-x64.log 2>&1; then exit 1; fi
	$(GREP) -q 'array parameter qualifiers are only valid' $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	$(GREP) -q 'array parameter qualifiers are only valid' $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	$(GREP) -q 'static array parameter requires a bound expression' $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	$(GREP) -q 'static array parameter requires a bound expression' $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	$(GREP) -q 'unspecified variable-length array is only valid' $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	$(GREP) -q 'unspecified variable-length array is only valid' $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	@echo "Dual-architecture VLA goto semantic tests completed"
endif

test-cxx-qualified-namespaces: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-qualified-namespaces)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-qualified-namespaces/x86.ro \
		tests/cxx_qualified_namespace.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-qualified-namespaces/x64.ro \
		tests/cxx_qualified_namespace.cpp
	$(GREP) -q '_ZN3api9transformEi' \
		$(TEST_OUT)/cxx-qualified-namespaces/x86.ro
	$(GREP) -q '_ZN3api6nested5applyEi' \
		$(TEST_OUT)/cxx-qualified-namespaces/x64.ro
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/cxx-qualified-namespaces/c-mode.ro tests/c_scope_operator_rejected.c,$(TEST_OUT)/cxx-qualified-namespaces/c-mode.log)
	@echo "RCC++ qualified namespace and C mode-isolation tests completed"

test-cxx-using: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-using)
ifeq ($(OS),Windows_NT)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-using/x86.ro tests/cxx_using.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-using/x64.ro tests/cxx_using.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-using/x86.s tests/cxx_using.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-using/x86.o \
		$(TEST_OUT)/cxx-using/x86.s
	objdump -f $(TEST_OUT)/cxx-using/x86.o > $(TEST_OUT)/cxx-using/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-using/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-using/x64.s tests/cxx_using.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-using/x64.o \
		$(TEST_OUT)/cxx-using/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_test_main $(TEST_OUT)/cxx-using/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-using/x64-host \
		tests/cxx_main_host.c $(TEST_OUT)/cxx-using/x64.o
	$(TEST_OUT)/cxx-using/x64-host
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-using/x86.s tests/cxx_using.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-using/x64.s tests/cxx_using.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-using/x86.o \
		$(TEST_OUT)/cxx-using/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-using/start.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-using/x86 \
		$(TEST_OUT)/cxx-using/start.o $(TEST_OUT)/cxx-using/x86.o
	$(TEST_OUT)/cxx-using/x86
	$(CC) -c -o $(TEST_OUT)/cxx-using/x64.o \
		$(TEST_OUT)/cxx-using/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-using/start64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-using/x64 \
		$(TEST_OUT)/cxx-using/start64.o $(TEST_OUT)/cxx-using/x64.o
	$(TEST_OUT)/cxx-using/x64
endif
	@echo "RCC++ using-directive, using-declaration, and alias tests completed"

test-cxx-numeric-separators: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-numeric-separators)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-numeric-separators/x86.ro \
		tests/cxx_numeric_separators.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-numeric-separators/x64.ro \
		tests/cxx_numeric_separators.cpp
ifeq ($(OS),Windows_NT)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-numeric-separators/x86.s \
		tests/cxx_numeric_separators.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-numeric-separators/x86.o \
		$(TEST_OUT)/cxx-numeric-separators/x86.s
	objdump -f $(TEST_OUT)/cxx-numeric-separators/x86.o > $(TEST_OUT)/cxx-numeric-separators/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-numeric-separators/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-numeric-separators/x64.s \
		tests/cxx_numeric_separators.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-numeric-separators/x64.o \
		$(TEST_OUT)/cxx-numeric-separators/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_test_main \
		$(TEST_OUT)/cxx-numeric-separators/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-numeric-separators/x64-host \
		tests/cxx_main_host.c $(TEST_OUT)/cxx-numeric-separators/x64.o
	$(TEST_OUT)/cxx-numeric-separators/x64-host
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-numeric-separators/x86.s \
		tests/cxx_numeric_separators.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-numeric-separators/x86.o \
		$(TEST_OUT)/cxx-numeric-separators/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-numeric-separators/start.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-numeric-separators/x86 \
		$(TEST_OUT)/cxx-numeric-separators/start.o \
		$(TEST_OUT)/cxx-numeric-separators/x86.o
	$(TEST_OUT)/cxx-numeric-separators/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-numeric-separators/x64.s \
		tests/cxx_numeric_separators.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-numeric-separators/x64.o \
		$(TEST_OUT)/cxx-numeric-separators/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-numeric-separators/start64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-numeric-separators/x64 \
		$(TEST_OUT)/cxx-numeric-separators/start64.o \
		$(TEST_OUT)/cxx-numeric-separators/x64.o
	$(TEST_OUT)/cxx-numeric-separators/x64
endif
	@echo "RCC++ digit separator tests completed"

test-cxx-user-defined-literals: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-user-defined-literals)
ifeq ($(OS),Windows_NT)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-user-defined-literals/x86.ro \
		tests/cxx_user_defined_literals.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-user-defined-literals/x64.ro \
		tests/cxx_user_defined_literals.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-user-defined-literals/x86.s \
		tests/cxx_user_defined_literals.cpp
	$(GREP) -q '_Zli7_answery' $(TEST_OUT)/cxx-user-defined-literals/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-user-defined-literals/x86.o \
		$(TEST_OUT)/cxx-user-defined-literals/x86.s
	objdump -f $(TEST_OUT)/cxx-user-defined-literals/x86.o > $(TEST_OUT)/cxx-user-defined-literals/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-user-defined-literals/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-user-defined-literals/x64.s \
		tests/cxx_user_defined_literals.cpp
	$(GREP) -q '_Zli7_answery' $(TEST_OUT)/cxx-user-defined-literals/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-user-defined-literals/x64.o \
		$(TEST_OUT)/cxx-user-defined-literals/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_test_main \
		$(TEST_OUT)/cxx-user-defined-literals/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-user-defined-literals/x64-host \
		tests/cxx_main_host.c $(TEST_OUT)/cxx-user-defined-literals/x64.o
	$(TEST_OUT)/cxx-user-defined-literals/x64-host
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-user-defined-literals/x86.s \
		tests/cxx_user_defined_literals.cpp
	$(GREP) -q '_Zli7_answery' \
		$(TEST_OUT)/cxx-user-defined-literals/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-user-defined-literals/x86.o \
		$(TEST_OUT)/cxx-user-defined-literals/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-user-defined-literals/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-user-defined-literals/x86 \
		$(TEST_OUT)/cxx-user-defined-literals/start-x86.o \
		$(TEST_OUT)/cxx-user-defined-literals/x86.o
	$(TEST_OUT)/cxx-user-defined-literals/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-user-defined-literals/x64.s \
		tests/cxx_user_defined_literals.cpp
	$(GREP) -q '_Zli7_answery' \
		$(TEST_OUT)/cxx-user-defined-literals/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-user-defined-literals/x64.o \
		$(TEST_OUT)/cxx-user-defined-literals/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-user-defined-literals/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-user-defined-literals/x64 \
		$(TEST_OUT)/cxx-user-defined-literals/start-x64.o \
		$(TEST_OUT)/cxx-user-defined-literals/x64.o
	$(TEST_OUT)/cxx-user-defined-literals/x64
endif
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-user-defined-literals/invalid-x86.ro tests/cxx_user_defined_literals_invalid.cpp,$(TEST_OUT)/cxx-user-defined-literals/invalid-x86.log)
	$(GREP) -F -q "bounded RCC++ user-defined literal operators require one unsigned long long, double, char, or const char*/size_t parameter form" \
		$(TEST_OUT)/cxx-user-defined-literals/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-user-defined-literals/invalid-x64.ro tests/cxx_user_defined_literals_invalid.cpp,$(TEST_OUT)/cxx-user-defined-literals/invalid-x64.log)
	$(GREP) -F -q "bounded RCC++ user-defined literal operators require one unsigned long long, double, char, or const char*/size_t parameter form" \
		$(TEST_OUT)/cxx-user-defined-literals/invalid-x64.log
	@echo "RCC++ user-defined literal tests completed"

test-string-embedded-nul: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/string-embedded-nul)
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/string-embedded-nul/x86.ro \
		tests/string_embedded_nul.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/string-embedded-nul/x64.ro \
		tests/string_embedded_nul.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/string-embedded-nul/x64.s \
		tests/string_embedded_nul.c
	$(CC) -o $(TEST_OUT)/string-embedded-nul/x64 \
		$(TEST_OUT)/string-embedded-nul/x64.s
	$(TEST_OUT)/string-embedded-nul/x64
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/string-embedded-nul/run-test \
		tests/string_embedded_nul_host_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/string-embedded-nul/run-test \
		$(TEST_OUT)/string-embedded-nul/x86.ro \
		$(TEST_OUT)/string-embedded-nul/x64.ro
else
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/string-embedded-nul/x86.s \
		tests/string_embedded_nul.c
	$(CC) -m32 -c -o $(TEST_OUT)/string-embedded-nul/x86.o \
		$(TEST_OUT)/string-embedded-nul/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/string-embedded-nul/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/string-embedded-nul/x86 \
		$(TEST_OUT)/string-embedded-nul/start-x86.o \
		$(TEST_OUT)/string-embedded-nul/x86.o
	$(TEST_OUT)/string-embedded-nul/x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/string-embedded-nul/x64.s \
		tests/string_embedded_nul.c
	$(CC) -c -o $(TEST_OUT)/string-embedded-nul/x64.o \
		$(TEST_OUT)/string-embedded-nul/x64.s
	$(CC) -c -o $(TEST_OUT)/string-embedded-nul/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/string-embedded-nul/x64 \
		$(TEST_OUT)/string-embedded-nul/start-x64.o \
		$(TEST_OUT)/string-embedded-nul/x64.o
	$(TEST_OUT)/string-embedded-nul/x64
endif
	@echo "C17 embedded-NUL string literal tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-member-methods: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-methods)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-methods/x86.s \
		tests/cxx_member_methods.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-methods/x64.s \
		tests/cxx_member_methods.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-methods/x86.o \
		$(TEST_OUT)/cxx-member-methods/x86.s
	objdump -f $(TEST_OUT)/cxx-member-methods/x86.o > $(TEST_OUT)/cxx-member-methods/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-member-methods/x86-arch.log
	$(CC) -c -o $(TEST_OUT)/cxx-member-methods/x64.o \
		$(TEST_OUT)/cxx-member-methods/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_test_main \
		$(TEST_OUT)/cxx-member-methods/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-member-methods/x64-host \
		tests/cxx_main_host.c $(TEST_OUT)/cxx-member-methods/x64.o
	$(TEST_OUT)/cxx-member-methods/x64-host
	@echo "RCC++ ordinary C++ member method tests completed"
else
test-cxx-member-methods: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-methods)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-methods/x86.s \
		tests/cxx_member_methods.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-methods/x64.s \
		tests/cxx_member_methods.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-methods/x86.o \
		$(TEST_OUT)/cxx-member-methods/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-member-methods/start.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-member-methods/x86 \
		$(TEST_OUT)/cxx-member-methods/start.o \
		$(TEST_OUT)/cxx-member-methods/x86.o
	$(TEST_OUT)/cxx-member-methods/x86
	$(CC) -c -o $(TEST_OUT)/cxx-member-methods/x64.o \
		$(TEST_OUT)/cxx-member-methods/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-member-methods/start64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-member-methods/x64 \
		$(TEST_OUT)/cxx-member-methods/start64.o \
		$(TEST_OUT)/cxx-member-methods/x64.o
	$(TEST_OUT)/cxx-member-methods/x64
	@echo "RCC++ ordinary C++ member method tests completed"
endif

.PHONY: test-cxx-static-members test-cxx-static-data-members test-cxx-class-template-static-data test-cxx-class-template-static-data-odr test-cxx-static-locals test-vla-declarations test-vla-declarator-variants test-cxx-constructor-body test-cxx-constructor-initializer-body test-aggregate-union-abi test-aggregate-flexible-abi test-aggregate-sse-abi test-aggregate-nested-abi
.PHONY: test-cxx-class-template-methods
.PHONY: test-cxx-class-template-specialization
.PHONY: test-cxx-class-template-dependent-base
.PHONY: test-cxx-class-template-dependent-base-lookup-invalid
.PHONY: test-cxx-class-template-specialization-ambiguous
.PHONY: test-cxx-class-template-specialization-partial-order-invalid
.PHONY: test-cxx-class-template-specialization-constraint-invalid
.PHONY: test-cxx-class-template-non-type
.PHONY: test-cxx-operator-overload
.PHONY: test-cxx-member-operator-forms
.PHONY: test-cxx-assignment-operator
.PHONY: test-cxx-lambda
.PHONY: test-cxx-conversion-operator
.PHONY: test-cxx-nonmember-operator
.PHONY: test-cxx-non-type-template-deduction
test-cxx-static-members-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-members)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-members/x86.s \
		tests/cxx_static_members.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-members/x64.s \
		tests/cxx_static_members.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-members/x86.o \
		$(TEST_OUT)/cxx-static-members/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-members/start.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-static-members/x86 \
		$(TEST_OUT)/cxx-static-members/start.o \
		$(TEST_OUT)/cxx-static-members/x86.o
	$(TEST_OUT)/cxx-static-members/x86
	$(CC) -c -o $(TEST_OUT)/cxx-static-members/x64.o \
		$(TEST_OUT)/cxx-static-members/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-static-members/start64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-static-members/x64 \
		$(TEST_OUT)/cxx-static-members/start64.o \
		$(TEST_OUT)/cxx-static-members/x64.o
	$(TEST_OUT)/cxx-static-members/x64
	@echo "RCC++ static C++ member method tests completed"

test-cxx-static-data-members-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-data-members)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-data-members/x86.s \
		tests/cxx_static_data_member.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-data-members/x64.s \
		tests/cxx_static_data_member.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-data-members/x86.o \
		$(TEST_OUT)/cxx-static-data-members/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-static-data-members/start.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-static-data-members/x86 \
		$(TEST_OUT)/cxx-static-data-members/start.o \
		$(TEST_OUT)/cxx-static-data-members/x86.o
	$(TEST_OUT)/cxx-static-data-members/x86
	$(CC) -c -o $(TEST_OUT)/cxx-static-data-members/x64.o \
		$(TEST_OUT)/cxx-static-data-members/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-static-data-members/start64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-static-data-members/x64 \
		$(TEST_OUT)/cxx-static-data-members/start64.o \
		$(TEST_OUT)/cxx-static-data-members/x64.o
	$(TEST_OUT)/cxx-static-data-members/x64
	@echo "RCC++ static data member tests completed"

test-cxx-static-member-tls: $(RCXX_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-member-tls)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-static-member-tls/x86.ro \
		tests/cxx_static_member_tls.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 --emit-unsigned-v3 \
		-o $(TEST_OUT)/cxx-static-member-tls/x86.rin \
		tests/cxx_static_member_tls.cpp
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-static-member-tls/x86.rin
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-static-member-tls/x64.ro \
		tests/cxx_static_member_tls.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 --emit-unsigned-v3 \
		-o $(TEST_OUT)/cxx-static-member-tls/x64.rin \
		tests/cxx_static_member_tls.cpp
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-static-member-tls/x64.rin
	@echo "RCC++ static thread-local data member tests completed"

test-cxx-class-template-static-data-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-static-data)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-static-data/x86.s \
		tests/cxx_class_template_static_data.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-static-data/x64.s \
		tests/cxx_class_template_static_data.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-static-data/x86.o \
		$(TEST_OUT)/cxx-class-template-static-data/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-static-data/start.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-static-data/x86 \
		$(TEST_OUT)/cxx-class-template-static-data/start.o \
		$(TEST_OUT)/cxx-class-template-static-data/x86.o
	$(TEST_OUT)/cxx-class-template-static-data/x86
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-static-data/x64.o \
		$(TEST_OUT)/cxx-class-template-static-data/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-static-data/start64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-static-data/x64 \
		$(TEST_OUT)/cxx-class-template-static-data/start64.o \
		$(TEST_OUT)/cxx-class-template-static-data/x64.o
	$(TEST_OUT)/cxx-class-template-static-data/x64
	@echo "RCC++ class-template static data member tests completed"

test-cxx-class-template-static-data-odr-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-static-data-odr)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x86.s \
		tests/cxx_class_template_static_data_a.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.s \
		tests/cxx_class_template_static_data_b.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x86.o \
		$(TEST_OUT)/cxx-class-template-static-data-odr/a-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.o \
		$(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.s
	$(OBJCOPY) --redefine-sym _rcc_entry=_rcc_entry_b \
		$(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.o
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-static-data-odr/x86 \
		$(TEST_OUT)/cxx-class-template-static-data-odr/start-x86.o \
		$(TEST_OUT)/cxx-class-template-static-data-odr/a-x86.o \
		$(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.o
	$(TEST_OUT)/cxx-class-template-static-data-odr/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.s \
		tests/cxx_class_template_static_data_a.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.s \
		tests/cxx_class_template_static_data_b.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.o \
		$(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.o \
		$(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.s
	$(OBJCOPY) --redefine-sym _rcc_entry=_rcc_entry_b \
		$(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.o
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-class-template-static-data-odr/x64 \
		$(TEST_OUT)/cxx-class-template-static-data-odr/start-x64.o \
		$(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.o \
		$(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.o
	$(TEST_OUT)/cxx-class-template-static-data-odr/x64
	@echo "RCC++ template static data ODR tests completed"

test-cxx-static-locals: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-locals)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-locals/x86.s \
		tests/cxx_static_locals.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-static-locals/x64.s \
		tests/cxx_static_locals.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 --emit-unsigned-v3 \
		-o $(TEST_OUT)/cxx-static-locals/x86.rin \
		tests/cxx_static_locals.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 --emit-unsigned-v3 \
		-o $(TEST_OUT)/cxx-static-locals/x64.rin \
		tests/cxx_static_locals.cpp
	@echo "Dual-architecture C++ static local generation tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-static-members: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-members)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-static-members,cxx_static_members.cpp)

test-cxx-static-data-members: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-static-data-members)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-static-data-members,cxx_static_data_member.cpp)

test-cxx-class-template-static-data: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-static-data)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-class-template-static-data,cxx_class_template_static_data.cpp)

test-cxx-class-template-static-data-odr: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-static-data-odr)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x86.s tests/cxx_class_template_static_data_a.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.s tests/cxx_class_template_static_data_b.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x86.o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.s
	objdump -f $(TEST_OUT)/cxx-class-template-static-data-odr/a-x86.o > $(TEST_OUT)/cxx-class-template-static-data-odr/a-x86-arch.log
	objdump -f $(TEST_OUT)/cxx-class-template-static-data-odr/b-x86.o > $(TEST_OUT)/cxx-class-template-static-data-odr/b-x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-class-template-static-data-odr/a-x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-class-template-static-data-odr/b-x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.s tests/cxx_class_template_static_data_a.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.s tests/cxx_class_template_static_data_b.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.o $(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.s
	$(OBJCOPY) --redefine-sym main=rcc_generated_main_a $(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.o
	$(OBJCOPY) --redefine-sym main=rcc_generated_main_b $(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.o
	$(OBJCOPY) --redefine-sym _rcc_entry=_rcc_entry_b $(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.o
	$(CC) $(CFLAGS) -Wl,--allow-multiple-definition -o $(TEST_OUT)/cxx-class-template-static-data-odr/x64-host tests/cxx_language_core_host.c $(TEST_OUT)/cxx-class-template-static-data-odr/a-x64.o $(TEST_OUT)/cxx-class-template-static-data-odr/b-x64.o
	$(TEST_OUT)/cxx-class-template-static-data-odr/x64-host
else
test-cxx-static-members: test-cxx-static-members-posix
test-cxx-static-data-members: test-cxx-static-data-members-posix
test-cxx-class-template-static-data: test-cxx-class-template-static-data-posix
test-cxx-class-template-static-data-odr: test-cxx-class-template-static-data-odr-posix
endif

test-vla-declarations: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-declarations)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-storage-x86.ro \
		tests/invalid_vla_storage.c,$(TEST_OUT)/vla-declarations/invalid-storage-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-member-x86.ro \
		tests/invalid_vla_member.c,$(TEST_OUT)/vla-declarations/invalid-member-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-storage-x64.ro \
		tests/invalid_vla_storage.c,$(TEST_OUT)/vla-declarations/invalid-storage-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-member-x64.ro \
		tests/invalid_vla_member.c,$(TEST_OUT)/vla-declarations/invalid-member-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-initializer-x86.ro \
		tests/invalid_vla_initializer.c,$(TEST_OUT)/vla-declarations/invalid-initializer-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-initializer-x64.ro \
		tests/invalid_vla_initializer.c,$(TEST_OUT)/vla-declarations/invalid-initializer-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-for-scope-x86.ro \
		tests/invalid_vla_for_initializer_scope.c,$(TEST_OUT)/vla-declarations/invalid-for-scope-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-for-scope-x64.ro \
		tests/invalid_vla_for_initializer_scope.c,$(TEST_OUT)/vla-declarations/invalid-for-scope-x64.log)
	$(GREP) -q "variably modified object cannot have linkage" \
		$(TEST_OUT)/vla-declarations/invalid-storage-x86.log
	$(GREP) -q "variably modified typedef is only valid at block scope" \
		$(TEST_OUT)/vla-declarations/invalid-storage-x86.log
	$(GREP) -q "variably modified type is not allowed for struct/union member" \
		$(TEST_OUT)/vla-declarations/invalid-member-x86.log
	$(GREP) -q "variably modified object cannot have linkage" \
		$(TEST_OUT)/vla-declarations/invalid-storage-x64.log
	$(GREP) -q "variably modified typedef is only valid at block scope" \
		$(TEST_OUT)/vla-declarations/invalid-storage-x64.log
	$(GREP) -q "variably modified type is not allowed for struct/union member" \
		$(TEST_OUT)/vla-declarations/invalid-member-x64.log
	$(GREP) -q "variable-length array cannot have an initializer" \
		$(TEST_OUT)/vla-declarations/invalid-initializer-x86.log
	$(GREP) -q "variable-length array cannot have an initializer" \
		$(TEST_OUT)/vla-declarations/invalid-initializer-x64.log
	$(GREP) -F -q "undefined identifier 'values'" \
		$(TEST_OUT)/vla-declarations/invalid-for-scope-x86.log
	$(GREP) -F -q "undefined identifier 'values'" \
		$(TEST_OUT)/vla-declarations/invalid-for-scope-x64.log
	@echo "C17 invalid variably modified declaration tests completed"

ifeq ($(OS),Windows_NT)
test-vla-declarator-variants: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-declarator-variants)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/vla-declarator-variants/x86.s \
		tests/vla_declarator_variants.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/vla-declarator-variants/x64.s \
		tests/vla_declarator_variants.c
	$(CC) -m32 -nostdlib -no-pie -Wl,--entry,main \
		-o $(TEST_OUT)/vla-declarator-variants/x86 \
		$(TEST_OUT)/vla-declarator-variants/x86.s
	$(TEST_OUT)/vla-declarator-variants/x86
	$(CC) -nostdlib -no-pie -Wl,--entry,main \
		-o $(TEST_OUT)/vla-declarator-variants/x64 \
		$(TEST_OUT)/vla-declarator-variants/x64.s
	$(TEST_OUT)/vla-declarator-variants/x64
	@echo "Dual-architecture VLA declarator variant lowering tests completed"
else
test-vla-declarator-variants: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-declarator-variants)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/vla-declarator-variants/x86.s \
		tests/vla_declarator_variants.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/vla-declarator-variants/x64.s \
		tests/vla_declarator_variants.c
	$(CC) -m32 -c -o $(TEST_OUT)/vla-declarator-variants/x86.o \
		$(TEST_OUT)/vla-declarator-variants/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/vla-declarator-variants/x86-start.o \
		tests/vla_runtime_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/vla-declarator-variants/x86 \
		$(TEST_OUT)/vla-declarator-variants/x86-start.o \
		$(TEST_OUT)/vla-declarator-variants/x86.o
	$(TEST_OUT)/vla-declarator-variants/x86
	$(CC) -c -o $(TEST_OUT)/vla-declarator-variants/x64.o \
		$(TEST_OUT)/vla-declarator-variants/x64.s
	$(CC) -c -o $(TEST_OUT)/vla-declarator-variants/x64-start.o \
		tests/vla_runtime_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/vla-declarator-variants/x64 \
		$(TEST_OUT)/vla-declarator-variants/x64-start.o \
		$(TEST_OUT)/vla-declarator-variants/x64.o
	$(TEST_OUT)/vla-declarator-variants/x64
	@echo "Dual-architecture VLA declarator variant lowering tests completed"
endif

ifeq ($(OS),Windows_NT)
test-cxx-constructor-body: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constructor-body)
	$(call CXX_WINDOWS_CONSTRUCTOR_TEST,cxx-constructor-body,cxx_constructor_body.cpp,cxx_constructor_body_run_test.c)
else
test-cxx-constructor-body: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constructor-body)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constructor-body/x86.s \
		tests/cxx_constructor_body.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constructor-body/x64.s \
		tests/cxx_constructor_body.cpp
	$(CC) -m32 -o $(TEST_OUT)/cxx-constructor-body/run-x86 \
		tests/cxx_constructor_body_run_test.c \
		$(TEST_OUT)/cxx-constructor-body/x86.s
	$(TEST_OUT)/cxx-constructor-body/run-x86
	$(CC) -o $(TEST_OUT)/cxx-constructor-body/run-x64 \
		tests/cxx_constructor_body_run_test.c \
		$(TEST_OUT)/cxx-constructor-body/x64.s
	$(TEST_OUT)/cxx-constructor-body/run-x64
	@echo "Dual-architecture C++ constructor-body lowering tests completed"
endif

test-cxx-constructor-initializer-body-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constructor-initializer-body)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constructor-initializer-body/x86.s \
		tests/cxx_constructor_initializer_body.cpp
	$(CC) -m32 -o $(TEST_OUT)/cxx-constructor-initializer-body/x86 \
		tests/cxx_constructor_initializer_body_run_test.c \
		$(TEST_OUT)/cxx-constructor-initializer-body/x86.s
	$(TEST_OUT)/cxx-constructor-initializer-body/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constructor-initializer-body/x64.s \
		tests/cxx_constructor_initializer_body.cpp
	$(CC) -o $(TEST_OUT)/cxx-constructor-initializer-body/x64 \
		tests/cxx_constructor_initializer_body_run_test.c \
		$(TEST_OUT)/cxx-constructor-initializer-body/x64.s
	$(TEST_OUT)/cxx-constructor-initializer-body/x64
	@echo "C++ constructor mem-initializer plus body tests completed"

test-cxx-base-constructor-initializer-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-base-constructor-initializer)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-base-constructor-initializer/x86.s \
		tests/cxx_base_constructor_initializer.cpp
	$(CC) -m32 -o $(TEST_OUT)/cxx-base-constructor-initializer/x86 \
		tests/cxx_base_constructor_initializer_run_test.c \
		$(TEST_OUT)/cxx-base-constructor-initializer/x86.s
	$(TEST_OUT)/cxx-base-constructor-initializer/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-base-constructor-initializer/x64.s \
		tests/cxx_base_constructor_initializer.cpp
	$(CC) -o $(TEST_OUT)/cxx-base-constructor-initializer/x64 \
		tests/cxx_base_constructor_initializer_run_test.c \
		$(TEST_OUT)/cxx-base-constructor-initializer/x64.s
	$(TEST_OUT)/cxx-base-constructor-initializer/x64
	@echo "C++ fixed-layout base constructor initializer tests completed"

test-cxx-default-member-initializer-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-default-member-initializer)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-default-member-initializer/x86.s \
		tests/cxx_default_member_initializer.cpp
	$(CC) -m32 -o $(TEST_OUT)/cxx-default-member-initializer/x86 \
		tests/cxx_default_member_initializer_run_test.c \
		$(TEST_OUT)/cxx-default-member-initializer/x86.s
	$(TEST_OUT)/cxx-default-member-initializer/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-default-member-initializer/x64.s \
		tests/cxx_default_member_initializer.cpp
	$(CC) -o $(TEST_OUT)/cxx-default-member-initializer/x64 \
		tests/cxx_default_member_initializer_run_test.c \
		$(TEST_OUT)/cxx-default-member-initializer/x64.s
	$(TEST_OUT)/cxx-default-member-initializer/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-default-member-initializer/invalid-x86.ro \
		tests/cxx_default_member_initializer_array_invalid.cpp \
		>$(TEST_OUT)/cxx-default-member-initializer/invalid-x86.log 2>&1
	$(GREP) -q "new requires scalar constant default member initializers" \
		$(TEST_OUT)/cxx-default-member-initializer/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-default-member-initializer/invalid-x64.ro \
		tests/cxx_default_member_initializer_array_invalid.cpp \
		>$(TEST_OUT)/cxx-default-member-initializer/invalid-x64.log 2>&1
	$(GREP) -q "new requires scalar constant default member initializers" \
		$(TEST_OUT)/cxx-default-member-initializer/invalid-x64.log
	@echo "C++ default member initializer tests completed"

test-cxx-delegating-constructor-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-delegating-constructor)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-delegating-constructor/x86.s \
		tests/cxx_delegating_constructor.cpp
	$(CC) -m32 -o $(TEST_OUT)/cxx-delegating-constructor/x86 \
		tests/cxx_delegating_constructor_run_test.c \
		$(TEST_OUT)/cxx-delegating-constructor/x86.s
	$(TEST_OUT)/cxx-delegating-constructor/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-delegating-constructor/x64.s \
		tests/cxx_delegating_constructor.cpp
	$(CC) -o $(TEST_OUT)/cxx-delegating-constructor/x64 \
		tests/cxx_delegating_constructor_run_test.c \
		$(TEST_OUT)/cxx-delegating-constructor/x64.s
	$(TEST_OUT)/cxx-delegating-constructor/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-delegating-constructor/invalid-x86.ro \
		tests/cxx_delegating_constructor_invalid.cpp \
		>$(TEST_OUT)/cxx-delegating-constructor/invalid-x86.log 2>&1
	$(GREP) -q "cyclic C++ delegating constructor" \
		$(TEST_OUT)/cxx-delegating-constructor/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-delegating-constructor/invalid-x64.ro \
		tests/cxx_delegating_constructor_invalid.cpp \
		>$(TEST_OUT)/cxx-delegating-constructor/invalid-x64.log 2>&1
	$(GREP) -q "cyclic C++ delegating constructor" \
		$(TEST_OUT)/cxx-delegating-constructor/invalid-x64.log
	@echo "C++ delegating constructor tests completed"

test-cxx-converting-constructor-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-converting-constructor)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-converting-constructor/x86.s \
		tests/cxx_converting_constructor.cpp
	$(CC) -m32 -o $(TEST_OUT)/cxx-converting-constructor/x86 \
		tests/cxx_converting_constructor_run_test.c \
		$(TEST_OUT)/cxx-converting-constructor/x86.s
	$(TEST_OUT)/cxx-converting-constructor/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-converting-constructor/x64.s \
		tests/cxx_converting_constructor.cpp
	$(CC) -o $(TEST_OUT)/cxx-converting-constructor/x64 \
		tests/cxx_converting_constructor_run_test.c \
		$(TEST_OUT)/cxx-converting-constructor/x64.s
	$(TEST_OUT)/cxx-converting-constructor/x64
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-converting-constructor/invalid-x86.ro \
		tests/cxx_explicit_copy_initialization_invalid.cpp \
		>$(TEST_OUT)/cxx-converting-constructor/invalid-x86.log 2>&1
	$(GREP) -q "no safely lowerable constructor accepts the C++ initializer" \
		$(TEST_OUT)/cxx-converting-constructor/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-converting-constructor/invalid-x64.ro \
		tests/cxx_explicit_copy_initialization_invalid.cpp \
		>$(TEST_OUT)/cxx-converting-constructor/invalid-x64.log 2>&1
	$(GREP) -q "no safely lowerable constructor accepts the C++ initializer" \
		$(TEST_OUT)/cxx-converting-constructor/invalid-x64.log
	@echo "C++ converting constructor tests completed"

test-cxx-inherited-constructor-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inherited-constructor)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-inherited-constructor/x86.s \
		tests/cxx_inherited_constructor.cpp
	$(CC) -m32 -o $(TEST_OUT)/cxx-inherited-constructor/x86 \
		tests/cxx_inherited_constructor_run_test.c \
		$(TEST_OUT)/cxx-inherited-constructor/x86.s
	$(TEST_OUT)/cxx-inherited-constructor/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-inherited-constructor/x64.s \
		tests/cxx_inherited_constructor.cpp
	$(CC) -o $(TEST_OUT)/cxx-inherited-constructor/x64 \
		tests/cxx_inherited_constructor_run_test.c \
		$(TEST_OUT)/cxx-inherited-constructor/x64.s
	$(TEST_OUT)/cxx-inherited-constructor/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-inherited-constructor/array-order-x86-freestanding.s \
		tests/cxx_inherited_constructor_array_order.cpp
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/cxx-inherited-constructor/array-order-x86-freestanding,$(TEST_OUT)/cxx-inherited-constructor/array-order-x86-freestanding.s)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inherited-constructor-array-order)
	$(call CXX_POSIX_ENTRY_TEST,cxx-inherited-constructor-array-order,cxx_inherited_constructor_array_order.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-inherited-constructor/array-initializer-x86-freestanding.s \
		tests/cxx_inherited_constructor_array_initializer.cpp
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/cxx-inherited-constructor/array-initializer-x86-freestanding,$(TEST_OUT)/cxx-inherited-constructor/array-initializer-x86-freestanding.s)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inherited-constructor-array-initializer)
	$(call CXX_POSIX_ENTRY_TEST,cxx-inherited-constructor-array-initializer,cxx_inherited_constructor_array_initializer.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/nested-array-x86.ro tests/cxx_inherited_constructor_invalid_array_nested.cpp,$(TEST_OUT)/cxx-inherited-constructor/nested-array-x86.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/nested-array-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/nested-array-x64.ro tests/cxx_inherited_constructor_invalid_array_nested.cpp,$(TEST_OUT)/cxx-inherited-constructor/nested-array-x64.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/nested-array-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x86.ro tests/cxx_inherited_constructor_invalid_array_cleanup.cpp,$(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x86.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x64.ro tests/cxx_inherited_constructor_invalid_array_cleanup.cpp,$(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x64.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x64.log
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inherited-constructor/virtual-x86.ro \
		tests/cxx_inherited_constructor_invalid_virtual.cpp \
		>$(TEST_OUT)/cxx-inherited-constructor/virtual-x86.log 2>&1
	$(GREP) -q "using-base constructor cannot name a virtual base" \
		$(TEST_OUT)/cxx-inherited-constructor/virtual-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inherited-constructor/virtual-x64.ro \
		tests/cxx_inherited_constructor_invalid_virtual.cpp \
		>$(TEST_OUT)/cxx-inherited-constructor/virtual-x64.log 2>&1
	$(GREP) -q "using-base constructor cannot name a virtual base" \
		$(TEST_OUT)/cxx-inherited-constructor/virtual-x64.log
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inherited-constructor/access-x86.ro \
		tests/cxx_inherited_constructor_invalid_access.cpp \
		>$(TEST_OUT)/cxx-inherited-constructor/access-x86.log 2>&1
	$(GREP) -q "using-base constructor requires a public direct base" \
		$(TEST_OUT)/cxx-inherited-constructor/access-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inherited-constructor/access-x64.ro \
		tests/cxx_inherited_constructor_invalid_access.cpp \
		>$(TEST_OUT)/cxx-inherited-constructor/access-x64.log 2>&1
	$(GREP) -q "using-base constructor requires a public direct base" \
		$(TEST_OUT)/cxx-inherited-constructor/access-x64.log
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inherited-constructor/member-x86.ro \
		tests/cxx_inherited_constructor_invalid_member.cpp \
		>$(TEST_OUT)/cxx-inherited-constructor/member-x86.log 2>&1
	$(GREP) -q "using-base constructors require scalar derived fields" \
		$(TEST_OUT)/cxx-inherited-constructor/member-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inherited-constructor/member-x64.ro \
		tests/cxx_inherited_constructor_invalid_member.cpp \
		>$(TEST_OUT)/cxx-inherited-constructor/member-x64.log 2>&1
	$(GREP) -q "using-base constructors require scalar derived fields" \
		$(TEST_OUT)/cxx-inherited-constructor/member-x64.log
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inherited-constructor/unknown-x86.ro \
		tests/cxx_inherited_constructor_invalid_unknown.cpp \
		>$(TEST_OUT)/cxx-inherited-constructor/unknown-x86.log 2>&1
	$(GREP) -q "using-base constructor names an unknown direct base" \
		$(TEST_OUT)/cxx-inherited-constructor/unknown-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inherited-constructor/unknown-x64.ro \
		tests/cxx_inherited_constructor_invalid_unknown.cpp \
		>$(TEST_OUT)/cxx-inherited-constructor/unknown-x64.log 2>&1
	$(GREP) -q "using-base constructor names an unknown direct base" \
		$(TEST_OUT)/cxx-inherited-constructor/unknown-x64.log
	@echo "C++ inherited constructor tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-constructor-initializer-body: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constructor-initializer-body)
	$(call CXX_WINDOWS_CONSTRUCTOR_TEST,cxx-constructor-initializer-body,cxx_constructor_initializer_body.cpp,cxx_constructor_initializer_body_run_test.c)

test-cxx-base-constructor-initializer: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-base-constructor-initializer)
	$(call CXX_WINDOWS_CONSTRUCTOR_TEST,cxx-base-constructor-initializer,cxx_base_constructor_initializer.cpp,cxx_base_constructor_initializer_run_test.c)

test-cxx-default-member-initializer: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-default-member-initializer)
	$(call CXX_WINDOWS_CONSTRUCTOR_TEST,cxx-default-member-initializer,cxx_default_member_initializer.cpp,cxx_default_member_initializer_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-default-member-initializer/invalid-x86.ro tests/cxx_default_member_initializer_array_invalid.cpp,$(TEST_OUT)/cxx-default-member-initializer/invalid-x86.log)
	$(GREP) -F -q "new requires scalar constant default member initializers" $(TEST_OUT)/cxx-default-member-initializer/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-default-member-initializer/invalid-x64.ro tests/cxx_default_member_initializer_array_invalid.cpp,$(TEST_OUT)/cxx-default-member-initializer/invalid-x64.log)
	$(GREP) -F -q "new requires scalar constant default member initializers" $(TEST_OUT)/cxx-default-member-initializer/invalid-x64.log

test-cxx-delegating-constructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-delegating-constructor)
	$(call CXX_WINDOWS_CONSTRUCTOR_TEST,cxx-delegating-constructor,cxx_delegating_constructor.cpp,cxx_delegating_constructor_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-delegating-constructor/invalid-x86.ro tests/cxx_delegating_constructor_invalid.cpp,$(TEST_OUT)/cxx-delegating-constructor/invalid-x86.log)
	$(GREP) -F -q "cyclic C++ delegating constructor" $(TEST_OUT)/cxx-delegating-constructor/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-delegating-constructor/invalid-x64.ro tests/cxx_delegating_constructor_invalid.cpp,$(TEST_OUT)/cxx-delegating-constructor/invalid-x64.log)
	$(GREP) -F -q "cyclic C++ delegating constructor" $(TEST_OUT)/cxx-delegating-constructor/invalid-x64.log

test-cxx-converting-constructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-converting-constructor)
	$(call CXX_WINDOWS_CONSTRUCTOR_TEST,cxx-converting-constructor,cxx_converting_constructor.cpp,cxx_converting_constructor_run_test.c)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-converting-constructor/invalid-x86.ro tests/cxx_explicit_copy_initialization_invalid.cpp,$(TEST_OUT)/cxx-converting-constructor/invalid-x86.log)
	$(GREP) -F -q "no safely lowerable constructor accepts the C++ initializer" $(TEST_OUT)/cxx-converting-constructor/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-converting-constructor/invalid-x64.ro tests/cxx_explicit_copy_initialization_invalid.cpp,$(TEST_OUT)/cxx-converting-constructor/invalid-x64.log)
	$(GREP) -F -q "no safely lowerable constructor accepts the C++ initializer" $(TEST_OUT)/cxx-converting-constructor/invalid-x64.log

test-cxx-inherited-constructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inherited-constructor)
	$(call CXX_WINDOWS_CONSTRUCTOR_TEST,cxx-inherited-constructor,cxx_inherited_constructor.cpp,cxx_inherited_constructor_run_test.c)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-inherited-constructor/array-order-x86-freestanding.s tests/cxx_inherited_constructor_array_order.cpp
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/cxx-inherited-constructor/array-order-x86-freestanding,$(TEST_OUT)/cxx-inherited-constructor/array-order-x86-freestanding.s)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inherited-constructor-array-order)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-inherited-constructor-array-order,cxx_inherited_constructor_array_order.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-inherited-constructor/array-initializer-x86-freestanding.s tests/cxx_inherited_constructor_array_initializer.cpp
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/cxx-inherited-constructor/array-initializer-x86-freestanding,$(TEST_OUT)/cxx-inherited-constructor/array-initializer-x86-freestanding.s)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inherited-constructor-array-initializer)
	$(call CXX_WINDOWS_ENTRY_TEST,cxx-inherited-constructor-array-initializer,cxx_inherited_constructor_array_initializer.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/nested-array-x86.ro tests/cxx_inherited_constructor_invalid_array_nested.cpp,$(TEST_OUT)/cxx-inherited-constructor/nested-array-x86.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/nested-array-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/nested-array-x64.ro tests/cxx_inherited_constructor_invalid_array_nested.cpp,$(TEST_OUT)/cxx-inherited-constructor/nested-array-x64.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/nested-array-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x86.ro tests/cxx_inherited_constructor_invalid_array_cleanup.cpp,$(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x86.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x64.ro tests/cxx_inherited_constructor_invalid_array_cleanup.cpp,$(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x64.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/array-cleanup-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/virtual-x86.ro tests/cxx_inherited_constructor_invalid_virtual.cpp,$(TEST_OUT)/cxx-inherited-constructor/virtual-x86.log)
	$(GREP) -F -q "using-base constructor cannot name a virtual base" $(TEST_OUT)/cxx-inherited-constructor/virtual-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/virtual-x64.ro tests/cxx_inherited_constructor_invalid_virtual.cpp,$(TEST_OUT)/cxx-inherited-constructor/virtual-x64.log)
	$(GREP) -F -q "using-base constructor cannot name a virtual base" $(TEST_OUT)/cxx-inherited-constructor/virtual-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/access-x86.ro tests/cxx_inherited_constructor_invalid_access.cpp,$(TEST_OUT)/cxx-inherited-constructor/access-x86.log)
	$(GREP) -F -q "using-base constructor requires a public direct base" $(TEST_OUT)/cxx-inherited-constructor/access-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/access-x64.ro tests/cxx_inherited_constructor_invalid_access.cpp,$(TEST_OUT)/cxx-inherited-constructor/access-x64.log)
	$(GREP) -F -q "using-base constructor requires a public direct base" $(TEST_OUT)/cxx-inherited-constructor/access-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/member-x86.ro tests/cxx_inherited_constructor_invalid_member.cpp,$(TEST_OUT)/cxx-inherited-constructor/member-x86.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/member-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/member-x64.ro tests/cxx_inherited_constructor_invalid_member.cpp,$(TEST_OUT)/cxx-inherited-constructor/member-x64.log)
	$(GREP) -F -q "using-base constructors require safely lowerable derived members" $(TEST_OUT)/cxx-inherited-constructor/member-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/unknown-x86.ro tests/cxx_inherited_constructor_invalid_unknown.cpp,$(TEST_OUT)/cxx-inherited-constructor/unknown-x86.log)
	$(GREP) -F -q "using-base constructor names an unknown direct base" $(TEST_OUT)/cxx-inherited-constructor/unknown-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inherited-constructor/unknown-x64.ro tests/cxx_inherited_constructor_invalid_unknown.cpp,$(TEST_OUT)/cxx-inherited-constructor/unknown-x64.log)
	$(GREP) -F -q "using-base constructor names an unknown direct base" $(TEST_OUT)/cxx-inherited-constructor/unknown-x64.log
else
test-cxx-constructor-initializer-body: test-cxx-constructor-initializer-body-posix
test-cxx-base-constructor-initializer: test-cxx-base-constructor-initializer-posix
test-cxx-default-member-initializer: test-cxx-default-member-initializer-posix
test-cxx-delegating-constructor: test-cxx-delegating-constructor-posix
test-cxx-converting-constructor: test-cxx-converting-constructor-posix
test-cxx-inherited-constructor: test-cxx-inherited-constructor-posix
endif

test-aggregate-union-abi: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/aggregate-union-abi)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-union-abi/x86.s \
		tests/aggregate_union_abi.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-union-abi/x64.s \
		tests/aggregate_union_abi.c
	$(call AGGREGATE_X86_ABI_TEST,aggregate-union-abi,aggregate_union_abi_host.c)
	$(CC) -c -o $(TEST_OUT)/aggregate-union-abi/x64.o \
		$(TEST_OUT)/aggregate-union-abi/x64.s
	$(CC) -c -o $(TEST_OUT)/aggregate-union-abi/host-x64.o \
		tests/aggregate_union_abi_host.c
	$(CC) -o $(TEST_OUT)/aggregate-union-abi/x64 \
		$(TEST_OUT)/aggregate-union-abi/host-x64.o \
		$(TEST_OUT)/aggregate-union-abi/x64.o
	$(TEST_OUT)/aggregate-union-abi/x64
	@echo "SysV union aggregate ABI boundary tests completed"

test-aggregate-flexible-abi: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/aggregate-flexible-abi)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-flexible-abi/x86.s \
		tests/aggregate_flexible_abi.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-flexible-abi/x64.s \
		tests/aggregate_flexible_abi.c
	$(call AGGREGATE_X86_ABI_TEST,aggregate-flexible-abi,aggregate_flexible_abi_host.c)
	$(CC) -c -o $(TEST_OUT)/aggregate-flexible-abi/x64.o \
		$(TEST_OUT)/aggregate-flexible-abi/x64.s
	$(CC) -c -o $(TEST_OUT)/aggregate-flexible-abi/host-x64.o \
		tests/aggregate_flexible_abi_host.c
	$(CC) -o $(TEST_OUT)/aggregate-flexible-abi/x64 \
		$(TEST_OUT)/aggregate-flexible-abi/host-x64.o \
		$(TEST_OUT)/aggregate-flexible-abi/x64.o
	$(TEST_OUT)/aggregate-flexible-abi/x64
	@echo "SysV flexible-array aggregate ABI tests completed"

test-aggregate-sse-abi: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/aggregate-sse-abi)
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc -Ibootstrap/include -S \
		-o $(TEST_OUT)/aggregate-sse-abi/x86.s \
		tests/aggregate_sse_abi.c
	$(call AGGREGATE_X86_ABI_TEST,aggregate-sse-abi,aggregate_sse_abi_host.c)
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include -S \
		-o $(TEST_OUT)/aggregate-sse-abi/x64.s \
		tests/aggregate_sse_abi.c
	$(CC) -c -o $(TEST_OUT)/aggregate-sse-abi/x64.o \
		$(TEST_OUT)/aggregate-sse-abi/x64.s
	$(CC) -c -o $(TEST_OUT)/aggregate-sse-abi/host-x64.o \
		tests/aggregate_sse_abi_host.c
	$(CC) -o $(TEST_OUT)/aggregate-sse-abi/x64 \
		$(TEST_OUT)/aggregate-sse-abi/host-x64.o \
		$(TEST_OUT)/aggregate-sse-abi/x64.o
	$(TEST_OUT)/aggregate-sse-abi/x64
	@echo "SysV SSE aggregate and variadic ABI tests completed"

test-aggregate-nested-abi: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/aggregate-nested-abi)
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc -Ibootstrap/include -S \
		-o $(TEST_OUT)/aggregate-nested-abi/x86.s \
		tests/aggregate_nested_abi.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include -S \
		-o $(TEST_OUT)/aggregate-nested-abi/x64.s \
		tests/aggregate_nested_abi.c
	$(call AGGREGATE_X86_ABI_TEST,aggregate-nested-abi,aggregate_nested_abi_host.c)
	$(CC) -c -o $(TEST_OUT)/aggregate-nested-abi/x64.o \
		$(TEST_OUT)/aggregate-nested-abi/x64.s
	$(CC) -c -o $(TEST_OUT)/aggregate-nested-abi/host-x64.o \
		tests/aggregate_nested_abi_host.c
	$(CC) -o $(TEST_OUT)/aggregate-nested-abi/x64 \
		$(TEST_OUT)/aggregate-nested-abi/host-x64.o \
		$(TEST_OUT)/aggregate-nested-abi/x64.o
	$(TEST_OUT)/aggregate-nested-abi/x64
	@echo "SysV nested aggregate ABI tests completed"

test-cxx-overloads: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-overloads)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/x86.ro tests/cxx_overload.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/x64.ro tests/cxx_overload.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/pointer-bool-x86.ro \
		tests/cxx_pointer_bool.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/pointer-bool-x64.ro \
		tests/cxx_pointer_bool.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/derived-base-reference-x86.ro \
		tests/cxx_derived_base_reference.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/derived-base-reference-x64.ro \
		tests/cxx_derived_base_reference.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/ambiguous-base-x86.ro tests/cxx_ambiguous_base_conversion_invalid.cpp,$(TEST_OUT)/cxx-overloads/ambiguous-base-x86.log)
	$(call CHECK_TEXT,incompatible type for argument 1 to 'take_base',$(TEST_OUT)/cxx-overloads/ambiguous-base-x86.log)
	$(call CHECK_TEXT,incompatible type for argument 1 to 'take_base_pointer',$(TEST_OUT)/cxx-overloads/ambiguous-base-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/ambiguous-base-x64.ro tests/cxx_ambiguous_base_conversion_invalid.cpp,$(TEST_OUT)/cxx-overloads/ambiguous-base-x64.log)
	$(call CHECK_TEXT,incompatible type for argument 1 to 'take_base',$(TEST_OUT)/cxx-overloads/ambiguous-base-x64.log)
	$(call CHECK_TEXT,incompatible type for argument 1 to 'take_base_pointer',$(TEST_OUT)/cxx-overloads/ambiguous-base-x64.log)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/cxx-overloads/verify \
		tests/cxx_overload_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/cxx-overloads/verify \
		$(TEST_OUT)/cxx-overloads/x86.ro \
		$(TEST_OUT)/cxx-overloads/x64.ro \
		$(TEST_OUT)/cxx-overloads/derived-base-reference-x86.ro \
		$(TEST_OUT)/cxx-overloads/derived-base-reference-x64.ro
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/ambiguous.ro tests/cxx_overload_ambiguous.cpp,$(TEST_OUT)/cxx-overloads/ambiguous.log)
	$(call CHECK_TEXT,ambiguous overload for 'ambiguous',$(TEST_OUT)/cxx-overloads/ambiguous.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/partial-order-invalid-x86.ro tests/cxx_overload_partial_order_invalid.cpp,$(TEST_OUT)/cxx-overloads/partial-order-invalid-x86.log)
	$(call CHECK_TEXT,ambiguous overload for 'select_rank',$(TEST_OUT)/cxx-overloads/partial-order-invalid-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/partial-order-invalid-x64.ro tests/cxx_overload_partial_order_invalid.cpp,$(TEST_OUT)/cxx-overloads/partial-order-invalid-x64.log)
	$(call CHECK_TEXT,ambiguous overload for 'select_rank',$(TEST_OUT)/cxx-overloads/partial-order-invalid-x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/nullptr-integer.ro tests/cxx_nullptr_integer_rejected.cpp,$(TEST_OUT)/cxx-overloads/nullptr-integer.log)
	$(call CHECK_TEXT,incompatible type for argument 1 to 'consume_integer',$(TEST_OUT)/cxx-overloads/nullptr-integer.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/nullptr-operators.ro tests/cxx_nullptr_operators_rejected.cpp,$(TEST_OUT)/cxx-overloads/nullptr-operators.log)
	$(call CHECK_TEXT,nullptr does not support arithmetic operators,$(TEST_OUT)/cxx-overloads/nullptr-operators.log)
	$(call CHECK_TEXT,nullptr does not support integer operators,$(TEST_OUT)/cxx-overloads/nullptr-operators.log)
	$(call CHECK_TEXT,comparison requires arithmetic or pointer operands,$(TEST_OUT)/cxx-overloads/nullptr-operators.log)
	$(call CHECK_TEXT,nullptr can only be assigned to a pointer,$(TEST_OUT)/cxx-overloads/nullptr-operators.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/default-arguments.ro tests/cxx_default_arguments_rejected.cpp,$(TEST_OUT)/cxx-overloads/default-arguments.log)
	$(call CHECK_TEXT,parameter without a default follows a default argument,$(TEST_OUT)/cxx-overloads/default-arguments.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/default-redefinition.ro tests/cxx_default_redefinition_rejected.cpp,$(TEST_OUT)/cxx-overloads/default-redefinition.log)
	$(call CHECK_TEXT,redefinition of default argument for parameter 1,$(TEST_OUT)/cxx-overloads/default-redefinition.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/default-type.ro tests/cxx_default_type_rejected.cpp,$(TEST_OUT)/cxx-overloads/default-type.log)
	$(call CHECK_TEXT,default argument is incompatible with parameter 1,$(TEST_OUT)/cxx-overloads/default-type.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-overloads/default-function-pointer.ro tests/cxx_default_function_pointer_rejected.cpp,$(TEST_OUT)/cxx-overloads/default-function-pointer.log)
	$(call CHECK_TEXT,too few arguments to function call,$(TEST_OUT)/cxx-overloads/default-function-pointer.log)
	@echo "RCC++ overload resolution tests completed"

test-cxx-inline-aggregates: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-inline-aggregates)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/x86.ro \
		tests/cxx_inline_aggregate.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/x64.ro \
		tests/cxx_inline_aggregate.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/cxx-inline-aggregates/verify \
		tests/cxx_inline_aggregate_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/cxx-inline-aggregates/verify \
		$(TEST_OUT)/cxx-inline-aggregates/x86.ro \
		$(TEST_OUT)/cxx-inline-aggregates/x64.ro
ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/cxx-inline-aggregates/run-x86 \
		tests/cxx_value_init_run_test.c src/emit_ro.c src/utils.c
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/cxx-inline-aggregates/run-x86 \
		tests/cxx_value_init_run_test.c src/emit_ro.c src/utils.c
endif
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/cxx-inline-aggregates/run-x64 \
		tests/cxx_value_init_run_test.c src/emit_ro.c src/utils.c
ifeq ($(OS),Windows_NT)
	$(TEST_OUT)/cxx-inline-aggregates/run-x86 \
		$(TEST_OUT)/cxx-inline-aggregates/x86.ro --inspect-only
else
	$(TEST_OUT)/cxx-inline-aggregates/run-x86 \
		$(TEST_OUT)/cxx-inline-aggregates/x86.ro
endif
	$(TEST_OUT)/cxx-inline-aggregates/run-x64 \
		$(TEST_OUT)/cxx-inline-aggregates/x64.ro
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/cxx-inline-aggregates/c-empty.ro tests/c_empty_initializer_rejected.c,$(TEST_OUT)/cxx-inline-aggregates/c-empty.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/private.ro tests/cxx_private_member_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/private.log)
	$(GREP) -q "member 'value' is not accessible" \
		$(TEST_OUT)/cxx-inline-aggregates/private.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/arity.ro tests/cxx_constructor_arity_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/arity.log)
	$(GREP) -q "no safely lowerable constructor accepts 0 arguments" \
		$(TEST_OUT)/cxx-inline-aggregates/arity.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/const-reference.ro tests/cxx_const_reference_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/const-reference.log)
	$(GREP) -q "incompatible type for argument 1 to 'reference_test::mutable_reference'" \
		$(TEST_OUT)/cxx-inline-aggregates/const-reference.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/private-method.ro tests/cxx_private_method_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/private-method.log)
	$(GREP) -q "method 'secret' is not accessible" \
		$(TEST_OUT)/cxx-inline-aggregates/private-method.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/template-arity.ro tests/cxx_template_constructor_arity_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/template-arity.log)
	$(GREP) -q "no safely lowerable constructor accepts 1 argument" \
		$(TEST_OUT)/cxx-inline-aggregates/template-arity.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/versioned-rejected.ro tests/cxx_versioned_template_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/versioned-rejected.log)
	$(GREP) -q "function template 'unsafe_versioned' is not safely lowerable" \
		$(TEST_OUT)/cxx-inline-aggregates/versioned-rejected.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/auto-rejected.ro tests/cxx_auto_initializer_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/auto-rejected.log)
	$(GREP) -q "auto variable requires an initializer" \
		$(TEST_OUT)/cxx-inline-aggregates/auto-rejected.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/cleanup-copy.ro tests/cxx_cleanup_copy_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/cleanup-copy.log)
	$(GREP) -F -q "C++ scope-cleanup object requires a validated direct constructor" \
		$(TEST_OUT)/cxx-inline-aggregates/cleanup-copy.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/cleanup-flow.ro tests/cxx_cleanup_control_flow_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/cleanup-flow.log)
	$(GREP) -F -q "goto enters a C++ scope-cleanup object lifetime" \
		$(TEST_OUT)/cxx-inline-aggregates/cleanup-flow.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/cleanup-switch-scope.ro tests/cxx_cleanup_switch_scope_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/cleanup-switch-scope.log)
	$(GREP) -F -q "case label crosses C++ scope-cleanup object initialization" \
		$(TEST_OUT)/cxx-inline-aggregates/cleanup-switch-scope.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/external-destructor.ro \
		tests/cxx_external_destructor.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-release.ro \
		tests/cxx_unsafe_release_rejected.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/explicit-bool.ro \
		tests/cxx_unsafe_bool_rejected.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/bool-delegate.ro \
		tests/cxx_unsafe_bool_delegate_rejected.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-close-delegate.ro \
		tests/cxx_unsafe_close_delegate_rejected.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/custom-move.ro \
		tests/cxx_custom_move.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-inline-aggregates/unsafe-move-assignment.ro tests/cxx_unsafe_move_assignment_rejected.cpp,$(TEST_OUT)/cxx-inline-aggregates/unsafe-move-assignment.log)
	$(GREP) -q "no matching member overload for 'operator='" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-move-assignment.log
	@echo "RCC++ inline C ABI aggregate wrapper tests completed"

test-cxx-parser-recovery: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-parser-recovery)
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -Command "$$out='$(TEST_OUT)/cxx-parser-recovery/invalid.log'; $$err='$(TEST_OUT)/cxx-parser-recovery/invalid.err'; $$p=Start-Process -FilePath './rcc++.exe' -ArgumentList '--target','x86_64-unknown-rinos','-std=c++20','-c','-o','$(TEST_OUT)/cxx-parser-recovery/invalid.ro','tests/cxx_parser_recovery.cpp' -RedirectStandardOutput $$out -RedirectStandardError $$err -PassThru; Wait-Process -Id $$p.Id -Timeout 10 -ErrorAction SilentlyContinue | Out-Null; $$p.Refresh(); if (-not $$p.HasExited) { Stop-Process -Id $$p.Id -Force -ErrorAction SilentlyContinue; Write-Error 'C++ parser recovery timed out'; exit 1 }; Get-Content $$err | Add-Content $$out; if ($$p.ExitCode -eq 0) { Write-Error 'invalid C++ fixture unexpectedly compiled'; exit 1 }"
else
	@set +e; timeout 10s $(RCXX_TARGET) --target x86_64-unknown-rinos \
		-std=c++20 -c -o $(TEST_OUT)/cxx-parser-recovery/invalid.ro \
		tests/cxx_parser_recovery.cpp \
		>$(TEST_OUT)/cxx-parser-recovery/invalid.log 2>&1; status=$$?; \
		set -e; \
		if [ $$status -eq 0 ]; then \
			echo "invalid C++ fixture unexpectedly compiled"; exit 1; \
		fi; \
		if [ $$status -eq 124 ] || [ $$status -eq 139 ]; then \
			echo "C++ parser recovery timed out or crashed"; exit 1; \
		fi
endif
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -SimpleMatch 'expected ;' '$(TEST_OUT)/cxx-parser-recovery/invalid.log')) { exit 1 }"
	powershell -NoProfile -Command "if (Select-String -Quiet -SimpleMatch 'too many errors' '$(TEST_OUT)/cxx-parser-recovery/invalid.log') { exit 1 }"
else
	$(GREP) -q "expected ;" $(TEST_OUT)/cxx-parser-recovery/invalid.log
	! $(GREP) -q "too many errors" $(TEST_OUT)/cxx-parser-recovery/invalid.log
endif
	@echo "RCC++ namespace parser recovery test completed"

ifeq ($(OS),Windows_NT)
test-cxx-exceptions: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-exceptions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-exceptions/x86.s tests/cxx_exceptions_rejected.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exceptions/x86.o $(TEST_OUT)/cxx-exceptions/x86.s
	objdump -f $(TEST_OUT)/cxx-exceptions/x86.o > $(TEST_OUT)/cxx-exceptions/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-exceptions/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-exceptions/x64.s tests/cxx_exceptions_rejected.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-exceptions/x64.o $(TEST_OUT)/cxx-exceptions/x64.s
	objdump -f $(TEST_OUT)/cxx-exceptions/x64.o > $(TEST_OUT)/cxx-exceptions/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-exceptions/x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -c -o $(TEST_OUT)/cxx-exceptions/x86-verified.ro tests/cxx_exceptions_rejected.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -fverified-backend -c -o $(TEST_OUT)/cxx-exceptions/x64-verified.ro tests/cxx_exceptions_rejected.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-exceptions/invalid-order-x86.ro tests/cxx_exceptions_invalid.cpp,$(TEST_OUT)/cxx-exceptions/invalid-order-x86.log)
	$(GREP) -F -q "C++ catch-all handler must be the last handler" $(TEST_OUT)/cxx-exceptions/invalid-order-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-exceptions/invalid-order-x64.ro tests/cxx_exceptions_invalid.cpp,$(TEST_OUT)/cxx-exceptions/invalid-order-x64.log)
	$(GREP) -F -q "C++ catch-all handler must be the last handler" $(TEST_OUT)/cxx-exceptions/invalid-order-x64.log
	@echo "RCC++ exception propagation and nested handler object tests completed; runtime execution requires RinOS exception runtime"
else
test-cxx-exceptions: test-cxx-exceptions-posix
endif

test-cxx-exceptions-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-exceptions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-exceptions/x86.s tests/cxx_exceptions_rejected.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-exceptions/x64.s tests/cxx_exceptions_rejected.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exceptions/x86.o \
		$(TEST_OUT)/cxx-exceptions/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exceptions/x86-start.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-exceptions/x86 \
		$(TEST_OUT)/cxx-exceptions/x86-start.o \
		$(TEST_OUT)/cxx-exceptions/x86.o
	$(TEST_OUT)/cxx-exceptions/x86
	$(CC) -c -o $(TEST_OUT)/cxx-exceptions/x64.o \
		$(TEST_OUT)/cxx-exceptions/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-exceptions/x64-start.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-exceptions/x64 \
		$(TEST_OUT)/cxx-exceptions/x64-start.o \
		$(TEST_OUT)/cxx-exceptions/x64.o
	$(TEST_OUT)/cxx-exceptions/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -S \
		-o $(TEST_OUT)/cxx-exceptions/x86-o2.s tests/cxx_exceptions_rejected.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exceptions/x86-o2.o \
		$(TEST_OUT)/cxx-exceptions/x86-o2.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-exceptions/x86-o2 \
		$(TEST_OUT)/cxx-exceptions/x86-start.o \
		$(TEST_OUT)/cxx-exceptions/x86-o2.o
	$(TEST_OUT)/cxx-exceptions/x86-o2
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -S \
		-o $(TEST_OUT)/cxx-exceptions/x64-o2.s tests/cxx_exceptions_rejected.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-exceptions/x64-o2.o \
		$(TEST_OUT)/cxx-exceptions/x64-o2.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-exceptions/x64-o2 \
		$(TEST_OUT)/cxx-exceptions/x64-start.o \
		$(TEST_OUT)/cxx-exceptions/x64-o2.o
	$(TEST_OUT)/cxx-exceptions/x64-o2
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-exceptions/x86-verified.ro \
		tests/cxx_exceptions_rejected.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-exceptions/x64-verified.ro \
		tests/cxx_exceptions_rejected.cpp
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-exceptions/invalid-order-x86.ro \
		tests/cxx_exceptions_invalid.cpp \
		>$(TEST_OUT)/cxx-exceptions/invalid-order-x86.log 2>&1
	$(GREP) -q "C++ catch-all handler must be the last handler" \
		$(TEST_OUT)/cxx-exceptions/invalid-order-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-exceptions/invalid-order-x64.ro \
		tests/cxx_exceptions_invalid.cpp \
		>$(TEST_OUT)/cxx-exceptions/invalid-order-x64.log 2>&1
	$(GREP) -q "C++ catch-all handler must be the last handler" \
		$(TEST_OUT)/cxx-exceptions/invalid-order-x64.log
	@echo "RCC++ exception propagation and nested handler tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-object-exceptions: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-object-exceptions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-object-exceptions/x86.s tests/cxx_object_exceptions.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-object-exceptions/x86.o $(TEST_OUT)/cxx-object-exceptions/x86.s
	objdump -f $(TEST_OUT)/cxx-object-exceptions/x86.o > $(TEST_OUT)/cxx-object-exceptions/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-object-exceptions/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-object-exceptions/x64.s tests/cxx_object_exceptions.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-object-exceptions/x64.o $(TEST_OUT)/cxx-object-exceptions/x64.s
	objdump -f $(TEST_OUT)/cxx-object-exceptions/x64.o > $(TEST_OUT)/cxx-object-exceptions/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-object-exceptions/x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -c -o $(TEST_OUT)/cxx-object-exceptions/x86-verified.ro tests/cxx_object_exceptions.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -fverified-backend -c -o $(TEST_OUT)/cxx-object-exceptions/x64-verified.ro tests/cxx_object_exceptions.cpp
	@echo "RCC++ trivially-copyable object exception tests completed; runtime execution requires RinOS exception runtime"
else
test-cxx-object-exceptions: test-cxx-object-exceptions-posix
endif

test-cxx-object-exceptions-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-object-exceptions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-object-exceptions/x86.s tests/cxx_object_exceptions.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-object-exceptions/x86.o \
		$(TEST_OUT)/cxx-object-exceptions/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-object-exceptions/x86-start.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-object-exceptions/x86 \
		$(TEST_OUT)/cxx-object-exceptions/x86-start.o \
		$(TEST_OUT)/cxx-object-exceptions/x86.o
	$(TEST_OUT)/cxx-object-exceptions/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-object-exceptions/x64.s tests/cxx_object_exceptions.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-object-exceptions/x64.o \
		$(TEST_OUT)/cxx-object-exceptions/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-object-exceptions/x64-start.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-object-exceptions/x64 \
		$(TEST_OUT)/cxx-object-exceptions/x64-start.o \
		$(TEST_OUT)/cxx-object-exceptions/x64.o
	$(TEST_OUT)/cxx-object-exceptions/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-object-exceptions/x86-verified.ro \
		tests/cxx_object_exceptions.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-object-exceptions/x64-verified.ro \
		tests/cxx_object_exceptions.cpp
	@echo "RCC++ trivially-copyable object exception tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-cross-library-exceptions: $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/cxx-cross-library-exceptions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.s tests/cxx_exception_provider.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.s tests/cxx_exception_consumer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.s
	objdump -f $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.o > $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86-arch.log
	objdump -f $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.o > $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.s tests/cxx_exception_provider.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.s tests/cxx_exception_consumer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.s
	objdump -f $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.o > $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64-arch.log
	objdump -f $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.o > $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -c -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.ro tests/cxx_exception_provider.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -c -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.ro tests/cxx_exception_consumer.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -fverified-backend -c -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.ro tests/cxx_exception_provider.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 -fverified-backend -c -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.ro tests/cxx_exception_consumer.cpp
	$(RLD_TARGET) --target i686-unknown-rinos --shared --emit-unsigned-v3 --dep rincrt.rll --import rin_cpp_exception_throw_object=rincrt.rll@function -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.rll $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --shared --emit-unsigned-v3 --dep rincrt.rll --import rin_cpp_exception_throw_object=rincrt.rll@function -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.rll $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.ro
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 --dep provider-x86.rll --dep rincrt.rll --import cxx_exception_provider_throw=provider-x86.rll@function --import setjmp=rincrt.rll@function --import rin_cpp_exception_install=rincrt.rll@function --import rin_cpp_exception_leave=rincrt.rll@function --import rin_cpp_exception_rethrow_frame=rincrt.rll@function --import rin_cpp_exception_release_frame=rincrt.rll@function -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.rin $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 --dep provider-x64.rll --dep rincrt.rll --import cxx_exception_provider_throw=provider-x64.rll@function --import setjmp=rincrt.rll@function --import rin_cpp_exception_install=rincrt.rll@function --import rin_cpp_exception_leave=rincrt.rll@function --import rin_cpp_exception_rethrow_frame=rincrt.rll@function --import rin_cpp_exception_release_frame=rincrt.rll@function -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.rin $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.ro
	$(RINVALIDATE) --kind library --arch x86 --allow-unsigned $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.rll
	$(RINVALIDATE) --kind library --arch x86_64 --allow-unsigned $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.rll
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.rin
	@echo "RCC++ cross-translation-unit and RLL exception ABI object/image tests completed; runtime execution requires RinOS exception runtime"
else
test-cxx-cross-library-exceptions: test-cxx-cross-library-exceptions-posix
endif

test-cxx-cross-library-exceptions-posix: $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/cxx-cross-library-exceptions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.s \
		tests/cxx_exception_provider.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.s \
		tests/cxx_exception_consumer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.o \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.o \
		$(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.s
	$(OBJCOPY) --redefine-sym _rcc_entry=provider_rcc_entry \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.o
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-library-exceptions/start-x86.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/native-x86 \
		$(TEST_OUT)/cxx-cross-library-exceptions/start-x86.o \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.o \
		$(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.o
	$(TEST_OUT)/cxx-cross-library-exceptions/native-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.s \
		tests/cxx_exception_provider.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.s \
		tests/cxx_exception_consumer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.o \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.o \
		$(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.s
	$(OBJCOPY) --redefine-sym _rcc_entry=provider_rcc_entry \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.o
	$(CC) -c -o $(TEST_OUT)/cxx-cross-library-exceptions/start-x64.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/native-x64 \
		$(TEST_OUT)/cxx-cross-library-exceptions/start-x64.o \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.o \
		$(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.o
	$(TEST_OUT)/cxx-cross-library-exceptions/native-x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.ro \
		tests/cxx_exception_provider.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.ro \
		tests/cxx_exception_consumer.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.ro \
		tests/cxx_exception_provider.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -c \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.ro \
		tests/cxx_exception_consumer.cpp
	$(RLD_TARGET) --target i686-unknown-rinos --shared --emit-unsigned-v3 \
		--dep rincrt.rll \
		--import rin_cpp_exception_throw_object=rincrt.rll@function \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.rll \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --shared --emit-unsigned-v3 \
		--dep rincrt.rll \
		--import rin_cpp_exception_throw_object=rincrt.rll@function \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.rll \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.ro
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		--dep provider-x86.rll --dep rincrt.rll \
		--import cxx_exception_provider_throw=provider-x86.rll@function \
		--import setjmp=rincrt.rll@function \
		--import rin_cpp_exception_install=rincrt.rll@function \
		--import rin_cpp_exception_leave=rincrt.rll@function \
		--import rin_cpp_exception_rethrow_frame=rincrt.rll@function \
		--import rin_cpp_exception_release_frame=rincrt.rll@function \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.rin \
		$(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		--dep provider-x64.rll --dep rincrt.rll \
		--import cxx_exception_provider_throw=provider-x64.rll@function \
		--import setjmp=rincrt.rll@function \
		--import rin_cpp_exception_install=rincrt.rll@function \
		--import rin_cpp_exception_leave=rincrt.rll@function \
		--import rin_cpp_exception_rethrow_frame=rincrt.rll@function \
		--import rin_cpp_exception_release_frame=rincrt.rll@function \
		-o $(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.rin \
		$(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.ro
	$(RINVALIDATE) --kind library --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x86.rll
	$(RINVALIDATE) --kind library --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-cross-library-exceptions/provider-x64.rll
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/cxx-cross-library-exceptions/consumer-x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/cxx-cross-library-exceptions/consumer-x64.rin
	@echo "RCC++ cross-translation-unit and RLL exception ABI tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-cross-translation-unit-virtual: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-cross-translation-unit-virtual)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.s tests/cxx_virtual_provider.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86.s tests/cxx_virtual_consumer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86.o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86.s
	objdump -f $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.o > $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86-arch.log
	objdump -f $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86.o > $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.s tests/cxx_virtual_provider.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.s tests/cxx_virtual_consumer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.s
	objdump -f $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.o > $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64-arch.log
	objdump -f $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.o > $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64-arch.log
	$(OBJCOPY) --redefine-sym _rcc_entry=provider_virtual_rcc_entry $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.o
	$(OBJCOPY) --redefine-sym main=rcc_generated_main $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.o
	$(CC) $(CFLAGS) -Wl,--allow-multiple-definition -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/x64-host tests/cxx_language_core_host.c $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.o
	$(TEST_OUT)/cxx-cross-translation-unit-virtual/x64-host
	@echo "RCC++ cross-translation-unit virtual/ODR tests completed"
else
test-cxx-cross-translation-unit-virtual: test-cxx-cross-translation-unit-virtual-posix
endif

test-cxx-cross-translation-unit-virtual-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-cross-translation-unit-virtual)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.s \
		tests/cxx_virtual_provider.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86.s \
		tests/cxx_virtual_consumer.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.o \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86.o \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86.s
	$(OBJCOPY) --redefine-sym _rcc_entry=provider_virtual_rcc_entry \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.o
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-cross-translation-unit-virtual/native-x86 \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/start-x86.o \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x86.o \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x86.o
	$(TEST_OUT)/cxx-cross-translation-unit-virtual/native-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.s \
		tests/cxx_virtual_provider.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.s \
		tests/cxx_virtual_consumer.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.o \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.o \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.s
	$(OBJCOPY) --redefine-sym _rcc_entry=provider_virtual_rcc_entry \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.o
	$(CC) -c -o $(TEST_OUT)/cxx-cross-translation-unit-virtual/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-cross-translation-unit-virtual/native-x64 \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/start-x64.o \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/provider-x64.o \
		$(TEST_OUT)/cxx-cross-translation-unit-virtual/consumer-x64.o
	$(TEST_OUT)/cxx-cross-translation-unit-virtual/native-x64
	@echo "RCC++ cross-translation-unit virtual/ODR tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-exception-cleanup: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-exception-cleanup)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-exception-cleanup/x86.s tests/cxx_exception_cleanup.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exception-cleanup/x86.o $(TEST_OUT)/cxx-exception-cleanup/x86.s
	objdump -f $(TEST_OUT)/cxx-exception-cleanup/x86.o > $(TEST_OUT)/cxx-exception-cleanup/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-exception-cleanup/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-exception-cleanup/x64.s tests/cxx_exception_cleanup.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-exception-cleanup/x64.o $(TEST_OUT)/cxx-exception-cleanup/x64.s
	objdump -f $(TEST_OUT)/cxx-exception-cleanup/x64.o > $(TEST_OUT)/cxx-exception-cleanup/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-exception-cleanup/x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-exception-cleanup/call-x86.s tests/cxx_exception_cleanup_call_rejected.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exception-cleanup/call-x86.o $(TEST_OUT)/cxx-exception-cleanup/call-x86.s
	objdump -f $(TEST_OUT)/cxx-exception-cleanup/call-x86.o > $(TEST_OUT)/cxx-exception-cleanup/call-x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-exception-cleanup/call-x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-exception-cleanup/call-x64.s tests/cxx_exception_cleanup_call_rejected.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-exception-cleanup/call-x64.o $(TEST_OUT)/cxx-exception-cleanup/call-x64.s
	objdump -f $(TEST_OUT)/cxx-exception-cleanup/call-x64.o > $(TEST_OUT)/cxx-exception-cleanup/call-x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-exception-cleanup/call-x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -c -o $(TEST_OUT)/cxx-exception-cleanup/x86.ro tests/cxx_exception_cleanup.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -fverified-backend -c -o $(TEST_OUT)/cxx-exception-cleanup/x64.ro tests/cxx_exception_cleanup.cpp
	@echo "RCC++ cross-call exception cleanup registration object tests completed; runtime execution requires RinOS exception runtime"
else
test-cxx-exception-cleanup: test-cxx-exception-cleanup-posix
endif

test-cxx-exception-cleanup-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-exception-cleanup)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-exception-cleanup/x86.s \
		tests/cxx_exception_cleanup.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exception-cleanup/x86.o \
		$(TEST_OUT)/cxx-exception-cleanup/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exception-cleanup/x86-start.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-exception-cleanup/x86 \
		$(TEST_OUT)/cxx-exception-cleanup/x86-start.o \
		$(TEST_OUT)/cxx-exception-cleanup/x86.o
	$(TEST_OUT)/cxx-exception-cleanup/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-exception-cleanup/x64.s \
		tests/cxx_exception_cleanup.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-exception-cleanup/x64.o \
		$(TEST_OUT)/cxx-exception-cleanup/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-exception-cleanup/x64-start.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-exception-cleanup/x64 \
		$(TEST_OUT)/cxx-exception-cleanup/x64-start.o \
		$(TEST_OUT)/cxx-exception-cleanup/x64.o
	$(TEST_OUT)/cxx-exception-cleanup/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-exception-cleanup/call-x86.s \
		tests/cxx_exception_cleanup_call_rejected.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exception-cleanup/call-x86.o \
		$(TEST_OUT)/cxx-exception-cleanup/call-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-exception-cleanup/call-x86-start.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-exception-cleanup/call-x86 \
		$(TEST_OUT)/cxx-exception-cleanup/call-x86-start.o \
		$(TEST_OUT)/cxx-exception-cleanup/call-x86.o
	$(TEST_OUT)/cxx-exception-cleanup/call-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-exception-cleanup/call-x64.s \
		tests/cxx_exception_cleanup_call_rejected.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-exception-cleanup/call-x64.o \
		$(TEST_OUT)/cxx-exception-cleanup/call-x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-exception-cleanup/call-x64-start.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-exception-cleanup/call-x64 \
		$(TEST_OUT)/cxx-exception-cleanup/call-x64-start.o \
		$(TEST_OUT)/cxx-exception-cleanup/call-x64.o
	$(TEST_OUT)/cxx-exception-cleanup/call-x64
	@echo "RCC++ cross-call exception cleanup registration tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-nontrivial-object-exceptions: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-nontrivial-object-exceptions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.s tests/cxx_nontrivial_object_exceptions.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.s
	objdump -f $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.o > $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.s tests/cxx_nontrivial_object_exceptions.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.s
	objdump -f $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.o > $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -c -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.ro tests/cxx_nontrivial_object_exceptions.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -fverified-backend -c -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.ro tests/cxx_nontrivial_object_exceptions.cpp
	@echo "RCC++ non-trivial object exception ownership object tests completed; runtime execution requires RinOS exception runtime"
else
test-cxx-nontrivial-object-exceptions: test-cxx-nontrivial-object-exceptions-posix
endif

test-cxx-nontrivial-object-exceptions-posix: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-nontrivial-object-exceptions)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.s \
		tests/cxx_nontrivial_object_exceptions.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.s \
		tests/cxx_nontrivial_object_exceptions.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.o \
		$(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.s
	$(CC) -c -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.o \
		$(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86-start.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -c -o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64-start.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x86 \
		$(TEST_OUT)/cxx-nontrivial-object-exceptions/x86-start.o \
		$(TEST_OUT)/cxx-nontrivial-object-exceptions/x86.o
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-nontrivial-object-exceptions/x64 \
		$(TEST_OUT)/cxx-nontrivial-object-exceptions/x64-start.o \
		$(TEST_OUT)/cxx-nontrivial-object-exceptions/x64.o
	$(TEST_OUT)/cxx-nontrivial-object-exceptions/x86
	$(TEST_OUT)/cxx-nontrivial-object-exceptions/x64
	@echo "RCC++ non-trivial object exception ownership tests completed"

test-cxx-const-member-overload: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-const-member-overload)
ifeq ($(OS),Windows_NT)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-const-member-overload/x86.s \
		tests/cxx_const_member_overload.cpp
	$(GREP) -F -q '_ZNK14QualifierProbe4readEv' $(TEST_OUT)/cxx-const-member-overload/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-const-member-overload/x86.o \
		$(TEST_OUT)/cxx-const-member-overload/x86.s
	objdump -f $(TEST_OUT)/cxx-const-member-overload/x86.o > $(TEST_OUT)/cxx-const-member-overload/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-const-member-overload/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-const-member-overload/x64.s \
		tests/cxx_const_member_overload.cpp
	$(GREP) -F -q '_ZNK14QualifierProbe4readEv' $(TEST_OUT)/cxx-const-member-overload/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-const-member-overload/x64.o \
		$(TEST_OUT)/cxx-const-member-overload/x64.s
	$(OBJCOPY) --redefine-sym main=rcc_test_main \
		$(TEST_OUT)/cxx-const-member-overload/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-const-member-overload/x64-host \
		tests/cxx_main_host.c $(TEST_OUT)/cxx-const-member-overload/x64.o
	$(TEST_OUT)/cxx-const-member-overload/x64-host
else
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-const-member-overload/x86.s \
		tests/cxx_const_member_overload.cpp
	$(GREP) -F -q '_ZNK14QualifierProbe4readEv' $(TEST_OUT)/cxx-const-member-overload/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-const-member-overload/x86.o \
		$(TEST_OUT)/cxx-const-member-overload/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-const-member-overload/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-const-member-overload/x86 \
		$(TEST_OUT)/cxx-const-member-overload/start-x86.o \
		$(TEST_OUT)/cxx-const-member-overload/x86.o
	$(TEST_OUT)/cxx-const-member-overload/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-const-member-overload/x64.s \
		tests/cxx_const_member_overload.cpp
	$(GREP) -F -q '_ZNK14QualifierProbe4readEv' $(TEST_OUT)/cxx-const-member-overload/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-const-member-overload/x64.o \
		$(TEST_OUT)/cxx-const-member-overload/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-const-member-overload/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-const-member-overload/x64 \
		$(TEST_OUT)/cxx-const-member-overload/start-x64.o \
		$(TEST_OUT)/cxx-const-member-overload/x64.o
	$(TEST_OUT)/cxx-const-member-overload/x64
endif
	@echo "RCC++ const member overload tests completed"

test-cxx-ref-qualified-overload: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-ref-qualified-overload)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-ref-qualified-overload/x86.s \
		tests/cxx_ref_qualified_overload.cpp
	$(GREP) -F -q '_ZNR17RefQualifiedProbe6selectEv' $(TEST_OUT)/cxx-ref-qualified-overload/x86.s
	$(GREP) -F -q '_ZNKR17RefQualifiedProbe6selectEv' $(TEST_OUT)/cxx-ref-qualified-overload/x86.s
	$(GREP) -F -q '_ZNO17RefQualifiedProbe6selectEv' $(TEST_OUT)/cxx-ref-qualified-overload/x86.s
	$(GREP) -F -q '_ZNKO17RefQualifiedProbe6selectEv' $(TEST_OUT)/cxx-ref-qualified-overload/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-ref-qualified-overload/x86.o \
		$(TEST_OUT)/cxx-ref-qualified-overload/x86.s
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-ref-qualified-overload/x64.s \
		tests/cxx_ref_qualified_overload.cpp
	$(GREP) -F -q '_ZNR17RefQualifiedProbe6selectEv' $(TEST_OUT)/cxx-ref-qualified-overload/x64.s
	$(GREP) -F -q '_ZNKR17RefQualifiedProbe6selectEv' $(TEST_OUT)/cxx-ref-qualified-overload/x64.s
	$(GREP) -F -q '_ZNO17RefQualifiedProbe6selectEv' $(TEST_OUT)/cxx-ref-qualified-overload/x64.s
	$(GREP) -F -q '_ZNKO17RefQualifiedProbe6selectEv' $(TEST_OUT)/cxx-ref-qualified-overload/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-ref-qualified-overload/x64.o \
		$(TEST_OUT)/cxx-ref-qualified-overload/x64.s
ifeq ($(OS),Windows_NT)
	objcopy --redefine-sym main=rcc_test_main \
		$(TEST_OUT)/cxx-ref-qualified-overload/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-ref-qualified-overload/x64-host \
		tests/cxx_main_host.c $(TEST_OUT)/cxx-ref-qualified-overload/x64.o
	$(TEST_OUT)/cxx-ref-qualified-overload/x64-host
else
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-ref-qualified-overload/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-ref-qualified-overload/x86 \
		$(TEST_OUT)/cxx-ref-qualified-overload/start-x86.o \
		$(TEST_OUT)/cxx-ref-qualified-overload/x86.o
	$(TEST_OUT)/cxx-ref-qualified-overload/x86
	$(CC) -c -o $(TEST_OUT)/cxx-ref-qualified-overload/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-ref-qualified-overload/x64 \
		$(TEST_OUT)/cxx-ref-qualified-overload/start-x64.o \
		$(TEST_OUT)/cxx-ref-qualified-overload/x64.o
	$(TEST_OUT)/cxx-ref-qualified-overload/x64
endif
	@echo "C++ ref-qualified member overload tests completed"

test-cxx-ref-qualified-overload-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-ref-qualified-overload-invalid)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.ro tests/cxx_ref_qualified_mixed_invalid.cpp,$(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.ro tests/cxx_ref_qualified_mixed_invalid.cpp,$(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.log)
	$(GREP) -F -q 'only one declaration has a ref-qualifier' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.log
	$(GREP) -F -q 'only one declaration has a ref-qualifier' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.log
	$(GREP) -F -q 'marked override but does not override a base class method' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.log
	$(GREP) -F -q 'marked override but does not override a base class method' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.log
	$(GREP) -F -q 'a static member function cannot have a ref-qualifier' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.log
	$(GREP) -F -q 'a static member function cannot have a ref-qualifier' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.log
	$(GREP) -F -q 'a constructor or destructor cannot have cv/ref qualifiers' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.log
	$(GREP) -F -q 'a constructor or destructor cannot have cv/ref qualifiers' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.log
	$(GREP) -F -q 'a friend function cannot have member cv/ref qualifiers' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.log
	$(GREP) -F -q 'a friend function cannot have member cv/ref qualifiers' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.log
	$(GREP) -F -q 'a static member function cannot have a volatile qualifier' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.log
	$(GREP) -F -q 'a static member function cannot have a volatile qualifier' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.log
	$(GREP) -F -q 'a constructor or destructor cannot have cv/ref qualifiers' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x86.log
	$(GREP) -F -q 'a constructor or destructor cannot have cv/ref qualifiers' $(TEST_OUT)/cxx-ref-qualified-overload-invalid/x64.log
	@echo "C++ mixed ref-qualifier diagnostics completed"

test-cxx-volatile-member-overload-invalid: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-volatile-member-overload-invalid)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-volatile-member-overload-invalid/x86.ro tests/cxx_volatile_override_invalid.cpp,$(TEST_OUT)/cxx-volatile-member-overload-invalid/x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/cxx-volatile-member-overload-invalid/x64.ro tests/cxx_volatile_override_invalid.cpp,$(TEST_OUT)/cxx-volatile-member-overload-invalid/x64.log)
	$(GREP) -F -q 'marked override but does not override a base class method' $(TEST_OUT)/cxx-volatile-member-overload-invalid/x86.log
	$(GREP) -F -q 'marked override but does not override a base class method' $(TEST_OUT)/cxx-volatile-member-overload-invalid/x64.log
	@echo "C++ volatile override diagnostics completed"

test-cxx-volatile-member-overload: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-volatile-member-overload)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-volatile-member-overload/x86.s \
		tests/cxx_volatile_member_overload.cpp
	$(GREP) -F -q '_ZNV19VolatileMemberProbe4readEv' $(TEST_OUT)/cxx-volatile-member-overload/x86.s
	$(GREP) -F -q '_ZNVK19VolatileMemberProbe4readEv' $(TEST_OUT)/cxx-volatile-member-overload/x86.s
	$(GREP) -F -q '_ZNVR19VolatileMemberProbe8read_refEv' $(TEST_OUT)/cxx-volatile-member-overload/x86.s
	$(GREP) -F -q '_ZNVKO19VolatileMemberProbe8read_refEv' $(TEST_OUT)/cxx-volatile-member-overload/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-volatile-member-overload/x86.o \
		$(TEST_OUT)/cxx-volatile-member-overload/x86.s
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-volatile-member-overload/x64.s \
		tests/cxx_volatile_member_overload.cpp
	$(GREP) -F -q '_ZNV19VolatileMemberProbe4readEv' $(TEST_OUT)/cxx-volatile-member-overload/x64.s
	$(GREP) -F -q '_ZNVK19VolatileMemberProbe4readEv' $(TEST_OUT)/cxx-volatile-member-overload/x64.s
	$(GREP) -F -q '_ZNVR19VolatileMemberProbe8read_refEv' $(TEST_OUT)/cxx-volatile-member-overload/x64.s
	$(GREP) -F -q '_ZNVKO19VolatileMemberProbe8read_refEv' $(TEST_OUT)/cxx-volatile-member-overload/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-volatile-member-overload/x64.o \
		$(TEST_OUT)/cxx-volatile-member-overload/x64.s
ifeq ($(OS),Windows_NT)
	objcopy --redefine-sym main=rcc_test_main \
		$(TEST_OUT)/cxx-volatile-member-overload/x64.o
	$(CC) $(CFLAGS) -o $(TEST_OUT)/cxx-volatile-member-overload/x64-host \
		tests/cxx_main_host.c $(TEST_OUT)/cxx-volatile-member-overload/x64.o
	$(TEST_OUT)/cxx-volatile-member-overload/x64-host
else
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-volatile-member-overload/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-volatile-member-overload/x86 \
		$(TEST_OUT)/cxx-volatile-member-overload/start-x86.o \
		$(TEST_OUT)/cxx-volatile-member-overload/x86.o
	$(TEST_OUT)/cxx-volatile-member-overload/x86
	$(CC) -c -o $(TEST_OUT)/cxx-volatile-member-overload/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-volatile-member-overload/x64 \
		$(TEST_OUT)/cxx-volatile-member-overload/start-x64.o \
		$(TEST_OUT)/cxx-volatile-member-overload/x64.o
	$(TEST_OUT)/cxx-volatile-member-overload/x64
endif
	@echo "C++ volatile member overload tests completed"

test-tool-relative-includes: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/tool-relative/cwd)
	cd $(TEST_OUT)/tool-relative/cwd && \
		$(abspath $(RCC_TARGET)) --target i686-unknown-rinos -c \
		-o c-x86.ro $(abspath tests/tool_relative_include.c)
	cd $(TEST_OUT)/tool-relative/cwd && \
		$(abspath $(RCXX_TARGET)) --target x86_64-unknown-rinos \
		-std=c++20 -c -o cxx-x64.ro \
		$(abspath tests/tool_relative_include.cpp)
	$(call MKDIR_P,$(TEST_OUT)/tool-relative/install/bin)
	$(call MKDIR_P,$(TEST_OUT)/tool-relative/install/include)
	$(call MKDIR_P,$(TEST_OUT)/tool-relative/install/cwd)
	cp $(RCC_TARGET) $(RCXX_TARGET) $(TEST_OUT)/tool-relative/install/bin/
	cp -R include/rcc $(TEST_OUT)/tool-relative/install/include/
	cd $(TEST_OUT)/tool-relative/install/cwd && \
		../bin/rcc --target x86_64-unknown-rinos -c \
		-o installed-c-x64.ro $(abspath tests/tool_relative_include.c)
	cd $(TEST_OUT)/tool-relative/install/cwd && \
		../bin/rcc++ --target i686-unknown-rinos -std=c++20 -c \
		-o installed-cxx-x86.ro \
		$(abspath tests/tool_relative_include.cpp)
	@echo "RCC/RCC++ executable-relative include tests completed"

test-preprocessor-continuation: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-DRCC_CONTINUATION_LEFT -DRCC_CONTINUATION_RIGHT \
		-o $(TEST_OUT)/preprocessor-continuation-x86.ro \
		tests/preprocessor_continuation.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-DRCC_CONTINUATION_LEFT -DRCC_CONTINUATION_RIGHT \
		-o $(TEST_OUT)/preprocessor-continuation-x64.ro \
		tests/preprocessor_continuation.c
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-literal-x86.ro \
		tests/preprocessor_literal.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-literal-x64.ro \
		tests/preprocessor_literal.c
	@echo "C17 backslash-newline splicing tests completed"

test-preprocessor-if: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-if-x86.ro \
		tests/preprocessor_if.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-if-x64.ro \
		tests/preprocessor_if.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/invalid-preprocessor-if-x86.ro tests/invalid_preprocessor_if.c,$(TEST_OUT)/invalid-preprocessor-if-x86.log)
	$(GREP) -F -q 'invalid #if expression' $(TEST_OUT)/invalid-preprocessor-if-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/invalid-preprocessor-if-x64.ro tests/invalid_preprocessor_if.c,$(TEST_OUT)/invalid-preprocessor-if-x64.log)
	$(GREP) -F -q 'invalid #if expression' $(TEST_OUT)/invalid-preprocessor-if-x64.log
	@echo "C17 #if integer constant expression tests completed"

test-preprocessor-line: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -E tests/preprocessor_line.c > \
		$(TEST_OUT)/preprocessor-line.i
	$(GREP) -F -q '#line 77 "rcc-line-marker.c"' \
		$(TEST_OUT)/preprocessor-line.i
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-line-x86.ro tests/preprocessor_line.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-line-x64.ro tests/preprocessor_line.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-line-cxx-x86.ro tests/preprocessor_line.c
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-line-cxx-x64.ro tests/preprocessor_line.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/invalid-preprocessor-line-x86.ro tests/invalid_preprocessor_line.c,$(TEST_OUT)/invalid-preprocessor-line-x86.log)
	$(GREP) -F -q 'expected a positive line number' \
		$(TEST_OUT)/invalid-preprocessor-line-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/invalid-preprocessor-line-x64.ro tests/invalid_preprocessor_line.c,$(TEST_OUT)/invalid-preprocessor-line-x64.log)
	$(GREP) -F -q 'expected a positive line number' \
		$(TEST_OUT)/invalid-preprocessor-line-x64.log
	@echo "C17/C++20 #line marker tests completed"

test-preprocessor-include: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -E -Itests tests/preprocessor_include.c > \
		$(TEST_OUT)/preprocessor-include-c.i
	$(GREP) -F -q 'int preprocessor_include_c =' \
		$(TEST_OUT)/preprocessor-include-c.i
	$(GREP) -F -q '17 + 17' $(TEST_OUT)/preprocessor-include-c.i
	$(RCXX_TARGET) -E -Itests tests/preprocessor_include.cpp > \
		$(TEST_OUT)/preprocessor-include-cxx.i
	$(GREP) -F -q 'constexpr int preprocessor_include_cxx =' \
		$(TEST_OUT)/preprocessor-include-cxx.i
	$(GREP) -F -q '17 + 17' $(TEST_OUT)/preprocessor-include-cxx.i
	$(RCC_TARGET) --target i686-unknown-rinos -Itests -c \
		-o $(TEST_OUT)/preprocessor-include-c-x86.ro \
		tests/preprocessor_include.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -Itests -c \
		-o $(TEST_OUT)/preprocessor-include-c-x64.ro \
		tests/preprocessor_include.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -Itests -c \
		-o $(TEST_OUT)/preprocessor-include-cxx-x86.ro \
		tests/preprocessor_include.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -Itests -c \
		-o $(TEST_OUT)/preprocessor-include-cxx-x64.ro \
		tests/preprocessor_include.cpp
	$(RCC_TARGET) -E -Itests tests/preprocessor_pragma_once.c > \
		$(TEST_OUT)/preprocessor-pragma-once-c.i
	$(GREP) -F -q 'int preprocessor_pragma_once_value(void)' \
		$(TEST_OUT)/preprocessor-pragma-once-c.i
	$(call CHECK_COUNT,int rcc_pragma_once_global = 19;,$(TEST_OUT)/preprocessor-pragma-once-c.i,1)
	$(RCXX_TARGET) -std=c++20 -E -Itests tests/preprocessor_pragma_once.cpp > \
		$(TEST_OUT)/preprocessor-pragma-once-cxx.i
	$(GREP) -F -q 'constexpr int preprocessor_pragma_once_value()' \
		$(TEST_OUT)/preprocessor-pragma-once-cxx.i
	$(call CHECK_COUNT,int rcc_pragma_once_global = 19;,$(TEST_OUT)/preprocessor-pragma-once-cxx.i,1)
	$(RCC_TARGET) --target i686-unknown-rinos -Itests -c \
		-o $(TEST_OUT)/preprocessor-pragma-once-c-x86.ro \
		tests/preprocessor_pragma_once.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -Itests -c \
		-o $(TEST_OUT)/preprocessor-pragma-once-c-x64.ro \
		tests/preprocessor_pragma_once.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -Itests -c \
		-o $(TEST_OUT)/preprocessor-pragma-once-cxx-x86.ro \
		tests/preprocessor_pragma_once.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -Itests -c \
		-o $(TEST_OUT)/preprocessor-pragma-once-cxx-x64.ro \
		tests/preprocessor_pragma_once.cpp
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -Itests -c -o $(TEST_OUT)/invalid-preprocessor-include.ro tests/invalid_preprocessor_include.c,$(TEST_OUT)/invalid-preprocessor-include.log)
	$(GREP) -F -q 'unexpected tokens after #include path' \
		$(TEST_OUT)/invalid-preprocessor-include.log
	@echo "C17/C++20 macro-expanded #include tests completed"

test-preprocessor-include-next: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -E -Itests/include -Itests/myinc \
		tests/preprocessor_include_next.c > \
		$(TEST_OUT)/preprocessor-include-next-c.i
	$(GREP) -F -q 'return 11 + 31' \
		$(TEST_OUT)/preprocessor-include-next-c.i
	$(RCXX_TARGET) -std=c++20 -E -Itests/include -Itests/myinc \
		tests/preprocessor_include_next.cpp > \
		$(TEST_OUT)/preprocessor-include-next-cxx.i
	$(GREP) -F -q 'return 11 + 31' \
		$(TEST_OUT)/preprocessor-include-next-cxx.i
	$(RCC_TARGET) --target i686-unknown-rinos -Itests/include -Itests/myinc -c \
		-o $(TEST_OUT)/preprocessor-include-next-c-x86.ro \
		tests/preprocessor_include_next.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -Itests/include -Itests/myinc -c \
		-o $(TEST_OUT)/preprocessor-include-next-c-x64.ro \
		tests/preprocessor_include_next.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-Itests/include -Itests/myinc -c \
		-o $(TEST_OUT)/preprocessor-include-next-cxx-x86.ro \
		tests/preprocessor_include_next.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-Itests/include -Itests/myinc -c \
		-o $(TEST_OUT)/preprocessor-include-next-cxx-x64.ro \
		tests/preprocessor_include_next.cpp
	@echo "C17/C++20 #include_next tests completed"

test-preprocessor-line-macro: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -E tests/preprocessor_line_macro.c > \
		$(TEST_OUT)/preprocessor-line-macro.i
	$(GREP) -F -q '#line 77 "rcc-macro-line.c"' \
		$(TEST_OUT)/preprocessor-line-macro.i
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/preprocessor-line-macro-c-x86.s \
		tests/preprocessor_line_macro.c
	$(GREP) -F -q '0x4d, 0x00, 0x00, 0x00' \
		$(TEST_OUT)/preprocessor-line-macro-c-x86.s
	$(GREP) -F -q '0x72, 0x63, 0x63, 0x2d, 0x6d, 0x61, 0x63, 0x72' \
		$(TEST_OUT)/preprocessor-line-macro-c-x86.s
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/preprocessor-line-macro-c-x64.s \
		tests/preprocessor_line_macro.c
	$(GREP) -F -q '0x4d, 0x00, 0x00, 0x00' \
		$(TEST_OUT)/preprocessor-line-macro-c-x64.s
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-line-macro-cxx-x86.ro \
		tests/preprocessor_line_macro.c
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/preprocessor-line-macro-cxx-x64.ro \
		tests/preprocessor_line_macro.c
	@echo "C17/C++20 macro-expanded #line tests completed"

test-preprocessor-operators: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -E tests/preprocessor_operators.c > $(TEST_OUT)/preprocessor-operators.i
	$(GREP) -F -q 'raw_string[] = "WORD + 1"' $(TEST_OUT)/preprocessor-operators.i
	$(GREP) -F -q 'expanded_string[] = "42 + 1"' $(TEST_OUT)/preprocessor-operators.i
	$(GREP) -F -q 'variadic_string[] = "one, two"' $(TEST_OUT)/preprocessor-operators.i
	$(GREP) -F -q 'pasted_identifier = 77' $(TEST_OUT)/preprocessor-operators.i
	$(GREP) -F -q 'pasted_number = 123' $(TEST_OUT)/preprocessor-operators.i
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-operators-x86.ro \
		tests/preprocessor_operators.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-operators-x64.ro \
		tests/preprocessor_operators.c
	@echo "C17 #/## replacement-list operator tests completed"

test-preprocessor-va-opt: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -E tests/preprocessor_va_opt.c > $(TEST_OUT)/preprocessor-va-opt.i
	$(GREP) -F -q 'optional_empty = (7 );' $(TEST_OUT)/preprocessor-va-opt.i
	$(GREP) -F -q 'optional_value = (7 + 8);' $(TEST_OUT)/preprocessor-va-opt.i
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-va-opt-x86.ro tests/preprocessor_va_opt.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-va-opt-x64.ro tests/preprocessor_va_opt.c
	@echo "C++20 __VA_OPT__ replacement-list tests completed"

test-atomic-builtins: $(RCC_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/atomic-x86)
	$(call MKDIR_P,$(TEST_OUT)/atomic-x64)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x86/atomic.ro tests/atomic_builtin.c
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/atomic-x86/atomic.rin \
		$(TEST_OUT)/atomic-x86/atomic.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x64/atomic.ro tests/atomic_builtin.c
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/atomic-x64/atomic.rin \
		$(TEST_OUT)/atomic-x64/atomic.ro
	$(call MKDIR_P,$(TEST_OUT)/atomic-cxx-x86)
	$(call MKDIR_P,$(TEST_OUT)/atomic-cxx-x64)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/atomic-cxx-x86/atomic.ro tests/atomic_lock_free.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/atomic-cxx-x64/atomic.ro tests/atomic_lock_free.cpp

ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/atomic-builtin-run-test \
		tests/atomic_builtin_run_test.c src/emit_ro.c src/utils.c -pthread
	$(TEST_OUT)/atomic-builtin-run-test \
		$(TEST_OUT)/atomic-x86/atomic.ro \
		$(TEST_OUT)/atomic-x64/atomic.ro

else
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/atomic-builtin-run-test \
		tests/atomic_builtin_run_test.c src/emit_ro.c src/utils.c -pthread
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/atomic-builtin-run-test-x86 \
		tests/atomic_builtin_run_test.c src/emit_ro.c src/utils.c -pthread
	$(TEST_OUT)/atomic-builtin-run-test-x86 \
		$(TEST_OUT)/atomic-x86/atomic.ro \
		$(TEST_OUT)/atomic-x64/atomic.ro
	$(TEST_OUT)/atomic-builtin-run-test \
		$(TEST_OUT)/atomic-x86/atomic.ro \
		$(TEST_OUT)/atomic-x64/atomic.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x86/invalid.ro \
		tests/invalid_atomic_builtin.c,$(TEST_OUT)/atomic-x86/invalid.log)
	$(GREP) -F -q "__atomic_test_and_set requires a byte-sized object pointer" \
		$(TEST_OUT)/atomic-x86/invalid.log
	$(GREP) -F -q "__atomic_clear requires a byte-sized object pointer" \
		$(TEST_OUT)/atomic-x86/invalid.log
	$(GREP) -F -q "__atomic_always_lock_free size argument must be an integer constant" \
		$(TEST_OUT)/atomic-x86/invalid.log
	$(GREP) -F -q "__atomic_is_lock_free size argument must have integer type" \
		$(TEST_OUT)/atomic-x86/invalid.log
	$(GREP) -F -q "__atomic_is_lock_free second argument must have pointer or null-pointer type" \
		$(TEST_OUT)/atomic-x86/invalid.log
	$(GREP) -F -q "__atomic_load requires a supported lock-free object pointer" \
		$(TEST_OUT)/atomic-x86/invalid.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x64/invalid-order.ro \
		tests/invalid_atomic_order.c,$(TEST_OUT)/atomic-x64/invalid-order.log)
	$(GREP) -F -q "__atomic_load does not accept release or acq_rel order" \
		$(TEST_OUT)/atomic-x64/invalid-order.log
	$(GREP) -F -q "__atomic_store accepts only relaxed, release, or seq_cst order" \
		$(TEST_OUT)/atomic-x64/invalid-order.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x86/invalid-pointer.ro \
		tests/invalid_pointer_atomic.c,$(TEST_OUT)/atomic-x86/invalid-pointer.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x64/invalid-pointer.ro \
		tests/invalid_pointer_atomic.c,$(TEST_OUT)/atomic-x64/invalid-pointer.log)
	@echo "Dual-architecture integer/pointer atomic tests completed"

test-atomic-language: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/atomic-language-x86)
	$(call MKDIR_P,$(TEST_OUT)/atomic-language-x64)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/atomic-language-x86/atomic.ro tests/atomic_language.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/atomic-language-x64/atomic.ro tests/atomic_language.c

ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/atomic-language-run-test \
		tests/atomic_language_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/atomic-language-run-test \
		$(TEST_OUT)/atomic-language-x64/atomic.ro
	$(TEST_OUT)/atomic-language-run-test --inspect \
		$(TEST_OUT)/atomic-language-x86/atomic.ro
else
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/atomic-language-run-test \
		tests/atomic_language_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/atomic-language-run-test \
		$(TEST_OUT)/atomic-language-x64/atomic.ro
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/atomic-language-run-test-x86 \
		tests/atomic_language_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/atomic-language-run-test-x86 \
		$(TEST_OUT)/atomic-language-x86/atomic.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/atomic-language-x86/invalid.ro \
		tests/invalid_atomic_language.c,$(TEST_OUT)/atomic-language-x86/invalid.log)
	$(GREP) -F -q "_Atomic requires an unqualified scalar object type" \
		$(TEST_OUT)/atomic-language-x86/invalid.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/atomic-language-x64/invalid.ro \
		tests/invalid_atomic_language.c,$(TEST_OUT)/atomic-language-x64/invalid.log)
	$(GREP) -F -q "_Atomic requires an unqualified scalar object type" \
		$(TEST_OUT)/atomic-language-x64/invalid.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/atomic-language-x86/invalid-rmw.ro \
		tests/invalid_atomic_rmw.c,$(TEST_OUT)/atomic-language-x86/invalid-rmw.log)
	$(GREP) -F -q "atomic ++/-- requires an integer or pointer object" \
		$(TEST_OUT)/atomic-language-x86/invalid-rmw.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/atomic-language-x64/invalid-rmw.ro \
		tests/invalid_atomic_rmw.c,$(TEST_OUT)/atomic-language-x64/invalid-rmw.log)
	$(GREP) -F -q "atomic ++/-- requires an integer or pointer object" \
		$(TEST_OUT)/atomic-language-x64/invalid-rmw.log
	@echo "C17 language _Atomic syntax and lowering tests completed"

test-x86-wide-scalar: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/x86-wide-scalar)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/x86-wide-scalar/scalar.ro \
		tests/x86_wide_scalar.c

ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/x86-wide-scalar/run-test \
		tests/x86_wide_scalar_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/x86-wide-scalar/run-test --inspect \
		$(TEST_OUT)/x86-wide-scalar/scalar.ro
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/x86-wide-scalar/run-test \
		tests/x86_wide_scalar_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/x86-wide-scalar/run-test \
		$(TEST_OUT)/x86-wide-scalar/scalar.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/x86-wide-scalar/invalid.ro \
		tests/invalid_x86_wide_scalar.c,$(TEST_OUT)/x86-wide-scalar/invalid.log)
	@echo "i686 64-bit scalar ABI test completed"

test-language-boundaries: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/language-boundaries)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/language-boundaries/c-x86.ro tests/unsupported_long_double.c,$(TEST_OUT)/language-boundaries/c-x86.log)
	$(GREP) -q "long double is not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/c-x86.log
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/language-boundaries/c17-x86.ro tests/integer_literal.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/language-boundaries/c-literal-x86.ro tests/unsupported_long_double_literal.c,$(TEST_OUT)/language-boundaries/c-literal-x86.log)
	$(GREP) -q "long double literals are not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/c-literal-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/language-boundaries/c-x64.ro tests/unsupported_long_double.c,$(TEST_OUT)/language-boundaries/c-x64.log)
	$(GREP) -q "long double is not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/c-x64.log
	$(RCC_TARGET) --target x86_64-unknown-rinos --std=gnu17 -c \
		-o $(TEST_OUT)/language-boundaries/c17-x64.ro tests/integer_literal.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/language-boundaries/c-literal-x64.ro tests/unsupported_long_double_literal.c,$(TEST_OUT)/language-boundaries/c-literal-x64.log)
	$(GREP) -q "long double literals are not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/c-literal-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/language-boundaries/cxx-x86.ro tests/unsupported_long_double.c,$(TEST_OUT)/language-boundaries/cxx-x86.log)
	$(GREP) -q "long double is not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/cxx-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/language-boundaries/cxx-literal-x86.ro tests/unsupported_long_double_literal.c,$(TEST_OUT)/language-boundaries/cxx-literal-x86.log)
	$(GREP) -q "long double literals are not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/cxx-literal-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/language-boundaries/cxx-x64.ro tests/unsupported_long_double.c,$(TEST_OUT)/language-boundaries/cxx-x64.log)
	$(GREP) -q "long double is not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/cxx-x64.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/language-boundaries/cxx-literal-x64.ro tests/unsupported_long_double_literal.c,$(TEST_OUT)/language-boundaries/cxx-literal-x64.log)
	$(GREP) -q "long double literals are not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/cxx-literal-x64.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c -o $(TEST_OUT)/language-boundaries/complex-x86.ro tests/unsupported_complex.c,$(TEST_OUT)/language-boundaries/complex-x86.log)
	$(GREP) -q "_Complex is not supported by the RinOS floating-point ABI" $(TEST_OUT)/language-boundaries/complex-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c -o $(TEST_OUT)/language-boundaries/complex-x64.ro tests/unsupported_complex.c,$(TEST_OUT)/language-boundaries/complex-x64.log)
	$(GREP) -q "_Complex is not supported by the RinOS floating-point ABI" $(TEST_OUT)/language-boundaries/complex-x64.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c -o $(TEST_OUT)/language-boundaries/imaginary-x86.ro tests/unsupported_imaginary.c,$(TEST_OUT)/language-boundaries/imaginary-x86.log)
	$(GREP) -q "_Imaginary is not supported by the RinOS floating-point ABI" $(TEST_OUT)/language-boundaries/imaginary-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c -o $(TEST_OUT)/language-boundaries/imaginary-x64.ro tests/unsupported_imaginary.c,$(TEST_OUT)/language-boundaries/imaginary-x64.log)
	$(GREP) -q "_Imaginary is not supported by the RinOS floating-point ABI" $(TEST_OUT)/language-boundaries/imaginary-x64.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/dynamic-cast-x86.ro \
		tests/unsupported_dynamic_cast.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/dynamic-cast-x64.ro \
		tests/unsupported_dynamic_cast.cpp
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/language-boundaries/const-cast-x86.ro tests/unsupported_const_cast.cpp,$(TEST_OUT)/language-boundaries/const-cast-x86.log)
	$(GREP) -q "const_cast requires the same object type" \
		$(TEST_OUT)/language-boundaries/const-cast-x86.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c -o $(TEST_OUT)/language-boundaries/const-cast-x64.ro tests/unsupported_const_cast.cpp,$(TEST_OUT)/language-boundaries/const-cast-x64.log)
	$(GREP) -q "const_cast requires the same object type" \
		$(TEST_OUT)/language-boundaries/const-cast-x64.log
	@echo "RCC/RCC++ unsupported language and floating-point boundary diagnostics completed"

test-noreturn: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/noreturn)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/noreturn/x86.ro tests/noreturn.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/noreturn/x64.ro tests/noreturn.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c -o $(TEST_OUT)/noreturn/invalid-x86.ro tests/unsupported_noreturn_object.c,$(TEST_OUT)/noreturn/invalid-x86.log)
	$(GREP) -q "_Noreturn declaration must declare a function" \
		$(TEST_OUT)/noreturn/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c -o $(TEST_OUT)/noreturn/invalid-x64.ro tests/unsupported_noreturn_object.c,$(TEST_OUT)/noreturn/invalid-x64.log)
	$(GREP) -q "_Noreturn declaration must declare a function" \
		$(TEST_OUT)/noreturn/invalid-x64.log
	@echo "Dual-architecture C17 _Noreturn tests completed"

test-compiler-builtins: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/compiler-builtins)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/compiler-builtins/c-x86.s tests/compiler_builtins.c
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/compiler-builtins/c-x86,$(TEST_OUT)/compiler-builtins/c-x86.s)
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/compiler-builtins/c-x64.s tests/compiler_builtins.c
	$(CC) -no-pie -o $(TEST_OUT)/compiler-builtins/c-x64 \
		$(TEST_OUT)/compiler-builtins/c-x64.s
	$(TEST_OUT)/compiler-builtins/c-x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/compiler-builtins/cxx-x86.s \
		tests/cxx_compiler_builtins.cpp
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/compiler-builtins/cxx-x86,$(TEST_OUT)/compiler-builtins/cxx-x86.s)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/compiler-builtins/cxx-x64.s \
		tests/cxx_compiler_builtins.cpp
	$(CC) -no-pie -o $(TEST_OUT)/compiler-builtins/cxx-x64 \
		$(TEST_OUT)/compiler-builtins/cxx-x64.s
	$(TEST_OUT)/compiler-builtins/cxx-x64
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -Iinclude -c \
		-o $(TEST_OUT)/compiler-builtins/intrin-x86.ro tests/intrin_all_test.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -Iinclude -c \
		-o $(TEST_OUT)/compiler-builtins/intrin-x64.ro tests/intrin_all_test.c
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -Iinclude -S \
		-o $(TEST_OUT)/compiler-builtins/mmx-x86.s tests/mmx_intrin.c
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/compiler-builtins/mmx-x86,$(TEST_OUT)/compiler-builtins/mmx-x86.s)
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -Iinclude -S \
		-o $(TEST_OUT)/compiler-builtins/mmx-x64.s tests/mmx_intrin.c
	$(CC) -no-pie -o $(TEST_OUT)/compiler-builtins/mmx-x64 \
		$(TEST_OUT)/compiler-builtins/mmx-x64.s
	$(TEST_OUT)/compiler-builtins/mmx-x64
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -Iinclude -S \
		-o $(TEST_OUT)/compiler-builtins/sse-x86.s tests/sse_intrin.c
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/compiler-builtins/sse-x86,$(TEST_OUT)/compiler-builtins/sse-x86.s)
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -Iinclude -S \
		-o $(TEST_OUT)/compiler-builtins/sse-x64.s tests/sse_intrin.c
	$(CC) -no-pie -o $(TEST_OUT)/compiler-builtins/sse-x64 \
		$(TEST_OUT)/compiler-builtins/sse-x64.s
	$(TEST_OUT)/compiler-builtins/sse-x64
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 \
		-c -o $(TEST_OUT)/compiler-builtins/sse-features-x86.ro \
		tests/sse_feature_macros.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 \
		-c -o $(TEST_OUT)/compiler-builtins/sse-features-x64.ro \
		tests/sse_feature_macros.c
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -mno-sse \
		-DEXPECT_NO_SSE -DEXPECT_NO_SSE2 -c \
		-o $(TEST_OUT)/compiler-builtins/no-sse-features-x86.ro \
		tests/sse_feature_macros.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -mno-sse2 \
		-DEXPECT_NO_SSE2 -c \
		-o $(TEST_OUT)/compiler-builtins/no-sse2-features-x64.ro \
		tests/sse_feature_macros.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -mno-sse -Iinclude -c -o $(TEST_OUT)/compiler-builtins/no-sse-x86.ro tests/sse_intrin.c,$(TEST_OUT)/compiler-builtins/no-sse-x86.log)
	$(GREP) -F -q "requires SSE; enable it with -msse" \
		$(TEST_OUT)/compiler-builtins/no-sse-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -mno-sse2 -Iinclude -c -o $(TEST_OUT)/compiler-builtins/no-sse2-x64.ro tests/sse_intrin.c,$(TEST_OUT)/compiler-builtins/no-sse2-x64.log)
	$(GREP) -F -q "requires SSE2; enable it with -msse2" \
		$(TEST_OUT)/compiler-builtins/no-sse2-x64.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -Iinclude -c -o $(TEST_OUT)/compiler-builtins/invalid-sse-x86.ro tests/invalid_sse_intrin.c,$(TEST_OUT)/compiler-builtins/invalid-sse-x86.log)
	$(GREP) -F -q "expects 2 arguments, got 1" \
		$(TEST_OUT)/compiler-builtins/invalid-sse-x86.log
	$(GREP) -F -q "requires an integer constant immediate" \
		$(TEST_OUT)/compiler-builtins/invalid-sse-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -Iinclude -c -o $(TEST_OUT)/compiler-builtins/invalid-sse-x64.ro tests/invalid_sse_intrin.c,$(TEST_OUT)/compiler-builtins/invalid-sse-x64.log)
	$(GREP) -F -q "expects 2 arguments, got 1" \
		$(TEST_OUT)/compiler-builtins/invalid-sse-x64.log
	$(GREP) -F -q "requires an integer constant immediate" \
		$(TEST_OUT)/compiler-builtins/invalid-sse-x64.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c -o $(TEST_OUT)/compiler-builtins/invalid-x86.ro tests/invalid_compiler_builtins.c,$(TEST_OUT)/compiler-builtins/invalid-x86.log)
	$(GREP) -F -q "__builtin_expect expected value must have integer type" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_choose_expr condition must be an integer constant expression" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_choose_expr expects 3 arguments, got 2" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_expect_with_probability probability must be a floating constant between 0 and 1" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_trap expects no arguments, got 1" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_bswap16 expects an integer argument no wider than 2 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_clz expects an integer argument no wider than 4 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_parity expects an integer argument no wider than 4 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_ffs expects an integer argument no wider than 4 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_clrsb expects an integer argument no wider than 4 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c -o $(TEST_OUT)/compiler-builtins/invalid-types-x86.ro tests/invalid_types_compatible.c,$(TEST_OUT)/compiler-builtins/invalid-types-x86.log)
	$(GREP) -F -q "__builtin_types_compatible_p requires a type name" \
		$(TEST_OUT)/compiler-builtins/invalid-types-x86.log
	$(GREP) -F -q "__builtin_prefetch rw argument must be 0 or 1" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_constant_p expects 1 argument, got 2" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_object_size expects 2 arguments, got 1" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_object_size type argument must be an integer constant between 0 and 3" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_strlen expects a pointer to character data or a string literal" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_add_overflow result argument must point to an integer type" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(GREP) -F -q "__builtin_add_overflow operands and result must have the same integer width and signedness" \
		$(TEST_OUT)/compiler-builtins/invalid-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c -o $(TEST_OUT)/compiler-builtins/invalid-x64.ro tests/invalid_compiler_builtins.c,$(TEST_OUT)/compiler-builtins/invalid-x64.log)
	$(GREP) -F -q "__builtin_expect expected value must have integer type" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_choose_expr condition must be an integer constant expression" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_choose_expr expects 3 arguments, got 2" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_expect_with_probability probability must be a floating constant between 0 and 1" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_trap expects no arguments, got 1" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_bswap16 expects an integer argument no wider than 2 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_clz expects an integer argument no wider than 4 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_parity expects an integer argument no wider than 4 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_ffs expects an integer argument no wider than 4 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_clrsb expects an integer argument no wider than 4 bytes" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c -o $(TEST_OUT)/compiler-builtins/invalid-types-x64.ro tests/invalid_types_compatible.c,$(TEST_OUT)/compiler-builtins/invalid-types-x64.log)
	$(GREP) -F -q "__builtin_types_compatible_p requires a type name" \
		$(TEST_OUT)/compiler-builtins/invalid-types-x64.log
	$(GREP) -F -q "__builtin_prefetch rw argument must be 0 or 1" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_constant_p expects 1 argument, got 2" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_object_size expects 2 arguments, got 1" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_object_size type argument must be an integer constant between 0 and 3" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_strlen expects a pointer to character data or a string literal" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_add_overflow result argument must point to an integer type" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	$(GREP) -F -q "__builtin_add_overflow operands and result must have the same integer width and signedness" \
		$(TEST_OUT)/compiler-builtins/invalid-x64.log
	@echo "C/C++ compiler builtin intrinsic tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-const-cast: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-const-cast)
	$(call CXX_WINDOWS_MAIN,cxx-const-cast,test,cxx_const_cast.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-const-cast/x86.ro tests/cxx_const_cast.cpp \
		>$(TEST_OUT)/cxx-const-cast/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-const-cast/x64.ro tests/cxx_const_cast.cpp \
		>$(TEST_OUT)/cxx-const-cast/x64.log 2>&1
	$(GREP) -F -q "Verified backend: 1 function(s) emitted" \
		$(TEST_OUT)/cxx-const-cast/x86.log
	$(GREP) -F -q "Verified backend: 1 function(s) emitted" \
		$(TEST_OUT)/cxx-const-cast/x64.log
	@echo "C++ cv-only const_cast tests completed"
else
test-cxx-const-cast: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-const-cast)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-const-cast/x86.s tests/cxx_const_cast.cpp
	$(CC) -m32 -no-pie -o $(TEST_OUT)/cxx-const-cast/x86 \
		$(TEST_OUT)/cxx-const-cast/x86.s
	$(TEST_OUT)/cxx-const-cast/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-const-cast/x64.s tests/cxx_const_cast.cpp
	$(CC) -no-pie -o $(TEST_OUT)/cxx-const-cast/x64 \
		$(TEST_OUT)/cxx-const-cast/x64.s
	$(TEST_OUT)/cxx-const-cast/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-const-cast/x86.ro tests/cxx_const_cast.cpp \
		>$(TEST_OUT)/cxx-const-cast/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-const-cast/x64.ro tests/cxx_const_cast.cpp \
		>$(TEST_OUT)/cxx-const-cast/x64.log 2>&1
	$(GREP) -F -q "Verified backend: 1 function(s) emitted" \
		$(TEST_OUT)/cxx-const-cast/x86.log
	$(GREP) -F -q "Verified backend: 1 function(s) emitted" \
		$(TEST_OUT)/cxx-const-cast/x64.log
	@echo "C++ cv-only const_cast tests completed"
endif

ifeq ($(OS),Windows_NT)
test-cxx-dynamic-cast: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dynamic-cast)
	$(call CXX_WINDOWS_MAIN,cxx-dynamic-cast,test,cxx_dynamic_cast.cpp)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast/x86.ro tests/cxx_dynamic_cast_verified.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast/x64.ro tests/cxx_dynamic_cast_verified.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast/x64.log 2>&1
	$(GREP) -F -q "Verified backend: 3 function(s) emitted" \
		$(TEST_OUT)/cxx-dynamic-cast/x86.log
	$(GREP) -F -q "Verified backend: 3 function(s) emitted" \
		$(TEST_OUT)/cxx-dynamic-cast/x64.log
	@echo "C++ statically known public-upcast dynamic_cast tests completed"
else
test-cxx-dynamic-cast: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dynamic-cast)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast/x86.s tests/cxx_dynamic_cast.cpp
	$(CC) -m32 -no-pie -o $(TEST_OUT)/cxx-dynamic-cast/x86 \
		$(TEST_OUT)/cxx-dynamic-cast/x86.s
	$(TEST_OUT)/cxx-dynamic-cast/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast/x64.s tests/cxx_dynamic_cast.cpp
	$(CC) -no-pie -o $(TEST_OUT)/cxx-dynamic-cast/x64 \
		$(TEST_OUT)/cxx-dynamic-cast/x64.s
	$(TEST_OUT)/cxx-dynamic-cast/x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast/x86.ro tests/cxx_dynamic_cast_verified.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast/x64.ro tests/cxx_dynamic_cast_verified.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast/x64.log 2>&1
	$(GREP) -F -q "Verified backend: 3 function(s) emitted" \
		$(TEST_OUT)/cxx-dynamic-cast/x86.log
	$(GREP) -F -q "Verified backend: 3 function(s) emitted" \
		$(TEST_OUT)/cxx-dynamic-cast/x64.log
	@echo "C++ statically known public-upcast dynamic_cast tests completed"
endif

ifeq ($(OS),Windows_NT)
test-cxx-dynamic-cast-downcast: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dynamic-cast-downcast)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-downcast/x86.s \
		tests/cxx_dynamic_cast_downcast.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-downcast/x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-downcast/x86.s
	objdump -f $(TEST_OUT)/cxx-dynamic-cast-downcast/x86.o > $(TEST_OUT)/cxx-dynamic-cast-downcast/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-dynamic-cast-downcast/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-downcast/x64.s \
		tests/cxx_dynamic_cast_downcast.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-downcast/x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-downcast/x64.s
	objdump -f $(TEST_OUT)/cxx-dynamic-cast-downcast/x64.o > $(TEST_OUT)/cxx-dynamic-cast-downcast/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-dynamic-cast-downcast/x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast-downcast/x86.ro \
		tests/cxx_dynamic_cast_downcast.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast-downcast/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast-downcast/x64.ro \
		tests/cxx_dynamic_cast_downcast.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast-downcast/x64.log 2>&1
	$(GREP) -F -q "Verified backend" $(TEST_OUT)/cxx-dynamic-cast-downcast/x86.log
	$(GREP) -F -q "Verified backend" $(TEST_OUT)/cxx-dynamic-cast-downcast/x64.log
	@echo "C++ exact public-downcast dynamic_cast object and verified-backend tests completed; mismatch runtime requires RinOS exception runtime"
else
test-cxx-dynamic-cast-downcast: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dynamic-cast-downcast)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-downcast/x86.s \
		tests/cxx_dynamic_cast_downcast.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-downcast/x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-downcast/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-downcast/start-x86.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-dynamic-cast-downcast/x86 \
		$(TEST_OUT)/cxx-dynamic-cast-downcast/start-x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-downcast/x86.o
	$(TEST_OUT)/cxx-dynamic-cast-downcast/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-downcast/x64.s \
		tests/cxx_dynamic_cast_downcast.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-downcast/x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-downcast/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-downcast/start-x64.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-dynamic-cast-downcast/x64 \
		$(TEST_OUT)/cxx-dynamic-cast-downcast/start-x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-downcast/x64.o
	$(TEST_OUT)/cxx-dynamic-cast-downcast/x64
	@echo "C++ exact public-downcast dynamic_cast tests completed"
endif

ifeq ($(OS),Windows_NT)
test-cxx-dynamic-cast-runtime: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dynamic-cast-runtime)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-runtime/x86.s \
		tests/cxx_dynamic_cast_virtual.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-runtime/x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-runtime/x86.s
	objdump -f $(TEST_OUT)/cxx-dynamic-cast-runtime/x86.o > $(TEST_OUT)/cxx-dynamic-cast-runtime/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-dynamic-cast-runtime/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-runtime/x64.s \
		tests/cxx_dynamic_cast_virtual.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-runtime/x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-runtime/x64.s
	objdump -f $(TEST_OUT)/cxx-dynamic-cast-runtime/x64.o > $(TEST_OUT)/cxx-dynamic-cast-runtime/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-dynamic-cast-runtime/x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast-runtime/x86.ro \
		tests/cxx_dynamic_cast_virtual.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast-runtime/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast-runtime/x64.ro \
		tests/cxx_dynamic_cast_virtual.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast-runtime/x64.log 2>&1
	$(GREP) -F -q "Verified backend" $(TEST_OUT)/cxx-dynamic-cast-runtime/x86.log
	$(GREP) -F -q "Verified backend" $(TEST_OUT)/cxx-dynamic-cast-runtime/x64.log
	@echo "C++ virtual-base dynamic_cast RTTI object and verified-backend tests completed; runtime execution requires RinOS RTTI runtime"
else
test-cxx-dynamic-cast-runtime: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dynamic-cast-runtime)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-runtime/x86.s \
		tests/cxx_dynamic_cast_virtual.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-runtime/x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-runtime/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-runtime/start-x86.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-dynamic-cast-runtime/x86 \
		$(TEST_OUT)/cxx-dynamic-cast-runtime/start-x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-runtime/x86.o
	$(TEST_OUT)/cxx-dynamic-cast-runtime/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-runtime/x64.s \
		tests/cxx_dynamic_cast_virtual.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-runtime/x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-runtime/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-runtime/start-x64.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-dynamic-cast-runtime/x64 \
		$(TEST_OUT)/cxx-dynamic-cast-runtime/start-x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-runtime/x64.o
	$(TEST_OUT)/cxx-dynamic-cast-runtime/x64
	@echo "C++ virtual-base dynamic_cast RTTI tests completed"
endif

ifeq ($(OS),Windows_NT)
test-cxx-dynamic-cast-reference: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dynamic-cast-reference)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-reference/x86.s \
		tests/cxx_dynamic_cast_reference.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-reference/x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-reference/x86.s
	objdump -f $(TEST_OUT)/cxx-dynamic-cast-reference/x86.o > $(TEST_OUT)/cxx-dynamic-cast-reference/x86-arch.log
	$(GREP) -F -q "pe-i386" $(TEST_OUT)/cxx-dynamic-cast-reference/x86-arch.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-reference/x64.s \
		tests/cxx_dynamic_cast_reference.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-reference/x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-reference/x64.s
	objdump -f $(TEST_OUT)/cxx-dynamic-cast-reference/x64.o > $(TEST_OUT)/cxx-dynamic-cast-reference/x64-arch.log
	$(GREP) -F -q "i386:x86-64" $(TEST_OUT)/cxx-dynamic-cast-reference/x64-arch.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast-reference/x86.ro \
		tests/cxx_dynamic_cast_reference.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast-reference/x86.log 2>&1
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/cxx-dynamic-cast-reference/x64.ro \
		tests/cxx_dynamic_cast_reference.cpp \
		>$(TEST_OUT)/cxx-dynamic-cast-reference/x64.log 2>&1
	$(GREP) -F -q "Verified backend" $(TEST_OUT)/cxx-dynamic-cast-reference/x86.log
	$(GREP) -F -q "Verified backend" $(TEST_OUT)/cxx-dynamic-cast-reference/x64.log
	@echo "C++ reference dynamic_cast object and verified-backend tests completed; bad_cast runtime requires RinOS exception runtime"
else
test-cxx-dynamic-cast-reference: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-dynamic-cast-reference)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-reference/x86.s \
		tests/cxx_dynamic_cast_reference.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-reference/x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-reference/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-dynamic-cast-reference/start-x86.o \
		tests/cxx_exceptions_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-dynamic-cast-reference/x86 \
		$(TEST_OUT)/cxx-dynamic-cast-reference/start-x86.o \
		$(TEST_OUT)/cxx-dynamic-cast-reference/x86.o
	$(TEST_OUT)/cxx-dynamic-cast-reference/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-dynamic-cast-reference/x64.s \
		tests/cxx_dynamic_cast_reference.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-reference/x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-reference/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-dynamic-cast-reference/start-x64.o \
		tests/cxx_exceptions_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-dynamic-cast-reference/x64 \
		$(TEST_OUT)/cxx-dynamic-cast-reference/start-x64.o \
		$(TEST_OUT)/cxx-dynamic-cast-reference/x64.o
	$(TEST_OUT)/cxx-dynamic-cast-reference/x64
	@echo "C++ reference dynamic_cast success and bad_cast tests completed"
endif

test-integer-literals: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/integer-literals)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/x86.ro tests/integer_literal.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/x64.ro tests/integer_literal.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/integer-literals/x86.s tests/integer_literal.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/integer-literals/run-test-x86 \
		$(TEST_OUT)/integer-literals/x86.s
	$(TEST_OUT)/integer-literals/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/integer-literals/x64.s tests/integer_literal.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/integer-literals/run-test-x64 \
		$(TEST_OUT)/integer-literals/x64.s
	$(TEST_OUT)/integer-literals/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/integer-literals/run-test-x86 \
		tests/integer_literal_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/integer-literals/run-test-x64 \
		tests/integer_literal_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/integer-literals/run-test-x86 \
		$(TEST_OUT)/integer-literals/x86.ro
	$(TEST_OUT)/integer-literals/run-test-x64 \
		$(TEST_OUT)/integer-literals/x64.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/invalid-decimal.ro \
		tests/invalid_integer_literal.c,$(TEST_OUT)/integer-literals/invalid-decimal.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/invalid-overflow.ro \
		tests/invalid_integer_literal_overflow.c,$(TEST_OUT)/integer-literals/invalid-overflow.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/invalid-suffix.ro \
		tests/invalid_integer_literal_suffix.c,$(TEST_OUT)/integer-literals/invalid-suffix.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/invalid-enumerator.ro \
		tests/invalid_enum_constant_expression.c,$(TEST_OUT)/integer-literals/invalid-enumerator.log)
	$(call CHECK_COUNT,enumerator value,$(TEST_OUT)/integer-literals/invalid-enumerator.log,3)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/invalid-enumerator-x64.ro \
		tests/invalid_enum_constant_expression.c,$(TEST_OUT)/integer-literals/invalid-enumerator-x64.log)
	$(call CHECK_COUNT,enumerator value,$(TEST_OUT)/integer-literals/invalid-enumerator-x64.log,3)
	@echo "C17 integer literal and enumerator constant-expression tests completed"

test-integer-promotions: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/integer-promotions)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/integer-promotions/x86.ro tests/integer_promotion.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-promotions/x64.ro tests/integer_promotion.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/integer-promotions/x86.s tests/integer_promotion.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/integer-promotions/run-test-x86 \
		$(TEST_OUT)/integer-promotions/x86.s
	$(TEST_OUT)/integer-promotions/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/integer-promotions/x64.s tests/integer_promotion.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/integer-promotions/run-test-x64 \
		$(TEST_OUT)/integer-promotions/x64.s
	$(TEST_OUT)/integer-promotions/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/integer-promotions/run-test-x86 \
		tests/integer_promotion_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/integer-promotions/run-test-x64 \
		tests/integer_promotion_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/integer-promotions/run-test-x86 \
		$(TEST_OUT)/integer-promotions/x86.ro
	$(TEST_OUT)/integer-promotions/run-test-x64 \
		$(TEST_OUT)/integer-promotions/x64.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-promotions/invalid.ro \
		tests/invalid_integer_operators.c,$(TEST_OUT)/integer-promotions/invalid.log)
	$(call CHECK_TEXT,remainder operator requires integer operands,$(TEST_OUT)/integer-promotions/invalid.log)
	$(call CHECK_TEXT,bitwise complement requires integer operand,$(TEST_OUT)/integer-promotions/invalid.log)
	$(call CHECK_TEXT,shift operator requires integer operands,$(TEST_OUT)/integer-promotions/invalid.log)
	$(call CHECK_TEXT,logical not requires scalar operand,$(TEST_OUT)/integer-promotions/invalid.log)
	@echo "Dual-architecture C17 integer promotion tests completed"

test-integer-conversions: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/integer-conversions)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/integer-conversions/x86.ro tests/integer_conversion.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-conversions/x64.ro tests/integer_conversion.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/integer-conversions/x86.s tests/integer_conversion.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/integer-conversions/run-test-x86 \
		$(TEST_OUT)/integer-conversions/x86.s
	$(TEST_OUT)/integer-conversions/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/integer-conversions/x64.s tests/integer_conversion.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/integer-conversions/run-test-x64 \
		$(TEST_OUT)/integer-conversions/x64.s
	$(TEST_OUT)/integer-conversions/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/integer-conversions/run-test-x86 \
		tests/integer_conversion_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/integer-conversions/run-test-x64 \
		tests/integer_conversion_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/integer-conversions/run-test-x86 \
		$(TEST_OUT)/integer-conversions/x86.ro
	$(TEST_OUT)/integer-conversions/run-test-x64 \
		$(TEST_OUT)/integer-conversions/x64.ro
endif
	@echo "Dual-architecture C17 integer conversion tests completed"

test-function-calls: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/function-calls)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/function-calls/x86.ro tests/function_call.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/function-calls/x64.ro tests/function_call.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/function-calls/x86.s tests/function_call.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/function-calls/run-test-x86 \
		$(TEST_OUT)/function-calls/x86.s
	$(TEST_OUT)/function-calls/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/function-calls/x64.s tests/function_call.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/function-calls/run-test-x64 \
		$(TEST_OUT)/function-calls/x64.s
	$(TEST_OUT)/function-calls/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/function-calls/run-test-x86 \
		tests/function_call_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/function-calls/run-test-x64 \
		tests/function_call_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/function-calls/run-test-x86 \
		$(TEST_OUT)/function-calls/x86.ro
	$(TEST_OUT)/function-calls/run-test-x64 \
		$(TEST_OUT)/function-calls/x64.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/function-calls/invalid-call.ro \
		tests/invalid_function_call.c,$(TEST_OUT)/function-calls/invalid-call.log)
	$(GREP) -q "too few arguments to function call" \
		$(TEST_OUT)/function-calls/invalid-call.log
	$(GREP) -q "too many arguments to function call" \
		$(TEST_OUT)/function-calls/invalid-call.log
	$(GREP) -q "incompatible type for argument 1" \
		$(TEST_OUT)/function-calls/invalid-call.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/function-calls/invalid-parameters.ro \
		tests/invalid_parameter_list.c,$(TEST_OUT)/function-calls/invalid-parameters.log)
	$(GREP) -q "ellipsis requires at least one named parameter" \
		$(TEST_OUT)/function-calls/invalid-parameters.log
	$(GREP) -q "expected parameter declaration after ','" \
		$(TEST_OUT)/function-calls/invalid-parameters.log
	@echo "Dual-architecture C17 function call contract tests completed"

test-inline-asm-execute: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/inline-asm)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm/x86.ro tests/inline_asm_execution.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm/x64.ro tests/inline_asm_execution.c

ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/inline-asm/run-test-x64 \
		tests/inline_asm_execution_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/inline-asm/run-test-x64 $(TEST_OUT)/inline-asm/x64.ro
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/inline-asm/run-test-x86 \
		tests/inline_asm_execution_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/inline-asm/run-test-x64 \
		tests/inline_asm_execution_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/inline-asm/run-test-x86 $(TEST_OUT)/inline-asm/x86.ro
	$(TEST_OUT)/inline-asm/run-test-x64 $(TEST_OUT)/inline-asm/x64.ro
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm/invalid.ro \
		tests/invalid_inline_asm_instruction.c \
		>$(TEST_OUT)/inline-asm/invalid.log 2>&1; then \
		echo "unsupported AMD64 inline asm unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "unsupported AMD64 inline asm instruction" \
		$(TEST_OUT)/inline-asm/invalid.log
	@if $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm/invalid-x86.ro \
		tests/invalid_inline_asm_instruction.c \
		>$(TEST_OUT)/inline-asm/invalid-x86.log 2>&1; then \
		echo "unsupported i686 inline asm unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "unsupported i686 inline asm instruction" \
		$(TEST_OUT)/inline-asm/invalid-x86.log
endif
	@echo "Dual-architecture inline asm execution/diagnostic tests completed"

test-inline-asm: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/inline-asm-compile)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm-compile/x86.ro tests/asm_test.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm-compile/x64.ro tests/asm_test.c
	@echo "Dual-architecture inline asm instruction tests completed"

test-inline-asm-encoding: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/inline-asm-encoding)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm-encoding/x86.ro tests/inline_asm_execution.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm-encoding/x64.ro tests/inline_asm_execution.c
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/inline-asm-encoding/cxx-x64.ro tests/inline_asm_cpp.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/inline-asm-encoding/verify \
		tests/inline_asm_encoding_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/inline-asm-encoding/verify \
		$(TEST_OUT)/inline-asm-encoding/x86.ro \
		$(TEST_OUT)/inline-asm-encoding/x64.ro \
		$(TEST_OUT)/inline-asm-encoding/cxx-x64.ro
	@echo "Dual-architecture inline asm placeholder encoding tests completed"

test-inline-asm-ports: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/inline-asm-ports)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm-ports/x86.ro tests/inline_asm_ports.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm-ports/x64.ro tests/inline_asm_ports.c
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/inline-asm-ports/cxx-x64.ro tests/inline_asm_ports.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/inline-asm-ports/verify \
		tests/inline_asm_ports_encoding_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/inline-asm-ports/verify \
		$(TEST_OUT)/inline-asm-ports/x86.ro \
		$(TEST_OUT)/inline-asm-ports/x64.ro \
		$(TEST_OUT)/inline-asm-ports/cxx-x64.ro
	@echo "Dual-architecture inline asm port constraint tests completed"

test-intrin-header: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/intrin-header)
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc -I$(INCDIR) -I$(INCDIR)/rcc -c \
		-o $(TEST_OUT)/intrin-header/x86.ro tests/intrin_header_compile.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -I$(INCDIR) -I$(INCDIR)/rcc -c \
		-o $(TEST_OUT)/intrin-header/x64.ro tests/intrin_header_compile.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -nostdinc -I$(INCDIR) -I$(INCDIR)/rcc -c \
		-o $(TEST_OUT)/intrin-header/cxx-x86.ro tests/intrin_header_compile.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -nostdinc -I$(INCDIR) -I$(INCDIR)/rcc -c \
		-o $(TEST_OUT)/intrin-header/cxx-x64.ro tests/intrin_header_compile.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/intrin-header/verify \
		tests/intrin_header_encoding_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/intrin-header/verify \
		$(TEST_OUT)/intrin-header/x86.ro \
		$(TEST_OUT)/intrin-header/x64.ro \
		$(TEST_OUT)/intrin-header/cxx-x64.ro
	@echo "Dual-architecture intrin.h compile tests completed"

test-inline-asm-validation: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/inline-asm-validation)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/inline-asm-validation/x86.ro tests/invalid_inline_asm_constraints.c,$(TEST_OUT)/inline-asm-validation/x86.log)
	$(GREP) -q "unsupported i686 inline asm output register constraint 'k'" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "modifiable lvalue" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "unsupported i686 inline asm clobber 'not_a_register'" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "unsupported i686 inline asm clobber 'r10'" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "outputs use the same fixed register" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "inputs use the same fixed register" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "clobber conflicts with an operand fixed register" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "clobbers list the same register twice" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "placeholder must be" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "placeholder index is out of range" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "scalar integer or pointer" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(GREP) -q "immediate input must be an integer constant expression" \
		$(TEST_OUT)/inline-asm-validation/x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/inline-asm-validation/x64.ro tests/invalid_inline_asm_constraints.c,$(TEST_OUT)/inline-asm-validation/x64.log)
	$(GREP) -q "unsupported AMD64 inline asm output register constraint 'k'" \
		$(TEST_OUT)/inline-asm-validation/x64.log
	$(GREP) -q "unsupported AMD64 inline asm clobber 'not_a_register'" \
		$(TEST_OUT)/inline-asm-validation/x64.log
	$(GREP) -q "outputs use the same fixed register" \
		$(TEST_OUT)/inline-asm-validation/x64.log
	$(GREP) -q "inputs use the same fixed register" \
		$(TEST_OUT)/inline-asm-validation/x64.log
	$(GREP) -q "clobber conflicts with an operand fixed register" \
		$(TEST_OUT)/inline-asm-validation/x64.log
	$(GREP) -q "clobbers list the same register twice" \
		$(TEST_OUT)/inline-asm-validation/x64.log
	$(GREP) -q "placeholder index is out of range" \
		$(TEST_OUT)/inline-asm-validation/x64.log
	$(GREP) -q "immediate input must be an integer constant expression" \
		$(TEST_OUT)/inline-asm-validation/x64.log
	@echo "Dual-architecture inline asm constraint validation tests completed"

ifeq ($(OS),Windows_NT)
test-varargs: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/varargs)
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/varargs/x86.ro \
		tests/varargs.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/varargs/x64.ro \
		tests/varargs.c
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc -Ibootstrap/include -S \
		-o $(TEST_OUT)/varargs/x86.s tests/varargs.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/varargs/run-test-x86 $(TEST_OUT)/varargs/x86.s
	$(TEST_OUT)/varargs/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include -S \
		-o $(TEST_OUT)/varargs/x64.s tests/varargs.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/varargs/run-test-x64 $(TEST_OUT)/varargs/x64.s
	$(TEST_OUT)/varargs/run-test-x64
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include -c -o $(TEST_OUT)/varargs/invalid.ro tests/invalid_varargs.c,$(TEST_OUT)/varargs/invalid.log)
	$(GREP) -F -q "va_start is only valid in a variadic function" $(TEST_OUT)/varargs/invalid.log
	$(GREP) -F -q "va_start requires the final named parameter" $(TEST_OUT)/varargs/invalid.log
	$(GREP) -F -q "va_copy requires two va_list objects" $(TEST_OUT)/varargs/invalid.log
	$(GREP) -F -q "va_end requires a va_list object" $(TEST_OUT)/varargs/invalid.log
	$(GREP) -F -q "va_arg requires a va_list object" $(TEST_OUT)/varargs/invalid.log
	$(GREP) -F -q "va_arg requires a complete fixed scalar or aggregate object type" $(TEST_OUT)/varargs/invalid.log
	@echo "Dual-architecture C17 scalar varargs tests completed"
else
test-varargs: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/varargs)
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/varargs/x86.ro \
		tests/varargs.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/varargs/x64.ro \
		tests/varargs.c
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/varargs/run-test-x86 \
		tests/varargs_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/varargs/run-test-x64 \
		tests/varargs_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/varargs/run-test-x86 $(TEST_OUT)/varargs/x86.ro
	$(TEST_OUT)/varargs/run-test-x64 $(TEST_OUT)/varargs/x64.ro
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/varargs/invalid.ro \
		tests/invalid_varargs.c \
		>$(TEST_OUT)/varargs/invalid.log 2>&1; then \
		echo "invalid varargs unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "va_start is only valid in a variadic function" \
		$(TEST_OUT)/varargs/invalid.log
	$(GREP) -q "va_start requires the final named parameter" \
		$(TEST_OUT)/varargs/invalid.log
	$(GREP) -q "va_copy requires two va_list objects" \
		$(TEST_OUT)/varargs/invalid.log
	$(GREP) -q "va_end requires a va_list object" \
		$(TEST_OUT)/varargs/invalid.log
	$(GREP) -q "va_arg requires a va_list object" \
		$(TEST_OUT)/varargs/invalid.log
	$(GREP) -q "va_arg requires a complete fixed scalar or aggregate object type" \
		$(TEST_OUT)/varargs/invalid.log
	@echo "Dual-architecture C17 scalar varargs tests completed"
endif

test-scalar-comparisons: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/scalar-comparisons)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/scalar-comparisons/x86.ro tests/scalar_comparison.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/scalar-comparisons/x64.ro tests/scalar_comparison.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/scalar-comparisons/x86.s tests/scalar_comparison.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/scalar-comparisons/run-test-x86 \
		$(TEST_OUT)/scalar-comparisons/x86.s
	$(TEST_OUT)/scalar-comparisons/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/scalar-comparisons/x64.s tests/scalar_comparison.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/scalar-comparisons/run-test-x64 \
		$(TEST_OUT)/scalar-comparisons/x64.s
	$(TEST_OUT)/scalar-comparisons/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/scalar-comparisons/run-test-x86 \
		tests/scalar_comparison_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/scalar-comparisons/run-test-x64 \
		tests/scalar_comparison_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/scalar-comparisons/run-test-x86 \
		$(TEST_OUT)/scalar-comparisons/x86.ro
	$(TEST_OUT)/scalar-comparisons/run-test-x64 \
		$(TEST_OUT)/scalar-comparisons/x64.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/scalar-comparisons/invalid.ro \
		tests/invalid_scalar_comparison.c,$(TEST_OUT)/scalar-comparisons/invalid.log)
	$(GREP) -q "comparison requires arithmetic or pointer operands" \
		$(TEST_OUT)/scalar-comparisons/invalid.log
	$(GREP) -q "logical operator requires scalar operands" \
		$(TEST_OUT)/scalar-comparisons/invalid.log
	@echo "Dual-architecture C17 scalar comparison tests completed"

test-aggregate-copy: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/aggregate-copy)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-copy/x86.ro tests/aggregate_copy.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-copy/x64.ro tests/aggregate_copy.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-copy/x86.s tests/aggregate_copy.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/aggregate-copy/run-test-x86 \
		$(TEST_OUT)/aggregate-copy/x86.s
	$(TEST_OUT)/aggregate-copy/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-copy/x64.s tests/aggregate_copy.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/aggregate-copy/run-test-x64 \
		$(TEST_OUT)/aggregate-copy/x64.s
	$(TEST_OUT)/aggregate-copy/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/aggregate-copy/run-test-x86 \
		tests/aggregate_copy_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/aggregate-copy/run-test-x64 \
		tests/aggregate_copy_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/aggregate-copy/run-test-x86 $(TEST_OUT)/aggregate-copy/x86.ro
	$(TEST_OUT)/aggregate-copy/run-test-x64 $(TEST_OUT)/aggregate-copy/x64.ro

endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-copy/invalid.ro \
		tests/invalid_anonymous_aggregate.c,$(TEST_OUT)/aggregate-copy/invalid.log)
	$(GREP) -q "duplicate member 'duplicate' from anonymous aggregate" \
		$(TEST_OUT)/aggregate-copy/invalid.log
	@echo "Dual-architecture C17 aggregate copy tests completed"

test-aggregate-returns: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/aggregate-returns)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-returns/x86.ro tests/aggregate_return.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-returns/x64.ro tests/aggregate_return.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-returns/x86.s tests/aggregate_return.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/aggregate-returns/run-test-x86 \
		$(TEST_OUT)/aggregate-returns/x86.s
	$(TEST_OUT)/aggregate-returns/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-returns/x64.s tests/aggregate_return.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/aggregate-returns/run-test-x64 \
		$(TEST_OUT)/aggregate-returns/x64.s
	$(TEST_OUT)/aggregate-returns/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/aggregate-returns/run-test-x86 \
		tests/aggregate_return_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/aggregate-returns/run-test-x64 \
		tests/aggregate_return_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/aggregate-returns/run-test-x86 \
		$(TEST_OUT)/aggregate-returns/x86.ro
	$(TEST_OUT)/aggregate-returns/run-test-x64 \
		$(TEST_OUT)/aggregate-returns/x64.ro
endif
	@echo "Dual-architecture C17 aggregate return ABI tests completed"

test-aggregate-packed-abi: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/aggregate-packed-abi)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-packed-abi/x86.ro \
		tests/aggregate_packed_abi.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-packed-abi/x64.ro \
		tests/aggregate_packed_abi.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-packed-abi/x86.s \
		tests/aggregate_packed_abi.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/aggregate-packed-abi/run-test-x86 \
		$(TEST_OUT)/aggregate-packed-abi/x86.s
	$(TEST_OUT)/aggregate-packed-abi/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-packed-abi/x64.s \
		tests/aggregate_packed_abi.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/aggregate-packed-abi/run-test-x64 \
		$(TEST_OUT)/aggregate-packed-abi/x64.s
	$(TEST_OUT)/aggregate-packed-abi/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/aggregate-packed-abi/run-test-x86 \
		tests/aggregate_packed_abi_host.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/aggregate-packed-abi/run-test-x64 \
		tests/aggregate_packed_abi_host.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/aggregate-packed-abi/run-test-x86 \
		$(TEST_OUT)/aggregate-packed-abi/x86.ro
	$(TEST_OUT)/aggregate-packed-abi/run-test-x64 \
		$(TEST_OUT)/aggregate-packed-abi/x64.ro
endif
	@echo "SysV packed aggregate ABI tests completed"

test-compound-literals: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/compound-literals)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/compound-literals/x86.ro tests/compound_literal.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/compound-literals/x64.ro tests/compound_literal.c
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/compound-literals/x86.s tests/compound_literal.c
	$(CC) -m32 -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/compound-literals/run-test-x86 \
		$(TEST_OUT)/compound-literals/x86.s
	$(TEST_OUT)/compound-literals/run-test-x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/compound-literals/x64.s tests/compound_literal.c
	$(CC) -nostdlib -no-pie '-Wl,--entry,main' \
		-o $(TEST_OUT)/compound-literals/run-test-x64 \
		$(TEST_OUT)/compound-literals/x64.s
	$(TEST_OUT)/compound-literals/run-test-x64
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/compound-literals/run-test-x86 \
		tests/compound_literal_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/compound-literals/run-test-x64 \
		tests/compound_literal_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/compound-literals/run-test-x86 \
		$(TEST_OUT)/compound-literals/x86.ro
	$(TEST_OUT)/compound-literals/run-test-x64 \
		$(TEST_OUT)/compound-literals/x64.ro

endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/compound-literals/invalid.ro \
		tests/invalid_compound_literal.c,$(TEST_OUT)/compound-literals/invalid.log)
	$(GREP) -q "compound literal requires a complete object type" \
		$(TEST_OUT)/compound-literals/invalid.log
	@echo "Dual-architecture C17 automatic compound literal tests completed"

test-static-compound-address: $(RCC_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/static-compound-address)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/static-compound-address/x86.s \
		tests/c_static_compound_address.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/static-compound-address/x64.s \
		tests/c_static_compound_address.c
	$(CC) -m32 -c -o $(TEST_OUT)/static-compound-address/x86.o \
		$(TEST_OUT)/static-compound-address/x86.s
	$(CC) -c -o $(TEST_OUT)/static-compound-address/x64.o \
		$(TEST_OUT)/static-compound-address/x64.s
	$(CC) $(TEST_OUT)/static-compound-address/x64.o \
		-o $(TEST_OUT)/static-compound-address/x64
	$(TEST_OUT)/static-compound-address/x64
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/static-compound-address/x86.ro \
		tests/c_static_compound_address.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/static-compound-address/x64.ro \
		tests/c_static_compound_address.c
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/static-compound-address/x86.rin \
		tests/c_static_compound_address.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/static-compound-address/x64.rin \
		tests/c_static_compound_address.c
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/static-compound-address/linked-x64.rin \
		$(TEST_OUT)/static-compound-address/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/static-compound-address/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/static-compound-address/x64.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/static-compound-address/linked-x64.rin
	@echo "Dual-architecture C17 static compound literal address tests completed"

ifeq ($(OS),Windows_NT)
test-static-locals: $(RCC_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/static-locals)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/static-locals/x86.s tests/static_locals.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/static-locals/x64.s tests/static_locals.c
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/static-locals/x86.rin tests/static_locals.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/static-locals/x64.rin tests/static_locals.c
	$(CC) -m32 -nostdlib -no-pie -Wl,--entry,main \
		-o $(TEST_OUT)/static-locals/x86 \
		$(TEST_OUT)/static-locals/x86.s
	$(TEST_OUT)/static-locals/x86
	$(CC) -nostdlib -no-pie -Wl,--entry,main \
		-o $(TEST_OUT)/static-locals/x64 \
		$(TEST_OUT)/static-locals/x64.s
	$(TEST_OUT)/static-locals/x64
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/static-locals/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/static-locals/x64.rin
	@echo "Dual-architecture C17 static local storage tests completed"
else
test-static-locals: $(RCC_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/static-locals)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/static-locals/x86.s tests/static_locals.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/static-locals/x64.s tests/static_locals.c
	$(CC) -m32 -c -o $(TEST_OUT)/static-locals/x86.o \
		$(TEST_OUT)/static-locals/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/static-locals/x86-start.o \
		tests/vla_runtime_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/static-locals/x86 \
		$(TEST_OUT)/static-locals/x86-start.o $(TEST_OUT)/static-locals/x86.o
	$(TEST_OUT)/static-locals/x86
	$(CC) -c -o $(TEST_OUT)/static-locals/x64.o \
		$(TEST_OUT)/static-locals/x64.s
	$(CC) -c -o $(TEST_OUT)/static-locals/x64-start.o \
		tests/vla_runtime_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/static-locals/x64 \
		$(TEST_OUT)/static-locals/x64-start.o $(TEST_OUT)/static-locals/x64.o
	$(TEST_OUT)/static-locals/x64
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/static-locals/x86.rin tests/static_locals.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/static-locals/x64.rin tests/static_locals.c
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/static-locals/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/static-locals/x64.rin
	@echo "Dual-architecture C17 static local storage tests completed"
endif

test-block-extern: $(RCC_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/block-extern)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/block-extern/x86.s tests/block_extern.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/block-extern/x64.s tests/block_extern.c
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/block-extern/x86.ro tests/block_extern.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/block-extern/x64.ro tests/block_extern.c
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/block-extern/x86.rin tests/block_extern.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/block-extern/x64.rin tests/block_extern.c
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/block-extern/linked-x64.rin \
		$(TEST_OUT)/block-extern/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/block-extern/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/block-extern/x64.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/block-extern/linked-x64.rin
	@echo "Dual-architecture C17 block-scope extern tests completed"

test-tls-block-scope: $(RCC_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/tls-block-scope)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/tls-block-scope/x86.ro tests/tls_block_scope.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/tls-block-scope/x64.ro tests/tls_block_scope.c
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/tls-block-scope/x86.rin tests/tls_block_scope.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/tls-block-scope/x64.rin tests/tls_block_scope.c
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/tls-block-scope/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/tls-block-scope/x64.rin
	@echo "Dual-architecture C11 block-scope TLS declaration tests completed"

test-bootstrap-core: $(RCC_TARGET)
	$(call MKDIR_P,$(BOOTSTRAP_ROOT)/stage1-a)
	$(call MKDIR_P,$(BOOTSTRAP_ROOT)/stage1-b)
ifeq ($(OS),Windows_NT)
	wsl.exe -d Ubuntu-24.04 bash $(WSL_RINCOMPILER_ROOT)/scripts/bootstrap_gate.sh core
else
	@set -e; \
	for target in i686-unknown-rinos x86_64-unknown-rinos; do \
		for source in $(BOOTSTRAP_CORE_SRCS); do \
			name=$$(basename $$source .c); \
			arch=$${target%%-*}; \
			$(RCC_TARGET) --target $$target $(BOOTSTRAP_INCLUDES) -c \
				-o $(BOOTSTRAP_ROOT)/stage1-a/$$name-$$arch.ro $$source; \
			$(RCC_TARGET) --target $$target $(BOOTSTRAP_INCLUDES) -c \
				-o $(BOOTSTRAP_ROOT)/stage1-b/$$name-$$arch.ro $$source; \
			cmp $(BOOTSTRAP_ROOT)/stage1-a/$$name-$$arch.ro \
				$(BOOTSTRAP_ROOT)/stage1-b/$$name-$$arch.ro; \
		done; \
	done
endif
	@echo "Reproducible dual-architecture stage0 core object bootstrap completed"

test-bootstrap-link: test-bootstrap-core $(RLD_TARGET)
	$(call MKDIR_P,$(BOOTSTRAP_ROOT)/images)
ifeq ($(OS),Windows_NT)
	wsl.exe -d Ubuntu-24.04 bash $(WSL_RINCOMPILER_ROOT)/scripts/bootstrap_gate.sh link
else
	@set -e; \
	for target in i686-unknown-rinos x86_64-unknown-rinos; do \
		arch=$${target%%-*}; \
		for stage in stage1-a stage1-b; do \
			objects=""; \
			for name in $(BOOTSTRAP_RCC_OBJECTS); do \
				objects="$$objects $(BOOTSTRAP_ROOT)/$$stage/$$name-$$arch.ro"; \
			done; \
			$(RLD_TARGET) --target $$target --emit-unsigned-v3 \
				--dep rincrt.rll $(BOOTSTRAP_RUNTIME_IMPORTS) \
				-o $(BOOTSTRAP_ROOT)/images/rcc-$$stage-$$arch.rin \
				$$objects; \
		done; \
		cmp $(BOOTSTRAP_ROOT)/images/rcc-stage1-a-$$arch.rin \
			$(BOOTSTRAP_ROOT)/images/rcc-stage1-b-$$arch.rin; \
	done
endif
	@echo "Reproducible dual-architecture linked stage1 rcc images completed"

test-bootstrap-execute: test-bootstrap-link
	$(call MKDIR_P,$(BOOTSTRAP_ROOT)/execute)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(BOOTSTRAP_ROOT)/execute/reference-i686.ro tests/hello.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(BOOTSTRAP_ROOT)/execute/reference-x86_64.ro tests/hello.c
ifeq ($(OS),Windows_NT)
	wsl.exe -d Ubuntu-24.04 bash $(WSL_RINCOMPILER_ROOT)/scripts/bootstrap_gate.sh execute
	@echo "Dual-architecture linked stage1 execution bootstrap completed"
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) -rdynamic \
		-o $(BOOTSTRAP_ROOT)/execute/run-i686 \
		tests/bootstrap_stage_runner.c -ldl
	$(CC) $(CFLAGS) -I$(INCDIR) -rdynamic \
		-o $(BOOTSTRAP_ROOT)/execute/run-x86_64 \
		tests/bootstrap_stage_runner.c -ldl
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(BOOTSTRAP_ROOT)/execute/reference-i686.ro tests/hello.c
	$(BOOTSTRAP_ROOT)/execute/run-i686 \
		$(BOOTSTRAP_ROOT)/images/rcc-stage1-a-i686.rin rcc-stage1 \
		--target i686-unknown-rinos -c \
		-o $(BOOTSTRAP_ROOT)/execute/stage1-i686.ro tests/hello.c
	cmp $(BOOTSTRAP_ROOT)/execute/reference-i686.ro \
		$(BOOTSTRAP_ROOT)/execute/stage1-i686.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(BOOTSTRAP_ROOT)/execute/reference-x86_64.ro tests/hello.c
	$(BOOTSTRAP_ROOT)/execute/run-x86_64 \
		$(BOOTSTRAP_ROOT)/images/rcc-stage1-a-x86_64.rin rcc-stage1 \
		--target x86_64-unknown-rinos -c \
		-o $(BOOTSTRAP_ROOT)/execute/stage1-x86_64.ro tests/hello.c
	cmp $(BOOTSTRAP_ROOT)/execute/reference-x86_64.ro \
		$(BOOTSTRAP_ROOT)/execute/stage1-x86_64.ro
	@echo "Dual-architecture linked stage1 execution bootstrap completed"
endif

test-bootstrap-stage2: test-bootstrap-execute
	$(call MKDIR_P,$(BOOTSTRAP_ROOT)/stage2)
ifeq ($(OS),Windows_NT)
# The i686 stage1 executable is a valid target image, but its 32-bit host
# address space cannot rebuild the largest compiler translation unit.  Keep
# i686 code generation in the stage2 target while running both rebuilds on the
# x86_64 stage1 host; test-bootstrap-execute still validates native i686 image
# execution separately.
	wsl.exe -d Ubuntu-24.04 bash $(WSL_RINCOMPILER_ROOT)/scripts/bootstrap_gate.sh stage2
else
	@set -e; \
	for target in i686-unknown-rinos x86_64-unknown-rinos; do \
		arch=$${target%%-*}; \
		runner=$(BOOTSTRAP_ROOT)/execute/run-$$arch; \
		image=$(BOOTSTRAP_ROOT)/images/rcc-stage1-a-$$arch.rin; \
		for source in $(BOOTSTRAP_CORE_SRCS); do \
			name=$$(basename $$source .c); \
			$$runner $$image rcc-stage1 --target $$target \
				$(BOOTSTRAP_INCLUDES) -c \
				-o $(BOOTSTRAP_ROOT)/stage2/$$name-$$arch.ro $$source; \
			cmp $(BOOTSTRAP_ROOT)/stage1-a/$$name-$$arch.ro \
				$(BOOTSTRAP_ROOT)/stage2/$$name-$$arch.ro; \
		done; \
		objects=""; \
		for name in $(BOOTSTRAP_RCC_OBJECTS); do \
			objects="$$objects $(BOOTSTRAP_ROOT)/stage2/$$name-$$arch.ro"; \
		done; \
		$(RLD_TARGET) --target $$target --emit-unsigned-v3 \
			--dep rincrt.rll $(BOOTSTRAP_RUNTIME_IMPORTS) \
			-o $(BOOTSTRAP_ROOT)/images/rcc-stage2-$$arch.rin $$objects; \
		cmp $$image $(BOOTSTRAP_ROOT)/images/rcc-stage2-$$arch.rin; \
	done
endif
	@echo "Reproducible dual-architecture stage1-to-stage2 compiler rebuild completed"

test-pragma-pack: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/pragma-pack)
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/pragma-pack/x86.ro \
		tests/pragma_pack.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/pragma-pack/x64.ro \
		tests/pragma_pack.c
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/pragma-pack/operator-x86.ro \
		tests/pragma_operator_pack.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/pragma-pack/operator-x64.ro \
		tests/pragma_operator_pack.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -nostdinc -S \
		-Ibootstrap/include -o $(TEST_OUT)/pragma-pack/operator-cxx-x86.s \
		tests/pragma_operator_pack.cpp
ifeq ($(OS),Windows_NT)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -nostdinc -c \
		-Ibootstrap/include -o $(TEST_OUT)/pragma-pack/operator-cxx-x86.ro \
		tests/pragma_operator_pack.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -nostdinc -c \
		-Ibootstrap/include -o $(TEST_OUT)/pragma-pack/operator-cxx-x64.ro \
		tests/pragma_operator_pack.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -nostdinc -S \
		-Ibootstrap/include -o $(TEST_OUT)/pragma-pack/operator-cxx-x64.s \
		tests/pragma_operator_pack.cpp
	$(CC) -o $(TEST_OUT)/pragma-pack/operator-cxx-x64 \
		$(TEST_OUT)/pragma-pack/operator-cxx-x64.s
	$(TEST_OUT)/pragma-pack/operator-cxx-x64
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/pragma-pack/operator-cxx-run-test \
		tests/pragma_operator_pack_host_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/pragma-pack/operator-cxx-run-test \
		$(TEST_OUT)/pragma-pack/operator-cxx-x86.ro \
		$(TEST_OUT)/pragma-pack/operator-cxx-x64.ro
else
	$(CC) -m32 -c -o $(TEST_OUT)/pragma-pack/operator-cxx-x86.o \
		$(TEST_OUT)/pragma-pack/operator-cxx-x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/pragma-pack/operator-cxx-start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/pragma-pack/operator-cxx-x86 \
		$(TEST_OUT)/pragma-pack/operator-cxx-start-x86.o \
		$(TEST_OUT)/pragma-pack/operator-cxx-x86.o
	$(TEST_OUT)/pragma-pack/operator-cxx-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -nostdinc -S \
		-Ibootstrap/include -o $(TEST_OUT)/pragma-pack/operator-cxx-x64.s \
		tests/pragma_operator_pack.cpp
	$(CC) -c -o $(TEST_OUT)/pragma-pack/operator-cxx-x64.o \
		$(TEST_OUT)/pragma-pack/operator-cxx-x64.s
	$(CC) -c -o $(TEST_OUT)/pragma-pack/operator-cxx-start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/pragma-pack/operator-cxx-x64 \
		$(TEST_OUT)/pragma-pack/operator-cxx-start-x64.o \
		$(TEST_OUT)/pragma-pack/operator-cxx-x64.o
	$(TEST_OUT)/pragma-pack/operator-cxx-x64
endif
	@echo "Dual-architecture pragma-pack and offsetof tests completed"

test-bitfields: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/bitfields)
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/bitfields/x86.s tests/bitfields.c
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/x86.ro tests/bitfields.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/bitfields/x64.s tests/bitfields.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/bitfields/run-test \
		tests/bitfields_host_run_test.c src/emit_ro.c src/utils.c \
		$(TEST_OUT)/bitfields/x64.s
	$(TEST_OUT)/bitfields/run-test
	$(TEST_OUT)/bitfields/run-test --inspect \
		$(TEST_OUT)/bitfields/x86.ro
else
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/bitfields/x86.s tests/bitfields.c
	$(CC) -m32 -c -o $(TEST_OUT)/bitfields/x86.o \
		$(TEST_OUT)/bitfields/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/bitfields/start-x86.o \
		tests/bitfields_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/bitfields/x86 \
		$(TEST_OUT)/bitfields/start-x86.o $(TEST_OUT)/bitfields/x86.o
	$(TEST_OUT)/bitfields/x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/bitfields/x64.s tests/bitfields.c
	$(CC) -c -o $(TEST_OUT)/bitfields/x64.o \
		$(TEST_OUT)/bitfields/x64.s
	$(CC) -c -o $(TEST_OUT)/bitfields/start-x64.o \
		tests/bitfields_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/bitfields/x64 \
		$(TEST_OUT)/bitfields/start-x64.o $(TEST_OUT)/bitfields/x64.o
	$(TEST_OUT)/bitfields/x64
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/invalid.ro tests/invalid_bitfields.c,$(TEST_OUT)/bitfields/invalid.log)
	$(GREP) -q "bit-field width" $(TEST_OUT)/bitfields/invalid.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/invalid-address.ro \
		tests/invalid_bitfield_address.c,$(TEST_OUT)/bitfields/invalid-address.log)
	$(GREP) -q "cannot take address of a bit-field" \
		$(TEST_OUT)/bitfields/invalid-address.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/invalid-offsetof.ro \
		tests/invalid_bitfield_offsetof.c,$(TEST_OUT)/bitfields/invalid-offsetof.log)
	$(GREP) -q "cannot compute offsetof for a bit-field" \
		$(TEST_OUT)/bitfields/invalid-offsetof.log
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/tls-x86.ro tests/bitfields_tls.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/tls-x64.ro tests/bitfields_tls.c
	@echo "Dual-architecture C17 bit-field tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-bitfields: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-bitfields)
	$(call CXX_WINDOWS_MAIN,cxx-bitfields,test,cxx_bitfields.cpp)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-bitfields/invalid.ro tests/invalid_cxx_bitfields.cpp,$(TEST_OUT)/cxx-bitfields/invalid.log)
	$(GREP) -F -q "C++ bit-field width" $(TEST_OUT)/cxx-bitfields/invalid.log
	@echo "Dual-architecture C++ bit-field tests completed"
else
test-cxx-bitfields: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-bitfields)
	$(RCXX_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/cxx-bitfields/x86.s tests/cxx_bitfields.cpp
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-bitfields/x86.o \
		$(TEST_OUT)/cxx-bitfields/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-bitfields/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-bitfields/x86 \
		$(TEST_OUT)/cxx-bitfields/start-x86.o $(TEST_OUT)/cxx-bitfields/x86.o
	$(TEST_OUT)/cxx-bitfields/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/cxx-bitfields/x64.s tests/cxx_bitfields.cpp
	$(CC) -c -o $(TEST_OUT)/cxx-bitfields/x64.o \
		$(TEST_OUT)/cxx-bitfields/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-bitfields/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-bitfields/x64 \
		$(TEST_OUT)/cxx-bitfields/start-x64.o $(TEST_OUT)/cxx-bitfields/x64.o
	$(TEST_OUT)/cxx-bitfields/x64
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-bitfields/invalid.ro tests/invalid_cxx_bitfields.cpp \
		>$(TEST_OUT)/cxx-bitfields/invalid.log 2>&1; then \
		echo "invalid C++ bit-field fixture unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "C++ bit-field width" $(TEST_OUT)/cxx-bitfields/invalid.log
	@echo "Dual-architecture C++ bit-field tests completed"
endif

test-compound-assignment: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/compound-assignment)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/compound-assignment/x86.ro \
		tests/compound_assignment.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/compound-assignment/x64.ro \
		tests/compound_assignment.c
ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/compound-assignment/run-test \
		tests/compound_assignment_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/compound-assignment/run-test \
		$(TEST_OUT)/compound-assignment/x64.ro
	$(TEST_OUT)/compound-assignment/run-test --inspect \
		$(TEST_OUT)/compound-assignment/x86.ro
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/compound-assignment/run-test-x86 \
		tests/compound_assignment_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/compound-assignment/run-test-x64 \
		tests/compound_assignment_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/compound-assignment/run-test-x86 \
		$(TEST_OUT)/compound-assignment/x86.ro
	$(TEST_OUT)/compound-assignment/run-test-x64 \
		$(TEST_OUT)/compound-assignment/x64.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/compound-assignment/invalid.ro \
		tests/invalid_compound_assignment.c,$(TEST_OUT)/compound-assignment/invalid.log)
	@echo "Dual-architecture C17 compound assignment tests completed"

test-switch-statement: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/switch-statement)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/switch-statement/x86.ro tests/switch_statement.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/switch-statement/x64.ro tests/switch_statement.c
ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/switch-statement/run-test \
		tests/switch_statement_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/switch-statement/run-test \
		$(TEST_OUT)/switch-statement/x64.ro
	$(TEST_OUT)/switch-statement/run-test --inspect \
		$(TEST_OUT)/switch-statement/x86.ro
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/switch-statement/run-test-x86 \
		tests/switch_statement_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/switch-statement/run-test-x64 \
		tests/switch_statement_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/switch-statement/run-test-x86 \
		$(TEST_OUT)/switch-statement/x86.ro
	$(TEST_OUT)/switch-statement/run-test-x64 \
		$(TEST_OUT)/switch-statement/x64.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/switch-statement/invalid.ro \
		tests/invalid_switch_statement.c,$(TEST_OUT)/switch-statement/invalid.log)
	$(GREP) -q "case label is not within a switch" \
		$(TEST_OUT)/switch-statement/invalid.log
	$(GREP) -q "default label is not within a switch" \
		$(TEST_OUT)/switch-statement/invalid.log
	$(GREP) -q "duplicate case value" $(TEST_OUT)/switch-statement/invalid.log
	$(GREP) -q "multiple default labels" $(TEST_OUT)/switch-statement/invalid.log
	$(GREP) -q "case label must be an integer constant expression" \
		$(TEST_OUT)/switch-statement/invalid.log
	$(GREP) -q "switch controlling expression must have integer type" \
		$(TEST_OUT)/switch-statement/invalid.log
	@echo "Dual-architecture C17 switch statement tests completed"

test-control-flow: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/control-flow)
	$(RCC_TARGET) --target i686-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/control-flow/x86.ro tests/control_flow.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/control-flow/x64.ro tests/control_flow.c
ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/control-flow/run-test \
		tests/control_flow_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/control-flow/run-test $(TEST_OUT)/control-flow/x64.ro
	$(TEST_OUT)/control-flow/run-test --inspect \
		$(TEST_OUT)/control-flow/x86.ro
else
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/control-flow/run-test-x86 \
		tests/control_flow_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/control-flow/run-test-x64 \
		tests/control_flow_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/control-flow/run-test-x86 $(TEST_OUT)/control-flow/x86.ro
	$(TEST_OUT)/control-flow/run-test-x64 $(TEST_OUT)/control-flow/x64.ro
endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/control-flow/invalid.ro \
		tests/invalid_control_flow.c,$(TEST_OUT)/control-flow/invalid.log)
	$(GREP) -q "break statement is not within a loop or switch" \
		$(TEST_OUT)/control-flow/invalid.log
	$(GREP) -q "continue statement is not within a loop" \
		$(TEST_OUT)/control-flow/invalid.log
	$(GREP) -q "undefined label 'missing'" \
		$(TEST_OUT)/control-flow/invalid.log
	$(GREP) -q "redefinition of label 'duplicate'" \
		$(TEST_OUT)/control-flow/invalid.log
	@echo "Dual-architecture C17 goto/label tests completed"

test-parser-recovery: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/parser-recovery)
ifeq ($(OS),Windows_NT)
	powershell -NoProfile -Command "$$out='$(TEST_OUT)/parser-recovery/invalid.log'; $$err='$(TEST_OUT)/parser-recovery/invalid.err'; $$p=Start-Process -FilePath './rcc.exe' -ArgumentList '--target','x86_64-unknown-rinos','-c','-o','$(TEST_OUT)/parser-recovery/invalid.ro','tests/parser_recovery.c' -RedirectStandardOutput $$out -RedirectStandardError $$err -PassThru; Wait-Process -Id $$p.Id -Timeout 10 -ErrorAction SilentlyContinue | Out-Null; $$p.Refresh(); if (-not $$p.HasExited) { Stop-Process -Id $$p.Id -Force -ErrorAction SilentlyContinue; Write-Error 'parser recovery timed out'; exit 1 }; Get-Content $$err | Add-Content $$out; if ($$p.ExitCode -eq 0) { Write-Error 'parser recovery fixture unexpectedly compiled'; exit 1 }"
else
	@set +e; timeout 10s $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/parser-recovery/invalid.ro \
		tests/parser_recovery.c \
		>$(TEST_OUT)/parser-recovery/invalid.log 2>&1; status=$$?; set -e; \
		if [ $$status -eq 0 ]; then \
			echo "parser recovery fixture unexpectedly compiled"; exit 1; \
		fi; \
		if [ $$status -eq 124 ] || [ $$status -eq 139 ]; then \
			echo "parser recovery timed out or crashed (status $$status)"; exit 1; \
		fi
endif
	$(GREP) -q "expected parameter type specifier" \
		$(TEST_OUT)/parser-recovery/invalid.log
	$(GREP) -q "expected field type specifier" \
		$(TEST_OUT)/parser-recovery/invalid.log
	$(GREP) -q "expected expression" $(TEST_OUT)/parser-recovery/invalid.log
	@echo "C17 parser progress and null-type recovery tests completed"

test-link: $(RCC_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -c -o $(TEST_OUT)/main.ro tests/main.c
	$(RCC_TARGET) -c -o $(TEST_OUT)/lib.ro tests/lib.c
	$(RLD_TARGET) -v --emit-unsigned-v3 -o $(TEST_OUT)/linked.rin \
		$(TEST_OUT)/main.ro $(TEST_OUT)/lib.ro
	@echo "RLD link test completed"

test-executable-imports: $(RCC_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/executable-imports)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/executable-imports/x86.ro \
		tests/executable_import.c
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		--dep rincrt.rll \
		--import imported_function=rincrt.rll@function \
		-o $(TEST_OUT)/executable-imports/x86.rin \
		$(TEST_OUT)/executable-imports/x86.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/executable-imports/x64.ro \
		tests/executable_import.c
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		--dep rincrt.rll \
		--import imported_function=rincrt.rll@function \
		-o $(TEST_OUT)/executable-imports/x64.rin \
		$(TEST_OUT)/executable-imports/x64.ro
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/executable-imports/verify \
		tests/executable_import_test.c
	$(TEST_OUT)/executable-imports/verify \
		$(TEST_OUT)/executable-imports/x86.rin \
		$(TEST_OUT)/executable-imports/x64.rin
	@echo "Dual-architecture executable import contract test completed"

test-archive: $(RCC_TARGET) $(RAR_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -c -o $(TEST_OUT)/lib.ro tests/lib.c
	$(RAR_TARGET) r $(TEST_OUT)/libtest.ra $(TEST_OUT)/lib.ro
	$(RAR_TARGET) t $(TEST_OUT)/libtest.ra
	@echo "RAR archive test completed"

test-archive-link: $(RCC_TARGET) $(RLD_TARGET) $(RAR_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/archive-x86)
	$(call MKDIR_P,$(TEST_OUT)/archive-x64)
	$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/archive-x86/main.ro tests/archive_link_main.c
	$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/archive-x86/helper.ro tests/archive_link_helper.c
	$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/archive-x86/unused.ro tests/archive_link_unused.c
	$(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/archive-x86/chosen.ro tests/archive_link_chosen.c
	$(RAR_TARGET) r $(TEST_OUT)/archive-x86/libselect.ra \
		$(TEST_OUT)/archive-x86/helper.ro $(TEST_OUT)/archive-x86/unused.ro \
		$(TEST_OUT)/archive-x86/chosen.ro
	$(RLD_TARGET) -m32 -v --emit-unsigned-v3 -o $(TEST_OUT)/archive-x86/selected.rin \
		$(TEST_OUT)/archive-x86/main.ro $(TEST_OUT)/archive-x86/libselect.ra
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/archive_corrupt_test \
		tests/archive_corrupt_test.c
	$(call COPY_FILE,$(TEST_OUT)/archive-x86/libselect.ra,$(TEST_OUT)/archive-x86/corrupt.ra)
	$(TEST_OUT)/archive_corrupt_test $(TEST_OUT)/archive-x86/corrupt.ra
	$(call EXPECT_FAILURE,$(RLD_TARGET) -m32 --emit-unsigned-v3 -o $(TEST_OUT)/archive-x86/corrupt.rin \
		$(TEST_OUT)/archive-x86/main.ro $(TEST_OUT)/archive-x86/corrupt.ra,$(TEST_OUT)/archive-x86/corrupt.log)
	$(call EXPECT_FAILURE,$(RLD_TARGET) -m32 -e archive_order_root --emit-unsigned-v3 \
		-o $(TEST_OUT)/archive-x86/wrong-order.rin \
		$(TEST_OUT)/archive-x86/libselect.ra $(TEST_OUT)/archive-x86/main.ro,$(TEST_OUT)/archive-x86/wrong-order.log)
	$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/archive-x64/main.ro tests/archive_link_main.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/archive-x64/helper.ro tests/archive_link_helper.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/archive-x64/unused.ro tests/archive_link_unused.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/archive-x64/chosen.ro tests/archive_link_chosen.c
	$(RAR_TARGET) r $(TEST_OUT)/archive-x64/libselect.ra \
		$(TEST_OUT)/archive-x64/helper.ro $(TEST_OUT)/archive-x64/unused.ro \
		$(TEST_OUT)/archive-x64/chosen.ro
	$(RLD_TARGET) -m64 -v --emit-unsigned-v3 -o $(TEST_OUT)/archive-x64/selected.rin \
		$(TEST_OUT)/archive-x64/main.ro $(TEST_OUT)/archive-x64/libselect.ra
	$(call EXPECT_FAILURE,$(RLD_TARGET) -m64 --emit-unsigned-v3 -o $(TEST_OUT)/archive-x64/wrong-arch.rin \
		$(TEST_OUT)/archive-x64/main.ro $(TEST_OUT)/archive-x86/libselect.ra,$(TEST_OUT)/archive-x64/wrong-arch.log)
	@echo "RLD unresolved-symbol archive selection tests completed"

test-static-assert: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) -c -o $(TEST_OUT)/static_assert_pass.ro tests/static_assert_pass.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) -c \
		-o $(TEST_OUT)/static_assert_fail.ro tests/static_assert_fail.c,$(TEST_OUT)/static_assert_fail.log)
	$(GREP) -q "static assertion failed" $(TEST_OUT)/static_assert_fail.log
	@echo "C17 static assertion test completed"

test-manifest: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/build_manifest_test \
		tests/build_manifest_test.c $(SRCDIR)/build_manifest.c $(SRCDIR)/utils.c
	$(TEST_OUT)/build_manifest_test
	$(RCC_TARGET) --manifest tests/build_manifest_compiler.rbm \
		--emit-unsigned-v3 -o $(TEST_OUT)/manifest_direct.rll tests/hello.c
	$(RCC_TARGET) --manifest tests/build_manifest_object.rbm \
		-o $(TEST_OUT)/manifest_main.ro tests/main.c
	$(RCC_TARGET) --manifest tests/build_manifest_object.rbm \
		-o $(TEST_OUT)/manifest_lib.ro tests/lib.c
	$(RLD_TARGET) --manifest tests/build_manifest_valid.rbm \
		--emit-unsigned-v3 -o $(TEST_OUT)/manifest_linked.rll \
		$(TEST_OUT)/manifest_main.ro $(TEST_OUT)/manifest_lib.ro
	$(call EXPECT_FAILURE,$(RCC_TARGET) --manifest tests/build_manifest_compiler.rbm \
		--emit-unsigned-v3 -c tests/hello.c,$(TEST_OUT)/manifest-c-conflict.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --manifest tests/build_manifest_compiler.rbm \
		--emit-unsigned-v3 -c tests/hello.cpp,$(TEST_OUT)/manifest-cxx-conflict.log)
	$(call EXPECT_FAILURE,$(RLD_TARGET) --manifest tests/build_manifest_valid.rbm -m32 \
		--emit-unsigned-v3 tests/missing.ro,$(TEST_OUT)/manifest-target-conflict.log)
	$(call EXPECT_FAILURE,$(RLD_TARGET) --manifest tests/build_manifest_executable.rbm -shared \
		--emit-unsigned-v3 tests/missing.ro,$(TEST_OUT)/manifest-artifact-conflict.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --manifest tests/build_manifest_compiler.rbm \
		--sign-profile release --emit-unsigned-v3 tests/hello.c,$(TEST_OUT)/manifest-signing-conflict.log)
	$(GREP) -q "CLI artifact conflicts with build manifest" $(TEST_OUT)/manifest-c-conflict.log
	$(GREP) -q "CLI artifact conflicts with build manifest" $(TEST_OUT)/manifest-cxx-conflict.log
	$(GREP) -q "CLI target conflicts with build manifest" $(TEST_OUT)/manifest-target-conflict.log
	$(GREP) -q "CLI artifact conflicts with build manifest" $(TEST_OUT)/manifest-artifact-conflict.log
	$(GREP) -q "CLI signing profile conflicts with build manifest" $(TEST_OUT)/manifest-signing-conflict.log
	@echo "Versioned build manifest conflict tests completed"

test-signing: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(SIGN_TEST_DIR)/argv ; spaces)
	$(call COPY_FILE,tests/fake_rinsign.py,$(SIGN_TEST_DIR)/argv ; spaces/fake signer.py)
	$(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug \
		--python python3 --rinsign "$(SIGN_TEST_DIR)/argv ; spaces/fake signer.py" \
		--sign-key tests/signing_test_private.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/direct x86.rin" tests/hello.c
	$(RCXX_TARGET) --target x86_64-unknown-rinos -shared --sign-profile debug \
		--python python3 --rinsign "$(SIGN_TEST_DIR)/argv ; spaces/fake signer.py" \
		--sign-key tests/signing_test_private.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/direct cxx x64.rll" tests/hello.cpp
	$(RCC_TARGET) --manifest tests/build_manifest_compiler.rbm \
		--python python3 --rinsign tests/fake_rinsign.py \
		--sign-key tests/signing_test_private.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/manifest debug.rll" tests/hello.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -driver --sign-profile release \
		--python python3 --rinsign "$(SIGN_TEST_DIR)/argv ; spaces/fake signer.py" \
		--sign-key tests/signing_test_private.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/direct x64.drv" tests/driver_policy_ok.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o "$(SIGN_TEST_DIR)/main x64.ro" tests/main.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o "$(SIGN_TEST_DIR)/lib x64.ro" tests/lib.c
	$(RLD_TARGET) --target x86_64-unknown-rinos --sign-profile release \
		--python python3 --rinsign "$(SIGN_TEST_DIR)/argv ; spaces/fake signer.py" \
		--sign-key tests/signing_test_private.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/linked x64.rin" \
		"$(SIGN_TEST_DIR)/main x64.ro" "$(SIGN_TEST_DIR)/lib x64.ro"
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos --python python3 --rinsign tests/fake_rinsign.py --sign-key tests/signing_test_private.key --public-key tests/signing_test_public.der -o "$(SIGN_TEST_DIR)/missing profile.rin" tests/hello.c,$(SIGN_TEST_DIR)/missing-profile.log)
	$(GREP) -F -q "final v3 output requires --sign-profile debug or release" $(SIGN_TEST_DIR)/missing-profile.log
	$(call COPY_FILE,$(SIGN_TEST_DIR)/direct x86.rin,$(SIGN_TEST_DIR)/preserved.rin)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug --python python3 --rinsign tests/fake_rinsign.py --sign-key tests/signing_test_fail.key --public-key tests/signing_test_public.der -o "$(SIGN_TEST_DIR)/preserved.rin" tests/hello.c,$(SIGN_TEST_DIR)/preserved-failure.log)
	$(call COMPARE_FILES,$(SIGN_TEST_DIR)/direct x86.rin,$(SIGN_TEST_DIR)/preserved.rin)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug --python python3 --rinsign tests/fake_rinsign.py --sign-key tests/signing_test_invalid.key --public-key tests/signing_test_public.der -o "$(SIGN_TEST_DIR)/invalid signer.rin" tests/hello.c,$(SIGN_TEST_DIR)/invalid-signer.log)
	$(GREP) -F -q "rinsign produced an invalid signed v3 artifact" $(SIGN_TEST_DIR)/invalid-signer.log
	$(call ASSERT_ABSENT,$(SIGN_TEST_DIR)/invalid signer.rin)
	$(PARALLEL_SIGNING)
	$(call CHECK_NO_SIGN_TEMP,$(SIGN_TEST_DIR))
	@echo "Isolated final signing and atomic publication tests completed"

# Audit the exact unsigned artifacts emitted by RCC/RCC++/RLD with the native
# validator.  This deliberately covers all three image families and both
# direct and linked RIN output; signing is exercised separately by
# test-signing because this target is about section/table/ABI conformance.
test-format-validation: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/format-validation)
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/format-validation/direct-x86.rin tests/hello.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/format-validation/direct-x64.rin tests/hello.c
	$(RCXX_TARGET) --target i686-unknown-rinos -shared --emit-unsigned-v3 \
		-o $(TEST_OUT)/format-validation/library-x86.rll tests/hello.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -shared --emit-unsigned-v3 \
		-o $(TEST_OUT)/format-validation/library-x64.rll tests/hello.cpp
	$(RCC_TARGET) --target i686-unknown-rinos -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/format-validation/driver-x86.drv tests/driver_policy_ok.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/format-validation/driver-x64.drv tests/driver_policy_ok.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/format-validation/main-x64.ro tests/main.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/format-validation/lib-x64.ro tests/lib.c
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/format-validation/linked-x64.rin \
		$(TEST_OUT)/format-validation/main-x64.ro \
		$(TEST_OUT)/format-validation/lib-x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/format-validation/direct-x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/format-validation/direct-x64.rin
	$(RINVALIDATE) --kind library --arch x86 --allow-unsigned \
		$(TEST_OUT)/format-validation/library-x86.rll
	$(RINVALIDATE) --kind library --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/format-validation/library-x64.rll
	$(RINVALIDATE) --kind driver --arch x86 --allow-unsigned \
		$(TEST_OUT)/format-validation/driver-x86.drv
	$(RINVALIDATE) --kind driver --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/format-validation/driver-x64.drv
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/format-validation/linked-x64.rin
	@echo "RCC/RCC++/RLD native RIN/RLL/NDRV v3 format validation completed"

test-pic-plt: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/pic-plt)
	$(RCC_TARGET) --target i686-unknown-rinos -fPIC -c \
		-o $(TEST_OUT)/pic-plt/x86.ro tests/pic_external.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -fPIE -c \
		-o $(TEST_OUT)/pic-plt/x64.ro tests/pic_external.c
	$(RCXX_TARGET) --target x86_64-unknown-rinos -fPIC -c \
		-o $(TEST_OUT)/pic-plt/cxx-x64.ro tests/hello.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/pic-plt/verify \
		tests/pic_relocation_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/pic-plt/verify \
		$(TEST_OUT)/pic-plt/x86.ro $(TEST_OUT)/pic-plt/x64.ro
	$(RLD_TARGET) --target i686-unknown-rinos -e call_import --emit-unsigned-v3 \
		--dep rincrt.rll --import imported_function=rincrt.rll@function \
		-o $(TEST_OUT)/pic-plt/x86.rin $(TEST_OUT)/pic-plt/x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos -e call_import --emit-unsigned-v3 \
		--dep rincrt.rll --import imported_function=rincrt.rll@function \
		-o $(TEST_OUT)/pic-plt/x64.rin $(TEST_OUT)/pic-plt/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/pic-plt/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/pic-plt/x64.rin
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -fPIC \
		--emit-unsigned-v3 -o $(TEST_OUT)/pic-plt/forbidden.rin \
		tests/pic_external.c,$(TEST_OUT)/pic-plt/forbidden.log)
	$(GREP) -q "direct RIN v3 output cannot contain unresolved relative relocation" \
		$(TEST_OUT)/pic-plt/forbidden.log
	@echo "PIC/PIE PLT32 object and RLD import-thunk tests completed"

test-pic-got: $(RCC_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/pic-got)
	$(RCC_TARGET) --target i686-unknown-rinos -fPIC -c \
		-o $(TEST_OUT)/pic-got/x86.ro tests/pic_got.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -fPIE -c \
		-o $(TEST_OUT)/pic-got/x64.ro tests/pic_got.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/pic-got/verify \
		tests/pic_got_relocation_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/pic-got/verify \
		$(TEST_OUT)/pic-got/x86.ro $(TEST_OUT)/pic-got/x64.ro
	$(RLD_TARGET) --target i686-unknown-rinos -e pic_got_use \
		--emit-unsigned-v3 --dep rincrt.rll \
		--import imported_data=rincrt.rll@data \
		--import imported_function=rincrt.rll@function \
		-o $(TEST_OUT)/pic-got/x86.rin $(TEST_OUT)/pic-got/x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos -e pic_got_use \
		--emit-unsigned-v3 --dep rincrt.rll \
		--import imported_data=rincrt.rll@data \
		--import imported_function=rincrt.rll@function \
		-o $(TEST_OUT)/pic-got/x64.rin $(TEST_OUT)/pic-got/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/pic-got/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/pic-got/x64.rin
	@echo "PIC/PIE GOT32 object and RLD resolution tests completed"

# Exercise runtime scalar global initialization in both frontends.  The host
# CRT used by MinGW does not dispatch RinOS .init_array entries, so the x64
# execution check invokes the generated callback explicitly.  The image
# validators still check the emitted .init_array section and relocation for
# every current artifact family.
test-global-initializers: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/global-initializers)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/global-initializers/c-x86.s tests/c_global_initializers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/global-initializers/c-x64.s tests/c_global_initializers.c
	$(RCXX_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/global-initializers/cxx-x86.s \
		tests/cxx_global_initializers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/global-initializers/cxx-x64.s \
		tests/cxx_global_initializers.cpp
	$(call CHECK_INIT_ARRAY_FILE,$(TEST_OUT)/global-initializers/c-x86.s)
	$(call CHECK_INIT_ARRAY_FILE,$(TEST_OUT)/global-initializers/c-x64.s)
	$(call CHECK_INIT_ARRAY_FILE,$(TEST_OUT)/global-initializers/cxx-x86.s)
	$(call CHECK_INIT_ARRAY_FILE,$(TEST_OUT)/global-initializers/cxx-x64.s)
	$(CC) -m32 -c $(TEST_OUT)/global-initializers/c-x86.s \
		-o $(TEST_OUT)/global-initializers/c-x86.o
	$(CC) -m32 -c $(TEST_OUT)/global-initializers/cxx-x86.s \
		-o $(TEST_OUT)/global-initializers/cxx-x86.o
	$(CC) -c $(TEST_OUT)/global-initializers/c-x64.s \
		-o $(TEST_OUT)/global-initializers/c-x64.o
	$(CC) -c $(TEST_OUT)/global-initializers/cxx-x64.s \
		-o $(TEST_OUT)/global-initializers/cxx-x64.o
	$(OBJCOPY) --redefine-sym main=rcc_global_initializers_main \
		$(TEST_OUT)/global-initializers/c-x64.o
	$(CC) $(TEST_OUT)/global-initializers/c-x64.o \
		tests/cxx_global_initializers_host.c \
		-o $(TEST_OUT)/global-initializers/c-x64-host
	$(TEST_OUT)/global-initializers/c-x64-host
	$(OBJCOPY) --redefine-sym main=rcc_global_initializers_main \
		$(TEST_OUT)/global-initializers/cxx-x64.o
	$(CC) $(TEST_OUT)/global-initializers/cxx-x64.o \
		tests/cxx_global_initializers_host.c \
		-o $(TEST_OUT)/global-initializers/cxx-x64-host
	$(TEST_OUT)/global-initializers/cxx-x64-host
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/global-initializers/c-x86.ro \
		tests/c_global_initializers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/global-initializers/c-x64.ro \
		tests/c_global_initializers.c
	$(RCXX_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/global-initializers/cxx-x86.ro \
		tests/cxx_global_initializers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/global-initializers/cxx-x64.ro \
		tests/cxx_global_initializers.cpp
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-initializers/c-x86.rin \
		tests/c_global_initializers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-initializers/c-x64.rin \
		tests/c_global_initializers.c
	$(RCXX_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-initializers/cxx-x86.rll -shared \
		tests/cxx_global_initializers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-initializers/cxx-x64.rll -shared \
		tests/cxx_global_initializers.cpp
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-initializers/c-x86.drv -driver \
		tests/c_global_initializers.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-initializers/c-x64.drv -driver \
		tests/c_global_initializers.c
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-initializers/c-linked-x64.rin \
		$(TEST_OUT)/global-initializers/c-x64.ro
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-initializers/cxx-linked-x86.rin \
		$(TEST_OUT)/global-initializers/cxx-x86.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/global-initializers/c-x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/global-initializers/c-x64.rin
	$(RINVALIDATE) --kind library --arch x86 --allow-unsigned \
		$(TEST_OUT)/global-initializers/cxx-x86.rll
	$(RINVALIDATE) --kind library --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/global-initializers/cxx-x64.rll
	$(RINVALIDATE) --kind driver --arch x86 --allow-unsigned \
		$(TEST_OUT)/global-initializers/c-x86.drv
	$(RINVALIDATE) --kind driver --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/global-initializers/c-x64.drv
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/global-initializers/c-linked-x64.rin
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/global-initializers/cxx-linked-x86.rin
	@echo "C17/C++20 scalar global initializer and current image format tests completed"

test-cxx-global-constructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-global-constructor)
	$(RCXX_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/cxx-global-constructor/x86.s \
		tests/cxx_global_constructor.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/cxx-global-constructor/x64.s \
		tests/cxx_global_constructor.cpp
	$(CC) -m32 -c $(TEST_OUT)/cxx-global-constructor/x86.s \
		-o $(TEST_OUT)/cxx-global-constructor/x86.o
	$(CC) -c $(TEST_OUT)/cxx-global-constructor/x64.s \
		-o $(TEST_OUT)/cxx-global-constructor/x64.o
	$(OBJCOPY) --redefine-sym main=rcc_cxx_global_constructor_main \
		$(TEST_OUT)/cxx-global-constructor/x64.o
	$(CC) $(TEST_OUT)/cxx-global-constructor/x64.o \
		tests/cxx_global_constructor_host.c \
		-o $(TEST_OUT)/cxx-global-constructor/x64-host
	$(TEST_OUT)/cxx-global-constructor/x64-host
	$(RCXX_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-global-constructor/x86.ro \
		tests/cxx_global_constructor.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-global-constructor/x64.ro \
		tests/cxx_global_constructor.cpp
	@echo "C++ static-storage constructor execution tests completed"

test-global-finalizers: $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
	$(call MKDIR_P,$(TEST_OUT)/global-finalizers)
	$(RCXX_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/global-finalizers/x86.s tests/cxx_global_finalizers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/global-finalizers/x64.s tests/cxx_global_finalizers.cpp
	$(call CHECK_FINI_ARRAY_FILE,$(TEST_OUT)/global-finalizers/x86.s)
	$(call CHECK_FINI_ARRAY_FILE,$(TEST_OUT)/global-finalizers/x64.s)
	$(CC) -m32 -c $(TEST_OUT)/global-finalizers/x86.s \
		-o $(TEST_OUT)/global-finalizers/x86.o
	$(CC) -c $(TEST_OUT)/global-finalizers/x64.s \
		-o $(TEST_OUT)/global-finalizers/x64.o
	$(OBJCOPY) --redefine-sym main=rcc_global_finalizers_main \
		$(TEST_OUT)/global-finalizers/x64.o
	$(CC) $(TEST_OUT)/global-finalizers/x64.o \
		tests/cxx_global_finalizers_host.c \
		-o $(TEST_OUT)/global-finalizers/x64-host
	$(TEST_OUT)/global-finalizers/x64-host
	$(RCXX_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/global-finalizers/x86.ro \
		tests/cxx_global_finalizers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/global-finalizers/x64.ro \
		tests/cxx_global_finalizers.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-finalizers/x86.rin \
		tests/cxx_global_finalizers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-finalizers/x64.rin \
		tests/cxx_global_finalizers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-shared -o $(TEST_OUT)/global-finalizers/x64.rll \
		tests/cxx_global_finalizers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-driver -o $(TEST_OUT)/global-finalizers/x64.drv \
		tests/cxx_global_finalizers.cpp
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/global-finalizers/linked-x64.rin \
		$(TEST_OUT)/global-finalizers/x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/global-finalizers/x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/global-finalizers/x64.rin
	$(RINVALIDATE) --kind library --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/global-finalizers/x64.rll
	$(RINVALIDATE) --kind driver --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/global-finalizers/x64.drv
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/global-finalizers/linked-x64.rin
	@echo "C++ static-storage finalizer and current image format tests completed"

test-sanitize:
	$(MAKE) OBJDIR=$(SANITIZER_ROOT)/obj BINDIR=$(SANITIZER_ROOT)/bin \
		TEST_OUT=$(SANITIZER_ROOT)/tests \
		CFLAGS="$(CFLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined" \
		all test-ir test-ir-lowering test-verified-backend test-manifest \
		test-signing test-executable-imports test-tls
	$(SANITIZER_ROOT)/bin/rcc++ --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/class.ro tests/class_test.cpp
	$(SANITIZER_ROOT)/bin/rcc++ --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(SANITIZER_ROOT)/tests/cxx-member-specifiers.ro \
		tests/cxx_member_specifiers.cpp
	$(SANITIZER_ROOT)/bin/rcc++ --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(SANITIZER_ROOT)/tests/cxx-function-templates.ro \
		tests/cxx_function_templates.cpp
	$(SANITIZER_ROOT)/bin/rcc++ --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(SANITIZER_ROOT)/tests/cxx-qualified-namespaces.ro \
		tests/cxx_qualified_namespace.cpp
	$(SANITIZER_ROOT)/bin/rcc++ --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(SANITIZER_ROOT)/tests/cxx-overloads.ro tests/cxx_overload.cpp
	$(SANITIZER_ROOT)/bin/rcc++ --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(SANITIZER_ROOT)/tests/cxx-inline-aggregates.ro \
		tests/cxx_inline_aggregate.cpp
	@set +e; $(SANITIZER_ROOT)/bin/rcc++ --target x86_64-unknown-rinos \
		-std=c++20 -c -o $(SANITIZER_ROOT)/tests/cxx-invalid.ro \
		tests/cxx_parser_recovery.cpp \
		>$(SANITIZER_ROOT)/tests/cxx-invalid.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	! $(GREP) -q "too many errors" $(SANITIZER_ROOT)/tests/cxx-invalid.log
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/direct-x86.ro tests/direct_relocation.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -O1 -c \
		-o $(SANITIZER_ROOT)/tests/direct-x64.ro tests/direct_relocation.c
	$(SANITIZER_ROOT)/bin/rcc -E -Itests/include \
		-DRCC_CXX_CLI_VALUE=23 tests/preproc_v2.c > /dev/null
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-DRCC_CONTINUATION_LEFT -DRCC_CONTINUATION_RIGHT \
		-o $(SANITIZER_ROOT)/tests/preprocessor-continuation.ro \
		tests/preprocessor_continuation.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/atomic-builtin.ro \
		tests/atomic_builtin.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/x86-wide-scalar.ro \
		tests/x86_wide_scalar.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/integer-literal-x86.ro \
		tests/integer_literal.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/integer-literal-x64.ro \
		tests/integer_literal.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/integer-promotion-x86.ro \
		tests/integer_promotion.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/integer-promotion-x64.ro \
		tests/integer_promotion.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/integer-conversion-x86.ro \
		tests/integer_conversion.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/integer-conversion-x64.ro \
		tests/integer_conversion.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/function-call-x86.ro \
		tests/function_call.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/function-call-x64.ro \
		tests/function_call.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/inline-asm-x86.ro \
		tests/inline_asm_execution.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/inline-asm-x64.ro \
		tests/inline_asm_execution.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -nostdinc \
		-Ibootstrap/include -c \
		-o $(SANITIZER_ROOT)/tests/varargs-x86.ro tests/varargs.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c \
		-o $(SANITIZER_ROOT)/tests/varargs-x64.ro tests/varargs.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/initializer-override-x86.ro \
		tests/initializer_override.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/initializer-override-x64.ro \
		tests/initializer_override.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/scalar-comparison-x86.ro \
		tests/scalar_comparison.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/scalar-comparison-x64.ro \
		tests/scalar_comparison.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/aggregate-copy-x86.ro \
		tests/aggregate_copy.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/aggregate-copy-x64.ro \
		tests/aggregate_copy.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/aggregate-return-x86.ro \
		tests/aggregate_return.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/aggregate-return-x64.ro \
		tests/aggregate_return.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/compound-literal-x86.ro \
		tests/compound_literal.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/compound-literal-x64.ro \
		tests/compound_literal.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -nostdinc \
		-Ibootstrap/include -c \
		-o $(SANITIZER_ROOT)/tests/pragma-pack-x86.ro \
		tests/pragma_pack.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c \
		-o $(SANITIZER_ROOT)/tests/pragma-pack-x64.ro \
		tests/pragma_pack.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/compound-assignment-x86.ro \
		tests/compound_assignment.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/compound-assignment-x64.ro \
		tests/compound_assignment.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/switch-statement-x86.ro \
		tests/switch_statement.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/switch-statement-x64.ro \
		tests/switch_statement.c
	$(SANITIZER_ROOT)/bin/rcc --target i686-unknown-rinos -O1 -c \
		-o $(SANITIZER_ROOT)/tests/control-flow-x86.ro \
		tests/control_flow.c
	$(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -O1 -c \
		-o $(SANITIZER_ROOT)/tests/control-flow-x64.ro \
		tests/control_flow.c
	@if $(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/parser-recovery.ro \
		tests/parser_recovery.c \
		>$(SANITIZER_ROOT)/tests/parser-recovery.log 2>&1; then \
		echo "parser recovery fixture unexpectedly compiled"; exit 1; \
	fi
	$(GREP) -q "expected parameter type specifier" \
		$(SANITIZER_ROOT)/tests/parser-recovery.log
	$(GREP) -q "expected field type specifier" \
		$(SANITIZER_ROOT)/tests/parser-recovery.log
	! $(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/invalid.ro \
		tests/invalid_designated_initializer.c
	! $(SANITIZER_ROOT)/bin/rcc++ -driver --emit-unsigned-v3 \
		-o $(SANITIZER_ROOT)/tests/invalid.drv tests/driver_policy_float.cpp
	@echo "ASan/UBSan and translation-unit lifetime tests completed"

test-driver-policy: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) --target i686-unknown-rinos -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_x86.drv tests/driver_policy_ok.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_x64.drv tests/driver_policy_ok.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_float.drv tests/driver_policy_float.c,$(TEST_OUT)/driver_policy_float.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_float_cxx.drv tests/driver_policy_float.cpp,$(TEST_OUT)/driver_policy_float_cxx.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_asm.drv tests/driver_policy_asm.c,$(TEST_OUT)/driver_policy_asm.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_constraint.drv tests/driver_policy_constraint.c,$(TEST_OUT)/driver_policy_constraint.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_clobber.drv tests/driver_policy_clobber.c,$(TEST_OUT)/driver_policy_clobber.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_mask_constraint.drv tests/driver_policy_mask_constraint.c,$(TEST_OUT)/driver_policy_mask_constraint.log)
	@echo "NDRV FPU/SIMD policy tests completed"

test-weak-link:
	$(call MKDIR_P,$(TEST_OUT))
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/weak_link_test \
		tests/weak_link_test.c $(SRCDIR)/linker.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/archive.c $(SRCDIR)/utils.c
	$(TEST_OUT)/weak_link_test $(TEST_OUT)/weak.ro $(TEST_OUT)/strong.ro
	@echo "Weak-to-strong linker replacement test completed"

test-weak-attribute: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/weak-attribute)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/weak-attribute/x86.ro tests/weak_attribute.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/weak-attribute/x64.ro tests/weak_attribute.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/weak_attribute_test \
		tests/weak_attribute_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/weak_attribute_test \
		$(TEST_OUT)/weak-attribute/x86.ro \
		$(TEST_OUT)/weak-attribute/x64.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/weak-attribute/linked-x64.rin \
		$(TEST_OUT)/weak-attribute/x64.ro
	../rinvalidate/rinvalidate.exe --kind executable --arch x86_64 \
		--allow-unsigned $(TEST_OUT)/weak-attribute/linked-x64.rin
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/weak-attribute/cxx-x86.ro tests/weak_attribute.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/weak-attribute/cxx-x64.ro tests/weak_attribute.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/weak_attribute_cpp_test \
		tests/weak_attribute_cpp_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/weak_attribute_cpp_test \
		$(TEST_OUT)/weak-attribute/cxx-x86.ro \
		$(TEST_OUT)/weak-attribute/cxx-x64.ro
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/weak-attribute/invalid.ro \
		tests/invalid_weak_attribute.c,$(TEST_OUT)/weak-attribute/invalid.log)
	$(GREP) -F -q "weak variable declaration requires external linkage" \
		$(TEST_OUT)/weak-attribute/invalid.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/weak-attribute/invalid-arguments.ro \
		tests/invalid_weak_attribute_arguments.c,$(TEST_OUT)/weak-attribute/invalid-arguments.log)
	$(GREP) -F -q "weak attribute does not accept arguments" \
		$(TEST_OUT)/weak-attribute/invalid-arguments.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/weak-attribute/invalid-cxx.ro \
		tests/invalid_cxx_weak_attribute.cpp,$(TEST_OUT)/weak-attribute/invalid-cxx.log)
	$(GREP) -F -q "[[gnu::weak]] does not accept arguments" \
		$(TEST_OUT)/weak-attribute/invalid-cxx.log
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/weak-attribute/invalid-cxx-member.ro \
		tests/invalid_cxx_weak_attribute_member.cpp,$(TEST_OUT)/weak-attribute/invalid-cxx-member.log)
	$(GREP) -F -q "[[gnu::weak]] requires a file-scope declaration" \
		$(TEST_OUT)/weak-attribute/invalid-cxx-member.log

test-comdat-link:
	$(call MKDIR_P,$(TEST_OUT)/comdat)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/comdat_link_test \
		tests/comdat_link_test.c $(SRCDIR)/linker.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/archive.c $(SRCDIR)/utils.c
	$(TEST_OUT)/comdat_link_test \
		$(TEST_OUT)/comdat/x86-first.ro $(TEST_OUT)/comdat/x86-second.ro \
		$(TEST_OUT)/comdat/x86-duplicate-a.ro \
		$(TEST_OUT)/comdat/x86-duplicate-b.ro x86
	$(TEST_OUT)/comdat_link_test \
		$(TEST_OUT)/comdat/x64-first.ro $(TEST_OUT)/comdat/x64-second.ro \
		$(TEST_OUT)/comdat/x64-duplicate-a.ro \
		$(TEST_OUT)/comdat/x64-duplicate-b.ro x64
	@echo "COMDAT ANY group selection and metadata rejection tests completed"

test-object-width: $(RCC_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/object_width_test \
		tests/object_width_test.c $(SRCDIR)/linker.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/archive.c $(SRCDIR)/utils.c
	$(TEST_OUT)/object_width_test $(TEST_OUT)/wide.ro $(TEST_OUT)/legacy-abs32.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/wide_main.ro tests/main.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/wide_lib.ro tests/lib.c
	$(RLD_TARGET) -m64 -T 0x100000000 --emit-unsigned-v3 \
		-o $(TEST_OUT)/wide_base.rin $(TEST_OUT)/wide_main.ro \
		$(TEST_OUT)/wide_lib.ro
	$(call EXPECT_FAILURE,$(RLD_TARGET) -T invalid-address --emit-unsigned-v3 \
		-o $(TEST_OUT)/invalid_base.rin $(TEST_OUT)/wide_main.ro,$(TEST_OUT)/invalid_base.log)
	@echo "64-bit object/linker width and typed relocation tests completed"

test-special-sections:
	$(call MKDIR_P,$(TEST_OUT)/special)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/special_sections_test \
		tests/special_sections_test.c $(SRCDIR)/linker.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/archive.c $(SRCDIR)/utils.c
	$(TEST_OUT)/special_sections_test \
		$(TEST_OUT)/special/x86.ro $(TEST_OUT)/special/x86.rin \
		$(TEST_OUT)/special/x64.ro $(TEST_OUT)/special/x64.rin \
		$(TEST_OUT)/special/wx.ro $(TEST_OUT)/special/bad-array.ro \
		$(TEST_OUT)/special/conflict-a.ro $(TEST_OUT)/special/conflict-b.ro
	@echo "TLS/unwind/init/fini section propagation tests completed"

test-tls: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/tls)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/tls/x86.ro tests/tls.c
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/tls/x86.rin tests/tls.c
	$(RLD_TARGET) -m32 --emit-unsigned-v3 \
		-o $(TEST_OUT)/tls/x86-linked.rin $(TEST_OUT)/tls/x86.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/tls/x64.ro tests/tls.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/tls/x64.rin tests/tls.c
	$(RLD_TARGET) -m64 --emit-unsigned-v3 \
		-o $(TEST_OUT)/tls/x64-linked.rin $(TEST_OUT)/tls/x64.ro
	$(RCXX_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/tls/cxx-x86.ro tests/tls.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/tls/cxx-x64.ro tests/tls.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/tls_codegen_test \
		tests/tls_codegen_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/tls/invalid-local.ro tests/tls_invalid_local.c,$(TEST_OUT)/tls/invalid-local.log)
	$(RCC_TARGET) --target i686-unknown-rinos -shared \
		--emit-unsigned-v3 -o $(TEST_OUT)/tls/x86.rll tests/tls.c
	$(RLD_TARGET) -m32 -shared --emit-unsigned-v3 \
		-o $(TEST_OUT)/tls/x86-linked.rll $(TEST_OUT)/tls/x86.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -shared \
		--emit-unsigned-v3 -o $(TEST_OUT)/tls/x64.rll tests/tls.c
	$(RLD_TARGET) -m64 -shared --emit-unsigned-v3 \
		-o $(TEST_OUT)/tls/x64-linked.rll $(TEST_OUT)/tls/x64.ro
	$(TEST_OUT)/tls_codegen_test \
		$(TEST_OUT)/tls/x86.ro $(TEST_OUT)/tls/x86.rin \
		$(TEST_OUT)/tls/x86-linked.rin $(TEST_OUT)/tls/x64.ro \
		$(TEST_OUT)/tls/x64.rin $(TEST_OUT)/tls/x64-linked.rin \
		$(TEST_OUT)/tls/cxx-x86.ro $(TEST_OUT)/tls/cxx-x64.ro \
		$(TEST_OUT)/tls/x86.rll $(TEST_OUT)/tls/x86-linked.rll \
		$(TEST_OUT)/tls/x64.rll $(TEST_OUT)/tls/x64-linked.rll
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -driver \
		--emit-unsigned-v3 -o $(TEST_OUT)/tls/invalid.drv tests/tls.c,$(TEST_OUT)/tls/invalid-driver.log)
	@echo "C17/C++20 local-exec TLS tests completed"

test-direct-relocation: $(RCC_TARGET) $(RLD_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/direct)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/x86.ro tests/direct_relocation.c
	$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/x86.rin tests/direct_relocation.c
	$(RLD_TARGET) -m32 --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/x86-linked.rin $(TEST_OUT)/direct/x86.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/x64.ro tests/direct_relocation.c
	$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/x64.rin tests/direct_relocation.c
	$(RLD_TARGET) -m64 --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/x64-linked.rin $(TEST_OUT)/direct/x64.ro
	$(RCC_TARGET) --target i686-unknown-rinos -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/x86.drv tests/direct_relocation.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/x64.drv tests/direct_relocation.c
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unresolved.ro tests/direct_unresolved.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/unresolved.rin tests/direct_unresolved.c,$(TEST_OUT)/direct/unresolved.log)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/definition-x86.ro \
		tests/direct_unresolved_definition.c
	$(RLD_TARGET) -m32 --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/resolved-x86.rin \
		$(TEST_OUT)/direct/unresolved.ro \
		$(TEST_OUT)/direct/definition-x86.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unresolved-x64.ro tests/direct_unresolved.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/unresolved-x64.rin tests/direct_unresolved.c,$(TEST_OUT)/direct/unresolved-x64.log)
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/definition-x64.ro \
		tests/direct_unresolved_definition.c
	$(RLD_TARGET) -m64 --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/resolved-x64.rin \
		$(TEST_OUT)/direct/unresolved-x64.ro \
		$(TEST_OUT)/direct/definition-x64.ro
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/bss-reference-x86.ro \
		tests/direct_bss_unresolved.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/bss-unresolved-x86.rin \
		tests/direct_bss_unresolved.c,$(TEST_OUT)/direct/bss-unresolved-x86.log)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/bss-definition-x86.ro \
		tests/direct_bss_definition.c
	$(RLD_TARGET) -m32 --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/bss-resolved-x86.rin \
		$(TEST_OUT)/direct/bss-reference-x86.ro \
		$(TEST_OUT)/direct/bss-definition-x86.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/bss-reference-x64.ro \
		tests/direct_bss_unresolved.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/bss-definition-x64.ro \
		tests/direct_bss_definition.c
	$(RLD_TARGET) -m64 --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/bss-resolved-x64.rin \
		$(TEST_OUT)/direct/bss-reference-x64.ro \
		$(TEST_OUT)/direct/bss-definition-x64.ro
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/global-redefinition.ro \
		tests/global_redefinition.c,$(TEST_OUT)/direct/global-redefinition.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/global-conflict.ro tests/global_conflict.c,$(TEST_OUT)/direct/global-conflict.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unsupported-static-x86.ro \
		tests/unsupported_static_pointer.c,$(TEST_OUT)/direct/unsupported-static-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unsupported-static-x64.ro \
		tests/unsupported_static_pointer.c,$(TEST_OUT)/direct/unsupported-static-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-array-x86.ro \
		tests/invalid_array_initializer.c,$(TEST_OUT)/direct/invalid-array-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-array-x64.ro \
		tests/invalid_array_initializer.c,$(TEST_OUT)/direct/invalid-array-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-designator-x86.ro \
		tests/invalid_designated_initializer.c,$(TEST_OUT)/direct/invalid-designator-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-designator-x64.ro \
		tests/invalid_designated_initializer.c,$(TEST_OUT)/direct/invalid-designator-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/nonconstant-designator-x86.ro \
		tests/nonconstant_designator.c,$(TEST_OUT)/direct/nonconstant-designator-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/nonconstant-designator-x64.ro \
		tests/nonconstant_designator.c,$(TEST_OUT)/direct/nonconstant-designator-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/empty-initializer-x86.ro \
		tests/empty_initializer.c,$(TEST_OUT)/direct/empty-initializer-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/empty-initializer-x64.ro \
		tests/empty_initializer.c,$(TEST_OUT)/direct/empty-initializer-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unsupported-local-array-x86.ro \
		tests/unsupported_local_array_initializer.c,$(TEST_OUT)/direct/unsupported-local-array-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unsupported-local-array-x64.ro \
		tests/unsupported_local_array_initializer.c,$(TEST_OUT)/direct/unsupported-local-array-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-constant-x86.ro \
		tests/invalid_static_constant.c,$(TEST_OUT)/direct/invalid-constant-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-constant-x64.ro \
		tests/invalid_static_constant.c,$(TEST_OUT)/direct/invalid-constant-x64.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-pointer-x86.ro \
		tests/invalid_pointer_arithmetic.c,$(TEST_OUT)/direct/invalid-pointer-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-pointer-x64.ro \
		tests/invalid_pointer_arithmetic.c,$(TEST_OUT)/direct/invalid-pointer-x64.log)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/direct_relocation_test \
		tests/direct_relocation_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/pointer_arithmetic_run_test \
		tests/pointer_arithmetic_run_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/direct_relocation_test \
		$(TEST_OUT)/direct/x86.ro $(TEST_OUT)/direct/x86.rin \
		$(TEST_OUT)/direct/x64.ro $(TEST_OUT)/direct/x64.rin \
		$(TEST_OUT)/direct/x86.ro $(TEST_OUT)/direct/x86.drv \
		$(TEST_OUT)/direct/x64.ro $(TEST_OUT)/direct/x64.drv \
		$(TEST_OUT)/direct/x86.ro $(TEST_OUT)/direct/x86-linked.rin \
		$(TEST_OUT)/direct/x64.ro $(TEST_OUT)/direct/x64-linked.rin \
		$(TEST_OUT)/direct/unresolved.ro \
		$(TEST_OUT)/direct/definition-x86.ro \
		$(TEST_OUT)/direct/resolved-x86.rin \
		$(TEST_OUT)/direct/unresolved-x64.ro \
		$(TEST_OUT)/direct/definition-x64.ro \
		$(TEST_OUT)/direct/resolved-x64.rin
	$(TEST_OUT)/pointer_arithmetic_run_test $(TEST_OUT)/direct/x64.ro
	@echo "Direct RIN/NDRV v3 symbol relocation tests completed"

test-ir:
	$(call MKDIR_P,$(TEST_OUT))
	$(CC) $(IR_X86_HOST_FLAGS) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/ir_test-x86 \
		tests/ir_test.c $(SRCDIR)/ir.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/ir_test-x64 \
		tests/ir_test.c $(SRCDIR)/ir.c $(SRCDIR)/utils.c
	$(CC) $(IR_X86_HOST_FLAGS) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/ir_mem2reg_test-x86 \
		tests/ir_mem2reg_test.c $(SRCDIR)/ir.c $(SRCDIR)/ir_pass.c \
		$(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/ir_mem2reg_test-x64 \
		tests/ir_mem2reg_test.c $(SRCDIR)/ir.c $(SRCDIR)/ir_pass.c \
		$(SRCDIR)/utils.c
	$(CC) $(IR_X86_HOST_FLAGS) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/mir_test-x86 \
		tests/mir_test.c $(SRCDIR)/ir.c $(SRCDIR)/mir.c \
		$(SRCDIR)/mir_alloc.c $(SRCDIR)/mir_phi.c \
		$(SRCDIR)/x86_abi.c $(SRCDIR)/x86_select.c \
		$(SRCDIR)/x86_legalize.c $(SRCDIR)/x86_encode.c \
		$(SRCDIR)/x86_object.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/mir_test-x64 \
		tests/mir_test.c $(SRCDIR)/ir.c $(SRCDIR)/mir.c \
		$(SRCDIR)/mir_alloc.c $(SRCDIR)/mir_phi.c \
		$(SRCDIR)/x86_abi.c $(SRCDIR)/x86_select.c \
		$(SRCDIR)/x86_legalize.c $(SRCDIR)/x86_encode.c \
		$(SRCDIR)/x86_object.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(CC) $(IR_X86_HOST_FLAGS) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/x86_encode_run_test-x86 \
		tests/x86_encode_run_test.c $(SRCDIR)/ir.c $(SRCDIR)/mir.c \
		$(SRCDIR)/mir_alloc.c $(SRCDIR)/mir_phi.c \
		$(SRCDIR)/x86_abi.c $(SRCDIR)/x86_select.c \
		$(SRCDIR)/x86_legalize.c $(SRCDIR)/x86_encode.c \
		$(SRCDIR)/x86_object.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/x86_encode_run_test-x64 \
		tests/x86_encode_run_test.c $(SRCDIR)/ir.c $(SRCDIR)/mir.c \
		$(SRCDIR)/mir_alloc.c $(SRCDIR)/mir_phi.c \
		$(SRCDIR)/x86_abi.c $(SRCDIR)/x86_select.c \
		$(SRCDIR)/x86_legalize.c $(SRCDIR)/x86_encode.c \
		$(SRCDIR)/x86_object.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/ir_test-x86
	$(TEST_OUT)/ir_test-x64
	$(TEST_OUT)/ir_mem2reg_test-x86
	$(TEST_OUT)/ir_mem2reg_test-x64
	$(TEST_OUT)/mir_test-x86
	$(TEST_OUT)/mir_test-x64
	$(TEST_OUT)/x86_encode_run_test-x86 \
		$(TEST_OUT)/encoded-native-x86.ro
	$(IR_X86_INSPECT_CMD)
	$(TEST_OUT)/x86_encode_run_test-x64 \
		$(TEST_OUT)/encoded-native-x64.ro

test-ir-lowering: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/ir-lowering)
	$(RCC_TARGET) --target i686-unknown-rinos -O1 -v -c \
		-o $(TEST_OUT)/ir-lowering/x86.ro tests/ir_lowering.c \
		>$(TEST_OUT)/ir-lowering/x86.log
	$(GREP) -F -q 'Typed SSA shadow verification: 4 function(s)' $(TEST_OUT)/ir-lowering/x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -O3 -v -c \
		-o $(TEST_OUT)/ir-lowering/x64.ro tests/ir_lowering.c \
		>$(TEST_OUT)/ir-lowering/x64.log
	$(GREP) -F -q 'Typed SSA shadow verification: 4 function(s)' $(TEST_OUT)/ir-lowering/x64.log
	@echo "Dual-architecture scalar AST to typed SSA lowering tests completed"

.PHONY: test-verified-goto test-verified-builtins test-verified-bitcounts

test-verified-goto: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/goto-x86.ro \
		tests/verified_backend_goto.c \
		>$(TEST_OUT)/verified-backend/goto-x86.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' $(TEST_OUT)/verified-backend/goto-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/goto-x64.ro \
		tests/verified_backend_goto.c \
		>$(TEST_OUT)/verified-backend/goto-x64.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' $(TEST_OUT)/verified-backend/goto-x64.log
	@echo "Verified backend goto/label tests completed"

test-verified-builtins: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/builtins-x86.ro \
		tests/verified_backend_builtins.c \
		>$(TEST_OUT)/verified-backend/builtins-x86.log
	$(GREP) -F -q 'Verified backend: 14 function(s) emitted' \
		$(TEST_OUT)/verified-backend/builtins-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/builtins-x64.ro \
		tests/verified_backend_builtins.c \
		>$(TEST_OUT)/verified-backend/builtins-x64.log
	$(GREP) -F -q 'Verified backend: 15 function(s) emitted' \
		$(TEST_OUT)/verified-backend/builtins-x64.log
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/bswap64-x86.ro \
		tests/verified_backend_bswap64.c \
		>$(TEST_OUT)/verified-backend/bswap64-x86.log
	$(GREP) -F -q 'Verified backend: 8 function(s) emitted' \
		$(TEST_OUT)/verified-backend/bswap64-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/bswap64-x64.ro \
		tests/verified_backend_bswap64.c \
		>$(TEST_OUT)/verified-backend/bswap64-x64.log
	$(GREP) -F -q 'Verified backend: 8 function(s) emitted' \
		$(TEST_OUT)/verified-backend/bswap64-x64.log
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/bswap64-run \
		tests/verified_backend_bswap64_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/bswap64-run \
		$(TEST_OUT)/verified-backend/bswap64-x64.ro
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/verified-backend/assume-legacy-x86.ro \
		tests/verified_backend_bswap64.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/verified-backend/assume-legacy-x64.ro \
		tests/verified_backend_bswap64.c
	$(TEST_OUT)/verified-backend/bswap64-run \
		$(TEST_OUT)/verified-backend/assume-legacy-x64.ro
	$(RCXX_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-builtins-x64.ro \
		tests/verified_backend_builtins.cpp \
		>$(TEST_OUT)/verified-backend/cxx-builtins-x64.log
	$(GREP) -F -q 'Verified backend: 19 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-builtins-x64.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos \
		-fverified-backend -c \
		-o $(TEST_OUT)/verified-backend/invalid-assume-x86.ro \
		tests/invalid_builtin_assume_aligned.c,\
		$(TEST_OUT)/verified-backend/invalid-assume-x86.log)
	$(GREP) -F -q 'alignment must be a positive power of two constant' \
		$(TEST_OUT)/verified-backend/invalid-assume-x86.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos \
		-fverified-backend -c \
		-o $(TEST_OUT)/verified-backend/invalid-assume-x64.ro \
		tests/invalid_builtin_assume_aligned.c,\
		$(TEST_OUT)/verified-backend/invalid-assume-x64.log)
	$(GREP) -F -q 'alignment must be a positive power of two constant' \
		$(TEST_OUT)/verified-backend/invalid-assume-x64.log
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/overflow-x86.ro \
		tests/verified_backend_overflow.c \
		>$(TEST_OUT)/verified-backend/overflow-x86.log
	$(GREP) -F -q 'Verified backend: 14 function(s) emitted' \
		$(TEST_OUT)/verified-backend/overflow-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/overflow-x64.ro \
		tests/verified_backend_overflow.c \
		>$(TEST_OUT)/verified-backend/overflow-x64.log
	$(GREP) -F -q 'Verified backend: 10 function(s) emitted' \
		$(TEST_OUT)/verified-backend/overflow-x64.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/overflow-cxx-x86.ro \
		tests/verified_backend_overflow.cpp \
		>$(TEST_OUT)/verified-backend/overflow-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 14 function(s) emitted' \
		$(TEST_OUT)/verified-backend/overflow-cxx-x86.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/overflow-cxx-x64.ro \
		tests/verified_backend_overflow.cpp \
		>$(TEST_OUT)/verified-backend/overflow-cxx-x64.log
	$(GREP) -F -q 'Verified backend: 10 function(s) emitted' \
		$(TEST_OUT)/verified-backend/overflow-cxx-x64.log
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/overflow-run \
		tests/verified_backend_overflow_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/overflow-run \
		$(TEST_OUT)/verified-backend/overflow-x64.ro
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/object-size-x86.ro \
		tests/verified_backend_object_size.c \
		>$(TEST_OUT)/verified-backend/object-size-x86.log
	$(GREP) -F -q 'Verified backend: 6 function(s) emitted' \
		$(TEST_OUT)/verified-backend/object-size-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/object-size-x64.ro \
		tests/verified_backend_object_size.c \
		>$(TEST_OUT)/verified-backend/object-size-x64.log
	$(GREP) -F -q 'Verified backend: 6 function(s) emitted' \
		$(TEST_OUT)/verified-backend/object-size-x64.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/object-size-cxx-x86.ro \
		tests/verified_backend_object_size.cpp \
		>$(TEST_OUT)/verified-backend/object-size-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/object-size-cxx-x86.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/object-size-cxx-x64.ro \
		tests/verified_backend_object_size.cpp \
		>$(TEST_OUT)/verified-backend/object-size-cxx-x64.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/object-size-cxx-x64.log
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/object-size-run \
		tests/verified_backend_object_size_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/object-size-run \
		$(TEST_OUT)/verified-backend/object-size-x64.ro
	@echo "Legacy and verified terminating/prediction/constant-p/ffs/assume-aligned/checked-arithmetic builtin tests completed"

test-verified-bitcounts: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/bitcounts-x86.ro \
		tests/verified_backend_bitcounts.c \
		>$(TEST_OUT)/verified-backend/bitcounts-x86.log
	$(GREP) -F -q 'Verified backend: 8 function(s) emitted' \
		$(TEST_OUT)/verified-backend/bitcounts-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/bitcounts-x64.ro \
		tests/verified_backend_bitcounts.c \
		>$(TEST_OUT)/verified-backend/bitcounts-x64.log
	$(GREP) -F -q 'Verified backend: 13 function(s) emitted' \
		$(TEST_OUT)/verified-backend/bitcounts-x64.log
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/bitcounts-run \
		tests/verified_backend_bitcounts_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/bitcounts-run \
		$(TEST_OUT)/verified-backend/bitcounts-x64.ro
	@echo "Verified backend clz/ctz/popcount/ffs/clrsb tests completed"

test-verified-volatile: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCC_TARGET) --target i686-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/volatile-x86.ro \
		tests/verified_backend_volatile.c \
		>$(TEST_OUT)/verified-backend/volatile-x86.log
	$(GREP) -F -q 'Verified backend: 5 function(s) emitted' \
		$(TEST_OUT)/verified-backend/volatile-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/volatile-x64.ro \
		tests/verified_backend_volatile.c \
		>$(TEST_OUT)/verified-backend/volatile-x64.log
	$(GREP) -F -q 'Verified backend: 5 function(s) emitted' \
		$(TEST_OUT)/verified-backend/volatile-x64.log
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/volatile-cxx-x86.ro \
		tests/verified_backend_volatile.cpp \
		>$(TEST_OUT)/verified-backend/volatile-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 4 function(s) emitted' \
		$(TEST_OUT)/verified-backend/volatile-cxx-x86.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/volatile-cxx-x64.ro \
		tests/verified_backend_volatile.cpp \
		>$(TEST_OUT)/verified-backend/volatile-cxx-x64.log
	$(GREP) -F -q 'Verified backend: 4 function(s) emitted' \
		$(TEST_OUT)/verified-backend/volatile-cxx-x64.log
	@echo "Verified backend volatile scalar access tests completed"

test-verified-cxx-reference-local: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-reference-local-x86.ro \
		tests/verified_backend_cxx_reference_local.cpp \
		>$(TEST_OUT)/verified-backend/cxx-reference-local-x86.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-reference-local-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-reference-local-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-reference-local-x64.ro \
		tests/verified_backend_cxx_reference_local.cpp \
		>$(TEST_OUT)/verified-backend/cxx-reference-local-x64.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-reference-local-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-reference-local-x64.log,0)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/cxx-reference-local-run \
		tests/verified_backend_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/cxx-reference-local-run \
		--cxx-reference-object \
		$(TEST_OUT)/verified-backend/cxx-reference-local-x86.ro x86
	$(TEST_OUT)/verified-backend/cxx-reference-local-run \
		--cxx-reference-object \
		$(TEST_OUT)/verified-backend/cxx-reference-local-x64.ro x64
	$(call MKDIR_P,$(TEST_OUT)/verified-cxx-reference-local)
	$(call CXX_WINDOWS_ENTRY_TEST,verified-cxx-reference-local,verified_backend_cxx_reference_local.cpp)

test-verified-cxx-reference-return: $(RCXX_TARGET) test-verified-cxx-reference-local
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-reference-return-x86.ro \
		tests/verified_backend_cxx_reference_return.cpp \
		>$(TEST_OUT)/verified-backend/cxx-reference-return-x86.log
	$(GREP) -F -q 'Verified backend: 2 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-reference-return-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-reference-return-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-reference-return-x64.ro \
		tests/verified_backend_cxx_reference_return.cpp \
		>$(TEST_OUT)/verified-backend/cxx-reference-return-x64.log
	$(GREP) -F -q 'Verified backend: 2 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-reference-return-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-reference-return-x64.log,0)
	$(TEST_OUT)/verified-backend/cxx-reference-local-run \
		--cxx-reference-object \
		$(TEST_OUT)/verified-backend/cxx-reference-return-x86.ro x86
	$(TEST_OUT)/verified-backend/cxx-reference-local-run \
		--cxx-reference-object \
		$(TEST_OUT)/verified-backend/cxx-reference-return-x64.ro x64

test-verified-cxx-conditional-aggregate: $(RCXX_TARGET) test-verified-cxx-reference-local
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x86.ro \
		tests/verified_backend_cxx_conditional_aggregate.cpp \
		>$(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x86.log
	$(GREP) -F -q 'Verified backend: 4 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x64.ro \
		tests/verified_backend_cxx_conditional_aggregate.cpp \
		>$(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x64.log
	$(GREP) -F -q 'Verified backend: 4 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x64.log,0)
	$(TEST_OUT)/verified-backend/cxx-reference-local-run \
		--cxx-reference-object \
		$(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x86.ro x86
	$(TEST_OUT)/verified-backend/cxx-reference-local-run \
		--cxx-reference-object \
		$(TEST_OUT)/verified-backend/cxx-conditional-aggregate-x64.ro x64
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-nontrivial-conditional-fallback-x64.ro \
		tests/cxx_function_template_references.cpp \
		>$(TEST_OUT)/verified-backend/cxx-nontrivial-conditional-fallback-x64.log
	$(GREP) -F -q "Verified backend fallback: function 'read_conditional_lifetime_argument' is outside the typed SSA subset" \
		$(TEST_OUT)/verified-backend/cxx-nontrivial-conditional-fallback-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-nontrivial-conditional-fallback-x64.log,1)

test-verified-cxx-temporary-cleanup: $(RCXX_TARGET) test-verified-cxx-reference-local
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x86.ro \
		tests/verified_backend_cxx_temporary_cleanup.cpp \
		>$(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x86.log
	$(GREP) -F -q 'Verified backend: 14 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x64.ro \
		tests/verified_backend_cxx_temporary_cleanup.cpp \
		>$(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x64.log
	$(GREP) -F -q 'Verified backend: 14 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x64.log,0)
	$(TEST_OUT)/verified-backend/cxx-reference-local-run \
		--cxx-reference-object \
		$(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x86.ro x86
	$(TEST_OUT)/verified-backend/cxx-reference-local-run \
		--cxx-reference-object \
		$(TEST_OUT)/verified-backend/cxx-temporary-cleanup-x64.ro x64

test-verified-i686-floating-arithmetic: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-i686-x86.ro tests/verified_backend_float_i686_arithmetic.c >$(TEST_OUT)/verified-backend/float-i686-x86.log
	$(GREP) -F -q 'Verified backend: 13 function(s) emitted' $(TEST_OUT)/verified-backend/float-i686-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-i686-x86.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -O2 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-i686-x86-o2.ro tests/verified_backend_float_i686_arithmetic.c >$(TEST_OUT)/verified-backend/float-i686-x86-o2.log
	$(GREP) -F -q 'Verified backend: 13 function(s) emitted' $(TEST_OUT)/verified-backend/float-i686-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-i686-x86-o2.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-i686-cxx-x86.ro tests/verified_backend_float_i686_arithmetic.c >$(TEST_OUT)/verified-backend/float-i686-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 13 function(s) emitted' $(TEST_OUT)/verified-backend/float-i686-cxx-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-i686-cxx-x86.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2.ro tests/verified_backend_float_i686_arithmetic.c >$(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2.log
	$(GREP) -F -q 'Verified backend: 13 function(s) emitted' $(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2.log,0)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/verified-backend/verify-float-i686-x87 tests/verified_backend_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/verify-float-i686-x87 --i686-float-arithmetic-object $(TEST_OUT)/verified-backend/float-i686-x86.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-x87 --i686-float-arithmetic-object $(TEST_OUT)/verified-backend/float-i686-x86-o2.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-x87 --i686-float-arithmetic-object $(TEST_OUT)/verified-backend/float-i686-cxx-x86.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-x87 --i686-float-arithmetic-object $(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2.ro
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/verified-backend/verified-i686-asm tests/verified_backend_i686_asm.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/verified-i686-asm $(TEST_OUT)/verified-backend/float-i686-x86.ro $(TEST_OUT)/verified-backend/float-i686-x86.s
	$(TEST_OUT)/verified-backend/verified-i686-asm $(TEST_OUT)/verified-backend/float-i686-x86-o2.ro $(TEST_OUT)/verified-backend/float-i686-x86-o2.s
	$(TEST_OUT)/verified-backend/verified-i686-asm $(TEST_OUT)/verified-backend/float-i686-cxx-x86.ro $(TEST_OUT)/verified-backend/float-i686-cxx-x86.s
	$(TEST_OUT)/verified-backend/verified-i686-asm $(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2.ro $(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2.s
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S -o $(TEST_OUT)/verified-backend/float-i686-harness.s tests/verified_backend_float_i686_harness.c
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-i686-x86-run,$(TEST_OUT)/verified-backend/float-i686-x86.s $(TEST_OUT)/verified-backend/float-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-i686-x86-o2-run,$(TEST_OUT)/verified-backend/float-i686-x86-o2.s $(TEST_OUT)/verified-backend/float-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-i686-cxx-x86-run,$(TEST_OUT)/verified-backend/float-i686-cxx-x86.s $(TEST_OUT)/verified-backend/float-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2-run,$(TEST_OUT)/verified-backend/float-i686-cxx-x86-o2.s $(TEST_OUT)/verified-backend/float-i686-harness.s)
	@echo "Verified i686 x87 typed floating arithmetic C/C++ O0/O2 execution passed"

test-verified-i686-floating-comparisons: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-compare-i686-x86.ro tests/verified_backend_i686_float_compare.c >$(TEST_OUT)/verified-backend/float-compare-i686-x86.log
	$(GREP) -F -q 'Verified backend: 4 function(s) emitted' $(TEST_OUT)/verified-backend/float-compare-i686-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-compare-i686-x86.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -O2 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-compare-i686-x86-o2.ro tests/verified_backend_i686_float_compare.c >$(TEST_OUT)/verified-backend/float-compare-i686-x86-o2.log
	$(GREP) -F -q 'Verified backend: 4 function(s) emitted' $(TEST_OUT)/verified-backend/float-compare-i686-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-compare-i686-x86-o2.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86.ro tests/verified_backend_i686_float_compare.c >$(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 4 function(s) emitted' $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2.ro tests/verified_backend_i686_float_compare.c >$(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2.log
	$(GREP) -F -q 'Verified backend: 4 function(s) emitted' $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2.log,0)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/verified-backend/verify-float-i686-compare tests/verified_backend_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/verify-float-i686-compare --i686-float-comparison-object $(TEST_OUT)/verified-backend/float-compare-i686-x86.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-compare --i686-float-comparison-object $(TEST_OUT)/verified-backend/float-compare-i686-x86-o2.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-compare --i686-float-comparison-object $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-compare --i686-float-comparison-object $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2.ro
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/verified-backend/verified-i686-float-compare-asm tests/verified_backend_i686_asm.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/verified-i686-float-compare-asm $(TEST_OUT)/verified-backend/float-compare-i686-x86.ro $(TEST_OUT)/verified-backend/float-compare-i686-x86.s
	$(TEST_OUT)/verified-backend/verified-i686-float-compare-asm $(TEST_OUT)/verified-backend/float-compare-i686-x86-o2.ro $(TEST_OUT)/verified-backend/float-compare-i686-x86-o2.s
	$(TEST_OUT)/verified-backend/verified-i686-float-compare-asm $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86.ro $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86.s
	$(TEST_OUT)/verified-backend/verified-i686-float-compare-asm $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2.ro $(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2.s
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S -o $(TEST_OUT)/verified-backend/float-compare-i686-harness.s tests/verified_backend_i686_float_compare_harness.c
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-compare-i686-x86-run,$(TEST_OUT)/verified-backend/float-compare-i686-x86.s $(TEST_OUT)/verified-backend/float-compare-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-compare-i686-x86-o2-run,$(TEST_OUT)/verified-backend/float-compare-i686-x86-o2.s $(TEST_OUT)/verified-backend/float-compare-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-run,$(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86.s $(TEST_OUT)/verified-backend/float-compare-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2-run,$(TEST_OUT)/verified-backend/float-compare-i686-cxx-x86-o2.s $(TEST_OUT)/verified-backend/float-compare-i686-harness.s)
	@echo "Verified i686 x87 floating comparison/truth C/C++ O0/O2 execution passed"

test-verified-i686-floating-operations: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-ops-i686-x86.ro tests/verified_backend_i686_float_operations.c >$(TEST_OUT)/verified-backend/float-ops-i686-x86.log
	$(GREP) -F -q 'Verified backend: 20 function(s) emitted' $(TEST_OUT)/verified-backend/float-ops-i686-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-ops-i686-x86.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -O2 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-ops-i686-x86-o2.ro tests/verified_backend_i686_float_operations.c >$(TEST_OUT)/verified-backend/float-ops-i686-x86-o2.log
	$(GREP) -F -q 'Verified backend: 20 function(s) emitted' $(TEST_OUT)/verified-backend/float-ops-i686-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-ops-i686-x86-o2.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-convert-i686-x86.ro tests/verified_backend_i686_float_conversion.c >$(TEST_OUT)/verified-backend/float-convert-i686-x86.log
	$(GREP) -F -q 'Verified backend: 18 function(s) emitted' $(TEST_OUT)/verified-backend/float-convert-i686-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-convert-i686-x86.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -O2 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-convert-i686-x86-o2.ro tests/verified_backend_i686_float_conversion.c >$(TEST_OUT)/verified-backend/float-convert-i686-x86-o2.log
	$(GREP) -F -q 'Verified backend: 18 function(s) emitted' $(TEST_OUT)/verified-backend/float-convert-i686-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-convert-i686-x86-o2.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86.ro tests/verified_backend_i686_float_operations.c >$(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 20 function(s) emitted' $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2.ro tests/verified_backend_i686_float_operations.c >$(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2.log
	$(GREP) -F -q 'Verified backend: 20 function(s) emitted' $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86.ro tests/verified_backend_i686_float_conversion.c >$(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 18 function(s) emitted' $(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 -fverified-backend -v -c -o $(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-o2.ro tests/verified_backend_i686_float_conversion.c >$(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-o2.log
	$(GREP) -F -q 'Verified backend: 18 function(s) emitted' $(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-o2.log,0)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/verified-backend/verified-i686-float-operations-asm tests/verified_backend_i686_asm.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/verified-i686-float-operations-asm $(TEST_OUT)/verified-backend/float-ops-i686-x86.ro $(TEST_OUT)/verified-backend/float-ops-i686-x86.s
	$(TEST_OUT)/verified-backend/verified-i686-float-operations-asm $(TEST_OUT)/verified-backend/float-ops-i686-x86-o2.ro $(TEST_OUT)/verified-backend/float-ops-i686-x86-o2.s
	$(TEST_OUT)/verified-backend/verified-i686-float-operations-asm $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86.ro $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86.s
	$(TEST_OUT)/verified-backend/verified-i686-float-operations-asm $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2.ro $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2.s
	$(TEST_OUT)/verified-backend/verified-i686-float-operations-asm $(TEST_OUT)/verified-backend/float-convert-i686-x86.ro $(TEST_OUT)/verified-backend/float-convert-i686-x86.s
	$(TEST_OUT)/verified-backend/verified-i686-float-operations-asm $(TEST_OUT)/verified-backend/float-convert-i686-x86-o2.ro $(TEST_OUT)/verified-backend/float-convert-i686-x86-o2.s
	$(TEST_OUT)/verified-backend/verified-i686-float-operations-asm $(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86.ro $(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86.s
	$(TEST_OUT)/verified-backend/verified-i686-float-operations-asm $(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-o2.ro $(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-o2.s
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/verified-backend/verify-float-i686-ops tests/verified_backend_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/verify-float-i686-ops --i686-float-operations-object $(TEST_OUT)/verified-backend/float-ops-i686-x86.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-ops --i686-float-operations-object $(TEST_OUT)/verified-backend/float-ops-i686-x86-o2.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-ops --i686-float-operations-object $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86.ro
	$(TEST_OUT)/verified-backend/verify-float-i686-ops --i686-float-operations-object $(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2.ro
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S -o $(TEST_OUT)/verified-backend/float-convert-i686-harness.s tests/verified_backend_i686_float_conversion_harness.c
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-convert-i686-x86-run,$(TEST_OUT)/verified-backend/float-convert-i686-x86.s $(TEST_OUT)/verified-backend/float-convert-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-convert-i686-x86-o2-run,$(TEST_OUT)/verified-backend/float-convert-i686-x86-o2.s $(TEST_OUT)/verified-backend/float-convert-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-run,$(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86.s $(TEST_OUT)/verified-backend/float-convert-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-o2-run,$(TEST_OUT)/verified-backend/float-convert-i686-cxx-x86-o2.s $(TEST_OUT)/verified-backend/float-convert-i686-harness.s)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S -o $(TEST_OUT)/verified-backend/float-ops-i686-harness.s tests/verified_backend_i686_float_operations_harness.c
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-ops-i686-x86-run,$(TEST_OUT)/verified-backend/float-ops-i686-x86.s $(TEST_OUT)/verified-backend/float-ops-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-ops-i686-x86-o2-run,$(TEST_OUT)/verified-backend/float-ops-i686-x86-o2.s $(TEST_OUT)/verified-backend/float-ops-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-run,$(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86.s $(TEST_OUT)/verified-backend/float-ops-i686-harness.s)
	$(call RUN_COMPILER_BUILTINS_X86,$(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2-run,$(TEST_OUT)/verified-backend/float-ops-i686-cxx-x86-o2.s $(TEST_OUT)/verified-backend/float-ops-i686-harness.s)
	@echo "Verified i686 x87 floating unary/update/select C/C++ O0/O2 execution passed"

test-verified-backend: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE) test-verified-goto test-verified-builtins test-verified-bitcounts test-verified-volatile test-verified-i686-floating-arithmetic test-verified-i686-floating-comparisons test-verified-i686-floating-operations test-verified-cxx-reference-local test-verified-cxx-reference-return test-verified-cxx-conditional-aggregate test-verified-cxx-temporary-cleanup
	$(call MKDIR_P,$(TEST_OUT)/verified-backend)
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/x86.ro tests/verified_backend.c \
		>$(TEST_OUT)/verified-backend/x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/x64.ro tests/verified_backend.c \
		>$(TEST_OUT)/verified-backend/x64.log
	$(GREP) -F -q 'Verified backend: 51 function(s) emitted' $(TEST_OUT)/verified-backend/x86.log
	$(GREP) -F -q 'Verified backend: 51 function(s) emitted' $(TEST_OUT)/verified-backend/x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/x86.log,0)
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/x64.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/x86-o2.ro tests/verified_backend.c \
		>$(TEST_OUT)/verified-backend/x86-o2.log
	$(GREP) -F -q 'Verified backend: 51 function(s) emitted' \
		$(TEST_OUT)/verified-backend/x86-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/x86-o2.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/x64-o2.ro tests/verified_backend.c \
		>$(TEST_OUT)/verified-backend/x64-o2.log
	$(GREP) -F -q 'Verified backend: 51 function(s) emitted' \
		$(TEST_OUT)/verified-backend/x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/x64-o2.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-x64.ro \
		tests/verified_backend.cpp \
		>$(TEST_OUT)/verified-backend/cxx-x64.log
	$(GREP) -F -q 'Verified backend: 14 function(s) emitted' $(TEST_OUT)/verified-backend/cxx-x64.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/float-binary-x64.ro \
		tests/verified_backend_float_binary.c \
		>$(TEST_OUT)/verified-backend/float-binary-x64.log
	$(GREP) -F -q 'Verified backend: 36 function(s) emitted' \
		$(TEST_OUT)/verified-backend/float-binary-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-binary-x64.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/float-binary-x64-o2.ro \
		tests/verified_backend_float_binary.c \
		>$(TEST_OUT)/verified-backend/float-binary-x64-o2.log
	$(GREP) -F -q 'Verified backend: 36 function(s) emitted' \
		$(TEST_OUT)/verified-backend/float-binary-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-binary-x64-o2.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/float-binary-cxx-x64.ro \
		tests/verified_backend_float_binary.c \
		>$(TEST_OUT)/verified-backend/float-binary-cxx-x64.log
	$(GREP) -F -q 'Verified backend: 36 function(s) emitted' \
		$(TEST_OUT)/verified-backend/float-binary-cxx-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-binary-cxx-x64.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/float-binary-cxx-x64-o2.ro \
		tests/verified_backend_float_binary.c \
		>$(TEST_OUT)/verified-backend/float-binary-cxx-x64-o2.log
	$(GREP) -F -q 'Verified backend: 36 function(s) emitted' \
		$(TEST_OUT)/verified-backend/float-binary-cxx-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/float-binary-cxx-x64-o2.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/typeinfo-x86.ro \
		tests/verified_backend_typeinfo.cpp \
		>$(TEST_OUT)/verified-backend/typeinfo-x86.log
	$(GREP) -F -q 'Verified backend: 5 function(s) emitted' \
		$(TEST_OUT)/verified-backend/typeinfo-x86.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/typeinfo-x64.ro \
		tests/verified_backend_typeinfo.cpp \
		>$(TEST_OUT)/verified-backend/typeinfo-x64.log
	$(GREP) -F -q 'Verified backend: 5 function(s) emitted' \
		$(TEST_OUT)/verified-backend/typeinfo-x64.log
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/globals-x86.ro \
		tests/verified_backend_globals.c \
		>$(TEST_OUT)/verified-backend/globals-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/globals-x64.ro \
		tests/verified_backend_globals.c \
		>$(TEST_OUT)/verified-backend/globals-x64.log
	$(GREP) -F -q 'Verified backend: 8 function(s) emitted' $(TEST_OUT)/verified-backend/globals-x86.log
	$(GREP) -F -q 'Verified backend: 8 function(s) emitted' $(TEST_OUT)/verified-backend/globals-x64.log
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/aggregate-return-x86.ro \
		tests/aggregate_return.c \
		>$(TEST_OUT)/verified-backend/aggregate-return-x86.log
	$(GREP) -F -q 'Verified backend: 8 function(s) emitted' \
		$(TEST_OUT)/verified-backend/aggregate-return-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/aggregate-return-x64.ro \
		tests/aggregate_return.c \
		>$(TEST_OUT)/verified-backend/aggregate-return-x64.log
	$(GREP) -F -q 'Verified backend: 8 function(s) emitted' \
		$(TEST_OUT)/verified-backend/aggregate-return-x64.log
	$(RCC_TARGET) --target i686-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/tls-x86.ro \
		tests/verified_backend_fallback.c \
		>$(TEST_OUT)/verified-backend/tls-x86.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/tls-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/tls-x86.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/tls-x64.ro \
		tests/verified_backend_fallback.c \
		>$(TEST_OUT)/verified-backend/tls-x64.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/tls-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/tls-x64.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/tls-import-x86.ro \
		tests/verified_backend_tls_import.c \
		>$(TEST_OUT)/verified-backend/tls-import-x86.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' \
		$(TEST_OUT)/verified-backend/tls-import-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/tls-import-x86.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/tls-import-x64.ro \
		tests/verified_backend_tls_import.c \
		>$(TEST_OUT)/verified-backend/tls-import-x64.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' \
		$(TEST_OUT)/verified-backend/tls-import-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/tls-import-x64.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/tls-cxx-x86.ro \
		tests/verified_backend_tls.cpp \
		>$(TEST_OUT)/verified-backend/tls-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/tls-cxx-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/tls-cxx-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/tls-cxx-x64.ro \
		tests/verified_backend_tls.cpp \
		>$(TEST_OUT)/verified-backend/tls-cxx-x64.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/tls-cxx-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/tls-cxx-x64.log,0)
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-e verified_fallback_read \
		-o $(TEST_OUT)/verified-backend/tls-x86.rin \
		$(TEST_OUT)/verified-backend/tls-x86.ro \
		$(TEST_OUT)/verified-backend/tls-import-x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-e verified_fallback_read \
		-o $(TEST_OUT)/verified-backend/tls-x64.rin \
		$(TEST_OUT)/verified-backend/tls-x64.ro \
		$(TEST_OUT)/verified-backend/tls-import-x64.ro
	$(RLD_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-e verified_fallback_read \
		-o $(TEST_OUT)/verified-backend/tls-cxx-x86.rin \
		$(TEST_OUT)/verified-backend/tls-cxx-x86.ro \
		$(TEST_OUT)/verified-backend/tls-import-x86.ro
	$(RLD_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-e verified_fallback_read \
		-o $(TEST_OUT)/verified-backend/tls-cxx-x64.rin \
		$(TEST_OUT)/verified-backend/tls-cxx-x64.ro \
		$(TEST_OUT)/verified-backend/tls-import-x64.ro
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/verified-backend/tls-x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/verified-backend/tls-x64.rin
	$(RINVALIDATE) --kind executable --arch x86 --allow-unsigned \
		$(TEST_OUT)/verified-backend/tls-cxx-x86.rin
	$(RINVALIDATE) --kind executable --arch x86_64 --allow-unsigned \
		$(TEST_OUT)/verified-backend/tls-cxx-x64.rin
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/switch-nested.ro \
		tests/verified_backend_switch_fallback.c \
		>$(TEST_OUT)/verified-backend/switch-nested.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' \
		$(TEST_OUT)/verified-backend/switch-nested.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/array-fallback.ro \
		tests/verified_backend_array_fallback.c \
		>$(TEST_OUT)/verified-backend/array-fallback.log
	$(GREP) -F -q 'Verified backend: 6 function(s) emitted' \
		$(TEST_OUT)/verified-backend/array-fallback.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/array-fallback.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/array-fallback-x64-o2.ro \
		tests/verified_backend_array_fallback.c \
		>$(TEST_OUT)/verified-backend/array-fallback-x64-o2.log
	$(GREP) -F -q 'Verified backend: 6 function(s) emitted' \
		$(TEST_OUT)/verified-backend/array-fallback-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/array-fallback-x64-o2.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64.ro \
		tests/verified_backend_sse_aggregate_return.cpp \
		>$(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64.log
	$(GREP) -F -q 'Verified backend: 6 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O2 \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64-o2.ro \
		tests/verified_backend_sse_aggregate_return.cpp \
		>$(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64-o2.log
	$(GREP) -F -q 'Verified backend: 6 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64-o2.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/aggregate-straddle-fallback.ro \
		tests/verified_backend_aggregate_straddle_fallback.c \
		>$(TEST_OUT)/verified-backend/aggregate-straddle-fallback.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' $(TEST_OUT)/verified-backend/aggregate-straddle-fallback.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/aggregate-straddle-fallback.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/packed-argument-fallback.ro \
		tests/verified_backend_packed_argument_fallback.c \
		>$(TEST_OUT)/verified-backend/packed-argument-fallback.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' $(TEST_OUT)/verified-backend/packed-argument-fallback.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/packed-argument-fallback.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-scalar-x86.ro \
		tests/verified_backend_wide_scalar_fallback.c \
		>$(TEST_OUT)/verified-backend/wide-scalar-x86.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' $(TEST_OUT)/verified-backend/wide-scalar-x86.log
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc -Ibootstrap/include \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-scalar-return-x86.ro \
		tests/verified_backend_wide_scalar_return.c \
		>$(TEST_OUT)/verified-backend/wide-scalar-return-x86.log
	$(GREP) -F -q 'Verified backend: 77 function(s) emitted' $(TEST_OUT)/verified-backend/wide-scalar-return-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/wide-scalar-return-x86.log,0)
	$(call CHECK_COUNT,Verified backend fallback: function 'verified_wide_scalar_forward_goto',$(TEST_OUT)/verified-backend/wide-scalar-return-x86.log,0)
	$(call CHECK_COUNT,Verified backend fallback: function 'verified_wide_scalar_backward_goto',$(TEST_OUT)/verified-backend/wide-scalar-return-x86.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-scalar-x64.ro \
		tests/verified_backend_wide_scalar_fallback.c \
		>$(TEST_OUT)/verified-backend/wide-scalar-x64.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' $(TEST_OUT)/verified-backend/wide-scalar-x64.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-scalar-return-x64.ro \
		tests/verified_backend_wide_scalar_return.c \
		>$(TEST_OUT)/verified-backend/wide-scalar-return-x64.log
	$(GREP) -F -q 'Verified backend: 77 function(s) emitted' $(TEST_OUT)/verified-backend/wide-scalar-return-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/wide-scalar-return-x64.log,0)
	$(call CHECK_COUNT,Verified backend fallback: function 'verified_wide_scalar_forward_goto',$(TEST_OUT)/verified-backend/wide-scalar-return-x64.log,0)
	$(call CHECK_COUNT,Verified backend fallback: function 'verified_wide_scalar_backward_goto',$(TEST_OUT)/verified-backend/wide-scalar-return-x64.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-scalar-return-x64-o2.ro \
		tests/verified_backend_wide_scalar_return.c \
		>$(TEST_OUT)/verified-backend/wide-scalar-return-x64-o2.log
	$(GREP) -F -q 'Verified backend: 77 function(s) emitted' $(TEST_OUT)/verified-backend/wide-scalar-return-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/wide-scalar-return-x64-o2.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/va-list-pointer-cxx-x86.ro \
		tests/verified_backend_va_list_pointer.cpp \
		>$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x86.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64.ro \
		tests/verified_backend_va_list_pointer.cpp \
		>$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64-o2.ro \
		tests/verified_backend_va_list_pointer.cpp \
		>$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64-o2.log
	$(GREP) -F -q 'Verified backend: 3 function(s) emitted' \
		$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64-o2.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-fp-x64.ro \
		tests/verified_backend_sysv_va_fp.cpp \
		>$(TEST_OUT)/verified-backend/sysv-va-fp-x64.log
	$(GREP) -F -q 'Verified backend: 30 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sysv-va-fp-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-fp-x64.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-fp-x64-o2.ro \
		tests/verified_backend_sysv_va_fp.cpp \
		>$(TEST_OUT)/verified-backend/sysv-va-fp-x64-o2.log
	$(GREP) -F -q 'Verified backend: 30 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sysv-va-fp-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-fp-x64-o2.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-aggregate-x64.ro \
		tests/verified_backend_sysv_va_aggregate.c \
		>$(TEST_OUT)/verified-backend/sysv-va-aggregate-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-aggregate-x64.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-aggregate-x64-o2.ro \
		tests/verified_backend_sysv_va_aggregate.c \
		>$(TEST_OUT)/verified-backend/sysv-va-aggregate-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-aggregate-x64-o2.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-aggregate-cxx-x64.ro \
		tests/verified_backend_sysv_va_aggregate.c \
		>$(TEST_OUT)/verified-backend/sysv-va-aggregate-cxx-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-aggregate-cxx-x64.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-aggregate-cxx-x64-o2.ro \
		tests/verified_backend_sysv_va_aggregate.c \
		>$(TEST_OUT)/verified-backend/sysv-va-aggregate-cxx-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-aggregate-cxx-x64-o2.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64.ro \
		tests/verified_backend_sysv_va_aggregate_fallback.c \
		>$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64.log
	$(GREP) -F -q 'Verified backend: 20 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64-o2.ro \
		tests/verified_backend_sysv_va_aggregate_fallback.c \
		>$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64-o2.log
	$(GREP) -F -q 'Verified backend: 20 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64-o2.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64.ro \
		tests/verified_backend_sysv_va_aggregate_fallback.c \
		>$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64.log
	$(GREP) -F -q 'Verified backend: 28 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-std=c++20 -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64-o2.ro \
		tests/verified_backend_sysv_va_aggregate_fallback.c \
		>$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64-o2.log
	$(GREP) -F -q 'Verified backend: 28 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64-o2.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64-o2.log,0)
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include \
		-fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/sysv-fp-variadic-promotion-x64.ro \
		tests/verified_backend_sysv_fp_variadic_promotion.c \
		>$(TEST_OUT)/verified-backend/sysv-fp-variadic-promotion-x64.log
	$(GREP) -F -q 'Verified backend: 1 function(s) emitted' \
		$(TEST_OUT)/verified-backend/sysv-fp-variadic-promotion-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/sysv-fp-variadic-promotion-x64.log,0)
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-variadic-call-x86.ro \
		tests/verified_backend_wide_variadic_call.c \
		>$(TEST_OUT)/verified-backend/wide-variadic-call-x86.log
	$(GREP) -F -q 'Verified backend: 2 function(s) emitted' \
		$(TEST_OUT)/verified-backend/wide-variadic-call-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-variadic-call-x64.ro \
		tests/verified_backend_wide_variadic_call.c \
		>$(TEST_OUT)/verified-backend/wide-variadic-call-x64.log
	$(GREP) -F -q 'Verified backend: 2 function(s) emitted' \
		$(TEST_OUT)/verified-backend/wide-variadic-call-x64.log
	$(RCXX_TARGET) --target i686-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/member-methods-x86.ro \
		tests/verified_backend_member_methods.cpp \
		>$(TEST_OUT)/verified-backend/member-methods-x86.log
	$(GREP) -F -q 'Verified backend: 16 function(s) emitted' \
		$(TEST_OUT)/verified-backend/member-methods-x86.log
	$(call CHECK_COUNT,incompatible return type,$(TEST_OUT)/verified-backend/member-methods-x86.log,0)
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/member-methods-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/member-methods-x64.ro \
		tests/verified_backend_member_methods.cpp \
		>$(TEST_OUT)/verified-backend/member-methods-x64.log
	$(GREP) -F -q 'Verified backend: 16 function(s) emitted' \
		$(TEST_OUT)/verified-backend/member-methods-x64.log
	$(call CHECK_COUNT,incompatible return type,$(TEST_OUT)/verified-backend/member-methods-x64.log,0)
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/member-methods-x64.log,0)
	$(RCXX_TARGET) --target i686-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/virtual-dispatch-x86.ro \
		tests/verified_backend_virtual_dispatch.cpp \
		>$(TEST_OUT)/verified-backend/virtual-dispatch-x86.log
	$(GREP) -F -q 'Verified backend: 14 function(s) emitted' \
		$(TEST_OUT)/verified-backend/virtual-dispatch-x86.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/virtual-dispatch-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -O2 -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/virtual-dispatch-x64.ro \
		tests/verified_backend_virtual_dispatch.cpp \
		>$(TEST_OUT)/verified-backend/virtual-dispatch-x64.log
	$(GREP) -F -q 'Verified backend: 14 function(s) emitted' \
		$(TEST_OUT)/verified-backend/virtual-dispatch-x64.log
	$(call CHECK_COUNT,Verified backend fallback:,$(TEST_OUT)/verified-backend/virtual-dispatch-x64.log,0)
	$(CC) $(VERIFIED_BACKEND_X86_HOST_CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/verify-x86 \
		tests/verified_backend_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/verify-x64 \
		tests/verified_backend_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/verify-x64 \
		--sysv-sse-aggregate-return-object \
		$(TEST_OUT)/verified-backend/array-fallback.ro \
		verified_aggregate_return_fallback \
		verified_aggregate_return_call_value \
		verified_aggregate_float_return_fallback \
		verified_aggregate_float_call_value \
		verified_aggregate_return_conditional \
		verified_aggregate_float_return_conditional
	$(TEST_OUT)/verified-backend/verify-x64 \
		--sysv-sse-aggregate-return-object \
		$(TEST_OUT)/verified-backend/array-fallback-x64-o2.ro \
		verified_aggregate_return_fallback \
		verified_aggregate_return_call_value \
		verified_aggregate_float_return_fallback \
		verified_aggregate_float_call_value \
		verified_aggregate_return_conditional \
		verified_aggregate_float_return_conditional
	$(TEST_OUT)/verified-backend/verify-x64 \
		--sysv-sse-aggregate-return-object \
		$(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64.ro \
		verified_sse_aggregate_round_trip \
		verified_sse_aggregate_call_value \
		verified_sse_aggregate_float_round_trip \
		verified_sse_aggregate_float_call_value \
		verified_sse_aggregate_conditional \
		verified_sse_aggregate_float_conditional
	$(TEST_OUT)/verified-backend/verify-x64 \
		--sysv-sse-aggregate-return-object \
		$(TEST_OUT)/verified-backend/sse-aggregate-return-cxx-x64-o2.ro \
		verified_sse_aggregate_round_trip \
		verified_sse_aggregate_call_value \
		verified_sse_aggregate_float_round_trip \
		verified_sse_aggregate_float_call_value \
		verified_sse_aggregate_conditional \
		verified_sse_aggregate_float_conditional
	$(TEST_OUT)/verified-backend/verify-x64 --wide-scalar-object \
		$(TEST_OUT)/verified-backend/wide-scalar-return-x64-o2.ro x64
	$(TEST_OUT)/verified-backend/verify-x64 --va-list-pointer-cxx-object \
		$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64.ro x64
	$(TEST_OUT)/verified-backend/verify-x64 --va-list-pointer-cxx-object \
		$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x64-o2.ro x64
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-fp-object \
		$(TEST_OUT)/verified-backend/sysv-va-fp-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-fp-object \
		$(TEST_OUT)/verified-backend/sysv-va-fp-x64-o2.ro
	$(TEST_OUT)/verified-backend/verify-x64 --float-binary-object \
		$(TEST_OUT)/verified-backend/float-binary-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 --float-binary-object \
		$(TEST_OUT)/verified-backend/float-binary-x64-o2.ro
	$(TEST_OUT)/verified-backend/verify-x64 --float-binary-object \
		$(TEST_OUT)/verified-backend/float-binary-cxx-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 --float-binary-object \
		$(TEST_OUT)/verified-backend/float-binary-cxx-x64-o2.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-aggregate-object \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-aggregate-object \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-x64-o2.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-aggregate-object \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-cxx-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-aggregate-object \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-cxx-x64-o2.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-aggregate-straddle-object \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-aggregate-straddle-object \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-x64-o2.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-aggregate-straddle-object \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 --sysv-va-aggregate-straddle-object \
		$(TEST_OUT)/verified-backend/sysv-va-aggregate-straddle-cxx-x64-o2.ro
	$(TEST_OUT)/verified-backend/verify-x86 --va-list-pointer-cxx-object \
		$(TEST_OUT)/verified-backend/va-list-pointer-cxx-x86.ro x86
	$(TEST_OUT)/verified-backend/verify-x86 \
		$(TEST_OUT)/verified-backend/x86.ro \
		$(TEST_OUT)/verified-backend/x64.ro \
		$(TEST_OUT)/verified-backend/cxx-x64.ro \
		$(TEST_OUT)/verified-backend/globals-x86.ro \
		$(TEST_OUT)/verified-backend/globals-x64.ro \
		$(TEST_OUT)/verified-backend/wide-scalar-return-x86.ro \
		$(TEST_OUT)/verified-backend/wide-scalar-return-x64.ro \
		$(TEST_OUT)/verified-backend/typeinfo-x86.ro \
		$(TEST_OUT)/verified-backend/typeinfo-x64.ro \
		$(TEST_OUT)/verified-backend/wide-variadic-call-x86.ro \
		$(TEST_OUT)/verified-backend/wide-variadic-call-x64.ro \
		$(TEST_OUT)/verified-backend/member-methods-x86.ro \
		$(TEST_OUT)/verified-backend/member-methods-x64.ro \
		$(TEST_OUT)/verified-backend/virtual-dispatch-x86.ro \
		$(TEST_OUT)/verified-backend/virtual-dispatch-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 \
		$(TEST_OUT)/verified-backend/x86.ro \
		$(TEST_OUT)/verified-backend/x64.ro \
		$(TEST_OUT)/verified-backend/cxx-x64.ro \
		$(TEST_OUT)/verified-backend/globals-x86.ro \
		$(TEST_OUT)/verified-backend/globals-x64.ro \
		$(TEST_OUT)/verified-backend/wide-scalar-return-x86.ro \
		$(TEST_OUT)/verified-backend/wide-scalar-return-x64.ro \
		$(TEST_OUT)/verified-backend/typeinfo-x86.ro \
		$(TEST_OUT)/verified-backend/typeinfo-x64.ro \
		$(TEST_OUT)/verified-backend/wide-variadic-call-x86.ro \
		$(TEST_OUT)/verified-backend/wide-variadic-call-x64.ro \
		$(TEST_OUT)/verified-backend/member-methods-x86.ro \
		$(TEST_OUT)/verified-backend/member-methods-x64.ro \
		$(TEST_OUT)/verified-backend/virtual-dispatch-x86.ro \
		$(TEST_OUT)/verified-backend/virtual-dispatch-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 --switch-loop-labels \
		$(TEST_OUT)/verified-backend/x64-o2.ro
	$(TEST_OUT)/verified-backend/verify-x64 --tls-object \
		$(TEST_OUT)/verified-backend/tls-x86.ro x86
	$(TEST_OUT)/verified-backend/verify-x64 --tls-object \
		$(TEST_OUT)/verified-backend/tls-x64.ro x64
	$(TEST_OUT)/verified-backend/verify-x64 --tls-import-object \
		$(TEST_OUT)/verified-backend/tls-import-x86.ro x86
	$(TEST_OUT)/verified-backend/verify-x64 --tls-import-object \
		$(TEST_OUT)/verified-backend/tls-import-x64.ro x64
	$(TEST_OUT)/verified-backend/verify-x64 --tls-object \
		$(TEST_OUT)/verified-backend/tls-cxx-x86.ro x86
	$(TEST_OUT)/verified-backend/verify-x64 --tls-object \
		$(TEST_OUT)/verified-backend/tls-cxx-x64.ro x64
	@echo "Verified backend production object and fallback tests completed"

test-assignment-constraints: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/assignment-constraints)
	$(RCC_TARGET) --target i686-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/assignment-constraints/c-x86.ro \
		tests/assignment_conversions.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/assignment-constraints/c-x64.ro \
		tests/assignment_conversions.c
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/assignment-constraints/cxx-x86.ro \
		tests/assignment_conversions.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/assignment-constraints/cxx-x64.ro \
		tests/assignment_conversions.cpp
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/assignment-constraints/invalid-c-x86.ro \
		tests/invalid_assignment_types.c,$(TEST_OUT)/assignment-constraints/invalid-c-x86.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/assignment-constraints/invalid-c-x64.ro \
		tests/invalid_assignment_types.c,$(TEST_OUT)/assignment-constraints/invalid-c-x64.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/assignment-constraints/invalid-cxx-x86.ro \
		tests/invalid_assignment_types.cpp,$(TEST_OUT)/assignment-constraints/invalid-cxx-x86.log)
	$(call EXPECT_FAILURE,$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/assignment-constraints/invalid-cxx-x64.ro \
		tests/invalid_assignment_types.cpp,$(TEST_OUT)/assignment-constraints/invalid-cxx-x64.log)
	$(call CHECK_COUNT,incompatible assignment,$(TEST_OUT)/assignment-constraints/invalid-c-x86.log,7)
	$(call CHECK_COUNT,incompatible assignment,$(TEST_OUT)/assignment-constraints/invalid-c-x64.log,7)
	$(call CHECK_COUNT,incompatible assignment,$(TEST_OUT)/assignment-constraints/invalid-cxx-x86.log,9)
	$(call CHECK_COUNT,incompatible assignment,$(TEST_OUT)/assignment-constraints/invalid-cxx-x64.log,9)
	@echo "C17/C++20 assignment conversion constraints passed for i686 and AMD64"

test-optimize: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/optimize)
	$(RCC_TARGET) --target i686-unknown-rinos -O0 -c \
		-o $(TEST_OUT)/optimize/loop-x86-o0.ro tests/optimizer_loop.c
	$(RCC_TARGET) --target i686-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/loop-x86-o1.ro tests/optimizer_loop.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O0 -c \
		-o $(TEST_OUT)/optimize/loop-x64-o0.ro tests/optimizer_loop.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/loop-x64-o1.ro tests/optimizer_loop.c
	$(RCC_TARGET) --target i686-unknown-rinos -O0 -c \
		-o $(TEST_OUT)/optimize/x86-o0.ro tests/optimizer_constant.c
	$(RCC_TARGET) --target i686-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/x86-o1.ro tests/optimizer_constant.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O0 -c \
		-o $(TEST_OUT)/optimize/x64-o0.ro tests/optimizer_constant.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/x64-o1.ro tests/optimizer_constant.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O3 -c \
		-o $(TEST_OUT)/optimize/x64-o3.ro tests/optimizer_constant.c
	$(call COMPARE_FILES,$(TEST_OUT)/optimize/x64-o1.ro,$(TEST_OUT)/optimize/x64-o3.ro)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-o1.ro tests/hello.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O0 -c \
		-o $(TEST_OUT)/optimize/cxx-optimizer-x86-o0.ro \
		tests/optimizer_constant_cpp.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-optimizer-x86-o1.ro \
		tests/optimizer_constant_cpp.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O0 -c \
		-o $(TEST_OUT)/optimize/cxx-optimizer-x64-o0.ro \
		tests/optimizer_constant_cpp.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-optimizer-x64-o1.ro \
		tests/optimizer_constant_cpp.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O0 -c \
		-o $(TEST_OUT)/optimize/cxx-loop-x86-o0.ro \
		tests/optimizer_loop_cpp.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-loop-x86-o1.ro \
		tests/optimizer_loop_cpp.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O0 -c \
		-o $(TEST_OUT)/optimize/cxx-loop-x64-o0.ro \
		tests/optimizer_loop_cpp.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-loop-x64-o1.ro \
		tests/optimizer_loop_cpp.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-cleanup-x86.ro \
		tests/cxx_inline_aggregate.cpp \
		>$(TEST_OUT)/optimize/cxx-cleanup-x86.log 2>&1
	$(call CHECK_COUNT,incompatible return type,$(TEST_OUT)/optimize/cxx-cleanup-x86.log,0)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-cleanup-x64.ro \
		tests/cxx_inline_aggregate.cpp \
		>$(TEST_OUT)/optimize/cxx-cleanup-x64.log 2>&1
	$(call CHECK_COUNT,incompatible return type,$(TEST_OUT)/optimize/cxx-cleanup-x64.log,0)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -driver \
		--emit-unsigned-v3 -o $(TEST_OUT)/optimize/forbidden.drv \
		tests/driver_policy_float.c,$(TEST_OUT)/optimize/forbidden.log)
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/invalid-qualifiers.ro \
		tests/invalid_qualifiers.c,$(TEST_OUT)/optimize/invalid-qualifiers.log)
	$(call CHECK_COUNT,requires modifiable lvalue,$(TEST_OUT)/optimize/invalid-qualifiers.log,4)
	$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/qualifier-conversions.ro \
		tests/qualifier_conversions.c \
		>$(TEST_OUT)/optimize/qualifier-conversions.log 2>&1
	$(call CHECK_COUNT,incompatible return type,$(TEST_OUT)/optimize/qualifier-conversions.log,2)
	$(CC) $(VERIFIED_BACKEND_X86_HOST_CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/optimizer_run_test-x86 \
		tests/optimizer_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/optimizer_run_test-x64 \
		tests/optimizer_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/optimizer_run_test-x86 \
		$(TEST_OUT)/optimize/x86-o0.ro $(TEST_OUT)/optimize/x86-o1.ro \
		$(TEST_OUT)/optimize/x64-o0.ro $(TEST_OUT)/optimize/x64-o1.ro
	$(TEST_OUT)/optimizer_run_test-x64 \
		$(TEST_OUT)/optimize/x86-o0.ro $(TEST_OUT)/optimize/x86-o1.ro \
		$(TEST_OUT)/optimize/x64-o0.ro $(TEST_OUT)/optimize/x64-o1.ro
	$(TEST_OUT)/optimizer_run_test-x86 \
		$(TEST_OUT)/optimize/cxx-optimizer-x86-o0.ro \
		$(TEST_OUT)/optimize/cxx-optimizer-x86-o1.ro \
		$(TEST_OUT)/optimize/cxx-optimizer-x64-o0.ro \
		$(TEST_OUT)/optimize/cxx-optimizer-x64-o1.ro
	$(TEST_OUT)/optimizer_run_test-x64 \
		$(TEST_OUT)/optimize/cxx-optimizer-x86-o0.ro \
		$(TEST_OUT)/optimize/cxx-optimizer-x86-o1.ro \
		$(TEST_OUT)/optimize/cxx-optimizer-x64-o0.ro \
		$(TEST_OUT)/optimize/cxx-optimizer-x64-o1.ro
	$(CC) $(VERIFIED_BACKEND_X86_HOST_CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/optimizer_loop_cpp_test-x86 \
		tests/optimizer_loop_cpp_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/optimizer_loop_cpp_test-x64 \
		tests/optimizer_loop_cpp_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/optimizer_loop_cpp_test-x86 \
		$(TEST_OUT)/optimize/cxx-loop-x86-o0.ro \
		$(TEST_OUT)/optimize/cxx-loop-x86-o1.ro \
		$(TEST_OUT)/optimize/cxx-loop-x64-o0.ro \
		$(TEST_OUT)/optimize/cxx-loop-x64-o1.ro
	$(TEST_OUT)/optimizer_loop_cpp_test-x64 \
		$(TEST_OUT)/optimize/cxx-loop-x86-o0.ro \
		$(TEST_OUT)/optimize/cxx-loop-x86-o1.ro \
		$(TEST_OUT)/optimize/cxx-loop-x64-o0.ro \
		$(TEST_OUT)/optimize/cxx-loop-x64-o1.ro
	$(CC) $(VERIFIED_BACKEND_X86_HOST_CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/optimizer_loop_test-x86 \
		tests/optimizer_loop_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/optimizer_loop_test-x64 \
		tests/optimizer_loop_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/optimizer_loop_test-x86 \
		$(TEST_OUT)/optimize/loop-x86-o0.ro \
		$(TEST_OUT)/optimize/loop-x86-o1.ro \
		$(TEST_OUT)/optimize/loop-x64-o0.ro \
		$(TEST_OUT)/optimize/loop-x64-o1.ro
	$(TEST_OUT)/optimizer_loop_test-x64 \
		$(TEST_OUT)/optimize/loop-x86-o0.ro \
		$(TEST_OUT)/optimize/loop-x86-o1.ro \
		$(TEST_OUT)/optimize/loop-x64-o0.ro \
		$(TEST_OUT)/optimize/loop-x64-o1.ro
	$(CXX_CLEANUP_X86_BUILD)
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/optimize/cxx-cleanup-run-x64 \
		tests/cxx_value_init_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(CXX_CLEANUP_X86_RUN)
	$(TEST_OUT)/optimize/cxx-cleanup-run-x64 \
		$(TEST_OUT)/optimize/cxx-cleanup-x64.ro
	@echo "Dual-architecture AST scalar folding and dead-code tests completed"

test-generic: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/generic)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/generic/x86.ro tests/generic_selection.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/generic/x64.ro tests/generic_selection.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/generic/invalid.ro tests/invalid_generic.c,$(TEST_OUT)/generic/invalid.log)
	$(GREP) -q "generic selection has compatible duplicate types" \
		$(TEST_OUT)/generic/invalid.log
	$(GREP) -q "generic selection has more than one default association" \
		$(TEST_OUT)/generic/invalid.log
	$(GREP) -q "generic association requires a complete object type" \
		$(TEST_OUT)/generic/invalid.log
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/generic/invalid-match.ro \
		tests/invalid_generic_match.c,$(TEST_OUT)/generic/invalid-match.log)
	$(GREP) -q "generic selection has no compatible association" \
		$(TEST_OUT)/generic/invalid-match.log
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/generic_selection_run_test \
		tests/generic_selection_run_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/generic_selection_run_test \
		$(TEST_OUT)/generic/x86.ro $(TEST_OUT)/generic/x64.ro
	@echo "C17 generic selection tests completed"

test-initializer-overrides: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/initializer-overrides)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/initializer-overrides/x86.ro \
		tests/initializer_override.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/initializer-overrides/x64.ro \
		tests/initializer_override.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/initializer_override_run_test \
		tests/initializer_override_run_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/initializer_override_run_test \
		$(TEST_OUT)/initializer-overrides/x86.ro \
		$(TEST_OUT)/initializer-overrides/x64.ro
	@echo "C17 initializer override tests completed"

test-alignof: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/alignof)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/alignof/x86.ro tests/alignof.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/alignof/x64.ro tests/alignof.c
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/alignof/invalid.ro tests/invalid_alignof.c,$(TEST_OUT)/alignof/invalid.log)
	$(GREP) -q "_Alignof requires a complete object type" \
		$(TEST_OUT)/alignof/invalid.log
ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/alignof_run_test \
		tests/alignof_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/alignof_run_test \
		$(TEST_OUT)/alignof/x86.ro $(TEST_OUT)/alignof/x64.ro
	$(TEST_OUT)/alignof_run_test --inspect \
		$(TEST_OUT)/alignof/x86.ro $(TEST_OUT)/alignof/x64.ro
else
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/alignof_run_test \
		tests/alignof_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/alignof_run_test \
		$(TEST_OUT)/alignof/x86.ro $(TEST_OUT)/alignof/x64.ro
endif
	@echo "C17 _Alignof tests completed"

test-alignas: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/alignas)
ifeq ($(OS),Windows_NT)
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/alignas/x86.ro tests/alignas.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/alignas/x64.ro tests/alignas.c
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 \
		-fverified-backend -c \
		-o $(TEST_OUT)/alignas/verified-x86.ro tests/alignas.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 \
		-fverified-backend -c \
		-o $(TEST_OUT)/alignas/verified-x64.ro tests/alignas.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/alignas/x64.s tests/alignas.c
	$(CC) -o $(TEST_OUT)/alignas/x64 \
		$(TEST_OUT)/alignas/x64.s
	$(TEST_OUT)/alignas/x64
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/alignas/run-test \
		tests/alignas_host_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/alignas/run-test \
		$(TEST_OUT)/alignas/x86.ro $(TEST_OUT)/alignas/x64.ro \
		$(TEST_OUT)/alignas/verified-x64.ro
else
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/alignas/x86.ro tests/alignas.c
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/alignas/x86.s tests/alignas.c
	$(CC) -m32 -c -o $(TEST_OUT)/alignas/x86.o \
		$(TEST_OUT)/alignas/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/alignas/start-x86.o \
		tests/cxx_member_methods_i686_start.s
	$(CC) -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/alignas/x86 \
		$(TEST_OUT)/alignas/start-x86.o $(TEST_OUT)/alignas/x86.o
	$(TEST_OUT)/alignas/x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/alignas/x64.ro tests/alignas.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -S \
		-o $(TEST_OUT)/alignas/x64.s tests/alignas.c
	$(CC) -c -o $(TEST_OUT)/alignas/x64.o \
		$(TEST_OUT)/alignas/x64.s
	$(CC) -c -o $(TEST_OUT)/alignas/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/alignas/x64 \
		$(TEST_OUT)/alignas/start-x64.o $(TEST_OUT)/alignas/x64.o
	$(TEST_OUT)/alignas/x64
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 \
		-fverified-backend -c \
		-o $(TEST_OUT)/alignas/verified-x86.ro tests/alignas.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 \
		-fverified-backend -c \
		-o $(TEST_OUT)/alignas/verified-x64.ro tests/alignas.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/alignas/run-test \
		tests/alignas_host_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/alignas/run-test \
		$(TEST_OUT)/alignas/x86.ro $(TEST_OUT)/alignas/x64.ro \
		$(TEST_OUT)/alignas/verified-x64.ro

endif
	$(call EXPECT_FAILURE,$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/alignas/invalid.ro tests/alignas_invalid.c,$(TEST_OUT)/alignas/invalid.log)
	$(GREP) -q "_Alignas alignment must be a power of two" \
		$(TEST_OUT)/alignas/invalid.log
	@echo "C17 _Alignas tests completed"

# Dependencies
$(OBJDIR)/main.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h $(INCDIR)/driver_policy.h $(INCDIR)/optimize.h $(INCDIR)/preproc.h
$(OBJDIR)/main_cxx.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/ast_cxx.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h $(INCDIR)/driver_policy.h $(INCDIR)/optimize.h $(INCDIR)/preproc.h
$(OBJDIR)/lexer.o: $(INCDIR)/rcc.h $(INCDIR)/token.h
$(OBJDIR)/parser.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h
$(OBJDIR)/ast.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h
$(OBJDIR)/ast_cxx.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/ast_cxx.h
$(OBJDIR)/parser_cxx.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/ast_cxx.h
$(OBJDIR)/symtab.o: $(INCDIR)/rcc.h $(INCDIR)/symtab.h
$(OBJDIR)/sema.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/symtab.h
$(OBJDIR)/codegen.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h
$(OBJDIR)/codegen64.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h
$(OBJDIR)/ir.o: $(INCDIR)/rcc.h $(INCDIR)/ir.h
$(OBJDIR)/ir_pass.o: $(INCDIR)/rcc.h $(INCDIR)/ir.h $(INCDIR)/ir_pass.h
$(OBJDIR)/mir.o: $(INCDIR)/rcc.h $(INCDIR)/ir.h $(INCDIR)/mir.h
$(OBJDIR)/mir_alloc.o: $(INCDIR)/rcc.h $(INCDIR)/mir.h $(INCDIR)/mir_alloc.h
$(OBJDIR)/mir_phi.o: $(INCDIR)/rcc.h $(INCDIR)/mir.h $(INCDIR)/mir_alloc.h $(INCDIR)/mir_phi.h
$(OBJDIR)/x86_abi.o: $(INCDIR)/rcc.h $(INCDIR)/mir.h $(INCDIR)/mir_alloc.h $(INCDIR)/x86_abi.h
$(OBJDIR)/x86_select.o: $(INCDIR)/rcc.h $(INCDIR)/mir.h $(INCDIR)/mir_alloc.h $(INCDIR)/mir_phi.h $(INCDIR)/x86_abi.h $(INCDIR)/x86_select.h
$(OBJDIR)/x86_legalize.o: $(INCDIR)/rcc.h $(INCDIR)/mir.h $(INCDIR)/mir_alloc.h $(INCDIR)/mir_phi.h $(INCDIR)/x86_abi.h $(INCDIR)/x86_select.h $(INCDIR)/x86_legalize.h
$(OBJDIR)/x86_encode.o: $(INCDIR)/rcc.h $(INCDIR)/mir.h $(INCDIR)/mir_alloc.h $(INCDIR)/mir_phi.h $(INCDIR)/x86_abi.h $(INCDIR)/x86_select.h $(INCDIR)/x86_legalize.h $(INCDIR)/x86_encode.h
$(OBJDIR)/x86_object.o: $(INCDIR)/rcc.h $(INCDIR)/objfile.h $(INCDIR)/mir.h $(INCDIR)/mir_alloc.h $(INCDIR)/mir_phi.h $(INCDIR)/x86_abi.h $(INCDIR)/x86_select.h $(INCDIR)/x86_legalize.h $(INCDIR)/x86_encode.h $(INCDIR)/x86_object.h
$(OBJDIR)/ir_lower.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/ir.h $(INCDIR)/ir_pass.h $(INCDIR)/mir.h $(INCDIR)/mir_alloc.h $(INCDIR)/mir_phi.h $(INCDIR)/x86_abi.h $(INCDIR)/x86_select.h $(INCDIR)/x86_legalize.h $(INCDIR)/ir_lower.h
$(OBJDIR)/optimize.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/ir_lower.h $(INCDIR)/optimize.h
$(OBJDIR)/preproc.o: $(INCDIR)/rcc.h $(INCDIR)/preproc.h
$(OBJDIR)/emit_rin.o: $(INCDIR)/rcc.h $(INCDIR)/codegen.h
$(OBJDIR)/emit_rll.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/codegen.h
$(OBJDIR)/emit_drv.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/codegen.h
$(OBJDIR)/emit_ro.o: $(INCDIR)/rcc.h $(INCDIR)/codegen.h $(INCDIR)/objfile.h
$(OBJDIR)/emit_asm.o: $(INCDIR)/rcc.h $(INCDIR)/codegen.h
$(OBJDIR)/utils.o: $(INCDIR)/rcc.h
$(OBJDIR)/build_manifest.o: $(INCDIR)/rcc.h $(INCDIR)/build_manifest.h
$(OBJDIR)/driver_policy.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/driver_policy.h
$(OBJDIR)/main_rld.o: $(INCDIR)/rcc.h $(INCDIR)/linker.h
$(OBJDIR)/linker.o: $(INCDIR)/rcc.h $(INCDIR)/linker.h $(INCDIR)/objfile.h
$(OBJDIR)/main_rar.o: $(INCDIR)/rcc.h $(INCDIR)/archive.h
$(OBJDIR)/archive.o: $(INCDIR)/rcc.h $(INCDIR)/archive.h $(INCDIR)/objfile.h
