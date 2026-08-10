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
              $(SRCDIR)/emit_asm.c $(SRCDIR)/build_manifest.c
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
RLD_COMMON_SRCS = $(SRCDIR)/utils.c $(SRCDIR)/emit_ro.c $(SRCDIR)/build_manifest.c
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

.PHONY: all clean test build-rcc build-rcxx build-rld build-rar test-cxx test-cxx-cli test-link test-archive test-static-assert test-manifest

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

# Dependencies
$(OBJDIR)/main.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h $(INCDIR)/preproc.h
$(OBJDIR)/main_cxx.o: $(INCDIR)/rcc.h $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/ast_cxx.h $(INCDIR)/symtab.h $(INCDIR)/codegen.h $(INCDIR)/preproc.h
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
$(OBJDIR)/main_rld.o: $(INCDIR)/rcc.h $(INCDIR)/linker.h
$(OBJDIR)/linker.o: $(INCDIR)/rcc.h $(INCDIR)/linker.h $(INCDIR)/objfile.h
$(OBJDIR)/main_rar.o: $(INCDIR)/rcc.h $(INCDIR)/archive.h
$(OBJDIR)/archive.o: $(INCDIR)/rcc.h $(INCDIR)/archive.h $(INCDIR)/objfile.h
