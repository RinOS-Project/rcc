# i686 typed-SSA floating-point status

## Implemented

The verified backend lowers i686 scalar `float` and `double` arithmetic,
ordered/unordered comparisons, truth conversion, signed and unsigned integer
conversions, unary minus, compound updates, pre/post increment and decrement,
and conditional selection. Values stay in typed stack slots; x87 is used
transiently for arithmetic, comparison, conversion, and the cdecl
floating-point ABI. The ABI path uses stack arguments and the x87 `ST0` return
value. Direct calls, external C calls, compound expressions, and calls mixing
integer and floating-point stack arguments are covered.

The backend does not assign virtual floating-point values to stable physical
FPRs on i686. This matches the x87 register-stack model and keeps allocation
and spills explicit in the existing frame slots.

## Verification

Run `make test-verified-i686-floating-arithmetic
test-verified-i686-floating-comparisons test-verified-i686-floating-operations`.
The targets compile C17 and C++20 fixtures at `-O0` and `-O2`, require zero
verified-backend fallback, inspect generated objects, and execute the generated
i686 code in a host process. Coverage includes the six comparison predicates,
NaN unordered behavior, signed zero, integer/floating conversions through
64-bit integers, and the supported update/select operations. All four language
and optimization configurations passed on 2026-10-09.
