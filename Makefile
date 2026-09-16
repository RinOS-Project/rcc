# SPDX-License-Identifier: Apache-2.0
# RCC/RCC++ - RinOS C/C++ Compiler
# Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2 -MMD -MP \
	-I$(RINOS_SDK_ROOT)/include
LDFLAGS =
OBJCOPY ?= objcopy

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
OBJDIR = obj
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
WSL_RINCOMPILER_ROOT ?= /mnt/e/RinOS/RinCompiler
WINDOWS_TEST_OUT = $(subst /,\,$(TEST_OUT))
CHECK_INIT_ARRAY = findstr /c:".section .init_array"
CHECK_INIT_ARRAY_FILE = $(CHECK_INIT_ARRAY) $(subst /,\,$(1))
CHECK_FINI_ARRAY = findstr /c:".section .fini_array"
CHECK_FINI_ARRAY_FILE = $(CHECK_FINI_ARRAY) $(subst /,\,$(1))
else
CHECK_INIT_ARRAY = grep -F -q ".section .init_array"
CHECK_INIT_ARRAY_FILE = $(CHECK_INIT_ARRAY) $(1)
CHECK_FINI_ARRAY = grep -F -q ".section .fini_array"
CHECK_FINI_ARRAY_FILE = $(CHECK_FINI_ARRAY) $(1)
endif

ifeq ($(OS),Windows_NT)
MKDIR_P = if not exist "$(1)\." mkdir "$(1)"
else
MKDIR_P = mkdir -p $(1)
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
BOOTSTRAP_RUNTIME_FUNCTIONS = __errno_location __rin_stderr _exit atexit atoi \
                              close execvp exit fclose feof ferror fgets fopen \
                              fork fprintf fputc fputs fread free fseek ftell \
                              fwrite isalnum isalpha isdigit isspace isxdigit \
                              malloc memchr memcmp memcpy memset mkstemp perror printf \
                              qsort realloc remove rename snprintf strchr strcmp \
                              strcpy strlen strcat strncat strncmp strncpy strrchr strstr strtod \
                              strtoll strtoull tolower vfprintf vsnprintf waitpid setjmp longjmp \
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
RCC_TARGET = $(BINDIR)/rcc$(EXE_SUFFIX)

