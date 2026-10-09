# i686 typed-SSA floating arithmetic status

## Implemented

The verified backend lowers `float` and `double` scalar addition, subtraction,
multiplication, and division on i686. Values stay in typed stack slots; x87 is
used transiently for arithmetic and for the cdecl floating-point ABI. The ABI
path uses stack arguments and the x87 `ST0` return value. Direct calls,
external C calls, three-term expressions, and calls mixing integer and
floating-point stack arguments are covered.

The backend does not assign virtual floating-point values to stable physical
FPRs on i686. This matches the x87 register-stack model and keeps allocation
and spills explicit in the existing frame slots.

## Verification

Run `make test-verified-i686-floating-arithmetic`. The target compiles the
same fixture as C17 and C++20, at `-O0` and `-O2`, checks that all 13 functions
are emitted with zero verified-backend fallback, checks x87 opcodes and
external relocations, then executes the generated i686 code in a host process.
All four configurations passed on 2026-10-09.

The separate `test-verified-i686-floating-comparisons` target covers all six
ordered predicates for `float` and `double`, NaN unordered behavior, signed
zero, and truth conversion. It also passes in C/C++ at `-O0` and `-O2` without
verified-backend fallback, including host execution of the generated i686 code.

## Remaining work

Integer/floating conversion, unary operations, compound updates, and floating
conditional selection remain outside these completed paths. The verified
lowering keeps their complete legacy-backend behavior; i686 typed-SSA lowering
and fallback-free execution remain open in `TODO.md`.
