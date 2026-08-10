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

# Common source files (shared between rcc and rcc++)
COMMON_SRCS = $(SRCDIR)/utils.c $(SRCDIR)/lexer.c $(SRCDIR)/parser.c $(SRCDIR)/ast.c \
              $(SRCDIR)/symtab.c $(SRCDIR)/sema.c $(SRCDIR)/codegen.c \
              $(SRCDIR)/codegen64.c $(SRCDIR)/preproc.c \
              $(SRCDIR)/emit_rin.c $(SRCDIR)/emit_rll.c $(SRCDIR)/emit_drv.c $(SRCDIR)/emit_ro.c \
              $(SRCDIR)/emit_asm.c $(SRCDIR)/build_manifest.c $(SRCDIR)/driver_policy.c
COMMON_OBJS = $(COMMON_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# RCC (C compiler)
RCC_SRCS = $(SRCDIR)/main.c
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

.PHONY: all clean test build-rcc build-rcxx build-rld build-rar test-cxx test-cxx-cli test-link test-archive test-archive-link test-static-assert test-manifest test-driver-policy test-weak-link test-object-width test-special-sections test-direct-relocation

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

test-link: $(RCC_TARGET) $(RLD_TARGET)
	mkdir -p $(TEST_OUT)
	$(RCC_TARGET) -c -o $(TEST_OUT)/main.ro tests/main.c
	$(RCC_TARGET) -c -o $(TEST_OUT)/lib.ro tests/lib.c
	$(RLD_TARGET) -v --emit-unsigned-v3 -o $(TEST_OUT)/linked.rin \
		$(TEST_OUT)/main.ro $(TEST_OUT)/lib.ro
	@echo "RLD link test completed"

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
		tests/build_manifest_test.c $(SRCDIR)/build_manifest.c
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
	@echo "Versioned build manifest conflict tests completed"

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
	$(CC) $(CFLAGS) -I$(INCDIR) -o $(TEST_OUT)/direct_relocation_test \
		tests/direct_relocation_test.c $(SRCDIR)/emit_ro.c $(SRCDIR)/utils.c
	$(TEST_OUT)/direct_relocation_test \
		$(TEST_OUT)/direct/x86.ro $(TEST_OUT)/direct/x86.rin \
		$(TEST_OUT)/direct/x64.ro $(TEST_OUT)/direct/x64.rin \
		$(TEST_OUT)/direct/x86.ro $(TEST_OUT)/direct/x86.drv \
		$(TEST_OUT)/direct/x64.ro $(TEST_OUT)/direct/x64.drv \
		$(TEST_OUT)/direct/x86.ro $(TEST_OUT)/direct/x86-linked.rin \
		$(TEST_OUT)/direct/x64.ro $(TEST_OUT)/direct/x64-linked.rin
	@echo "Direct RIN/NDRV v3 symbol relocation tests completed"

# Dependencies
$(OBJDIR)/main.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h $(INCDIR)/driver_policy.h $(INCDIR)/preproc.h
$(OBJDIR)/main_cxx.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/ast_cxx.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h $(INCDIR)/driver_policy.h $(INCDIR)/preproc.h
$(OBJDIR)/lexer.o: $(INCDIR)/rcc.h $(INCDIR)/token.h
$(OBJDIR)/parser.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h
$(OBJDIR)/ast.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h
$(OBJDIR)/ast_cxx.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/ast_cxx.h
$(OBJDIR)/parser_cxx.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/ast_cxx.h
$(OBJDIR)/symtab.o: $(INCDIR)/rcc.h $(INCDIR)/symtab.h
$(OBJDIR)/sema.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/symtab.h
$(OBJDIR)/codegen.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h
$(OBJDIR)/codegen64.o: $(INCDIR)/rcc.h $(INCDIR)/ast.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h
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
