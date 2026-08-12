/*
 * RCC - x86 fixed-register legalization
 */

#ifndef RCC_X86_LEGALIZE_H
#define RCC_X86_LEGALIZE_H

#include "x86_select.h"

typedef enum {
    RCC_X86_VALUE_GPR,
    RCC_X86_VALUE_FRAME,
    RCC_X86_VALUE_INCOMING_ARGUMENT,
    RCC_X86_VALUE_OUTGOING_ARGUMENT,
} RccX86ValueKind;

typedef struct {
    RccX86ValueKind kind;
    RccX86HardwareGpr gpr;
    uint32_t frame_offset;
    uint16_t size;
    uint16_t alignment;
} RccX86Value;

typedef enum {
    RCC_X86_LEGAL_SELECTED,
    RCC_X86_LEGAL_COPY,
    RCC_X86_LEGAL_PREPARE_UNSIGNED_DIVIDEND,
    RCC_X86_LEGAL_PREPARE_SIGNED_DIVIDEND,
    RCC_X86_LEGAL_DIVIDE,
    RCC_X86_LEGAL_SHIFT,
    RCC_X86_LEGAL_CALL,
    RCC_X86_LEGAL_RETURN,
} RccX86LegalOpcode;

typedef struct RccX86LegalInstruction RccX86LegalInstruction;
typedef struct RccX86LegalBlock RccX86LegalBlock;

struct RccX86LegalInstruction {
    RccX86LegalOpcode opcode;
    RccX86Opcode selected_opcode;
    RccMirType type;
    bool has_destination;
    RccX86Value destination;
    RccX86Value* operands;
    RccMirType* operand_types;
    size_t operand_count;
    uint32_t* targets;
    size_t target_count;
    uint64_t immediate;
    uint64_t auxiliary;
    RccIrIntPredicate predicate;
    char* symbol;
    bool cycle_break;
    RccX86LegalInstruction* previous;
    RccX86LegalInstruction* next;
};

struct RccX86LegalBlock {
    uint32_t id;
    RccMirBlockId source_block;
    bool edge_split;
    RccMirBlockId edge_predecessor;
    RccMirBlockId edge_successor;
    RccX86LegalInstruction* first;
    RccX86LegalInstruction* last;
    RccX86LegalBlock* next;
};

typedef struct {
    RccX86Target target;
    uint16_t pointer_size;
    uint16_t stack_alignment;
    uint32_t frame_size;
    RccMirType return_type;
    uint32_t outgoing_stack_offset;
    uint32_t outgoing_stack_size;
    bool parameter_ingress_complete;
    bool has_parallel_copy_temporary;
    uint32_t parallel_copy_temporary_offset;
    size_t original_block_count;
    size_t block_count;
    size_t source_instruction_count;
    size_t legal_instruction_count;
    RccX86LegalBlock* first_block;
    RccX86LegalBlock* last_block;
} RccX86LegalFunction;

bool rcc_x86_legalize_function(
    const RccX86Function* selected,
    const RccMirRegisterPolicy* policy,
    RccX86LegalFunction** legal_out,
    char* error, size_t error_size);
bool rcc_x86_verify_legal_function(
    const RccX86LegalFunction* function,
    const RccMirRegisterPolicy* policy,
    char* error, size_t error_size);
void rcc_x86_legal_function_destroy(RccX86LegalFunction* function);

#endif /* RCC_X86_LEGALIZE_H */
