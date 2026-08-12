/*
 * RCC - i686 and AMD64 SysV register contracts
 */

#ifndef RCC_X86_ABI_H
#define RCC_X86_ABI_H

#include "mir_alloc.h"

typedef enum {
    RCC_X86_TARGET_I686,
    RCC_X86_TARGET_X86_64,
} RccX86Target;

typedef enum {
    RCC_X86_GPR_AX = 0,
    RCC_X86_GPR_CX = 1,
    RCC_X86_GPR_DX = 2,
    RCC_X86_GPR_BX = 3,
    RCC_X86_GPR_SP = 4,
    RCC_X86_GPR_BP = 5,
    RCC_X86_GPR_SI = 6,
    RCC_X86_GPR_DI = 7,
    RCC_X86_GPR_R8 = 8,
    RCC_X86_GPR_R9 = 9,
    RCC_X86_GPR_R10 = 10,
    RCC_X86_GPR_R11 = 11,
    RCC_X86_GPR_R12 = 12,
    RCC_X86_GPR_R13 = 13,
    RCC_X86_GPR_R14 = 14,
    RCC_X86_GPR_R15 = 15,
} RccX86HardwareGpr;

typedef struct {
    RccX86Target target;
    uint16_t pointer_size;
    uint16_t stack_alignment;
    RccX86HardwareGpr gpr_map[14];
    size_t gpr_count;
    size_t fpr_count;
    uint64_t caller_saved_abstract_mask;
    uint64_t callee_saved_abstract_mask;
    uint64_t caller_saved_fpr_mask;
    uint64_t division_fixed_abstract_mask;
    uint64_t shift_count_fixed_abstract_mask;
    RccX86HardwareGpr integer_arguments[6];
    size_t integer_argument_count;
    RccX86HardwareGpr return_low;
    RccX86HardwareGpr return_high;
    RccX86HardwareGpr shift_count;
    RccX86HardwareGpr stack_pointer;
    RccX86HardwareGpr frame_pointer;
} RccX86Abi;

void rcc_x86_abi_i686(RccX86Abi* abi);
void rcc_x86_abi_x86_64(RccX86Abi* abi);
bool rcc_x86_abi_for_target(RccX86Target target, RccX86Abi* abi);
bool rcc_x86_abi_verify_policy(
    const RccX86Abi* abi, const RccMirRegisterPolicy* policy,
    char* error, size_t error_size);
bool rcc_x86_abi_hardware_gpr(
    const RccX86Abi* abi, uint16_t abstract_register,
    RccX86HardwareGpr* hardware_out);

#endif /* RCC_X86_ABI_H */
