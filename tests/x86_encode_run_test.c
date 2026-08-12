#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "ir.h"
#include "mir.h"
#include "mir_alloc.h"
#include "mir_phi.h"
#include "x86_select.h"
#include "x86_legalize.h"
#include "x86_encode.h"
#include "x86_object.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static RccX86Target host_target(void)
{
    return sizeof(void*) == 8u ? RCC_X86_TARGET_X86_64
                               : RCC_X86_TARGET_I686;
}

static RccIrValue append_const(RccIrBlock* block, RccIrType type,
                               uint64_t value)
{
    RccIrInstruction* instruction = rcc_ir_append(
        block, RCC_IR_CONST_INT, type, NULL, 0u, NULL, 0u);
    assert(instruction != NULL);
    rcc_ir_set_immediate(instruction, value);
    return instruction->result;
}

static RccX86EncodedFunction encode_function(RccIrFunction* ir)
{
    RccMirFunction* mir = NULL;
    RccMirRegisterPolicy policy;
    RccMirAllocation allocation;
    RccMirPhiPlan phi_plan;
    RccX86Function* selected = NULL;
    RccX86LegalFunction* legal = NULL;
    RccX86EncodedFunction encoded;
    RccX86Target target = host_target();
    char error[256];
    memset(&encoded, 0, sizeof(encoded));
    assert(rcc_mir_lower_ir(ir, &mir, error, sizeof(error)));
    if (target == RCC_X86_TARGET_X86_64) {
        rcc_mir_register_policy_x86_64(&policy);
    } else {
        rcc_mir_register_policy_i686(&policy);
    }
    assert(rcc_mir_linear_scan_allocate(
        mir, &policy, &allocation, error, sizeof(error)));
    assert(rcc_mir_build_phi_plan(
        mir, &policy, &allocation, &phi_plan, error, sizeof(error)));
    assert(rcc_x86_select_function(
        mir, target, &policy, &allocation, &phi_plan,
        &selected, error, sizeof(error)));
    assert(rcc_x86_legalize_function(
        selected, &policy, &legal, error, sizeof(error)));
    assert(rcc_x86_encode_function(
        legal, &policy, &encoded, error, sizeof(error)));
    assert(encoded.relocation_count == 0u);
    rcc_x86_legal_function_destroy(legal);
    rcc_x86_function_destroy(selected);
    rcc_mir_phi_plan_release(&phi_plan);
    rcc_mir_allocation_release(&allocation);
    rcc_mir_function_destroy(mir);
    return encoded;
}

static void* map_code(const RccX86EncodedFunction* encoded,
                      size_t* mapping_size)
{
    long page = sysconf(_SC_PAGESIZE);
    size_t size;
    void* memory;
    assert(page > 0);
    size = (encoded->code_size + (size_t)page - 1u) &
        ~((size_t)page - 1u);
    memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(memory != MAP_FAILED);
    memcpy(memory, encoded->code, encoded->code_size);
    assert(mprotect(memory, size, PROT_READ | PROT_EXEC) == 0);
    *mapping_size = size;
    return memory;
}

static void verify_add_execution(const char* object_path)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "encoded_add", i32, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrInstruction* sum = rcc_ir_append(
        entry, RCC_IR_ADD, i32, ir->parameters, 2u, NULL, 0u);
    RccX86EncodedFunction encoded;
    size_t mapping_size;
    void* memory;
    int (*function)(int, int);
    ObjectFile* object;
    ObjectFile* roundtrip;
    ObjSection* text;
    ObjSymbol* symbol;
    char error[256];
    assert(sum != NULL);
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &sum->result, 1u, NULL, 0u) != NULL);
    encoded = encode_function(ir);
    memory = map_code(&encoded, &mapping_size);
    memcpy(&function, &memory, sizeof(function));
    assert(function(13, 29) == 42);
    assert(function(-17, 5) == -12);
    assert(munmap(memory, mapping_size) == 0);
    object = objfile_new(
        object_path, host_target() == RCC_X86_TARGET_X86_64
                         ? ARCH_X64 : ARCH_X86);
    assert(rcc_x86_object_add_function(
        object, "encoded_add", SYM_GLOBAL, &encoded,
        error, sizeof(error)));
    assert(objfile_write(object, object_path));
    objfile_free(object);
    roundtrip = objfile_read(object_path);
    assert(roundtrip != NULL);
    text = objfile_get_section(roundtrip, ".text");
    symbol = objfile_find_symbol(roundtrip, "encoded_add");
    assert(text != NULL && text->size == encoded.code_size);
    assert(memcmp(text->data, encoded.code, encoded.code_size) == 0);
    assert(text->relocs == NULL);
    assert(symbol != NULL && symbol->value == 0u &&
           symbol->size == encoded.code_size);
    objfile_free(roundtrip);
    rcc_x86_encoded_function_release(&encoded);
    rcc_ir_module_destroy(module);
}