# RCC++ (C++ compiler)
RCXX_SRCS = $(SRCDIR)/main_cxx.c $(SRCDIR)/ast_cxx.c $(SRCDIR)/parser_cxx.c
RCXX_OBJS = $(RCXX_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RCXX_TARGET = $(BINDIR)/rcc++$(EXE_SUFFIX)

# RLD (Linker) - uses minimal common code
RLD_COMMON_SRCS = $(SRCDIR)/utils.c $(SRCDIR)/emit_ro.c $(SRCDIR)/archive.c $(SRCDIR)/build_manifest.c
RLD_COMMON_OBJS = $(RLD_COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RLD_SRCS = $(SRCDIR)/main_rld.c $(SRCDIR)/linker.c
RLD_OBJS = $(RLD_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RLD_TARGET = $(BINDIR)/rld$(EXE_SUFFIX)

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
AQC_TARGET = $(BINDIR)/aqc$(EXE_SUFFIX)

# RAR (Archiver) - uses minimal common code
RAR_COMMON_SRCS = $(SRCDIR)/utils.c
RAR_COMMON_OBJS = $(RAR_COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RAR_SRCS = $(SRCDIR)/main_rar.c $(SRCDIR)/archive.c
RAR_OBJS = $(RAR_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RAR_TARGET = $(BINDIR)/rar$(EXE_SUFFIX)

# Keep object/header dependencies in the build tree so a changed public
# header can never leave incompatible compiler objects mixed together.
-include $(wildcard $(OBJDIR)/*.d)

.PHONY: all clean build-rcc build-rcxx build-rld build-rar test-cxx test-cxx-cli test-cxx-language-core test-cxx-multiple-inheritance-virtual test-cxx-secondary-virtual-override test-cxx-virtual-base test-cxx-destructor-body test-cxx-array-destructor test-cxx-constexpr test-cxx-constexpr-aggregate test-cxx-enum-class test-cxx-constraints test-cxx-new-array test-cxx-language-linkage test-cxx-member-specifiers test-cxx-member-methods test-cxx-function-templates test-cxx-function-template-overloads test-cxx-function-template-references test-cxx-non-type-templates test-initializer-brace-elision test-initializer-mixed test-flexible-arrays test-floating-static-initializers test-floating-runtime-x64 test-floating-runtime-i686 test-vla-runtime test-vla-semantics test-static-locals test-block-extern test-tls-block-scope test-cxx-qualified-namespaces test-cxx-using test-cxx-overloads test-cxx-inline-aggregates test-cxx-parser-recovery test-cxx-exceptions test-cxx-object-exceptions test-tool-relative-includes test-preprocessor-continuation test-preprocessor-if test-preprocessor-operators test-preprocessor-va-opt test-atomic-builtins test-x86-wide-scalar test-language-boundaries test-integer-literals test-integer-promotions test-integer-conversions test-function-calls test-inline-asm test-inline-asm-execute test-varargs test-scalar-comparisons test-aggregate-copy test-aggregate-returns test-aggregate-packed-abi test-compound-literals test-static-compound-address test-bootstrap-core test-bootstrap-link test-bootstrap-execute test-bootstrap-stage2 test-executable-imports test-pragma-pack test-bitfields test-cxx-bitfields test-compound-assignment test-switch-statement test-control-flow test-parser-recovery test-link test-archive-link test-static-assert test-manifest test-signing test-sanitize test-driver-policy test-weak-link test-comdat-link test-object-width test-special-sections test-direct-relocation test-format-validation test-global-initializers test-global-finalizers test-ir test-ir-lowering test-verified-backend test-optimize test-generic test-initializer-overrides test-alignof test-tls test-pic-plt test-pic-got test-pic-tls test-pic-direct-internal test-golden-artifacts
.PHONY: test-cxx-range-for test-cxx-exception-cleanup test-cxx-const-member-overload test-cxx-member-lifetime test-cxx-global-constructor
.PHONY: test-cxx-nontrivial-object-exceptions test-cxx-cross-library-exceptions
.PHONY: test-cxx-cross-translation-unit-virtual
.PHONY: test-cxx-shared-virtual-base
.PHONY: test-cxx-shared-virtual-base-method test-cxx-virtual-base-conversion \
test-cxx-virtual-base-constructor test-cxx-virtual-base-constructor-order
.PHONY: test-cxx-lambda-function-pointer
.PHONY: test-cxx-if-constexpr test-cxx-if-constexpr-template \
test-cxx-adl-multiple-namespaces test-cxx-using-overload-namespaces \
	test-cxx-template-two-phase-namespace test-cxx-template-two-phase-adl \
	test-cxx-template-two-phase-ordinary test-cxx-template-parameter-pack
.PHONY: test-cxx-constexpr-pointer
.PHONY: test-cxx-constexpr-pointer-mutation
.PHONY: test-cxx-constexpr-pointer-aggregate
.PHONY: test-cxx-noexcept-expression
.PHONY: test-cxx-auto-return test-cxx-decltype test-cxx-decltype-auto \
	test-cxx-auto-local-refs test-cxx-const-cast test-cxx-dynamic-cast
.PHONY: test-cxx-dynamic-cast-downcast test-cxx-dynamic-cast-runtime test-cxx-dynamic-cast-reference
.PHONY: test-cxx-default-member-initializer test-cxx-base-constructor-initializer test-cxx-delegating-constructor test-cxx-converting-constructor
.PHONY: test-cxx-qualified-class-initialization test-cxx-static-member-tls
.PHONY: test-cxx-constructor-general
.PHONY: test-cxx-auto-non-type-template

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
test: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT))
	$(RCC_TARGET) --emit-unsigned-v3 -o $(TEST_OUT)/hello.rin tests/hello.c
	@echo "RCC test completed"

test-aqc: $(AQC_TARGET)
	mkdir -p $(TEST_OUT)
	$(AQC_TARGET) -o $(TEST_OUT)/passthrough_vertex.rsh \
		$(RINOS_ROOT)/resources/shaders/passthrough_vertex.aq
	$(AQC_TARGET) -o $(TEST_OUT)/sample_fragment.rsh \
		$(RINOS_ROOT)/resources/shaders/sample_fragment.aq
	@echo "AQC test completed"

test-cxx: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCXX_TARGET) --emit-unsigned-v3 -o $(TEST_OUT)/hello_cxx.rin tests/hello.cpp
	@echo "RCC++ test completed"

test-golden-artifacts: $(RCC_TARGET) $(RCXX_TARGET)
	python3 ../../../scripts/check_rcc_golden.py --rcc $(RCC_TARGET) --rccxx $(RCXX_TARGET)

test-cxx-enum-class: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-enum-class)
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
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-enum-class/invalid-x86.ro \
		tests/cxx_enum_class_invalid.cpp \
		>$(TEST_OUT)/cxx-enum-class/invalid-x86.log 2>&1
	grep -q 'scoped enum' $(TEST_OUT)/cxx-enum-class/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-enum-class/invalid-x64.ro \
		tests/cxx_enum_class_invalid.cpp \
		>$(TEST_OUT)/cxx-enum-class/invalid-x64.log 2>&1
	grep -q 'scoped enum' $(TEST_OUT)/cxx-enum-class/invalid-x64.log
	@echo "RCC++ scoped enum test completed"

test-cxx-cli: $(RCC_TARGET) $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -O3 -c -MMD \
		-MF $(TEST_OUT)/cxx_cli_options.d -nostdinc -Itests/include \
		-DRCC_CXX_CLI_VALUE=23 -DRCC_CXX_REMOVE_ME -URCC_CXX_REMOVE_ME \
		-o $(TEST_OUT)/cxx_cli_options.ro tests/cxx_cli_options.cpp
	! $(RCC_TARGET) -O4 -c -o $(TEST_OUT)/invalid-o-c.ro tests/hello.c \
		>$(TEST_OUT)/invalid-o-c.log 2>&1
	grep -q 'expected -O0 through -O3' $(TEST_OUT)/invalid-o-c.log
	! $(RCC_TARGET) -Wunknown -c -o $(TEST_OUT)/invalid-w-c.ro tests/hello.c \
		>$(TEST_OUT)/invalid-w-c.log 2>&1
	grep -q 'unsupported warning option' $(TEST_OUT)/invalid-w-c.log
	! $(RCC_TARGET) -funknown -c -o $(TEST_OUT)/invalid-f-c.ro tests/hello.c \
		>$(TEST_OUT)/invalid-f-c.log 2>&1
	grep -q 'unsupported code-generation option' $(TEST_OUT)/invalid-f-c.log
	! $(RCXX_TARGET) -Ofoo -c -o $(TEST_OUT)/invalid-o-cxx.ro \
		tests/cxx_cli_options.cpp >$(TEST_OUT)/invalid-o-cxx.log 2>&1
	grep -q 'expected -O0 through -O3' $(TEST_OUT)/invalid-o-cxx.log
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
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/new-delete-x86.o \
		$(TEST_OUT)/cxx-language-core/new-delete-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/new-delete-start-x86.o \
		tests/cxx_new_delete_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/new-delete-x86 \
		$(TEST_OUT)/cxx-language-core/new-delete-start-x86.o \
		$(TEST_OUT)/cxx-language-core/new-delete-x86.o
	$(TEST_OUT)/cxx-language-core/new-delete-x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/new-delete-x64.s \
		tests/cxx_new_delete.cpp
	gcc -c -o $(TEST_OUT)/cxx-language-core/new-delete-x64.o \
		$(TEST_OUT)/cxx-language-core/new-delete-x64.s
	gcc -c -o $(TEST_OUT)/cxx-language-core/new-delete-start-x64.o \
		tests/cxx_new_delete_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/new-delete-x64 \
		$(TEST_OUT)/cxx-language-core/new-delete-start-x64.o \
		$(TEST_OUT)/cxx-language-core/new-delete-x64.o
	$(TEST_OUT)/cxx-language-core/new-delete-x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/adl-x86.s \
		tests/cxx_adl.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/adl-x64.s \
		tests/cxx_adl.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/adl-x86.o \
		$(TEST_OUT)/cxx-language-core/adl-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/adl-start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/adl-x86 \
		$(TEST_OUT)/cxx-language-core/adl-start-x86.o \
		$(TEST_OUT)/cxx-language-core/adl-x86.o
	$(TEST_OUT)/cxx-language-core/adl-x86
	gcc -c -o $(TEST_OUT)/cxx-language-core/adl-x64.o \
		$(TEST_OUT)/cxx-language-core/adl-x64.s
	gcc -c -o $(TEST_OUT)/cxx-language-core/adl-start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/adl-x64 \
		$(TEST_OUT)/cxx-language-core/adl-start-x64.o \
		$(TEST_OUT)/cxx-language-core/adl-x64.o
	$(TEST_OUT)/cxx-language-core/adl-x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.s \
		tests/cxx_virtual_dispatch.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.s \
		tests/cxx_virtual_dispatch.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.o \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/virtual-dispatch-start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x86 \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-start-x86.o \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-x86.o
	$(TEST_OUT)/cxx-language-core/virtual-dispatch-x86
	gcc -c -o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.o \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.s
	gcc -c -o $(TEST_OUT)/cxx-language-core/virtual-dispatch-start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/virtual-dispatch-x64 \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-start-x64.o \
		$(TEST_OUT)/cxx-language-core/virtual-dispatch-x64.o
	$(TEST_OUT)/cxx-language-core/virtual-dispatch-x64

	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/field-initializers-x86.s \
		tests/cxx_field_initializers.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/field-initializers-x64.s \
		tests/cxx_field_initializers.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/field-initializers-x86.o \
		$(TEST_OUT)/cxx-language-core/field-initializers-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/field-initializers-start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/field-initializers-x86 \
		$(TEST_OUT)/cxx-language-core/field-initializers-start-x86.o \
		$(TEST_OUT)/cxx-language-core/field-initializers-x86.o
	$(TEST_OUT)/cxx-language-core/field-initializers-x86
	gcc -c -o $(TEST_OUT)/cxx-language-core/field-initializers-x64.o \
		$(TEST_OUT)/cxx-language-core/field-initializers-x64.s
	gcc -c -o $(TEST_OUT)/cxx-language-core/field-initializers-start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/field-initializers-x64 \
		$(TEST_OUT)/cxx-language-core/field-initializers-start-x64.o \
		$(TEST_OUT)/cxx-language-core/field-initializers-x64.o
	$(TEST_OUT)/cxx-language-core/field-initializers-x64
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/inheritance-x86.s \
		tests/cxx_inheritance.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-language-core/inheritance-x64.s \
		tests/cxx_inheritance.cpp
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/inheritance-x86.o \
		$(TEST_OUT)/cxx-language-core/inheritance-x86.s
	gcc -m32 -c -o $(TEST_OUT)/cxx-language-core/inheritance-start-x86.o \
		tests/cxx_member_methods_i686_start.s
	gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/inheritance-x86 \
		$(TEST_OUT)/cxx-language-core/inheritance-start-x86.o \
		$(TEST_OUT)/cxx-language-core/inheritance-x86.o
	$(TEST_OUT)/cxx-language-core/inheritance-x86
	gcc -c -o $(TEST_OUT)/cxx-language-core/inheritance-x64.o \
		$(TEST_OUT)/cxx-language-core/inheritance-x64.s
	gcc -c -o $(TEST_OUT)/cxx-language-core/inheritance-start-x64.o \
		tests/cxx_member_methods_x64_start.s
	gcc -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-language-core/inheritance-x64 \
		$(TEST_OUT)/cxx-language-core/inheritance-start-x64.o \
		$(TEST_OUT)/cxx-language-core/inheritance-x64.o
	$(TEST_OUT)/cxx-language-core/inheritance-x64
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
	! $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/invalid-array-new-x86.ro \
		tests/cxx_new_array_invalid.cpp \
		>$(TEST_OUT)/cxx-language-core/invalid-array-new-x86.log 2>&1
	grep -q 'array new has no lowerable constructor for its element initializers' \
		$(TEST_OUT)/cxx-language-core/invalid-array-new-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-core/invalid-array-new-x64.ro \
		tests/cxx_new_array_invalid.cpp \
		>$(TEST_OUT)/cxx-language-core/invalid-array-new-x64.log 2>&1
	grep -q 'array new has no lowerable constructor for its element initializers' \
		$(TEST_OUT)/cxx-language-core/invalid-array-new-x64.log
	@echo "RCC++ core language tests completed"

test-cxx-multiple-inheritance-virtual: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-multiple-inheritance-virtual)
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
	@echo "C++ secondary virtual-base vptr test completed"

test-cxx-secondary-virtual-override: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-secondary-virtual-override)
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
	@echo "C++ secondary virtual override execution test completed"

test-cxx-virtual-base: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-virtual-base)
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
	@echo "C++ direct virtual-base layout and dispatch tests completed"

test-cxx-virtual-base-conversion: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-virtual-base-conversion)
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
	@echo "C++ virtual-base conversion tests completed"

test-cxx-virtual-base-constructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-virtual-base-constructor)
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
	@echo "C++ virtual-base constructor tests completed"

test-cxx-constructor-general: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-constructor-general)
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
	@echo "C++ general constructor-body tests completed"

test-cxx-virtual-base-constructor-order: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-virtual-base-constructor-order)
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
	@echo "C++ virtual-base construction-order tests completed"

test-cxx-shared-virtual-base: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-shared-virtual-base)
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
	@echo "C++ shared virtual-base diamond test completed"

test-cxx-shared-virtual-base-method: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-shared-virtual-base-method)
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
	@echo "C++ shared virtual-base member dispatch test completed"

test-cxx-destructor-body: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-destructor-body)
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
	@echo "C++ explicit destructor body and lifetime tests completed"

test-cxx-member-lifetime: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-lifetime)
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
	@echo "C++ nested member construction and destruction tests completed"

test-cxx-array-destructor: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-array-destructor)
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
	@echo "C++ array destructor cookie and reverse-lifetime tests completed"

test-cxx-constexpr: $(RCXX_TARGET)
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
	grep -q "constexpr variable requires an initializer" \
		$(TEST_OUT)/cxx-constexpr/invalid-x86.log
	grep -q "constexpr variable initializer is not a supported constant expression" \
		$(TEST_OUT)/cxx-constexpr/invalid-x86.log
	grep -q "consteval call is not a constant expression" \
		$(TEST_OUT)/cxx-constexpr/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constexpr/invalid-x64.ro \
		tests/cxx_constexpr_invalid.cpp \
		>$(TEST_OUT)/cxx-constexpr/invalid-x64.log 2>&1
	grep -q "constexpr variable requires an initializer" \
		$(TEST_OUT)/cxx-constexpr/invalid-x64.log
	grep -q "constexpr variable initializer is not a supported constant expression" \
		$(TEST_OUT)/cxx-constexpr/invalid-x64.log
	grep -q "consteval call is not a constant expression" \
		$(TEST_OUT)/cxx-constexpr/invalid-x64.log
	@echo "RCC++ scalar constexpr folding tests completed"

test-cxx-constexpr-aggregate: $(RCXX_TARGET)
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
	powershell -NoProfile -Command "& '$(RCXX_TARGET)' --target x86_64-unknown-rinos -std=c++20 -c -o '$(TEST_OUT)/cxx-new-array/invalid.ro' tests/cxx_new_array_parenthesized_rejected.cpp *> '$(TEST_OUT)/cxx-new-array/invalid.log'; if ($$LASTEXITCODE -eq 0) { exit 1 } else { exit 0 }"
	powershell -NoProfile -Command "if (-not (Select-String -SimpleMatch -Quiet 'array new element initializers require braces' '$(TEST_OUT)/cxx-new-array/invalid.log')) { exit 1 }"
else
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-new-array/invalid.ro \
		tests/cxx_new_array_parenthesized_rejected.cpp \
		>$(TEST_OUT)/cxx-new-array/invalid.log 2>&1; then \
		echo "parenthesized array-new initializer unexpectedly compiled"; exit 1; \
	fi
	grep -q "array new element initializers require braces" \
		$(TEST_OUT)/cxx-new-array/invalid.log
endif
	@echo "RCC++ scalar and constructor array-new initializer tests completed"

test-cxx-language-linkage: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-language-linkage
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-linkage/x86.ro \
		tests/cxx_language_linkage.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-language-linkage/x64.ro \
		tests/cxx_language_linkage.cpp
	strings $(TEST_OUT)/cxx-language-linkage/x86.ro | \
		grep -x -q '_Z21call_language_linkagei'
	strings $(TEST_OUT)/cxx-language-linkage/x64.ro | \
		grep -x -q '_Z18cpp_linkage_importi'
	strings $(TEST_OUT)/cxx-language-linkage/x64.ro | \
		grep -x -q '_Z19cpp_linkage_counter'
	strings $(TEST_OUT)/cxx-language-linkage/x86.ro | \
		grep -x -q 'linkage_import'
	strings $(TEST_OUT)/cxx-language-linkage/x86.ro | \
		grep -x -q 'second_linkage_import'
	strings $(TEST_OUT)/cxx-language-linkage/x64.ro | \
		grep -x -q 'c_linkage_counter'
	! strings $(TEST_OUT)/cxx-language-linkage/x64.ro | \
		grep -x -q '_Z14linkage_importi'
	@echo "RCC++ C/C++ language-linkage tests completed"

test-cxx-member-specifiers: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-member-specifiers
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
	strings $(TEST_OUT)/cxx-function-templates/x86.ro | \
		grep -F -x -q '_ZN8identityEi'
	strings $(TEST_OUT)/cxx-function-templates/x86.ro | \
		grep -F -x -q '_ZN8identityEl'
	strings $(TEST_OUT)/cxx-function-templates/x86.ro | \
		grep -F -x -q '_ZN6detail16pointer_identityEPi'
	strings $(TEST_OUT)/cxx-function-templates/x86.ro | \
		grep -F -x -q '_ZN6detail15default_deducedEIlEi'
	strings $(TEST_OUT)/cxx-function-templates/x86.ro | \
		grep -F -x -q '_ZN6detail18type_only_templateEIiEv'
	strings $(TEST_OUT)/cxx-function-templates/x86.ro | \
		grep -F -x -q '_ZN6detail18type_only_templateEIlEv'
	@echo "RCC++ function template syntax tests completed"

test-cxx-function-template-overloads: $(RCXX_TARGET)
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
	@echo "RCC++ function-template overload and expression-deduction tests completed"

test-cxx-function-template-references: $(RCXX_TARGET)
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
	grep -q "no matching function template overload for 'read_rvalue'" \
		$(TEST_OUT)/cxx-function-template-references/invalid.log
	@echo "RCC++ function-template reference and function-pointer deduction tests completed"

test-cxx-constraints: $(RCXX_TARGET)
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
	grep -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-constraints/invalid-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-constraints/invalid-x64.ro \
		tests/cxx_constraints_invalid.cpp \
		>$(TEST_OUT)/cxx-constraints/invalid-x64.log 2>&1; then \
		echo "invalid constrained template unexpectedly compiled"; exit 1; \
	fi
	grep -q "template constraints are not satisfied" \
		$(TEST_OUT)/cxx-constraints/invalid-x64.log
	@echo "RCC++ integral template constraint tests completed"

test-cxx-class-template-methods: $(RCXX_TARGET)
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

test-cxx-class-template-specialization: $(RCXX_TARGET)
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

test-cxx-class-template-specialization-ambiguous: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-class-template-specialization-ambiguous)
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.ro \
		tests/cxx_class_template_specialization_ambiguous.cpp \
		>$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.log 2>&1; then \
		echo "ambiguous class template specialization unexpectedly compiled"; exit 1; \
	fi
	grep -q "ambiguous class template partial specialization" \
		$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.ro \
		tests/cxx_class_template_specialization_ambiguous.cpp \
		>$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.log 2>&1; then \
		echo "ambiguous class template specialization unexpectedly compiled"; exit 1; \
	fi
	grep -q "ambiguous class template partial specialization" \
		$(TEST_OUT)/cxx-class-template-specialization-ambiguous/x64.log
	@echo "RCC++ ambiguous class-template specialization diagnostic completed"

test-cxx-class-template-non-type: $(RCXX_TARGET)
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

test-cxx-operator-overload: $(RCXX_TARGET)
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

test-cxx-member-operator-forms: $(RCXX_TARGET)
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

test-cxx-assignment-operator: $(RCXX_TARGET)
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

test-cxx-lambda: $(RCXX_TARGET)
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

test-cxx-conversion-operator: $(RCXX_TARGET)
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

test-cxx-nonmember-operator: $(RCXX_TARGET)
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

test-cxx-non-type-templates: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-non-type-templates)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-non-type-templates/x86.ro \
		tests/cxx_non_type_templates.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-non-type-templates/x64.ro \
		tests/cxx_non_type_templates.cpp
	strings $(TEST_OUT)/cxx-non-type-templates/x86.ro | \
		grep -F -x -q '_ZN12add_constantEILi3EEi'
	strings $(TEST_OUT)/cxx-non-type-templates/x86.ro | \
		grep -F -x -q '_ZN12add_constantEILin2EEi'
	strings $(TEST_OUT)/cxx-non-type-templates/x64.ro | \
		grep -F -x -q '_ZN20add_default_constantEILi4EEi'
	strings $(TEST_OUT)/cxx-non-type-templates/x86.ro | \
		grep -F -x -q '_ZN22add_default_from_valueEILi3ELi4EEi'
	@echo "RCC++ non-type integer template tests completed"

test-cxx-auto-non-type-template: $(RCXX_TARGET)
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

test-cxx-range-for: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-range-for)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-range-for/x86.s \
		tests/cxx_function_pointer_probe.cpp
	grep -F -x -q '.globl _Z12call_pointerPFiiEi' \
		$(TEST_OUT)/cxx-range-for/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/cxx-range-for/x86.o \
		$(TEST_OUT)/cxx-range-for/x86.s
	$(CC) -m32 -o $(TEST_OUT)/cxx-range-for/x86 \
		tests/cxx_range_for_run_test.c \
		$(TEST_OUT)/cxx-range-for/x86.o
	$(TEST_OUT)/cxx-range-for/x86
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-range-for/x64.s \
		tests/cxx_function_pointer_probe.cpp
	grep -F -x -q '.globl _Z12call_pointerPFiiEi' \
		$(TEST_OUT)/cxx-range-for/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-range-for/x64.o \
		$(TEST_OUT)/cxx-range-for/x64.s
	$(CC) -o $(TEST_OUT)/cxx-range-for/x64 \
		tests/cxx_range_for_run_test.c \
		$(TEST_OUT)/cxx-range-for/x64.o
	$(TEST_OUT)/cxx-range-for/x64
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-range-for/invalid.ro \
		tests/cxx_range_for_invalid.cpp \
		>$(TEST_OUT)/cxx-range-for/invalid.log 2>&1; then \
		echo "const auto&& range-for unexpectedly compiled"; exit 1; \
	fi
	grep -q "const auto&& range variable cannot bind to an array lvalue" \
		$(TEST_OUT)/cxx-range-for/invalid.log
	@echo "C++ array range-for tests completed"

test-cxx-lambda-function-pointer: $(RCXX_TARGET)
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

test-cxx-if-constexpr: $(RCXX_TARGET)
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
	grep -q "if constexpr condition is not a constant expression" \
		$(TEST_OUT)/cxx-if-constexpr/nonconstant.log
	@echo "C++ if constexpr selection and diagnostics tests completed"

test-cxx-if-constexpr-template: $(RCXX_TARGET)
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

test-cxx-adl-multiple-namespaces: $(RCXX_TARGET)
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

test-cxx-using-overload-namespaces: $(RCXX_TARGET)
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
	grep -q "ambiguous overload for 'choose'" \
		$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.ro \
		tests/cxx_using_overload_ambiguous.cpp \
		>$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.log 2>&1
	grep -q "ambiguous overload for 'choose'" \
		$(TEST_OUT)/cxx-using-overload-namespaces/ambiguous-x64.log
	@echo "C++ using-namespace overload tests completed"

test-cxx-template-two-phase-namespace: $(RCXX_TARGET)
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

test-cxx-template-two-phase-adl: $(RCXX_TARGET)
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

test-cxx-template-two-phase-ordinary: $(RCXX_TARGET)
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

test-cxx-template-parameter-pack: $(RCXX_TARGET)
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
	grep -q "only a single integral non-type pack is supported in class templates" \
		$(TEST_OUT)/cxx-template-parameter-pack/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.ro \
		tests/cxx_template_parameter_pack_invalid.cpp \
		>$(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.log 2>&1
	grep -q "only a single integral non-type pack is supported in class templates" \
		$(TEST_OUT)/cxx-template-parameter-pack/invalid-x64.log
	@echo "C++ type parameter pack arity tests completed"

test-cxx-qualified-class-initialization: $(RCXX_TARGET)
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

test-cxx-constexpr-pointer: $(RCXX_TARGET)
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

test-cxx-constexpr-pointer-mutation: $(RCXX_TARGET)
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

test-cxx-constexpr-pointer-aggregate: $(RCXX_TARGET)
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

test-cxx-noexcept-expression: $(RCXX_TARGET)
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

test-cxx-auto-return: $(RCXX_TARGET)
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
	grep -q "inconsistent deduction for auto return type" \
		$(TEST_OUT)/cxx-auto-return/invalid.log
	grep -q "auto return type requires a function definition" \
		$(TEST_OUT)/cxx-auto-return/invalid.log
	@echo "C++ auto return deduction tests completed"

test-cxx-decltype: $(RCXX_TARGET)
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
		echo "unsupported decltype expression unexpectedly compiled"; exit 1; \
	fi

test-cxx-decltype-auto: $(RCXX_TARGET)
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
	grep -q "auto return type requires a function definition" \
		$(TEST_OUT)/cxx-decltype-auto/invalid.log
	@echo "C++ decltype(auto) tests completed"

test-cxx-auto-local-refs: $(RCXX_TARGET)
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
	grep -q "auto& initializer must be an lvalue" \
		$(TEST_OUT)/cxx-auto-local-refs/invalid.log
	grep -q "auto\* initializer must be a pointer or array" \
		$(TEST_OUT)/cxx-auto-local-refs/invalid.log
	@echo "C++ local auto reference tests completed"
	grep -q "unsupported operator in decltype expression" \
		$(TEST_OUT)/cxx-decltype/invalid.log
	@echo "C++ decltype tests completed"

test-cxx-non-type-template-deduction: $(RCXX_TARGET)
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
	mkdir -p $(TEST_OUT)/initializer-mixed
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/initializer-mixed/x86.ro \
		tests/initializer_mixed.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/initializer-mixed/x64.ro \
		tests/initializer_mixed.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/initializer_mixed_run_test \
		tests/initializer_mixed_run_test.c src/emit_ro.c \
		src/utils.c
	$(TEST_OUT)/initializer_mixed_run_test \
		$(TEST_OUT)/initializer-mixed/x86.ro \
		$(TEST_OUT)/initializer-mixed/x64.ro
	@echo "C17 mixed designator and string-row initializer tests completed"

test-flexible-arrays: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/flexible-arrays)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/x86.ro tests/flexible_array.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/x64.ro tests/flexible_array.c
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/flexible-arrays/run-test-x86 \
		tests/flexible_array_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/flexible-arrays/run-test-x64 \
		tests/flexible_array_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/flexible-arrays/run-test-x86 \
		$(TEST_OUT)/flexible-arrays/x86.ro
	$(TEST_OUT)/flexible-arrays/run-test-x64 \
		$(TEST_OUT)/flexible-arrays/x64.ro
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/invalid-x86.ro \
		tests/invalid_flexible_array.c \
		>$(TEST_OUT)/flexible-arrays/invalid-x86.log 2>&1
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/invalid-x64.ro \
		tests/invalid_flexible_array.c \
		>$(TEST_OUT)/flexible-arrays/invalid-x64.log 2>&1
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/invalid-initializer-x86.ro \
		tests/invalid_flexible_initializer.c \
		>$(TEST_OUT)/flexible-arrays/invalid-initializer-x86.log 2>&1
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/flexible-arrays/invalid-initializer-x64.ro \
		tests/invalid_flexible_initializer.c \
		>$(TEST_OUT)/flexible-arrays/invalid-initializer-x64.log 2>&1
	grep -q "flexible array member requires another named member" \
		$(TEST_OUT)/flexible-arrays/invalid-x86.log
	grep -q "flexible array member must be the last member" \
		$(TEST_OUT)/flexible-arrays/invalid-x86.log
	grep -q "flexible array member is not allowed in a union" \
		$(TEST_OUT)/flexible-arrays/invalid-x86.log
	grep -q "flexible array member cannot be initialized" \
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
	strings $(TEST_OUT)/floating-static-initializers/x64.s | grep -F -q "0x00, 0x00, 0xe0, 0x3f"
	strings $(TEST_OUT)/floating-static-initializers/x64.s | grep -F -q "0x00, 0x00, 0xf8, 0xbf"
	@echo "RCC C17 floating static/TLS initializer tests completed"

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
	wsl -d Ubuntu-24.04 bash -lc "gcc -m32 -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/floating-runtime-i686/runtime.o $(WSL_RINCOMPILER_ROOT)/build/tests/floating-runtime-i686/runtime.s; gcc -m32 -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/floating-runtime-i686/start.o $(WSL_RINCOMPILER_ROOT)/tests/floating_runtime_i686_start.s; gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start -o $(WSL_RINCOMPILER_ROOT)/build/tests/floating-runtime-i686/runtime $(WSL_RINCOMPILER_ROOT)/build/tests/floating-runtime-i686/start.o $(WSL_RINCOMPILER_ROOT)/build/tests/floating-runtime-i686/runtime.o; $(WSL_RINCOMPILER_ROOT)/build/tests/floating-runtime-i686/runtime"
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

ifeq ($(OS),Windows_NT)
test-vla-runtime: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-runtime)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/vla-runtime/x86.s tests/vla_runtime.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/vla-runtime/x64.s tests/vla_runtime.c
	wsl -d Ubuntu-24.04 bash -lc "set -e; gcc -m32 -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x86.o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x86.s; gcc -m32 -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x86-start.o $(WSL_RINCOMPILER_ROOT)/tests/vla_runtime_i686_start.s; gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start -o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x86 $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x86-start.o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x86.o; $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x86; gcc -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x64.o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x64.s; gcc -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x64-start.o $(WSL_RINCOMPILER_ROOT)/tests/vla_runtime_x64_start.s; gcc -nostdlib -static -no-pie -Wl,--entry=_start -o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x64 $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x64-start.o $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x64.o; $(WSL_RINCOMPILER_ROOT)/build/tests/vla-runtime/x64"
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
	powershell -NoProfile -Command "& './rcc.exe' --target i686-unknown-rinos -c -o '$(TEST_OUT)/vla-semantics/invalid-x86.ro' tests/invalid_vla_goto.c *> '$(TEST_OUT)/vla-semantics/invalid-x86.log'; if ($$LASTEXITCODE -eq 0) { exit 1 } else { exit 0 }"
	powershell -NoProfile -Command "& './rcc.exe' --target x86_64-unknown-rinos -c -o '$(TEST_OUT)/vla-semantics/invalid-x64.ro' tests/invalid_vla_goto.c *> '$(TEST_OUT)/vla-semantics/invalid-x64.log'; if ($$LASTEXITCODE -eq 0) { exit 1 } else { exit 0 }"
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -Pattern 'goto enters a variable-length array scope' -Path '$(TEST_OUT)/vla-semantics/invalid-x86.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -Pattern 'goto enters a variable-length array scope' -Path '$(TEST_OUT)/vla-semantics/invalid-x64.log')) { exit 1 }"
	powershell -NoProfile -Command "& './rcc.exe' --target i686-unknown-rinos -c -o '$(TEST_OUT)/vla-semantics/invalid-array-x86.ro' tests/invalid_array_parameter_qualifiers.c *> '$(TEST_OUT)/vla-semantics/invalid-array-x86.log'; if ($$LASTEXITCODE -eq 0) { exit 1 } else { exit 0 }"
	powershell -NoProfile -Command "& './rcc.exe' --target x86_64-unknown-rinos -c -o '$(TEST_OUT)/vla-semantics/invalid-array-x64.ro' tests/invalid_array_parameter_qualifiers.c *> '$(TEST_OUT)/vla-semantics/invalid-array-x64.log'; if ($$LASTEXITCODE -eq 0) { exit 1 } else { exit 0 }"
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -Pattern 'array parameter qualifiers are only valid' -Path '$(TEST_OUT)/vla-semantics/invalid-array-x86.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -Pattern 'array parameter qualifiers are only valid' -Path '$(TEST_OUT)/vla-semantics/invalid-array-x64.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -Pattern 'static array parameter requires a bound expression' -Path '$(TEST_OUT)/vla-semantics/invalid-array-x86.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -Pattern 'static array parameter requires a bound expression' -Path '$(TEST_OUT)/vla-semantics/invalid-array-x64.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -Pattern 'unspecified variable-length array is only valid' -Path '$(TEST_OUT)/vla-semantics/invalid-array-x86.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -Quiet -Pattern 'unspecified variable-length array is only valid' -Path '$(TEST_OUT)/vla-semantics/invalid-array-x64.log')) { exit 1 }"
	@echo "Dual-architecture VLA goto semantic tests completed"
else
test-vla-semantics: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-semantics)
	if $(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-x86.ro tests/invalid_vla_goto.c >$(TEST_OUT)/vla-semantics/invalid-x86.log 2>&1; then exit 1; fi
	if $(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-x64.ro tests/invalid_vla_goto.c >$(TEST_OUT)/vla-semantics/invalid-x64.log 2>&1; then exit 1; fi
	grep -q 'goto enters a variable-length array scope' $(TEST_OUT)/vla-semantics/invalid-x86.log
	grep -q 'goto enters a variable-length array scope' $(TEST_OUT)/vla-semantics/invalid-x64.log
	if $(RCC_TARGET) --target i686-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-array-x86.ro tests/invalid_array_parameter_qualifiers.c >$(TEST_OUT)/vla-semantics/invalid-array-x86.log 2>&1; then exit 1; fi
	if $(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/vla-semantics/invalid-array-x64.ro tests/invalid_array_parameter_qualifiers.c >$(TEST_OUT)/vla-semantics/invalid-array-x64.log 2>&1; then exit 1; fi
	grep -q 'array parameter qualifiers are only valid' $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	grep -q 'array parameter qualifiers are only valid' $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	grep -q 'static array parameter requires a bound expression' $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	grep -q 'static array parameter requires a bound expression' $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	grep -q 'unspecified variable-length array is only valid' $(TEST_OUT)/vla-semantics/invalid-array-x86.log
	grep -q 'unspecified variable-length array is only valid' $(TEST_OUT)/vla-semantics/invalid-array-x64.log
	@echo "Dual-architecture VLA goto semantic tests completed"
endif

test-cxx-qualified-namespaces: $(RCC_TARGET) $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-qualified-namespaces
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-qualified-namespaces/x86.ro \
		tests/cxx_qualified_namespace.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-qualified-namespaces/x64.ro \
		tests/cxx_qualified_namespace.cpp
	grep -a -q '_ZN3api9transformEi' \
		$(TEST_OUT)/cxx-qualified-namespaces/x86.ro
	grep -a -q '_ZN3api6nested5applyEi' \
		$(TEST_OUT)/cxx-qualified-namespaces/x64.ro
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-qualified-namespaces/c-mode.ro \
		tests/c_scope_operator_rejected.c
	@echo "RCC++ qualified namespace and C mode-isolation tests completed"

test-cxx-using: $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-using)
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
	@echo "RCC++ using-directive, using-declaration, and alias tests completed"

ifeq ($(OS),Windows_NT)
test-cxx-member-methods: $(RCC_TARGET) $(RCXX_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/cxx-member-methods)
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-methods/x86.s \
		tests/cxx_member_methods.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-member-methods/x64.s \
		tests/cxx_member_methods.cpp
	wsl -d Ubuntu-24.04 bash -lc "set -e; gcc -m32 -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x86.o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x86.s; gcc -m32 -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/start.o $(WSL_RINCOMPILER_ROOT)/tests/cxx_member_methods_i686_start.s; gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start -o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x86 $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/start.o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x86.o; $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x86; gcc -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x64.o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x64.s; gcc -c -o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/start64.o $(WSL_RINCOMPILER_ROOT)/tests/cxx_member_methods_x64_start.s; gcc -nostdlib -static -no-pie -Wl,--entry=_start -o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x64 $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/start64.o $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x64.o; $(WSL_RINCOMPILER_ROOT)/build/tests/cxx-member-methods/x64"
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
.PHONY: test-cxx-class-template-specialization-ambiguous
.PHONY: test-cxx-class-template-non-type
.PHONY: test-cxx-operator-overload
.PHONY: test-cxx-member-operator-forms
.PHONY: test-cxx-assignment-operator
.PHONY: test-cxx-lambda
.PHONY: test-cxx-conversion-operator
.PHONY: test-cxx-nonmember-operator
.PHONY: test-cxx-non-type-template-deduction
test-cxx-static-members: $(RCXX_TARGET)
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

test-cxx-static-data-members: $(RCXX_TARGET)
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

test-cxx-class-template-static-data: $(RCXX_TARGET)
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

test-cxx-class-template-static-data-odr: $(RCXX_TARGET)
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

test-vla-declarations: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/vla-declarations)
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-storage-x86.ro \
		tests/invalid_vla_storage.c \
		>$(TEST_OUT)/vla-declarations/invalid-storage-x86.log 2>&1
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-member-x86.ro \
		tests/invalid_vla_member.c \
		>$(TEST_OUT)/vla-declarations/invalid-member-x86.log 2>&1
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-storage-x64.ro \
		tests/invalid_vla_storage.c \
		>$(TEST_OUT)/vla-declarations/invalid-storage-x64.log 2>&1
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-member-x64.ro \
		tests/invalid_vla_member.c \
		>$(TEST_OUT)/vla-declarations/invalid-member-x64.log 2>&1
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-initializer-x86.ro \
		tests/invalid_vla_initializer.c \
		>$(TEST_OUT)/vla-declarations/invalid-initializer-x86.log 2>&1
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/vla-declarations/invalid-initializer-x64.ro \
		tests/invalid_vla_initializer.c \
		>$(TEST_OUT)/vla-declarations/invalid-initializer-x64.log 2>&1
	grep -q "variably modified object cannot have linkage" \
		$(TEST_OUT)/vla-declarations/invalid-storage-x86.log
	grep -q "variably modified typedef is only valid at block scope" \
		$(TEST_OUT)/vla-declarations/invalid-storage-x86.log
	grep -q "variably modified type is not allowed for struct/union member" \
		$(TEST_OUT)/vla-declarations/invalid-member-x86.log
	grep -q "variably modified object cannot have linkage" \
		$(TEST_OUT)/vla-declarations/invalid-storage-x64.log
	grep -q "variably modified typedef is only valid at block scope" \
		$(TEST_OUT)/vla-declarations/invalid-storage-x64.log
	grep -q "variably modified type is not allowed for struct/union member" \
		$(TEST_OUT)/vla-declarations/invalid-member-x64.log
	grep -q "variable-length array cannot have an initializer" \
		$(TEST_OUT)/vla-declarations/invalid-initializer-x86.log
	grep -q "variable-length array cannot have an initializer" \
		$(TEST_OUT)/vla-declarations/invalid-initializer-x64.log
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
	@echo "Dual-architecture VLA declarator variant assembly generation completed; runtime execution is verified by the direct WSL host check"
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
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constructor-body/x86.s \
		tests/cxx_constructor_body.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-constructor-body/x64.s \
		tests/cxx_constructor_body.cpp
	@echo "Dual-architecture C++ constructor-body assembly generation completed; host execution is verified by the direct WSL check"
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

test-cxx-constructor-initializer-body: $(RCXX_TARGET)
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

test-cxx-base-constructor-initializer: $(RCXX_TARGET)
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

test-cxx-default-member-initializer: $(RCXX_TARGET)
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
	grep -q "new requires scalar constant default member initializers" \
		$(TEST_OUT)/cxx-default-member-initializer/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-default-member-initializer/invalid-x64.ro \
		tests/cxx_default_member_initializer_array_invalid.cpp \
		>$(TEST_OUT)/cxx-default-member-initializer/invalid-x64.log 2>&1
	grep -q "new requires scalar constant default member initializers" \
		$(TEST_OUT)/cxx-default-member-initializer/invalid-x64.log
	@echo "C++ default member initializer tests completed"

test-cxx-delegating-constructor: $(RCXX_TARGET)
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
	grep -q "cyclic C++ delegating constructor" \
		$(TEST_OUT)/cxx-delegating-constructor/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-delegating-constructor/invalid-x64.ro \
		tests/cxx_delegating_constructor_invalid.cpp \
		>$(TEST_OUT)/cxx-delegating-constructor/invalid-x64.log 2>&1
	grep -q "cyclic C++ delegating constructor" \
		$(TEST_OUT)/cxx-delegating-constructor/invalid-x64.log
	@echo "C++ delegating constructor tests completed"

test-cxx-converting-constructor: $(RCXX_TARGET)
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
	grep -q "no safely lowerable constructor accepts the C++ initializer" \
		$(TEST_OUT)/cxx-converting-constructor/invalid-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-converting-constructor/invalid-x64.ro \
		tests/cxx_explicit_copy_initialization_invalid.cpp \
		>$(TEST_OUT)/cxx-converting-constructor/invalid-x64.log 2>&1
	grep -q "no safely lowerable constructor accepts the C++ initializer" \
		$(TEST_OUT)/cxx-converting-constructor/invalid-x64.log
	@echo "C++ converting constructor tests completed"

test-aggregate-union-abi: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/aggregate-union-abi)
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-union-abi/x86.s \
		tests/aggregate_union_abi.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-union-abi/x64.s \
		tests/aggregate_union_abi.c
	$(CC) -m32 -c -o $(TEST_OUT)/aggregate-union-abi/x86.o \
		$(TEST_OUT)/aggregate-union-abi/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/aggregate-union-abi/host-x86.o \
		tests/aggregate_union_abi_host.c
	$(CC) -m32 -o $(TEST_OUT)/aggregate-union-abi/x86 \
		$(TEST_OUT)/aggregate-union-abi/host-x86.o \
		$(TEST_OUT)/aggregate-union-abi/x86.o
	$(TEST_OUT)/aggregate-union-abi/x86
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
	$(CC) -m32 -c -o $(TEST_OUT)/aggregate-flexible-abi/x86.o \
		$(TEST_OUT)/aggregate-flexible-abi/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/aggregate-flexible-abi/host-x86.o \
		tests/aggregate_flexible_abi_host.c
	$(CC) -m32 -o $(TEST_OUT)/aggregate-flexible-abi/x86 \
		$(TEST_OUT)/aggregate-flexible-abi/host-x86.o \
		$(TEST_OUT)/aggregate-flexible-abi/x86.o
	$(TEST_OUT)/aggregate-flexible-abi/x86
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
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-sse-abi/x86.s \
		tests/aggregate_sse_abi.c
	$(CC) -m32 -c -o $(TEST_OUT)/aggregate-sse-abi/x86.o \
		$(TEST_OUT)/aggregate-sse-abi/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/aggregate-sse-abi/host-x86.o \
		tests/aggregate_sse_abi_host.c
	$(CC) -m32 -o $(TEST_OUT)/aggregate-sse-abi/x86 \
		$(TEST_OUT)/aggregate-sse-abi/host-x86.o \
		$(TEST_OUT)/aggregate-sse-abi/x86.o
	$(TEST_OUT)/aggregate-sse-abi/x86
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
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
	$(RCC_TARGET) --target i686-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-nested-abi/x86.s \
		tests/aggregate_nested_abi.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -S \
		-o $(TEST_OUT)/aggregate-nested-abi/x64.s \
		tests/aggregate_nested_abi.c
	$(CC) -m32 -c -o $(TEST_OUT)/aggregate-nested-abi/x86.o \
		$(TEST_OUT)/aggregate-nested-abi/x86.s
	$(CC) -m32 -c -o $(TEST_OUT)/aggregate-nested-abi/host-x86.o \
		tests/aggregate_nested_abi_host.c
	$(CC) -m32 -o $(TEST_OUT)/aggregate-nested-abi/x86 \
		$(TEST_OUT)/aggregate-nested-abi/host-x86.o \
		$(TEST_OUT)/aggregate-nested-abi/x86.o
	$(TEST_OUT)/aggregate-nested-abi/x86
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
	mkdir -p $(TEST_OUT)/cxx-overloads
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/x86.ro tests/cxx_overload.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/x64.ro tests/cxx_overload.cpp
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/cxx-overloads/verify \
		tests/cxx_overload_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/cxx-overloads/verify \
		$(TEST_OUT)/cxx-overloads/x86.ro \
		$(TEST_OUT)/cxx-overloads/x64.ro
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/ambiguous.ro \
		tests/cxx_overload_ambiguous.cpp \
		>$(TEST_OUT)/cxx-overloads/ambiguous.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "ambiguous overload for 'ambiguous'" \
		$(TEST_OUT)/cxx-overloads/ambiguous.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/nullptr-integer.ro \
		tests/cxx_nullptr_integer_rejected.cpp \
		>$(TEST_OUT)/cxx-overloads/nullptr-integer.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "incompatible type for argument 1 to 'consume_integer'" \
		$(TEST_OUT)/cxx-overloads/nullptr-integer.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/nullptr-operators.ro \
		tests/cxx_nullptr_operators_rejected.cpp \
		>$(TEST_OUT)/cxx-overloads/nullptr-operators.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "nullptr does not support arithmetic operators" \
		$(TEST_OUT)/cxx-overloads/nullptr-operators.log
	grep -q "nullptr does not support integer operators" \
		$(TEST_OUT)/cxx-overloads/nullptr-operators.log
	grep -q "comparison requires arithmetic or pointer operands" \
		$(TEST_OUT)/cxx-overloads/nullptr-operators.log
	grep -q "nullptr can only be assigned to a pointer" \
		$(TEST_OUT)/cxx-overloads/nullptr-operators.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/default-arguments.ro \
		tests/cxx_default_arguments_rejected.cpp \
		>$(TEST_OUT)/cxx-overloads/default-arguments.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "parameter without a default follows a default argument" \
		$(TEST_OUT)/cxx-overloads/default-arguments.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/default-redefinition.ro \
		tests/cxx_default_redefinition_rejected.cpp \
		>$(TEST_OUT)/cxx-overloads/default-redefinition.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "redefinition of default argument for parameter 1" \
		$(TEST_OUT)/cxx-overloads/default-redefinition.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/default-type.ro \
		tests/cxx_default_type_rejected.cpp \
		>$(TEST_OUT)/cxx-overloads/default-type.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "default argument is incompatible with parameter 1" \
		$(TEST_OUT)/cxx-overloads/default-type.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-overloads/default-function-pointer.ro \
		tests/cxx_default_function_pointer_rejected.cpp \
		>$(TEST_OUT)/cxx-overloads/default-function-pointer.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "too few arguments to function call" \
		$(TEST_OUT)/cxx-overloads/default-function-pointer.log
	@echo "RCC++ overload resolution tests completed"

test-cxx-inline-aggregates: $(RCC_TARGET) $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-inline-aggregates
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
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/cxx-inline-aggregates/run-x86 \
		tests/cxx_value_init_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/cxx-inline-aggregates/run-x64 \
		tests/cxx_value_init_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/cxx-inline-aggregates/run-x86 \
		$(TEST_OUT)/cxx-inline-aggregates/x86.ro
	$(TEST_OUT)/cxx-inline-aggregates/run-x64 \
		$(TEST_OUT)/cxx-inline-aggregates/x64.ro
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/c-empty.ro \
		tests/c_empty_initializer_rejected.c
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/private.ro \
		tests/cxx_private_member_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/private.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "member 'value' is not accessible" \
		$(TEST_OUT)/cxx-inline-aggregates/private.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/arity.ro \
		tests/cxx_constructor_arity_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/arity.log 2>&1; status=$$?; set -e; \
		test $$status -ne 0
	grep -q "no safely lowerable constructor accepts 0 arguments" \
		$(TEST_OUT)/cxx-inline-aggregates/arity.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/const-reference.ro \
		tests/cxx_const_reference_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/const-reference.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "incompatible type for argument 1 to 'reference_test::mutable_reference'" \
		$(TEST_OUT)/cxx-inline-aggregates/const-reference.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/private-method.ro \
		tests/cxx_private_method_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/private-method.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "method 'secret' is not accessible" \
		$(TEST_OUT)/cxx-inline-aggregates/private-method.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/template-arity.ro \
		tests/cxx_template_constructor_arity_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/template-arity.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "no safely lowerable constructor accepts 1 argument" \
		$(TEST_OUT)/cxx-inline-aggregates/template-arity.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/versioned-rejected.ro \
		tests/cxx_versioned_template_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/versioned-rejected.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "function template 'unsafe_versioned' is not safely lowerable" \
		$(TEST_OUT)/cxx-inline-aggregates/versioned-rejected.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/auto-rejected.ro \
		tests/cxx_auto_initializer_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/auto-rejected.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "auto variable requires an initializer" \
		$(TEST_OUT)/cxx-inline-aggregates/auto-rejected.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/cleanup-copy.ro \
		tests/cxx_cleanup_copy_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/cleanup-copy.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "C++ scope-cleanup object requires a validated direct constructor" \
		$(TEST_OUT)/cxx-inline-aggregates/cleanup-copy.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/cleanup-flow.ro \
		tests/cxx_cleanup_control_flow_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/cleanup-flow.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "goto enters a C++ scope-cleanup object lifetime" \
		$(TEST_OUT)/cxx-inline-aggregates/cleanup-flow.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/cleanup-switch-scope.ro \
		tests/cxx_cleanup_switch_scope_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/cleanup-switch-scope.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "case label crosses C++ scope-cleanup object initialization" \
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
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-move.ro \
		tests/cxx_unsafe_move_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/unsafe-move.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "C++ move construction requires a validated release constructor" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-move.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-move-assignment.ro \
		tests/cxx_unsafe_move_assignment_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/unsafe-move-assignment.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "no matching member overload for 'operator='" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-move-assignment.log
	@echo "RCC++ inline C ABI aggregate wrapper tests completed"

test-cxx-parser-recovery: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-parser-recovery
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
	grep -q "expected ;" $(TEST_OUT)/cxx-parser-recovery/invalid.log
	! grep -q "too many errors" $(TEST_OUT)/cxx-parser-recovery/invalid.log
	@echo "RCC++ namespace parser recovery test completed"

test-cxx-exceptions: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-exceptions
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
	grep -q "C++ catch-all handler must be the last handler" \
		$(TEST_OUT)/cxx-exceptions/invalid-order-x86.log
	! $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-exceptions/invalid-order-x64.ro \
		tests/cxx_exceptions_invalid.cpp \
		>$(TEST_OUT)/cxx-exceptions/invalid-order-x64.log 2>&1
	grep -q "C++ catch-all handler must be the last handler" \
		$(TEST_OUT)/cxx-exceptions/invalid-order-x64.log
	@echo "RCC++ exception propagation and nested handler tests completed"

test-cxx-object-exceptions: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-object-exceptions
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

test-cxx-cross-library-exceptions: $(RCXX_TARGET) $(RLD_TARGET) $(RINVALIDATE)
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

test-cxx-cross-translation-unit-virtual: $(RCXX_TARGET)
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

test-cxx-exception-cleanup: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-exception-cleanup
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
	@set +e; $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-exception-cleanup/call-x86.ro \
		tests/cxx_exception_cleanup_call_rejected.cpp \
		>$(TEST_OUT)/cxx-exception-cleanup/call-x86.log 2>&1; status=$$?; \
		set -e; test $$status -ne 0
	grep -q "C++ exception cleanup requires a call-free protected body" \
		$(TEST_OUT)/cxx-exception-cleanup/call-x86.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-exception-cleanup/call-x64.ro \
		tests/cxx_exception_cleanup_call_rejected.cpp \
		>$(TEST_OUT)/cxx-exception-cleanup/call-x64.log 2>&1; status=$$?; \
		set -e; test $$status -ne 0
	grep -q "C++ exception cleanup requires a call-free protected body" \
		$(TEST_OUT)/cxx-exception-cleanup/call-x64.log
	@echo "RCC++ same-function exception cleanup tests completed"

test-cxx-nontrivial-object-exceptions: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-nontrivial-object-exceptions
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
	mkdir -p $(TEST_OUT)/cxx-const-member-overload
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -S \
		-o $(TEST_OUT)/cxx-const-member-overload/x86.s \
		tests/cxx_const_member_overload.cpp
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
	$(CC) -c -o $(TEST_OUT)/cxx-const-member-overload/x64.o \
		$(TEST_OUT)/cxx-const-member-overload/x64.s
	$(CC) -c -o $(TEST_OUT)/cxx-const-member-overload/start-x64.o \
		tests/cxx_member_methods_x64_start.s
	$(CC) -nostdlib -static -no-pie -Wl,--entry=_start \
		-o $(TEST_OUT)/cxx-const-member-overload/x64 \
		$(TEST_OUT)/cxx-const-member-overload/start-x64.o \
		$(TEST_OUT)/cxx-const-member-overload/x64.o
	$(TEST_OUT)/cxx-const-member-overload/x64
	@echo "RCC++ const member overload tests completed"

test-tool-relative-includes: $(RCC_TARGET) $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/tool-relative/cwd
	cd $(TEST_OUT)/tool-relative/cwd && \
		$(abspath $(RCC_TARGET)) --target i686-unknown-rinos -c \
		-o c-x86.ro $(abspath tests/tool_relative_include.c)
	cd $(TEST_OUT)/tool-relative/cwd && \
		$(abspath $(RCXX_TARGET)) --target x86_64-unknown-rinos \
		-std=c++20 -c -o cxx-x64.ro \
		$(abspath tests/tool_relative_include.cpp)
	mkdir -p $(TEST_OUT)/tool-relative/install/bin \
		$(TEST_OUT)/tool-relative/install/include \
		$(TEST_OUT)/tool-relative/install/cwd
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
	mkdir -p $(TEST_OUT)
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
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-if-x86.ro \
		tests/preprocessor_if.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-if-x64.ro \
		tests/preprocessor_if.c
	if $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/invalid-preprocessor-if-x86.ro \
		tests/invalid_preprocessor_if.c >$(TEST_OUT)/invalid-preprocessor-if-x86.log 2>&1; then exit 1; fi
	grep -F -q 'invalid #if expression' $(TEST_OUT)/invalid-preprocessor-if-x86.log
	if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/invalid-preprocessor-if-x64.ro \
		tests/invalid_preprocessor_if.c >$(TEST_OUT)/invalid-preprocessor-if-x64.log 2>&1; then exit 1; fi
	grep -F -q 'invalid #if expression' $(TEST_OUT)/invalid-preprocessor-if-x64.log
	@echo "C17 #if integer constant expression tests completed"

test-preprocessor-operators: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) -E tests/preprocessor_operators.c > $(TEST_OUT)/preprocessor-operators.i
	grep -F -q 'raw_string[] = "WORD + 1"' $(TEST_OUT)/preprocessor-operators.i
	grep -F -q 'expanded_string[] = "42 + 1"' $(TEST_OUT)/preprocessor-operators.i
	grep -F -q 'variadic_string[] = "one, two"' $(TEST_OUT)/preprocessor-operators.i
	grep -F -q 'pasted_identifier = 77' $(TEST_OUT)/preprocessor-operators.i
	grep -F -q 'pasted_number = 123' $(TEST_OUT)/preprocessor-operators.i
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-operators-x86.ro \
		tests/preprocessor_operators.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-operators-x64.ro \
		tests/preprocessor_operators.c
	@echo "C17 #/## replacement-list operator tests completed"

test-preprocessor-va-opt: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) -E tests/preprocessor_va_opt.c > $(TEST_OUT)/preprocessor-va-opt.i
	grep -F -q 'optional_empty = (7 );' $(TEST_OUT)/preprocessor-va-opt.i
	grep -F -q 'optional_value = (7 + 8);' $(TEST_OUT)/preprocessor-va-opt.i
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-va-opt-x86.ro tests/preprocessor_va_opt.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/preprocessor-va-opt-x64.ro tests/preprocessor_va_opt.c
	@echo "C++20 __VA_OPT__ replacement-list tests completed"

test-atomic-builtins: $(RCC_TARGET) $(RLD_TARGET)
	mkdir -p $(TEST_OUT)/atomic-x86 $(TEST_OUT)/atomic-x64
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
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x86/invalid.ro \
		tests/invalid_atomic_builtin.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x64/invalid-order.ro \
		tests/invalid_atomic_order.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x86/invalid-pointer.ro \
		tests/invalid_pointer_atomic.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/atomic-x64/invalid-pointer.ro \
		tests/invalid_pointer_atomic.c
	@echo "Dual-architecture integer/pointer atomic tests completed"

test-x86-wide-scalar: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/x86-wide-scalar
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/x86-wide-scalar/scalar.ro \
		tests/x86_wide_scalar.c
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/x86-wide-scalar/run-test \
		tests/x86_wide_scalar_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/x86-wide-scalar/run-test \
		$(TEST_OUT)/x86-wide-scalar/scalar.ro
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/x86-wide-scalar/invalid.ro \
		tests/invalid_x86_wide_scalar.c
	@echo "i686 64-bit scalar ABI test completed"

test-language-boundaries: $(RCC_TARGET) $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/language-boundaries
	@if $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/language-boundaries/c-x86.ro \
		tests/unsupported_long_double.c \
		>$(TEST_OUT)/language-boundaries/c-x86.log 2>&1; then \
		echo "C long double fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "long double is not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/c-x86.log
	$(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
		-o $(TEST_OUT)/language-boundaries/c17-x86.ro tests/integer_literal.c
	@if $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/language-boundaries/c-literal-x86.ro \
		tests/unsupported_long_double_literal.c \
		>$(TEST_OUT)/language-boundaries/c-literal-x86.log 2>&1; then \
		echo "C long double literal fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "long double literals are not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/c-literal-x86.log
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/language-boundaries/c-x64.ro \
		tests/unsupported_long_double.c \
		>$(TEST_OUT)/language-boundaries/c-x64.log 2>&1; then \
		echo "C long double fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "long double is not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/c-x64.log
	$(RCC_TARGET) --target x86_64-unknown-rinos --std=gnu17 -c \
		-o $(TEST_OUT)/language-boundaries/c17-x64.ro tests/integer_literal.c
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/language-boundaries/c-literal-x64.ro \
		tests/unsupported_long_double_literal.c \
		>$(TEST_OUT)/language-boundaries/c-literal-x64.log 2>&1; then \
		echo "C long double literal fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "long double literals are not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/c-literal-x64.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/cxx-x86.ro \
		tests/unsupported_long_double.c \
		>$(TEST_OUT)/language-boundaries/cxx-x86.log 2>&1; then \
		echo "C++ long double fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "long double is not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/cxx-x86.log
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/cxx-literal-x86.ro \
		tests/unsupported_long_double_literal.c \
		>$(TEST_OUT)/language-boundaries/cxx-literal-x86.log 2>&1; then \
		echo "C++ long double literal fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "long double literals are not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/cxx-literal-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/cxx-x64.ro \
		tests/unsupported_long_double.c \
		>$(TEST_OUT)/language-boundaries/cxx-x64.log 2>&1; then \
		echo "C++ long double fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "long double is not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/cxx-x64.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/cxx-literal-x64.ro \
		tests/unsupported_long_double_literal.c \
		>$(TEST_OUT)/language-boundaries/cxx-literal-x64.log 2>&1; then \
		echo "C++ long double literal fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "long double literals are not supported by the RinOS floating-point ABI" \
		$(TEST_OUT)/language-boundaries/cxx-literal-x64.log
	@for fixture in complex imaginary language_atomic noreturn; do \
		case "$$fixture" in \
			complex) message="_Complex is not supported by the RinOS floating-point ABI" ;; \
			imaginary) message="_Imaginary is not supported by the RinOS floating-point ABI" ;; \
			language_atomic) message="language _Atomic is not supported; use RinOS atomic builtins" ;; \
			noreturn) message="_Noreturn is not supported by the RinOS function ABI" ;; \
		esac; \
		if $(RCC_TARGET) --target i686-unknown-rinos -std=c17 -c \
			-o $(TEST_OUT)/language-boundaries/$$fixture-x86.ro \
			tests/unsupported_$$fixture.c \
			>$(TEST_OUT)/language-boundaries/$$fixture-x86.log 2>&1; then \
			echo "C $$fixture fixture unexpectedly compiled"; exit 1; \
		fi; \
		grep -q "$$message" $(TEST_OUT)/language-boundaries/$$fixture-x86.log; \
		if $(RCC_TARGET) --target x86_64-unknown-rinos -std=c17 -c \
			-o $(TEST_OUT)/language-boundaries/$$fixture-x64.ro \
			tests/unsupported_$$fixture.c \
			>$(TEST_OUT)/language-boundaries/$$fixture-x64.log 2>&1; then \
			echo "C $$fixture fixture unexpectedly compiled"; exit 1; \
		fi; \
		grep -q "$$message" $(TEST_OUT)/language-boundaries/$$fixture-x64.log; \
	done
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/dynamic-cast-x86.ro \
		tests/unsupported_dynamic_cast.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/dynamic-cast-x64.ro \
		tests/unsupported_dynamic_cast.cpp
	@if $(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/const-cast-x86.ro \
		tests/unsupported_const_cast.cpp \
		>$(TEST_OUT)/language-boundaries/const-cast-x86.log 2>&1; then \
		echo "C++ const_cast fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "const_cast requires the same object type" \
		$(TEST_OUT)/language-boundaries/const-cast-x86.log
	@if $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/language-boundaries/const-cast-x64.ro \
		tests/unsupported_const_cast.cpp \
		>$(TEST_OUT)/language-boundaries/const-cast-x64.log 2>&1; then \
		echo "C++ const_cast fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "const_cast requires the same object type" \
		$(TEST_OUT)/language-boundaries/const-cast-x64.log
	@echo "RCC/RCC++ unsupported language and floating-point boundary diagnostics completed"

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
	grep -q "Verified backend: 1 function(s) emitted" \
		$(TEST_OUT)/cxx-const-cast/x86.log
	grep -q "Verified backend: 1 function(s) emitted" \
		$(TEST_OUT)/cxx-const-cast/x64.log
	@echo "C++ cv-only const_cast tests completed"

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
	grep -q "Verified backend: 3 function(s) emitted" \
		$(TEST_OUT)/cxx-dynamic-cast/x86.log
	grep -q "Verified backend: 3 function(s) emitted" \
		$(TEST_OUT)/cxx-dynamic-cast/x64.log
	@echo "C++ statically known public-upcast dynamic_cast tests completed"

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

test-integer-literals: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/integer-literals
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/x86.ro tests/integer_literal.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/x64.ro tests/integer_literal.c
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
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/invalid-decimal.ro \
		tests/invalid_integer_literal.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/invalid-overflow.ro \
		tests/invalid_integer_literal_overflow.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-literals/invalid-suffix.ro \
		tests/invalid_integer_literal_suffix.c
	@echo "C17 integer literal type and value tests completed"

test-integer-promotions: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/integer-promotions
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/integer-promotions/x86.ro tests/integer_promotion.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-promotions/x64.ro tests/integer_promotion.c
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
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-promotions/invalid.ro \
		tests/invalid_integer_operators.c \
		>$(TEST_OUT)/integer-promotions/invalid.log 2>&1; then \
		echo "invalid integer-operator fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "remainder operator requires integer operands" \
		$(TEST_OUT)/integer-promotions/invalid.log
	grep -q "bitwise complement requires integer operand" \
		$(TEST_OUT)/integer-promotions/invalid.log
	grep -q "shift operator requires integer operands" \
		$(TEST_OUT)/integer-promotions/invalid.log
	grep -q "logical not requires scalar operand" \
		$(TEST_OUT)/integer-promotions/invalid.log
	@echo "Dual-architecture C17 integer promotion tests completed"

test-integer-conversions: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/integer-conversions
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/integer-conversions/x86.ro tests/integer_conversion.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/integer-conversions/x64.ro tests/integer_conversion.c
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
	@echo "Dual-architecture C17 integer conversion tests completed"

test-function-calls: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/function-calls
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/function-calls/x86.ro tests/function_call.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/function-calls/x64.ro tests/function_call.c
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
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/function-calls/invalid-call.ro \
		tests/invalid_function_call.c \
		>$(TEST_OUT)/function-calls/invalid-call.log 2>&1; then \
		echo "invalid function calls unexpectedly compiled"; exit 1; \
	fi
	grep -q "too few arguments to function call" \
		$(TEST_OUT)/function-calls/invalid-call.log
	grep -q "too many arguments to function call" \
		$(TEST_OUT)/function-calls/invalid-call.log
	grep -q "incompatible type for argument 1" \
		$(TEST_OUT)/function-calls/invalid-call.log
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/function-calls/invalid-parameters.ro \
		tests/invalid_parameter_list.c \
		>$(TEST_OUT)/function-calls/invalid-parameters.log 2>&1; then \
		echo "invalid parameter lists unexpectedly compiled"; exit 1; \
	fi
	grep -q "ellipsis requires at least one named parameter" \
		$(TEST_OUT)/function-calls/invalid-parameters.log
	grep -q "expected parameter declaration after ','" \
		$(TEST_OUT)/function-calls/invalid-parameters.log
	@echo "Dual-architecture C17 function call contract tests completed"

test-inline-asm-execute: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/inline-asm)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm/x86.ro tests/inline_asm_execution.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm/x64.ro tests/inline_asm_execution.c
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
	grep -q "unsupported AMD64 inline asm instruction" \
		$(TEST_OUT)/inline-asm/invalid.log
	@if $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm/invalid-x86.ro \
		tests/invalid_inline_asm_instruction.c \
		>$(TEST_OUT)/inline-asm/invalid-x86.log 2>&1; then \
		echo "unsupported i686 inline asm unexpectedly compiled"; exit 1; \
	fi
	grep -q "unsupported i686 inline asm instruction" \
		$(TEST_OUT)/inline-asm/invalid-x86.log
	@echo "Dual-architecture fixed-register inline asm tests completed"

test-inline-asm: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/inline-asm-compile)
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm-compile/x86.ro tests/asm_test.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/inline-asm-compile/x64.ro tests/asm_test.c
	@echo "Dual-architecture inline asm instruction tests completed"

ifeq ($(OS),Windows_NT)
test-varargs: $(RCC_TARGET)
	$(call MKDIR_P,$(TEST_OUT)/varargs)
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/varargs/x86.ro \
		tests/varargs.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/varargs/x64.ro \
		tests/varargs.c
	wsl -d Ubuntu-24.04 bash -lc "gcc -m32 $(CFLAGS) -I$(WSL_RINCOMPILER_ROOT)/include -o $(WSL_RINCOMPILER_ROOT)/build/tests/varargs/run-test-x86 $(WSL_RINCOMPILER_ROOT)/tests/varargs_run_test.c $(WSL_RINCOMPILER_ROOT)/src/emit_ro.c $(WSL_RINCOMPILER_ROOT)/src/utils.c; gcc $(CFLAGS) -I$(WSL_RINCOMPILER_ROOT)/include -o $(WSL_RINCOMPILER_ROOT)/build/tests/varargs/run-test-x64 $(WSL_RINCOMPILER_ROOT)/tests/varargs_run_test.c $(WSL_RINCOMPILER_ROOT)/src/emit_ro.c $(WSL_RINCOMPILER_ROOT)/src/utils.c; $(WSL_RINCOMPILER_ROOT)/build/tests/varargs/run-test-x86 $(WSL_RINCOMPILER_ROOT)/build/tests/varargs/x86.ro; $(WSL_RINCOMPILER_ROOT)/build/tests/varargs/run-test-x64 $(WSL_RINCOMPILER_ROOT)/build/tests/varargs/x64.ro"
	powershell -NoProfile -Command "& './rcc.exe' --target x86_64-unknown-rinos -nostdinc -Ibootstrap/include -c -o '$(TEST_OUT)/varargs/invalid.ro' tests/invalid_varargs.c *> '$(TEST_OUT)/varargs/invalid.log'; if ($$LASTEXITCODE -eq 0) { exit 1 } else { exit 0 }"
	powershell -NoProfile -Command "if (-not (Select-String -SimpleMatch -Quiet 'va_start is only valid in a variadic function' '$(TEST_OUT)/varargs/invalid.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -SimpleMatch -Quiet 'va_start requires the final named parameter' '$(TEST_OUT)/varargs/invalid.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -SimpleMatch -Quiet 'va_copy requires two va_list objects' '$(TEST_OUT)/varargs/invalid.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -SimpleMatch -Quiet 'va_end requires a va_list object' '$(TEST_OUT)/varargs/invalid.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -SimpleMatch -Quiet 'va_arg requires a va_list object' '$(TEST_OUT)/varargs/invalid.log')) { exit 1 }"
	powershell -NoProfile -Command "if (-not (Select-String -SimpleMatch -Quiet 'va_arg requires a complete fixed scalar or aggregate object type' '$(TEST_OUT)/varargs/invalid.log')) { exit 1 }"
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
	grep -q "va_start is only valid in a variadic function" \
		$(TEST_OUT)/varargs/invalid.log
	grep -q "va_start requires the final named parameter" \
		$(TEST_OUT)/varargs/invalid.log
	grep -q "va_copy requires two va_list objects" \
		$(TEST_OUT)/varargs/invalid.log
	grep -q "va_end requires a va_list object" \
		$(TEST_OUT)/varargs/invalid.log
	grep -q "va_arg requires a va_list object" \
		$(TEST_OUT)/varargs/invalid.log
	grep -q "va_arg requires a complete fixed scalar or aggregate object type" \
		$(TEST_OUT)/varargs/invalid.log
	@echo "Dual-architecture C17 scalar varargs tests completed"
endif

test-scalar-comparisons: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/scalar-comparisons
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/scalar-comparisons/x86.ro tests/scalar_comparison.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/scalar-comparisons/x64.ro tests/scalar_comparison.c
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
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/scalar-comparisons/invalid.ro \
		tests/invalid_scalar_comparison.c \
		>$(TEST_OUT)/scalar-comparisons/invalid.log 2>&1; then \
		echo "invalid scalar comparisons unexpectedly compiled"; exit 1; \
	fi
	grep -q "comparison requires arithmetic or pointer operands" \
		$(TEST_OUT)/scalar-comparisons/invalid.log
	grep -q "logical operator requires scalar operands" \
		$(TEST_OUT)/scalar-comparisons/invalid.log
	@echo "Dual-architecture C17 scalar comparison tests completed"

test-aggregate-copy: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/aggregate-copy
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-copy/x86.ro tests/aggregate_copy.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-copy/x64.ro tests/aggregate_copy.c
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/aggregate-copy/run-test-x86 \
		tests/aggregate_copy_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/aggregate-copy/run-test-x64 \
		tests/aggregate_copy_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/aggregate-copy/run-test-x86 $(TEST_OUT)/aggregate-copy/x86.ro
	$(TEST_OUT)/aggregate-copy/run-test-x64 $(TEST_OUT)/aggregate-copy/x64.ro
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-copy/invalid.ro \
		tests/invalid_anonymous_aggregate.c \
		>$(TEST_OUT)/aggregate-copy/invalid.log 2>&1; then \
		echo "duplicate anonymous member unexpectedly compiled"; exit 1; \
	fi
	grep -q "duplicate member 'duplicate' from anonymous aggregate" \
		$(TEST_OUT)/aggregate-copy/invalid.log
	@echo "Dual-architecture C17 aggregate copy tests completed"

test-aggregate-returns: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/aggregate-returns
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-returns/x86.ro tests/aggregate_return.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-returns/x64.ro tests/aggregate_return.c
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
	@echo "Dual-architecture C17 aggregate return ABI tests completed"

test-aggregate-packed-abi: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/aggregate-packed-abi
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-packed-abi/x86.ro \
		tests/aggregate_packed_abi.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/aggregate-packed-abi/x64.ro \
		tests/aggregate_packed_abi.c
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
	@echo "SysV packed aggregate ABI tests completed"

test-compound-literals: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/compound-literals
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/compound-literals/x86.ro tests/compound_literal.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/compound-literals/x64.ro tests/compound_literal.c
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
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/compound-literals/invalid.ro \
		tests/invalid_compound_literal.c \
		>$(TEST_OUT)/compound-literals/invalid.log 2>&1; then \
		echo "incomplete compound literal unexpectedly compiled"; exit 1; \
	fi
	grep -q "compound literal requires a complete object type" \
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
	powershell -NoProfile -Command "& wsl.exe -d Ubuntu-24.04 bash -lc 'set -e; gcc -m32 -c -o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x86.o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x86.s; gcc -m32 -c -o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x86-start.o $(WSL_RINCOMPILER_ROOT)/tests/vla_runtime_i686_start.s; gcc -m32 -nostdlib -static -no-pie -Wl,--entry=_start -o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x86 $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x86-start.o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x86.o; $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x86; gcc -c -o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x64.o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x64.s; gcc -c -o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x64-start.o $(WSL_RINCOMPILER_ROOT)/tests/vla_runtime_x64_start.s; gcc -nostdlib -static -no-pie -Wl,--entry=_start -o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x64 $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x64-start.o $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x64.o; $(WSL_RINCOMPILER_ROOT)/$(TEST_OUT)/static-locals/x64'"
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
	mkdir -p $(BOOTSTRAP_ROOT)/stage1-a $(BOOTSTRAP_ROOT)/stage1-b
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
	@echo "Reproducible dual-architecture stage0 core object bootstrap completed"

test-bootstrap-link: test-bootstrap-core $(RLD_TARGET)
	mkdir -p $(BOOTSTRAP_ROOT)/images
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
	@echo "Reproducible dual-architecture linked stage1 rcc images completed"

test-bootstrap-execute: test-bootstrap-link
	mkdir -p $(BOOTSTRAP_ROOT)/execute
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

test-bootstrap-stage2: test-bootstrap-execute
	mkdir -p $(BOOTSTRAP_ROOT)/stage2
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
	@echo "Reproducible dual-architecture stage1-to-stage2 compiler rebuild completed"

test-pragma-pack: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/pragma-pack
	$(RCC_TARGET) --target i686-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/pragma-pack/x86.ro \
		tests/pragma_pack.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -nostdinc \
		-Ibootstrap/include -c -o $(TEST_OUT)/pragma-pack/x64.ro \
		tests/pragma_pack.c
	@echo "Dual-architecture pragma-pack and offsetof tests completed"

test-bitfields: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/bitfields
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
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/invalid.ro tests/invalid_bitfields.c \
		>$(TEST_OUT)/bitfields/invalid.log 2>&1; then \
		echo "invalid bit-field fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "bit-field width" $(TEST_OUT)/bitfields/invalid.log
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/invalid-address.ro \
		tests/invalid_bitfield_address.c \
		>$(TEST_OUT)/bitfields/invalid-address.log 2>&1; then \
		echo "invalid bit-field address fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "cannot take address of a bit-field" \
		$(TEST_OUT)/bitfields/invalid-address.log
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/invalid-offsetof.ro \
		tests/invalid_bitfield_offsetof.c \
		>$(TEST_OUT)/bitfields/invalid-offsetof.log 2>&1; then \
		echo "invalid bit-field offsetof fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "cannot compute offsetof for a bit-field" \
		$(TEST_OUT)/bitfields/invalid-offsetof.log
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/tls-x86.ro tests/bitfields_tls.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/bitfields/tls-x64.ro tests/bitfields_tls.c
	@echo "Dual-architecture C17 bit-field tests completed"

test-cxx-bitfields: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/cxx-bitfields
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
	grep -q "C++ bit-field width" $(TEST_OUT)/cxx-bitfields/invalid.log
	@echo "Dual-architecture C++ bit-field tests completed"

test-compound-assignment: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/compound-assignment
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/compound-assignment/x86.ro \
		tests/compound_assignment.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/compound-assignment/x64.ro \
		tests/compound_assignment.c
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
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/compound-assignment/invalid.ro \
		tests/invalid_compound_assignment.c
	@echo "Dual-architecture C17 compound assignment tests completed"

test-switch-statement: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/switch-statement
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/switch-statement/x86.ro tests/switch_statement.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/switch-statement/x64.ro tests/switch_statement.c
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
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/switch-statement/invalid.ro \
		tests/invalid_switch_statement.c \
		>$(TEST_OUT)/switch-statement/invalid.log 2>&1; then \
		echo "invalid switch fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "case label is not within a switch" \
		$(TEST_OUT)/switch-statement/invalid.log
	grep -q "default label is not within a switch" \
		$(TEST_OUT)/switch-statement/invalid.log
	grep -q "duplicate case value" $(TEST_OUT)/switch-statement/invalid.log
	grep -q "multiple default labels" $(TEST_OUT)/switch-statement/invalid.log
	grep -q "case label must be an integer constant expression" \
		$(TEST_OUT)/switch-statement/invalid.log
	grep -q "switch controlling expression must have integer type" \
		$(TEST_OUT)/switch-statement/invalid.log
	@echo "Dual-architecture C17 switch statement tests completed"

test-control-flow: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/control-flow
	$(RCC_TARGET) --target i686-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/control-flow/x86.ro tests/control_flow.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/control-flow/x64.ro tests/control_flow.c
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/control-flow/run-test-x86 \
		tests/control_flow_run_test.c src/emit_ro.c src/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/control-flow/run-test-x64 \
		tests/control_flow_run_test.c src/emit_ro.c src/utils.c
	$(TEST_OUT)/control-flow/run-test-x86 $(TEST_OUT)/control-flow/x86.ro
	$(TEST_OUT)/control-flow/run-test-x64 $(TEST_OUT)/control-flow/x64.ro
	@if $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/control-flow/invalid.ro \
		tests/invalid_control_flow.c \
		>$(TEST_OUT)/control-flow/invalid.log 2>&1; then \
		echo "invalid control-flow fixture unexpectedly compiled"; exit 1; \
	fi
	grep -q "break statement is not within a loop or switch" \
		$(TEST_OUT)/control-flow/invalid.log
	grep -q "continue statement is not within a loop" \
		$(TEST_OUT)/control-flow/invalid.log
	grep -q "undefined label 'missing'" \
		$(TEST_OUT)/control-flow/invalid.log
	grep -q "redefinition of label 'duplicate'" \
		$(TEST_OUT)/control-flow/invalid.log
	@echo "Dual-architecture C17 goto/label tests completed"

test-parser-recovery: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/parser-recovery
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
	grep -q "expected parameter type specifier" \
		$(TEST_OUT)/parser-recovery/invalid.log
	grep -q "expected field type specifier" \
		$(TEST_OUT)/parser-recovery/invalid.log
	grep -q "expected expression" $(TEST_OUT)/parser-recovery/invalid.log
	@echo "C17 parser progress and null-type recovery tests completed"

test-link: $(RCC_TARGET) $(RLD_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) -c -o $(TEST_OUT)/main.ro tests/main.c
	$(RCC_TARGET) -c -o $(TEST_OUT)/lib.ro tests/lib.c
	$(RLD_TARGET) -v --emit-unsigned-v3 -o $(TEST_OUT)/linked.rin \
		$(TEST_OUT)/main.ro $(TEST_OUT)/lib.ro
	@echo "RLD link test completed"

test-executable-imports: $(RCC_TARGET) $(RLD_TARGET)
	mkdir -p $(TEST_OUT)/executable-imports
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
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) -c -o $(TEST_OUT)/lib.ro tests/lib.c
	$(RAR_TARGET) r $(TEST_OUT)/libtest.ra $(TEST_OUT)/lib.ro
	$(RAR_TARGET) t $(TEST_OUT)/libtest.ra
	@echo "RAR archive test completed"

test-archive-link: $(RCC_TARGET) $(RLD_TARGET) $(RAR_TARGET)
	mkdir -p $(TEST_OUT)/archive-x86 $(TEST_OUT)/archive-x64
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
	cp $(TEST_OUT)/archive-x86/libselect.ra $(TEST_OUT)/archive-x86/corrupt.ra
	$(TEST_OUT)/archive_corrupt_test $(TEST_OUT)/archive-x86/corrupt.ra
	! $(RLD_TARGET) -m32 --emit-unsigned-v3 -o $(TEST_OUT)/archive-x86/corrupt.rin \
		$(TEST_OUT)/archive-x86/main.ro $(TEST_OUT)/archive-x86/corrupt.ra
	! $(RLD_TARGET) -m32 -e archive_order_root --emit-unsigned-v3 \
		-o $(TEST_OUT)/archive-x86/wrong-order.rin \
		$(TEST_OUT)/archive-x86/libselect.ra $(TEST_OUT)/archive-x86/main.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/archive-x64/main.ro tests/archive_link_main.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/archive-x64/helper.ro tests/archive_link_helper.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/archive-x64/unused.ro tests/archive_link_unused.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c -o $(TEST_OUT)/archive-x64/chosen.ro tests/archive_link_chosen.c
	$(RAR_TARGET) r $(TEST_OUT)/archive-x64/libselect.ra \
		$(TEST_OUT)/archive-x64/helper.ro $(TEST_OUT)/archive-x64/unused.ro \
		$(TEST_OUT)/archive-x64/chosen.ro
	$(RLD_TARGET) -m64 -v --emit-unsigned-v3 -o $(TEST_OUT)/archive-x64/selected.rin \
		$(TEST_OUT)/archive-x64/main.ro $(TEST_OUT)/archive-x64/libselect.ra
	! $(RLD_TARGET) -m64 --emit-unsigned-v3 -o $(TEST_OUT)/archive-x64/wrong-arch.rin \
		$(TEST_OUT)/archive-x64/main.ro $(TEST_OUT)/archive-x86/libselect.ra
	@echo "RLD unresolved-symbol archive selection tests completed"

test-static-assert: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) -c -o $(TEST_OUT)/static_assert_pass.ro tests/static_assert_pass.c
	! $(RCC_TARGET) -c -o $(TEST_OUT)/static_assert_fail.ro tests/static_assert_fail.c
	@echo "C17 static assertion test completed"

test-manifest: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET)
	mkdir -p $(TEST_OUT)
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
	! $(RCC_TARGET) --manifest tests/build_manifest_compiler.rbm \
		--emit-unsigned-v3 -c tests/hello.c
	! $(RCXX_TARGET) --manifest tests/build_manifest_compiler.rbm \
		--emit-unsigned-v3 -c tests/hello.cpp
	! $(RLD_TARGET) --manifest tests/build_manifest_valid.rbm -m32 \
		--emit-unsigned-v3 tests/missing.ro
	! $(RLD_TARGET) --manifest tests/build_manifest_executable.rbm -shared \
		--emit-unsigned-v3 tests/missing.ro
	! $(RCC_TARGET) --manifest tests/build_manifest_compiler.rbm \
		--sign-profile release --emit-unsigned-v3 tests/hello.c
	@echo "Versioned build manifest conflict tests completed"

test-signing: $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET)
	mkdir -p "$(SIGN_TEST_DIR)/argv ; spaces"
	cp tests/fake_rinsign.py "$(SIGN_TEST_DIR)/argv ; spaces/fake signer.py"
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
	! $(RCC_TARGET) --target i686-unknown-rinos \
		--python python3 --rinsign tests/fake_rinsign.py \
		--sign-key tests/signing_test_private.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/missing profile.rin" tests/hello.c
	cp "$(SIGN_TEST_DIR)/direct x86.rin" "$(SIGN_TEST_DIR)/preserved.rin"
	! $(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug \
		--python python3 --rinsign tests/fake_rinsign.py \
		--sign-key tests/signing_test_fail.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/preserved.rin" tests/hello.c
	cmp "$(SIGN_TEST_DIR)/direct x86.rin" "$(SIGN_TEST_DIR)/preserved.rin"
	! $(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug \
		--python python3 --rinsign tests/fake_rinsign.py \
		--sign-key tests/signing_test_invalid.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/invalid signer.rin" tests/hello.c
	test ! -e "$(SIGN_TEST_DIR)/invalid signer.rin"
	$(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug \
		--python python3 --rinsign tests/fake_rinsign.py \
		--sign-key tests/signing_test_private.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/parallel.rin" tests/hello.c & first=$$!; \
	$(RCC_TARGET) --target i686-unknown-rinos --sign-profile debug \
		--python python3 --rinsign tests/fake_rinsign.py \
		--sign-key tests/signing_test_private.key \
		--public-key tests/signing_test_public.der \
		-o "$(SIGN_TEST_DIR)/parallel.rin" tests/hello.c & second=$$!; \
	wait $$first; wait $$second
	test -z "$$(find "$(SIGN_TEST_DIR)" -type f \
		\( -name '*.rcc-unsigned-*' -o -name '*.rld-unsigned-*' \
		-o -name '*.rcc-signed-*' \) -print -quit)"
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
	! $(RCC_TARGET) --target x86_64-unknown-rinos -fPIC \
		--emit-unsigned-v3 -o $(TEST_OUT)/pic-plt/forbidden.rin \
		tests/pic_external.c >$(TEST_OUT)/pic-plt/forbidden.log 2>&1
	grep -q "direct RIN v3 output cannot contain unresolved relative relocation" \
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
	! grep -q "too many errors" $(SANITIZER_ROOT)/tests/cxx-invalid.log
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
	grep -q "expected parameter type specifier" \
		$(SANITIZER_ROOT)/tests/parser-recovery.log
	grep -q "expected field type specifier" \
		$(SANITIZER_ROOT)/tests/parser-recovery.log
	! $(SANITIZER_ROOT)/bin/rcc --target x86_64-unknown-rinos -c \
		-o $(SANITIZER_ROOT)/tests/invalid.ro \
		tests/invalid_designated_initializer.c
	! $(SANITIZER_ROOT)/bin/rcc++ -driver --emit-unsigned-v3 \
		-o $(SANITIZER_ROOT)/tests/invalid.drv tests/driver_policy_float.cpp
	@echo "ASan/UBSan and translation-unit lifetime tests completed"

test-driver-policy: $(RCC_TARGET) $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) --target i686-unknown-rinos -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_x86.drv tests/driver_policy_ok.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_x64.drv tests/driver_policy_ok.c
	! $(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_float.drv tests/driver_policy_float.c
	! $(RCXX_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_float_cxx.drv tests/driver_policy_float.cpp
	! $(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_asm.drv tests/driver_policy_asm.c
	! $(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_constraint.drv tests/driver_policy_constraint.c
	! $(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_clobber.drv tests/driver_policy_clobber.c
	! $(RCC_TARGET) -driver --emit-unsigned-v3 \
		-o $(TEST_OUT)/driver_policy_mask_constraint.drv tests/driver_policy_mask_constraint.c
	@echo "NDRV FPU/SIMD policy tests completed"

test-weak-link:
	mkdir -p $(TEST_OUT)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/weak_link_test \
		tests/weak_link_test.c $(SRCDIR)/linker.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/archive.c $(SRCDIR)/utils.c
	$(TEST_OUT)/weak_link_test $(TEST_OUT)/weak.ro $(TEST_OUT)/strong.ro
	@echo "Weak-to-strong linker replacement test completed"

test-comdat-link:
	mkdir -p $(TEST_OUT)/comdat
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
	mkdir -p $(TEST_OUT)
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
	! $(RLD_TARGET) -T invalid-address --emit-unsigned-v3 \
		-o $(TEST_OUT)/invalid_base.rin $(TEST_OUT)/wide_main.ro
	@echo "64-bit object/linker width and typed relocation tests completed"

test-special-sections:
	mkdir -p $(TEST_OUT)/special
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
	mkdir -p $(TEST_OUT)/tls
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
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/tls/invalid-local.ro tests/tls_invalid_local.c
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
	! $(RCC_TARGET) --target x86_64-unknown-rinos -driver \
		--emit-unsigned-v3 -o $(TEST_OUT)/tls/invalid.drv tests/tls.c
	@echo "C17/C++20 local-exec TLS tests completed"

test-direct-relocation: $(RCC_TARGET) $(RLD_TARGET)
	mkdir -p $(TEST_OUT)/direct
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
	! $(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/unresolved.rin tests/direct_unresolved.c
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/definition-x86.ro \
		tests/direct_unresolved_definition.c
	$(RLD_TARGET) -m32 --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/resolved-x86.rin \
		$(TEST_OUT)/direct/unresolved.ro \
		$(TEST_OUT)/direct/definition-x86.ro
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unresolved-x64.ro tests/direct_unresolved.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/unresolved-x64.rin tests/direct_unresolved.c
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
	! $(RCC_TARGET) --target i686-unknown-rinos --emit-unsigned-v3 \
		-o $(TEST_OUT)/direct/bss-unresolved-x86.rin \
		tests/direct_bss_unresolved.c
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
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/global-redefinition.ro \
		tests/global_redefinition.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/global-conflict.ro tests/global_conflict.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unsupported-static-x86.ro \
		tests/unsupported_static_pointer.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unsupported-static-x64.ro \
		tests/unsupported_static_pointer.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-array-x86.ro \
		tests/invalid_array_initializer.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-array-x64.ro \
		tests/invalid_array_initializer.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-designator-x86.ro \
		tests/invalid_designated_initializer.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-designator-x64.ro \
		tests/invalid_designated_initializer.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/nonconstant-designator-x86.ro \
		tests/nonconstant_designator.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/nonconstant-designator-x64.ro \
		tests/nonconstant_designator.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/empty-initializer-x86.ro \
		tests/empty_initializer.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/empty-initializer-x64.ro \
		tests/empty_initializer.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unsupported-local-array-x86.ro \
		tests/unsupported_local_array_initializer.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/unsupported-local-array-x64.ro \
		tests/unsupported_local_array_initializer.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-constant-x86.ro \
		tests/invalid_static_constant.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-constant-x64.ro \
		tests/invalid_static_constant.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-pointer-x86.ro \
		tests/invalid_pointer_arithmetic.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/direct/invalid-pointer-x64.ro \
		tests/invalid_pointer_arithmetic.c
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
	mkdir -p $(TEST_OUT)
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/ir_test-x86 \
		tests/ir_test.c $(SRCDIR)/ir.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/ir_test-x64 \
		tests/ir_test.c $(SRCDIR)/ir.c $(SRCDIR)/utils.c
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/ir_mem2reg_test-x86 \
		tests/ir_mem2reg_test.c $(SRCDIR)/ir.c $(SRCDIR)/ir_pass.c \
		$(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/ir_mem2reg_test-x64 \
		tests/ir_mem2reg_test.c $(SRCDIR)/ir.c $(SRCDIR)/ir_pass.c \
		$(SRCDIR)/utils.c
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/mir_test-x86 \
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
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/x86_encode_run_test-x86 \
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
	$(TEST_OUT)/x86_encode_run_test-x64 \
		$(TEST_OUT)/encoded-native-x64.ro

test-ir-lowering: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/ir-lowering
	$(RCC_TARGET) --target i686-unknown-rinos -O1 -v -c \
		-o $(TEST_OUT)/ir-lowering/x86.ro tests/ir_lowering.c \
		>$(TEST_OUT)/ir-lowering/x86.log
	grep -q 'Typed SSA shadow verification: 4 function(s)' \
		$(TEST_OUT)/ir-lowering/x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -O3 -v -c \
		-o $(TEST_OUT)/ir-lowering/x64.ro tests/ir_lowering.c \
		>$(TEST_OUT)/ir-lowering/x64.log
	grep -q 'Typed SSA shadow verification: 4 function(s)' \
		$(TEST_OUT)/ir-lowering/x64.log
	@echo "Dual-architecture scalar AST to typed SSA lowering tests completed"

test-verified-backend: $(RCC_TARGET) $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/verified-backend
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/x86.ro tests/verified_backend.c \
		>$(TEST_OUT)/verified-backend/x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/x64.ro tests/verified_backend.c \
		>$(TEST_OUT)/verified-backend/x64.log
	grep -q 'Verified backend: 38 function(s) emitted' \
		$(TEST_OUT)/verified-backend/x86.log
	grep -q 'Verified backend: 38 function(s) emitted' \
		$(TEST_OUT)/verified-backend/x64.log
	$(RCXX_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/cxx-x64.ro \
		tests/verified_backend.cpp \
		>$(TEST_OUT)/verified-backend/cxx-x64.log
	grep -q 'Verified backend: 1 function(s) emitted' \
		$(TEST_OUT)/verified-backend/cxx-x64.log
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/globals-x86.ro \
		tests/verified_backend_globals.c \
		>$(TEST_OUT)/verified-backend/globals-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/globals-x64.ro \
		tests/verified_backend_globals.c \
		>$(TEST_OUT)/verified-backend/globals-x64.log
	grep -q 'Verified backend: 8 function(s) emitted' \
		$(TEST_OUT)/verified-backend/globals-x86.log
	grep -q 'Verified backend: 8 function(s) emitted' \
		$(TEST_OUT)/verified-backend/globals-x64.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/fallback.ro \
		tests/verified_backend_fallback.c \
		>$(TEST_OUT)/verified-backend/fallback.log
	grep -q 'Verified backend fallback: translation unit contains thread-local data' \
		$(TEST_OUT)/verified-backend/fallback.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/switch-fallback.ro \
		tests/verified_backend_switch_fallback.c \
		>$(TEST_OUT)/verified-backend/switch-fallback.log
	grep -q "Verified backend fallback: function 'verified_switch_nested_label_fallback' is outside the typed SSA subset" \
		$(TEST_OUT)/verified-backend/switch-fallback.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/array-fallback.ro \
		tests/verified_backend_array_fallback.c \
		>$(TEST_OUT)/verified-backend/array-fallback.log
	grep -q "Verified backend fallback: function 'verified_aggregate_return_fallback' is outside the typed SSA subset" \
		$(TEST_OUT)/verified-backend/array-fallback.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/aggregate-straddle-fallback.ro \
		tests/verified_backend_aggregate_straddle_fallback.c \
		>$(TEST_OUT)/verified-backend/aggregate-straddle-fallback.log
	grep -q "Verified backend fallback: function 'verified_aggregate_register_straddle_fallback' is outside the typed SSA subset" \
		$(TEST_OUT)/verified-backend/aggregate-straddle-fallback.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/packed-argument-fallback.ro \
		tests/verified_backend_packed_argument_fallback.c \
		>$(TEST_OUT)/verified-backend/packed-argument-fallback.log
	grep -q "Verified backend fallback: function 'verified_packed_argument_fallback' is outside the typed SSA subset" \
		$(TEST_OUT)/verified-backend/packed-argument-fallback.log
	$(RCC_TARGET) --target i686-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-scalar-x86.ro \
		tests/verified_backend_wide_scalar_fallback.c \
		>$(TEST_OUT)/verified-backend/wide-scalar-x86.log
	grep -q "Verified backend fallback: function 'verified_wide_scalar_fallback' is outside the typed SSA subset" \
		$(TEST_OUT)/verified-backend/wide-scalar-x86.log
	$(RCC_TARGET) --target x86_64-unknown-rinos -fverified-backend -v -c \
		-o $(TEST_OUT)/verified-backend/wide-scalar-x64.ro \
		tests/verified_backend_wide_scalar_fallback.c \
		>$(TEST_OUT)/verified-backend/wide-scalar-x64.log
	grep -q 'Verified backend: 1 function(s) emitted' \
		$(TEST_OUT)/verified-backend/wide-scalar-x64.log
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/verify-x86 \
		tests/verified_backend_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/verified-backend/verify-x64 \
		tests/verified_backend_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/verified-backend/verify-x86 \
		$(TEST_OUT)/verified-backend/x86.ro \
		$(TEST_OUT)/verified-backend/x64.ro \
		$(TEST_OUT)/verified-backend/cxx-x64.ro \
		$(TEST_OUT)/verified-backend/globals-x86.ro \
		$(TEST_OUT)/verified-backend/globals-x64.ro
	$(TEST_OUT)/verified-backend/verify-x64 \
		$(TEST_OUT)/verified-backend/x86.ro \
		$(TEST_OUT)/verified-backend/x64.ro \
		$(TEST_OUT)/verified-backend/cxx-x64.ro \
		$(TEST_OUT)/verified-backend/globals-x86.ro \
		$(TEST_OUT)/verified-backend/globals-x64.ro
	@echo "Verified backend production object and fallback tests completed"

test-optimize: $(RCC_TARGET) $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)/optimize
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
	cmp $(TEST_OUT)/optimize/x64-o1.ro $(TEST_OUT)/optimize/x64-o3.ro
	$(RCXX_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-o1.ro tests/hello.cpp
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-cleanup-x86.ro \
		tests/cxx_inline_aggregate.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -O1 -c \
		-o $(TEST_OUT)/optimize/cxx-cleanup-x64.ro \
		tests/cxx_inline_aggregate.cpp
	! $(RCC_TARGET) --target x86_64-unknown-rinos -O1 -driver \
		--emit-unsigned-v3 -o $(TEST_OUT)/optimize/forbidden.drv \
		tests/driver_policy_float.c
	@set +e; $(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/invalid-qualifiers.ro \
		tests/invalid_qualifiers.c \
		>$(TEST_OUT)/optimize/invalid-qualifiers.log 2>&1; status=$$?; \
		set -e; if [ $$status -eq 0 ]; then \
		echo "const-qualified writes unexpectedly compiled"; exit 1; fi
	test "$$(grep -c 'requires modifiable lvalue' \
		$(TEST_OUT)/optimize/invalid-qualifiers.log)" -eq 4
	$(RCC_TARGET) --target x86_64-unknown-rinos -O1 -c \
		-o $(TEST_OUT)/optimize/qualifier-conversions.ro \
		tests/qualifier_conversions.c \
		>$(TEST_OUT)/optimize/qualifier-conversions.log 2>&1
	test "$$(grep -c 'incompatible return type' \
		$(TEST_OUT)/optimize/qualifier-conversions.log)" -eq 2
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
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
	$(CC) -m32 $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/optimize/cxx-cleanup-run-x86 \
		tests/cxx_value_init_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) \
		-o $(TEST_OUT)/optimize/cxx-cleanup-run-x64 \
		tests/cxx_value_init_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/optimize/cxx-cleanup-run-x86 \
		$(TEST_OUT)/optimize/cxx-cleanup-x86.ro
	$(TEST_OUT)/optimize/cxx-cleanup-run-x64 \
		$(TEST_OUT)/optimize/cxx-cleanup-x64.ro
	@echo "Dual-architecture AST integer folding and dead-code tests completed"

test-generic: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/generic
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/generic/x86.ro tests/generic_selection.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/generic/x64.ro tests/generic_selection.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/generic/invalid.ro tests/invalid_generic.c
	! $(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/generic/invalid-match.ro \
		tests/invalid_generic_match.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/generic_selection_run_test \
		tests/generic_selection_run_test.c $(SRCDIR)/emit_ro.c \
		$(SRCDIR)/utils.c
	$(TEST_OUT)/generic_selection_run_test \
		$(TEST_OUT)/generic/x86.ro $(TEST_OUT)/generic/x64.ro
	@echo "C17 generic selection tests completed"

test-initializer-overrides: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/initializer-overrides
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
	mkdir -p $(TEST_OUT)/alignof
	$(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/alignof/x86.ro tests/alignof.c
	$(RCC_TARGET) --target x86_64-unknown-rinos -c \
		-o $(TEST_OUT)/alignof/x64.ro tests/alignof.c
	! $(RCC_TARGET) --target i686-unknown-rinos -c \
		-o $(TEST_OUT)/alignof/invalid.ro tests/invalid_alignof.c
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/alignof_run_test \
		tests/alignof_run_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/alignof_run_test \
		$(TEST_OUT)/alignof/x86.ro $(TEST_OUT)/alignof/x64.ro
	@echo "C17 _Alignof tests completed"

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
