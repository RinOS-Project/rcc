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

int read_variadic_integer_pair(
    int marker0, int marker1, int marker2, int marker3, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker3);
    VariadicIntegerPair pair =
        __builtin_va_arg(arguments, VariadicIntegerPair);
    int stack_integer = __builtin_va_arg(arguments, int);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + (int)pair.first +
           (int)pair.second + stack_integer;
}

int read_variadic_integer_pair_gp_exhausted(
    int marker0, int marker1, int marker2, int marker3, int marker4, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker4);
    VariadicIntegerPair pair =
        __builtin_va_arg(arguments, VariadicIntegerPair);
    int remaining_gp_integer = __builtin_va_arg(arguments, int);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 +
           (int)pair.first + (int)pair.second + remaining_gp_integer;
}

int read_variadic_reverse_mixed_gp_exhausted(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, double fp0, double fp1, double fp2, double fp3, double fp4,
    ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, fp4);
    VariadicReverseMixedAggregate aggregate =
        __builtin_va_arg(arguments, VariadicReverseMixedAggregate);
    double sse_register_after_fallback =
        __builtin_va_arg(arguments, double);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 + marker5 +
           (fp0 == 0.0 && fp1 == 0.0 && fp2 == 0.0 && fp3 == 0.0 &&
                    fp4 == 0.0
                ? 1
                : 0) +
           (int)aggregate.floating + (int)aggregate.integer +
           (int)sse_register_after_fallback;
}

int read_variadic_reverse_mixed_sse_exhausted(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    double fp0, double fp1, double fp2, double fp3, double fp4, double fp5,
    double fp6, double fp7, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, fp7);
    VariadicReverseMixedAggregate aggregate =
        __builtin_va_arg(arguments, VariadicReverseMixedAggregate);
    int gp_register_after_fallback = __builtin_va_arg(arguments, int);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 +
           (fp0 == 0.0 && fp1 == 0.0 && fp2 == 0.0 && fp3 == 0.0 &&
                    fp4 == 0.0 && fp5 == 0.0 && fp6 == 0.0 && fp7 == 0.0
                ? 1
                : 0) +
           (int)aggregate.floating + (int)aggregate.integer +
           gp_register_after_fallback;
}

int read_variadic_packed_memory_aggregate(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker5);
    VariadicPackedMemoryAggregate aggregate =
        __builtin_va_arg(arguments, VariadicPackedMemoryAggregate);
    double sse_register_after_memory_argument =
        __builtin_va_arg(arguments, double);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 + marker5 +
           (int)aggregate.tag + (int)aggregate.unaligned_floating +
           (int)sse_register_after_memory_argument;
}

int read_variadic_aligned_aggregate(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker5);
    int spilled_integer = __builtin_va_arg(arguments, int);
    VariadicAlignedMemoryAggregate aligned =
        __builtin_va_arg(arguments, VariadicAlignedMemoryAggregate);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 + marker5 +
           spilled_integer + (int)aligned.first + (int)aligned.second +
           (int)aligned.third + (int)aligned.fourth;
}

int read_variadic_aligned64_aggregate(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker5);
    int spilled_integer = __builtin_va_arg(arguments, int);
    VariadicAligned64MemoryAggregate aligned =
        __builtin_va_arg(arguments, VariadicAligned64MemoryAggregate);
    VariadicMemoryAggregate trailing =
        __builtin_va_arg(arguments, VariadicMemoryAggregate);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 + marker5 +
           spilled_integer + (int)aligned.first + (int)aligned.second +
           (int)aligned.third + (int)aligned.fourth + (int)aligned.fifth +
           (int)aligned.sixth + (int)aligned.seventh + (int)aligned.eighth +
           (int)trailing.first + (int)trailing.second + (int)trailing.third;
}

int read_variadic_sse_aggregate(
    double fp0, double fp1, double fp2, double fp3, double fp4, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, fp4);
    VariadicDoublePair in_registers =
        __builtin_va_arg(arguments, VariadicDoublePair);
    VariadicDoublePair on_stack =
        __builtin_va_arg(arguments, VariadicDoublePair);
    double in_last_sse_register = __builtin_va_arg(arguments, double);
    __builtin_va_end(arguments);

    return (fp0 == 0.0 && fp1 == 0.0 && fp2 == 0.0 && fp3 == 0.0 &&
                    fp4 == 0.0
                ? 1
                : 0) +
           (int)in_registers.first + (int)in_registers.second +
           (int)on_stack.first + (int)on_stack.second +
           (int)in_last_sse_register;
}