static void verify_branch_execution(void)
{
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i1};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "encoded_branch", i32, parameters, 1u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrBlock* yes = rcc_ir_block_add(ir, "yes");
    RccIrBlock* no = rcc_ir_block_add(ir, "no");
    RccIrBlockId targets[] = {yes->id, no->id};
    RccIrValue yes_value;
    RccIrValue no_value;
    RccX86EncodedFunction encoded;
    size_t mapping_size;
    void* memory;
    int (*function)(int);
    assert(rcc_ir_append(entry, RCC_IR_COND_BRANCH, rcc_ir_type_void(),
                         &ir->parameters[0], 1u,
                         targets, 2u) != NULL);
    yes_value = append_const(yes, i32, 11u);
    assert(rcc_ir_append(yes, RCC_IR_RETURN, rcc_ir_type_void(),
                         &yes_value, 1u, NULL, 0u) != NULL);
    no_value = append_const(no, i32, 29u);
    assert(rcc_ir_append(no, RCC_IR_RETURN, rcc_ir_type_void(),
                         &no_value, 1u, NULL, 0u) != NULL);
    encoded = encode_function(ir);
    memory = map_code(&encoded, &mapping_size);
    memcpy(&function, &memory, sizeof(function));
    assert(function(1) == 11);
    assert(function(0) == 29);
    assert(munmap(memory, mapping_size) == 0);
    rcc_x86_encoded_function_release(&encoded);
    rcc_ir_module_destroy(module);
}

static int execute_binary_function(RccIrOpcode opcode,
                                   int left, int right)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "encoded_fixed", i32, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrInstruction* result = rcc_ir_append(
        entry, opcode, i32, ir->parameters, 2u, NULL, 0u);
    RccX86EncodedFunction encoded;
    size_t mapping_size;
    void* memory;
    int (*function)(int, int);
    int value;
    assert(result != NULL);
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &result->result, 1u, NULL, 0u) != NULL);
    encoded = encode_function(ir);
    memory = map_code(&encoded, &mapping_size);
    memcpy(&function, &memory, sizeof(function));
    value = function(left, right);
    assert(munmap(memory, mapping_size) == 0);
    rcc_x86_encoded_function_release(&encoded);
    rcc_ir_module_destroy(module);
    return value;
}

static void verify_fixed_register_execution(void)
{
    assert(execute_binary_function(RCC_IR_UDIV, 84, 2) == 42);
    assert(execute_binary_function(RCC_IR_UREM, 85, 7) == 1);
    assert(execute_binary_function(RCC_IR_SDIV, -84, 4) == -21);
    assert(execute_binary_function(RCC_IR_SREM, -85, 7) == -1);
    assert(execute_binary_function(RCC_IR_SHL, 21, 1) == 42);
    assert(execute_binary_function(RCC_IR_LSHR, 84, 1) == 42);
    assert(execute_binary_function(RCC_IR_ASHR, -84, 1) == -42);
}

static int execute_compare_function(RccIrIntPredicate predicate,
                                    int left, int right)
{
    RccIrType i1 = rcc_ir_type_integer(1u);
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameters[] = {i32, i32};
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "encoded_compare", i32, parameters, 2u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrInstruction* compare = rcc_ir_append(
        entry, RCC_IR_ICMP, i1, ir->parameters, 2u, NULL, 0u);
    RccIrInstruction* extended;
    RccX86EncodedFunction encoded;
    size_t mapping_size;
    void* memory;
    int (*function)(int, int);
    int value;
    assert(compare != NULL);
    rcc_ir_set_predicate(compare, predicate);
    extended = rcc_ir_append(
        entry, RCC_IR_ZEXT, i32, &compare->result, 1u, NULL, 0u);
    assert(extended != NULL);
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &extended->result, 1u, NULL, 0u) != NULL);
    encoded = encode_function(ir);
    memory = map_code(&encoded, &mapping_size);
    memcpy(&function, &memory, sizeof(function));
    value = function(left, right);
    assert(munmap(memory, mapping_size) == 0);
    rcc_x86_encoded_function_release(&encoded);
    rcc_ir_module_destroy(module);
    return value;
}

