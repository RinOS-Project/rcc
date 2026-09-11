/*
 * RCC - i686 and AMD64 SysV register contracts
 */

#include "rcc.h"
#include "x86_abi.h"

#include <stdarg.h>

static bool x86_abi_error(char* error, size_t error_size,
                          const char* format, ...) {
    va_list arguments;
    if (error && error_size != 0u) {
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

void rcc_x86_abi_i686(RccX86Abi* abi) {
    static const RccX86HardwareGpr registers[] = {
        RCC_X86_GPR_AX, RCC_X86_GPR_CX, RCC_X86_GPR_DX,
        RCC_X86_GPR_BX, RCC_X86_GPR_SI, RCC_X86_GPR_DI,
    };
    if (!abi) return;
    memset(abi, 0, sizeof(*abi));
    abi->target = RCC_X86_TARGET_I686;
    abi->pointer_size = 4u;
    abi->stack_alignment = 16u;
    memcpy(abi->gpr_map, registers, sizeof(registers));
    abi->gpr_count = sizeof(registers) / sizeof(registers[0]);
    abi->fpr_count = 8u;
    abi->caller_saved_abstract_mask = UINT64_C(0x07);
    abi->callee_saved_abstract_mask = UINT64_C(0x38);
    abi->caller_saved_fpr_mask = UINT64_C(0xff);
    abi->division_fixed_abstract_mask = UINT64_C(0x05);
    abi->shift_count_fixed_abstract_mask = UINT64_C(0x02);
    abi->return_low = RCC_X86_GPR_AX;
    abi->return_high = RCC_X86_GPR_DX;
    abi->shift_count = RCC_X86_GPR_CX;
    abi->stack_pointer = RCC_X86_GPR_SP;
    abi->frame_pointer = RCC_X86_GPR_BP;
}

void rcc_x86_abi_x86_64(RccX86Abi* abi) {
    static const RccX86HardwareGpr registers[] = {
        RCC_X86_GPR_AX, RCC_X86_GPR_CX, RCC_X86_GPR_DX,
        RCC_X86_GPR_SI, RCC_X86_GPR_DI, RCC_X86_GPR_R8,
        RCC_X86_GPR_R9, RCC_X86_GPR_R10, RCC_X86_GPR_R11,
        RCC_X86_GPR_BX, RCC_X86_GPR_R12, RCC_X86_GPR_R13,
        RCC_X86_GPR_R14, RCC_X86_GPR_R15,
    };
    static const RccX86HardwareGpr arguments[] = {
        RCC_X86_GPR_DI, RCC_X86_GPR_SI, RCC_X86_GPR_DX,
        RCC_X86_GPR_CX, RCC_X86_GPR_R8, RCC_X86_GPR_R9,
    };
    if (!abi) return;
    memset(abi, 0, sizeof(*abi));
    abi->target = RCC_X86_TARGET_X86_64;
    abi->pointer_size = 8u;
    abi->stack_alignment = 16u;
    memcpy(abi->gpr_map, registers, sizeof(registers));
    abi->gpr_count = sizeof(registers) / sizeof(registers[0]);
    abi->fpr_count = 16u;
    abi->caller_saved_abstract_mask = UINT64_C(0x01ff);
    abi->callee_saved_abstract_mask = UINT64_C(0x3e00);
    abi->caller_saved_fpr_mask = UINT64_C(0xffff);
    abi->division_fixed_abstract_mask = UINT64_C(0x05);
    abi->shift_count_fixed_abstract_mask = UINT64_C(0x02);
    memcpy(abi->integer_arguments, arguments, sizeof(arguments));
    abi->integer_argument_count =
        sizeof(arguments) / sizeof(arguments[0]);
    abi->return_low = RCC_X86_GPR_AX;
    abi->return_high = RCC_X86_GPR_DX;
    abi->shift_count = RCC_X86_GPR_CX;
    abi->stack_pointer = RCC_X86_GPR_SP;
    abi->frame_pointer = RCC_X86_GPR_BP;
}

bool rcc_x86_abi_for_target(RccX86Target target, RccX86Abi* abi) {
    if (!abi) return false;
    if (target == RCC_X86_TARGET_I686) {
        rcc_x86_abi_i686(abi);
        return true;
    }
    if (target == RCC_X86_TARGET_X86_64) {
        rcc_x86_abi_x86_64(abi);
        return true;
    }
    memset(abi, 0, sizeof(*abi));
    return false;
}

bool rcc_x86_abi_hardware_gpr(
    const RccX86Abi* abi, uint16_t abstract_register,
    RccX86HardwareGpr* hardware_out) {
    if (!abi || !hardware_out || abstract_register >= abi->gpr_count) {
        return false;
    }
    *hardware_out = abi->gpr_map[abstract_register];
    return true;
}

bool rcc_x86_abi_verify_policy(
    const RccX86Abi* abi, const RccMirRegisterPolicy* policy,
    char* error, size_t error_size) {
    uint64_t expected_gpr_mask;
    uint64_t expected_fpr_mask;
    size_t index;
    if (error && error_size != 0u) error[0] = '\0';
    if (!abi || !policy || abi->gpr_count == 0u ||
        abi->gpr_count >= 64u || abi->fpr_count == 0u ||
        abi->fpr_count >= 64u) {
        return x86_abi_error(error, error_size,
                             "x86 ABI descriptor is invalid");
    }
    expected_gpr_mask = (UINT64_C(1) << abi->gpr_count) - 1u;
    expected_fpr_mask = (UINT64_C(1) << abi->fpr_count) - 1u;
    if (policy->pointer_size != abi->pointer_size ||
        policy->stack_alignment != abi->stack_alignment ||
        policy->allocatable_gpr_mask != expected_gpr_mask ||
        policy->caller_saved_gpr_mask !=
            abi->caller_saved_abstract_mask ||
        policy->allocatable_fpr_mask != expected_fpr_mask ||
        policy->caller_saved_fpr_mask != abi->caller_saved_fpr_mask ||
        policy->division_fixed_gpr_mask !=
            abi->division_fixed_abstract_mask ||
        policy->shift_count_fixed_gpr_mask !=
            abi->shift_count_fixed_abstract_mask ||
        abi->callee_saved_abstract_mask !=
            (expected_gpr_mask & ~abi->caller_saved_abstract_mask)) {
        return x86_abi_error(error, error_size,
                             "MIR register policy disagrees with x86 ABI");
    }
    for (index = 0u; index < abi->gpr_count; ++index) {
        size_t other;
        if (abi->gpr_map[index] == abi->stack_pointer ||
            abi->gpr_map[index] == abi->frame_pointer) {
            return x86_abi_error(error, error_size,
                                 "x86 ABI allocates stack/frame pointer");
        }
        for (other = 0u; other < index; ++other) {
            if (abi->gpr_map[index] == abi->gpr_map[other]) {
                return x86_abi_error(error, error_size,
                                     "x86 ABI register map is not unique");
            }
        }
    }
    return true;
}
