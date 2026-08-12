/*
 * RCC - Target-independent typed SSA intermediate representation
 */

#ifndef RCC_IR_H
#define RCC_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RCC_IR_VALUE_NONE UINT32_MAX
#define RCC_IR_BLOCK_NONE UINT32_MAX

typedef uint32_t RccIrValue;
typedef uint32_t RccIrBlockId;

typedef enum {
    RCC_IR_TYPE_VOID,
    RCC_IR_TYPE_INTEGER,
    RCC_IR_TYPE_FLOAT,
    RCC_IR_TYPE_POINTER,
    RCC_IR_TYPE_AGGREGATE,
} RccIrTypeKind;

typedef struct {
    RccIrTypeKind kind;
    uint16_t bit_width;
    uint16_t lanes;
    uint32_t address_space;
    uint32_t aggregate_id;
} RccIrType;

typedef enum {
    RCC_IR_CONST_INT,
    RCC_IR_ADD,
    RCC_IR_SUB,
    RCC_IR_MUL,
    RCC_IR_UDIV,
    RCC_IR_SDIV,
    RCC_IR_UREM,
    RCC_IR_SREM,
    RCC_IR_AND,
    RCC_IR_OR,
    RCC_IR_XOR,
    RCC_IR_SHL,
    RCC_IR_LSHR,
    RCC_IR_ASHR,
    RCC_IR_ICMP,
    RCC_IR_TRUNC,
    RCC_IR_ZEXT,
    RCC_IR_SEXT,
    RCC_IR_PTR_TO_INT,
    RCC_IR_INT_TO_PTR,
    RCC_IR_BITCAST,
    RCC_IR_PHI,
    RCC_IR_SELECT,
    RCC_IR_ALLOCA,
    RCC_IR_LOAD,
    RCC_IR_STORE,
    RCC_IR_GEP,
    RCC_IR_CALL,
    RCC_IR_BRANCH,
    RCC_IR_COND_BRANCH,
    RCC_IR_RETURN,
    RCC_IR_UNREACHABLE,
} RccIrOpcode;

typedef enum {
    RCC_IR_ICMP_EQ,
    RCC_IR_ICMP_NE,
    RCC_IR_ICMP_ULT,
    RCC_IR_ICMP_ULE,
    RCC_IR_ICMP_UGT,
    RCC_IR_ICMP_UGE,
    RCC_IR_ICMP_SLT,
    RCC_IR_ICMP_SLE,
    RCC_IR_ICMP_SGT,
    RCC_IR_ICMP_SGE,
} RccIrIntPredicate;

typedef struct RccIrInstruction RccIrInstruction;
typedef struct RccIrBlock RccIrBlock;
typedef struct RccIrFunction RccIrFunction;
typedef struct RccIrModule RccIrModule;

struct RccIrInstruction {
    RccIrOpcode opcode;
    RccIrType type;
    RccIrValue result;
    RccIrValue* operands;
    size_t operand_count;
    RccIrBlockId* targets;
    size_t target_count;
    uint64_t immediate;
    RccIrIntPredicate predicate;
    char* callee;
    RccIrBlock* block;
    RccIrInstruction* previous;
    RccIrInstruction* next;
};

struct RccIrBlock {
    RccIrBlockId id;
    char* name;
    RccIrInstruction* first;
    RccIrInstruction* last;
    RccIrFunction* function;
    RccIrBlock* next;
};

struct RccIrFunction {
    char* name;
    RccIrType return_type;
    RccIrType* parameter_types;
    RccIrValue* parameters;
    size_t parameter_count;
    RccIrType* value_types;
    size_t value_count;
    size_t value_capacity;
    RccIrBlock* first_block;
    RccIrBlock* last_block;
    size_t block_count;
    RccIrModule* module;
    RccIrFunction* next;
};

struct RccIrModule {
    RccIrFunction* first_function;
    RccIrFunction* last_function;
    size_t function_count;
};

RccIrType rcc_ir_type_void(void);
RccIrType rcc_ir_type_integer(uint16_t bit_width);
RccIrType rcc_ir_type_float(uint16_t bit_width);
RccIrType rcc_ir_type_pointer(uint32_t address_space);
RccIrType rcc_ir_type_aggregate(uint32_t aggregate_id);
bool rcc_ir_type_equal(RccIrType left, RccIrType right);

RccIrModule* rcc_ir_module_create(void);
void rcc_ir_module_destroy(RccIrModule* module);
RccIrFunction* rcc_ir_function_add(RccIrModule* module, const char* name,
                                  RccIrType return_type,
                                  const RccIrType* parameter_types,
                                  size_t parameter_count);
RccIrBlock* rcc_ir_block_add(RccIrFunction* function, const char* name);

RccIrInstruction* rcc_ir_append(RccIrBlock* block, RccIrOpcode opcode,
                                RccIrType result_type,
                                const RccIrValue* operands,
                                size_t operand_count,
                                const RccIrBlockId* targets,
                                size_t target_count);
void rcc_ir_set_immediate(RccIrInstruction* instruction, uint64_t immediate);
void rcc_ir_set_predicate(RccIrInstruction* instruction,
                          RccIrIntPredicate predicate);
void rcc_ir_set_callee(RccIrInstruction* instruction, const char* callee);

bool rcc_ir_verify_function(const RccIrFunction* function, char* error,
                            size_t error_size);
bool rcc_ir_verify_module(const RccIrModule* module, char* error,
                          size_t error_size);

#endif /* RCC_IR_H */
