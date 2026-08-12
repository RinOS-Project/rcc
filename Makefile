# RCC/RCC++ - RinOS C/C++ Compiler
# Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDFLAGS =

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = .
TEST_OUT = build/tests
SIGN_TEST_DIR = $(TEST_OUT)/signing
SANITIZER_ROOT = build/sanitizers
BOOTSTRAP_ROOT = build/bootstrap
BOOTSTRAP_INCLUDES = -nostdinc -Ibootstrap/include -Iinclude
BOOTSTRAP_CORE_SRCS = src/ast.c src/symtab.c src/lexer.c src/sema.c src/parser.c \
                      src/parser_cxx_stub.c \
                      src/ir.c src/optimize.c src/codegen.c src/codegen64.c \
                      src/preproc.c src/driver_policy.c src/emit_asm.c \
                      src/emit_ro.c src/emit_rin.c src/emit_rll.c \
                      src/emit_drv.c src/archive.c src/linker.c \
                      src/ast_cxx.c src/parser_cxx.c \
                      src/build_manifest.c src/utils.c src/main.c \
                      src/main_cxx.c src/main_rld.c src/main_rar.c
BOOTSTRAP_RCC_OBJECTS = utils lexer parser ast symtab sema codegen codegen64 \
                        preproc ir optimize emit_rin emit_rll emit_drv emit_ro \
                        emit_asm build_manifest driver_policy parser_cxx_stub \
                        main
BOOTSTRAP_RUNTIME_FUNCTIONS = __errno_location __rin_stderr _exit atexit atoi \
                              close execvp exit fclose feof ferror fgets fopen \
                              fork fprintf fputc fputs fread free fseek ftell \
                              fwrite isalnum isalpha isdigit isspace isxdigit \
                              malloc memchr memcpy memset mkstemp perror printf \
                              qsort realloc remove rename snprintf strchr strcmp \
                              strcpy strlen strncat strncmp strncpy strrchr strtod \
                              strtoull tolower vfprintf vsnprintf waitpid
BOOTSTRAP_RUNTIME_IMPORTS = $(foreach symbol,$(BOOTSTRAP_RUNTIME_FUNCTIONS),\
                              --import $(symbol)=rincrt.rll@function)

# Common source files (shared between rcc and rcc++)
COMMON_SRCS = $(SRCDIR)/utils.c $(SRCDIR)/lexer.c $(SRCDIR)/parser.c $(SRCDIR)/ast.c \
              $(SRCDIR)/symtab.c $(SRCDIR)/sema.c $(SRCDIR)/codegen.c \
              $(SRCDIR)/codegen64.c $(SRCDIR)/preproc.c \
              $(SRCDIR)/ir.c $(SRCDIR)/optimize.c \
              $(SRCDIR)/emit_rin.c $(SRCDIR)/emit_rll.c $(SRCDIR)/emit_drv.c $(SRCDIR)/emit_ro.c \
              $(SRCDIR)/emit_asm.c $(SRCDIR)/build_manifest.c $(SRCDIR)/driver_policy.c
