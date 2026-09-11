/*
 * RCC - Target-independent typed SSA optimization passes
 */

#ifndef RCC_IR_PASS_H
#define RCC_IR_PASS_H

#include "ir.h"

typedef struct {
    size_t promoted_allocas;
    size_t removed_loads;
    size_t removed_stores;
    size_t inserted_phis;
} RccIrMem2RegStats;

typedef struct {
    size_t folded_instructions;
    size_t commoned_instructions;
    size_t removed_instructions;
} RccIrSimplifyStats;

typedef struct {
    unsigned level;
    size_t simplify_rounds;
    RccIrMem2RegStats mem2reg;
    RccIrSimplifyStats simplify;
} RccIrOptimizationStats;

bool rcc_ir_mem2reg(RccIrFunction* function, RccIrMem2RegStats* stats,
                    char* error, size_t error_size);
bool rcc_ir_simplify(RccIrFunction* function, RccIrSimplifyStats* stats,
                     char* error, size_t error_size);
bool rcc_ir_optimize_function(RccIrFunction* function, unsigned level,
                              RccIrOptimizationStats* stats,
                              char* error, size_t error_size);

#endif /* RCC_IR_PASS_H */
