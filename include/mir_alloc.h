/*
 * RCC - MIR liveness and register allocation
 */

#ifndef RCC_MIR_ALLOC_H
#define RCC_MIR_ALLOC_H

#include "mir.h"

typedef enum {
    RCC_MIR_REGCLASS_GPR,
    RCC_MIR_REGCLASS_FPR,
} RccMirRegisterClass;

typedef struct {
    size_t start;
    size_t end;
    RccMirRegisterClass register_class;
    bool crosses_call;
} RccMirLiveInterval;

typedef enum {
    RCC_MIR_LOCATION_PHYSICAL,
    RCC_MIR_LOCATION_SPILL,
} RccMirLocationKind;

typedef struct {
    RccMirLocationKind kind;
    RccMirRegisterClass register_class;
    uint16_t physical_register;
    uint32_t spill_offset;
    uint16_t spill_size;
    uint16_t spill_alignment;
} RccMirLocation;

typedef struct {
    uint64_t allocatable_gpr_mask;
    uint64_t allocatable_fpr_mask;
    uint64_t caller_saved_gpr_mask;
    uint64_t caller_saved_fpr_mask;
    uint16_t pointer_size;
    uint16_t stack_alignment;
} RccMirRegisterPolicy;

typedef struct {
    RccMirLiveInterval* intervals;
    RccMirLocation* locations;
    size_t register_count;
    uint32_t spill_area_size;
    size_t physical_count;
    size_t spill_count;
} RccMirAllocation;

void rcc_mir_register_policy_i686(RccMirRegisterPolicy* policy);
void rcc_mir_register_policy_x86_64(RccMirRegisterPolicy* policy);
bool rcc_mir_linear_scan_allocate(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    RccMirAllocation* allocation, char* error, size_t error_size);
bool rcc_mir_verify_allocation(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, char* error, size_t error_size);
void rcc_mir_allocation_release(RccMirAllocation* allocation);

#endif /* RCC_MIR_ALLOC_H */