COMMON_OBJS = $(COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# RCC (C compiler)
RCC_SRCS = $(SRCDIR)/main.c $(SRCDIR)/parser_cxx_stub.c
RCC_OBJS = $(RCC_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RCC_TARGET = $(BINDIR)/rcc

# RCC++ (C++ compiler)
RCXX_SRCS = $(SRCDIR)/main_cxx.c $(SRCDIR)/ast_cxx.c $(SRCDIR)/parser_cxx.c
RCXX_OBJS = $(RCXX_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RCXX_TARGET = $(BINDIR)/rcc++

# RLD (Linker) - uses minimal common code
RLD_COMMON_SRCS = $(SRCDIR)/utils.c $(SRCDIR)/emit_ro.c $(SRCDIR)/archive.c $(SRCDIR)/build_manifest.c
RLD_COMMON_OBJS = $(RLD_COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RLD_SRCS = $(SRCDIR)/main_rld.c $(SRCDIR)/linker.c
RLD_OBJS = $(RLD_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RLD_TARGET = $(BINDIR)/rld

# RAR (Archiver) - uses minimal common code
RAR_COMMON_SRCS = $(SRCDIR)/utils.c
RAR_COMMON_OBJS = $(RAR_COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RAR_SRCS = $(SRCDIR)/main_rar.c $(SRCDIR)/archive.c
RAR_OBJS = $(RAR_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
RAR_TARGET = $(BINDIR)/rar

.PHONY: all clean test build-rcc build-rcxx build-rld build-rar test-cxx test-cxx-cli test-cxx-language-linkage test-cxx-member-specifiers test-cxx-function-templates test-cxx-qualified-namespaces test-cxx-overloads test-cxx-inline-aggregates test-cxx-parser-recovery test-tool-relative-includes test-preprocessor-continuation test-atomic-builtins test-x86-wide-scalar test-integer-literals test-integer-promotions test-integer-conversions test-function-calls test-inline-asm-execute test-varargs test-scalar-comparisons test-aggregate-copy test-aggregate-returns test-compound-literals test-bootstrap-core test-bootstrap-link test-bootstrap-execute test-bootstrap-stage2 test-executable-imports test-pragma-pack test-compound-assignment test-switch-statement test-control-flow test-parser-recovery test-link test-archive test-archive-link test-static-assert test-manifest test-signing test-sanitize test-driver-policy test-weak-link test-comdat-link test-object-width test-special-sections test-direct-relocation test-ir test-optimize test-generic test-initializer-overrides test-alignof test-tls

all: $(OBJDIR) $(BINDIR) $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET) $(RAR_TARGET)

build-rcc: $(OBJDIR) $(RCC_TARGET)

build-rcxx: $(OBJDIR) $(RCXX_TARGET)

build-rld: $(OBJDIR) $(RLD_TARGET)

build-rar: $(OBJDIR) $(RAR_TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(RCC_TARGET): $(COMMON_OBJS) $(RCC_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(RCXX_TARGET): $(COMMON_OBJS) $(RCXX_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(RLD_TARGET): $(RLD_COMMON_OBJS) $(RLD_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(RAR_TARGET): $(RAR_COMMON_OBJS) $(RAR_OBJS) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(RCC_TARGET) $(RCXX_TARGET) $(RLD_TARGET) $(RAR_TARGET)

# Test
test: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) --emit-unsigned-v3 -o $(TEST_OUT)/hello.rin tests/hello.c
	@echo "RCC test completed"

test-cxx: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCXX_TARGET) --emit-unsigned-v3 -o $(TEST_OUT)/hello_cxx.rin tests/hello.cpp
	@echo "RCC++ test completed"

test-cxx-cli: $(RCXX_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCXX_TARGET) --target x86_64-unknown-rinos -c -MMD \
		-MF $(TEST_OUT)/cxx_cli_options.d -nostdinc -Itests/include \
		-DRCC_CXX_CLI_VALUE=23 -DRCC_CXX_REMOVE_ME -URCC_CXX_REMOVE_ME \
		-o $(TEST_OUT)/cxx_cli_options.ro tests/cxx_cli_options.cpp
	@echo "RCC++ command-line compatibility test completed"

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
	mkdir -p $(TEST_OUT)/cxx-function-templates
	$(RCXX_TARGET) --target i686-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-function-templates/x86.ro \
		tests/cxx_function_templates.cpp
	$(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-function-templates/x64.ro \
		tests/cxx_function_templates.cpp
	@echo "RCC++ function template syntax tests completed"

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
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-destructor.ro \
		tests/cxx_unsafe_destructor_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/unsafe-destructor.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "expected ;, got '{'" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-destructor.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-release.ro \
		tests/cxx_unsafe_release_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/unsafe-release.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "no member named 'release'" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-release.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-bool.ro \
		tests/cxx_unsafe_bool_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/unsafe-bool.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "condition requires scalar type or validated operator bool" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-bool.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-bool-delegate.ro \
		tests/cxx_unsafe_bool_delegate_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/unsafe-bool-delegate.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "condition requires scalar type or validated operator bool" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-bool-delegate.log
	@set +e; $(RCXX_TARGET) --target x86_64-unknown-rinos -std=c++20 -c \
		-o $(TEST_OUT)/cxx-inline-aggregates/unsafe-close-delegate.ro \
		tests/cxx_unsafe_close_delegate_rejected.cpp \
		>$(TEST_OUT)/cxx-inline-aggregates/unsafe-close-delegate.log 2>&1; \
		status=$$?; set -e; test $$status -ne 0
	grep -q "no member named 'reset'" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-close-delegate.log
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
	grep -q "C++ ownership assignment requires a validated rvalue operator=" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-move-assignment.log
	grep -q "C++ scope-cleanup object assignment requires a validated operator=" \
		$(TEST_OUT)/cxx-inline-aggregates/unsafe-move-assignment.log
	grep -q "no member named 'close'" \
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
	mkdir -p $(TEST_OUT)/inline-asm
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
	@echo "Dual-architecture fixed-register inline asm tests completed"

test-varargs: $(RCC_TARGET)
	mkdir -p $(TEST_OUT)/varargs
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
	grep -q "va_arg currently supports integer and pointer scalars up to 64 bits" \
		$(TEST_OUT)/varargs/invalid.log
	@echo "Dual-architecture C17 scalar varargs tests completed"

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

test-sanitize:
	$(MAKE) OBJDIR=$(SANITIZER_ROOT)/obj BINDIR=$(SANITIZER_ROOT)/bin \
		TEST_OUT=$(SANITIZER_ROOT)/tests \
		CFLAGS="$(CFLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined" \
		all test-ir test-manifest test-signing test-executable-imports test-tls
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
	$(TEST_OUT)/ir_test-x86
	$(TEST_OUT)/ir_test-x64

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
$(OBJDIR)/optimize.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/optimize.h
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
