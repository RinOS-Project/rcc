/*
 * RCC - Target-independent machine intermediate representation
 */

#ifndef RCC_MIR_H
#define RCC_MIR_H

#include "ir.h"

#define RCC_MIR_VREG_NONE UINT32_MAX
#define RCC_MIR_BLOCK_NONE UINT32_MAX

typedef uint32_t RccMirVReg;
typedef uint32_t RccMirBlockId;

typedef enum {
    RCC_MIR_TYPE_VOID,
    RCC_MIR_TYPE_INTEGER,
    RCC_MIR_TYPE_FLOAT,
    RCC_MIR_TYPE_POINTER,
} RccMirTypeKind;

typedef struct {
    RccMirTypeKind kind;
    uint16_t bit_width;
} RccMirType;

typedef enum {
    RCC_MIR_CONST_INT,
    RCC_MIR_ADD,
    RCC_MIR_SUB,
    RCC_MIR_MUL,
    RCC_MIR_UDIV,
    RCC_MIR_SDIV,
    RCC_MIR_UREM,
    RCC_MIR_SREM,
    RCC_MIR_AND,
    RCC_MIR_OR,
    RCC_MIR_XOR,
    RCC_MIR_SHL,
    RCC_MIR_LSHR,
    RCC_MIR_ASHR,
    RCC_MIR_ICMP,
    RCC_MIR_TRUNC,
    RCC_MIR_ZEXT,
    RCC_MIR_SEXT,
    RCC_MIR_PTR_TO_INT,
    RCC_MIR_INT_TO_PTR,
    RCC_MIR_BITCAST,
    RCC_MIR_PHI,
    RCC_MIR_SELECT,
    RCC_MIR_ALLOCA,
    RCC_MIR_LOAD,
    RCC_MIR_STORE,
    RCC_MIR_GEP,
    RCC_MIR_CALL,
    RCC_MIR_BRANCH,
    RCC_MIR_COND_BRANCH,
    RCC_MIR_RETURN,
    RCC_MIR_UNREACHABLE,
} RccMirOpcode;

typedef struct RccMirInstruction RccMirInstruction;
typedef struct RccMirBlock RccMirBlock;
typedef struct RccMirFunction RccMirFunction;

struct RccMirInstruction {
    RccMirOpcode opcode;
    RccMirType type;
    RccMirVReg definition;
    RccMirVReg* operands;
    size_t operand_count;
    RccMirBlockId* targets;
    size_t target_count;
    uint64_t immediate;
    RccIrIntPredicate predicate;
    char* callee;
    RccMirBlock* block;
    RccMirInstruction* previous;
    RccMirInstruction* next;
};

struct RccMirBlock {
    RccMirBlockId id;
    char* name;
    RccMirInstruction* first;
    RccMirInstruction* last;
    RccMirFunction* function;
    RccMirBlock* next;
};

struct RccMirFunction {
    char* name;
    RccMirType return_type;
    RccMirType* parameter_types;
    RccMirVReg* parameters;
    size_t parameter_count;
    RccMirType* register_types;
    size_t register_count;
    RccMirBlock* first_block;
    RccMirBlock* last_block;
    size_t block_count;
};

RccMirType rcc_mir_type_void(void);
RccMirType rcc_mir_type_integer(uint16_t bit_width);
RccMirType rcc_mir_type_float(uint16_t bit_width);
RccMirType rcc_mir_type_pointer(void);
bool rcc_mir_type_equal(RccMirType left, RccMirType right);

void rcc_mir_function_destroy(RccMirFunction* function);
bool rcc_mir_verify_function(const RccMirFunction* function, char* error,
                             size_t error_size);
bool rcc_mir_lower_ir(const RccIrFunction* ir_function,
                      RccMirFunction** mir_out,
                      char* error, size_t error_size);

#endif /* RCC_MIR_H */
