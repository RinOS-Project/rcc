/* SPDX-License-Identifier: MIT */
#ifndef RCC_DRIVER_POLICY_H
#define RCC_DRIVER_POLICY_H

#include <stdbool.h>

struct AST;

/* NDRV code runs without an FPU/SIMD context. Reject source constructs that
 * would make the driver ABI or generated code depend on that state. */
bool rcc_validate_driver_policy(struct AST* ast);

#endif /* RCC_DRIVER_POLICY_H */
