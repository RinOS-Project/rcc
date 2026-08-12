/*
 * RCC - MIR phi edge-copy lowering
 */

#ifndef RCC_MIR_PHI_H
#define RCC_MIR_PHI_H

#include "mir_alloc.h"

typedef struct {
    RccMirVReg source;
    RccMirVReg destination;
    RccMirType type;
} RccMirParallelCopy;

typedef struct {
    RccMirLocation source;
    RccMirLocation destination;
    RccMirType type;
    bool cycle_break;
} RccMirScheduledMove;

typedef struct {
    RccMirBlockId predecessor;
    RccMirBlockId successor;
    bool requires_edge_block;
    RccMirParallelCopy* copies;
    size_t copy_count;
    RccMirScheduledMove* moves;
    size_t move_count;
} RccMirEdgeCopies;

typedef struct {
    RccMirEdgeCopies* edges;
    size_t edge_count;
    bool has_cycle_temporary;
    uint32_t cycle_temporary_offset;
    uint16_t cycle_temporary_size;
    uint16_t cycle_temporary_alignment;
    uint32_t frame_size;
} RccMirPhiPlan;

bool rcc_mir_build_phi_plan(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, RccMirPhiPlan* plan,
    char* error, size_t error_size);
bool rcc_mir_verify_phi_plan(
    const RccMirFunction* function, const RccMirRegisterPolicy* policy,
    const RccMirAllocation* allocation, const RccMirPhiPlan* plan,
    char* error, size_t error_size);
void rcc_mir_phi_plan_release(RccMirPhiPlan* plan);

#endif /* RCC_MIR_PHI_H */
