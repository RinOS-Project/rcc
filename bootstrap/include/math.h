#ifndef RCC_BOOTSTRAP_MATH_H
#define RCC_BOOTSTRAP_MATH_H

/* RCC itself needs the standard classification macro while bootstrapping the
 * semantic evaluator.  Runtime math functions are provided by their owning
 * RinOS libraries and are not fabricated in bootstrap libc. */
#include <float.h>
#define isfinite(value) \
    ((value) == (value) && (value) <= DBL_MAX && (value) >= -DBL_MAX)

#endif
