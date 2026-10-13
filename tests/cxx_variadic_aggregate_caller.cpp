#include "cxx_variadic_aggregate.hpp"

int main() {
    VariadicMixedAggregate first = {11, 1.0};
    VariadicMixedAggregate second = {17, 2.0};
    VariadicMemoryAggregate memory = {19, 23, 29};
    VariadicMixedAggregate gp_exhausted = {31, 4.0};
    VariadicIntegerPair integer_pair = {19, 23};
    VariadicIntegerPair integer_pair_gp_exhausted = {31, 37};
    VariadicReverseMixedAggregate reverse_mixed = {23.0, 29};
    VariadicAlignedMemoryAggregate aligned = {11, 13, 17, 19};
    VariadicAligned64MemoryAggregate aligned64 =
        {2, 3, 5, 7, 11, 13, 17, 19};
    VariadicMemoryAggregate trailing = {23, 29, 31};
    VariadicDoublePair sse_first = {7.0, 11.0};
    VariadicDoublePair sse_second = {13.0, 17.0};

    int registers_result = read_variadic_aggregate_registers(
        1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, first, second,
        memory, 3.0);
    int gp_exhausted_result = read_variadic_aggregate_gp_exhausted(
        1, 0, 0, 0, 0, 0, gp_exhausted, 5.0);
    int integer_pair_result = read_variadic_integer_pair(
        1, 0, 0, 0, integer_pair, 31);
    int integer_pair_gp_exhausted_result =
        read_variadic_integer_pair_gp_exhausted(
            1, 0, 0, 0, 0, integer_pair_gp_exhausted, 41);
    int reverse_mixed_result = read_variadic_reverse_mixed_gp_exhausted(
        1, 0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, reverse_mixed, 31.0);
    int reverse_mixed_sse_exhausted_result =
        read_variadic_reverse_mixed_sse_exhausted(
            1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
            reverse_mixed, 31);
    int aligned_result = read_variadic_aligned_aggregate(
        1, 0, 0, 0, 0, 0, 7, aligned);
    int aligned64_result = read_variadic_aligned64_aggregate(
        1, 0, 0, 0, 0, 0, 7, aligned64, trailing);
    int sse_aggregate_result = read_variadic_sse_aggregate(
        0.0, 0.0, 0.0, 0.0, 0.0, sse_first, sse_second, 19.0);

    if (registers_result != 107) return 1;
    if (gp_exhausted_result != 41) return 2;
    if (integer_pair_result != 74) return 6;
    if (integer_pair_gp_exhausted_result != 110) return 8;
    if (reverse_mixed_result != 85) return 7;
    if (reverse_mixed_sse_exhausted_result != 85) return 9;
    if (aligned_result != 68) return 3;
    if (sse_aggregate_result != 68) return 4;
    if (aligned64_result != 168) return 5;
    return 0;
}
