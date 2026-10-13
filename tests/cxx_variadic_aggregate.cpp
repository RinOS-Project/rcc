#include "cxx_variadic_aggregate.hpp"

int read_variadic_aggregate_registers(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    double fp0, double fp1, double fp2, double fp3, double fp4, double fp5,
    double fp6, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, fp6);
    VariadicMixedAggregate in_registers =
        __builtin_va_arg(arguments, VariadicMixedAggregate);
    VariadicMixedAggregate on_stack =
        __builtin_va_arg(arguments, VariadicMixedAggregate);
    VariadicMemoryAggregate memory =
        __builtin_va_arg(arguments, VariadicMemoryAggregate);
    double stack_double = __builtin_va_arg(arguments, double);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 +
           (fp0 == 0.0 && fp1 == 0.0 && fp2 == 0.0 && fp3 == 0.0 &&
                    fp4 == 0.0 && fp5 == 0.0 && fp6 == 0.0
                ? 1
                : 0) +
           (int)in_registers.integer + (int)in_registers.floating +
           (int)on_stack.integer + (int)on_stack.floating + (int)memory.first +
           (int)memory.second + (int)memory.third + (int)stack_double;
}

int read_variadic_aggregate_gp_exhausted(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker5);
    VariadicMixedAggregate on_stack =
        __builtin_va_arg(arguments, VariadicMixedAggregate);
    double in_sse_register = __builtin_va_arg(arguments, double);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 + marker5 +
           (int)on_stack.integer + (int)on_stack.floating +
           (int)in_sse_register;
}
