/*
 * RCC - i686/x86-64 MIR instruction selection
 */

#ifndef RCC_X86_SELECT_H
#define RCC_X86_SELECT_H

#include "mir_phi.h"
#include "x86_abi.h"

typedef enum {
    RCC_X86_COPY,
    RCC_X86_MOV_IMMEDIATE,
    RCC_X86_ADD,
    RCC_X86_SUB,
    RCC_X86_MUL,
    RCC_X86_UDIV,
    RCC_X86_SDIV,
    RCC_X86_UREM,
    RCC_X86_SREM,
    RCC_X86_AND,
    RCC_X86_OR,
    RCC_X86_XOR,
    RCC_X86_SHL,
    RCC_X86_SHR,
    RCC_X86_SAR,
    RCC_X86_COMPARE_SET,
    RCC_X86_TRUNCATE,
    RCC_X86_ZERO_EXTEND,
    RCC_X86_SIGN_EXTEND,
    RCC_X86_REINTERPRET,
    RCC_X86_SELECT,
    RCC_X86_STACK_ADDRESS,
    RCC_X86_LOAD,
    RCC_X86_STORE,
    RCC_X86_GEP,
    RCC_X86_CALL,
    RCC_X86_JUMP,
    RCC_X86_JUMP_IF,
    RCC_X86_RETURN,
    RCC_X86_TRAP,
} RccX86Opcode;

typedef struct RccX86Instruction RccX86Instruction;
typedef struct RccX86Block RccX86Block;

struct RccX86Instruction {
    RccX86Opcode opcode;
    RccMirType type;
    bool has_destination;
    RccMirLocation destination;
    RccMirLocation* operands;
    RccMirType* operand_types;
    size_t operand_count;
    uint32_t* targets;
    size_t target_count;
    uint64_t immediate;
    uint64_t auxiliary;
    RccIrIntPredicate predicate;
    char* symbol;
    bool cycle_break;
    RccX86Instruction* next;
};

struct RccX86Block {
    uint32_t id;
    RccMirBlockId source_block;
    bool edge_split;
    RccMirBlockId edge_predecessor;
    RccMirBlockId edge_successor;
    RccX86Instruction* first;
    RccX86Instruction* last;
    RccX86Block* next;
};

typedef struct {
    RccX86Target target;
    uint16_t pointer_size;
    uint16_t stack_alignment;
    uint32_t frame_size;
    RccMirType return_type;
    RccMirType* parameter_types;
    RccMirLocation* parameters;
    size_t parameter_count;
    size_t original_block_count;
    size_t block_count;
    size_t source_instruction_count;
    RccX86Block* first_block;
    RccX86Block* last_block;
} RccX86Function;

bool rcc_x86_select_function(
    const RccMirFunction* function, RccX86Target target,
    const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, const RccMirPhiPlan* phi_plan,
    RccX86Function** selected_out, char* error, size_t error_size);
bool rcc_x86_verify_function(
    const RccX86Function* function,
    const RccMirRegisterPolicy* policy,
    char* error, size_t error_size);
void rcc_x86_function_destroy(RccX86Function* function);

#endif /* RCC_X86_SELECT_H */
