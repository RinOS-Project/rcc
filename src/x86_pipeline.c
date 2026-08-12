/*
 * RCC - typed SSA to verified x86 encoding pipeline
 */

#include "rcc.h"
#include "x86_pipeline.h"

#include "mir.h"
#include "mir_alloc.h"
#include "mir_phi.h"
#include "x86_legalize.h"
#include "x86_select.h"

bool rcc_x86_encode_ir_function(
    const RccIrFunction* function, RccX86Target target,
    RccX86EncodedFunction* encoded_out,
    char* error, size_t error_size) {
    RccMirFunction* mir = NULL;
    RccMirRegisterPolicy policy;
    RccMirAllocation allocation;
    RccMirPhiPlan phi_plan;
    RccX86Function* selected = NULL;
    RccX86LegalFunction* legal = NULL;
    RccX86EncodedFunction encoded;
    bool allocated = false;
    bool phi_built = false;
    bool ok = false;
    if (encoded_out) memset(encoded_out, 0, sizeof(*encoded_out));
    memset(&allocation, 0, sizeof(allocation));
    memset(&phi_plan, 0, sizeof(phi_plan));
    memset(&encoded, 0, sizeof(encoded));
    if (error && error_size != 0u) error[0] = '\0';
    if (!function || !encoded_out ||
        (target != RCC_X86_TARGET_I686 &&
         target != RCC_X86_TARGET_X86_64)) {
        if (error && error_size != 0u) {
            snprintf(error, error_size,
                     "invalid typed SSA x86 pipeline input");
        }
        return false;
    }
    if (!rcc_mir_lower_ir(function, &mir, error, error_size)) goto cleanup;
    if (target == RCC_X86_TARGET_X86_64) {
        rcc_mir_register_policy_x86_64(&policy);
    } else {
        rcc_mir_register_policy_i686(&policy);
    }
    if (!rcc_mir_linear_scan_allocate(
            mir, &policy, &allocation, error, error_size)) goto cleanup;
    allocated = true;
    if (!rcc_mir_build_phi_plan(
            mir, &policy, &allocation, &phi_plan,
            error, error_size)) goto cleanup;
    phi_built = true;
    if (!rcc_x86_select_function(
            mir, target, &policy, &allocation, &phi_plan,
            &selected, error, error_size) ||
        !rcc_x86_legalize_function(
            selected, &policy, &legal, error, error_size) ||
        !rcc_x86_encode_function(
            legal, &policy, &encoded, error, error_size)) goto cleanup;
    *encoded_out = encoded;
    memset(&encoded, 0, sizeof(encoded));
    ok = true;

cleanup:
    rcc_x86_encoded_function_release(&encoded);
    rcc_x86_legal_function_destroy(legal);
    rcc_x86_function_destroy(selected);
    if (phi_built) rcc_mir_phi_plan_release(&phi_plan);
    if (allocated) rcc_mir_allocation_release(&allocation);
    rcc_mir_function_destroy(mir);
    return ok;
}