static int execute_conversion_function(bool sign_extend,
                                       bool truncate, int input)
{
    RccIrType i8 = rcc_ir_type_integer(8u);
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType parameter = truncate ? i32 : i8;
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "encoded_conversion", i32, &parameter, 1u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrInstruction* first;
    RccIrInstruction* result;
    RccX86EncodedFunction encoded;
    size_t mapping_size;
    void* memory;
    int (*function)(int);
    int value;
    if (truncate) {
        first = rcc_ir_append(
            entry, RCC_IR_TRUNC, i8, ir->parameters, 1u, NULL, 0u);
        assert(first != NULL);
        result = rcc_ir_append(
            entry, RCC_IR_ZEXT, i32, &first->result, 1u, NULL, 0u);
    } else {
        result = rcc_ir_append(
            entry, sign_extend ? RCC_IR_SEXT : RCC_IR_ZEXT,
            i32, ir->parameters, 1u, NULL, 0u);
    }
    assert(result != NULL);
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &result->result, 1u, NULL, 0u) != NULL);
    encoded = encode_function(ir);
    memory = map_code(&encoded, &mapping_size);
    memcpy(&function, &memory, sizeof(function));
    value = function(input);
    assert(munmap(memory, mapping_size) == 0);
    rcc_x86_encoded_function_release(&encoded);
    rcc_ir_module_destroy(module);
    return value;
}

static void verify_compare_and_conversion_execution(void)
{
    assert(execute_compare_function(RCC_IR_ICMP_EQ, 7, 7) == 1);
    assert(execute_compare_function(RCC_IR_ICMP_NE, 7, 7) == 0);
    assert(execute_compare_function(RCC_IR_ICMP_ULT, 1, -1) == 1);
    assert(execute_compare_function(RCC_IR_ICMP_UGE, 1, -1) == 0);
    assert(execute_compare_function(RCC_IR_ICMP_SLT, -1, 1) == 1);
    assert(execute_compare_function(RCC_IR_ICMP_SGE, -1, 1) == 0);
    assert(execute_conversion_function(false, false, 0xff) == 255);
    assert(execute_conversion_function(true, false, 0xff) == -1);
    assert(execute_conversion_function(false, true, 0x1234) == 0x34);
}

static void verify_stack_memory_execution(void)
{
    RccIrType i32 = rcc_ir_type_integer(32u);
    RccIrType pointer = rcc_ir_type_pointer(0u);
    RccIrType parameter = i32;
    RccIrModule* module = rcc_ir_module_create();
    RccIrFunction* ir = rcc_ir_function_add(
        module, "encoded_stack", i32, &parameter, 1u);
    RccIrBlock* entry = rcc_ir_block_add(ir, "entry");
    RccIrInstruction* allocation = rcc_ir_append(
        entry, RCC_IR_ALLOCA, pointer, NULL, 0u, NULL, 0u);
    RccIrValue store_operands[2];
    RccIrInstruction* load;
    RccX86EncodedFunction encoded;
    size_t mapping_size;
    void* memory;
    int (*function)(int);
    assert(allocation != NULL);
    rcc_ir_set_immediate(allocation, 4u);
    store_operands[0] = ir->parameters[0];
    store_operands[1] = allocation->result;
    assert(rcc_ir_append(entry, RCC_IR_STORE, rcc_ir_type_void(),
                         store_operands, 2u, NULL, 0u) != NULL);
    load = rcc_ir_append(entry, RCC_IR_LOAD, i32,
                         &allocation->result, 1u, NULL, 0u);
    assert(load != NULL);
    assert(rcc_ir_append(entry, RCC_IR_RETURN, rcc_ir_type_void(),
                         &load->result, 1u, NULL, 0u) != NULL);
    encoded = encode_function(ir);
    memory = map_code(&encoded, &mapping_size);
    memcpy(&function, &memory, sizeof(function));
    assert(function(0x12345678) == 0x12345678);
    assert(function(-137) == -137);
    assert(munmap(memory, mapping_size) == 0);
    rcc_x86_encoded_function_release(&encoded);
    rcc_ir_module_destroy(module);
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    verify_add_execution(argv[1]);
    verify_branch_execution();
    verify_fixed_register_execution();
    verify_compare_and_conversion_execution();
    verify_stack_memory_execution();
    puts("Native legal-IR x86 encoding execution tests passed");
    return 0;
}
