#include "cxx_variadic_aggregate.hpp"

int main() {
    VariadicMixedAggregate first = {11, 1.0};
    VariadicMixedAggregate second = {17, 2.0};
    VariadicMemoryAggregate memory = {19, 23, 29};
    VariadicMixedAggregate gp_exhausted = {31, 4.0};

    int registers_result = read_variadic_aggregate_registers(
        1, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, first, second,
        memory, 3.0);
    int gp_exhausted_result = read_variadic_aggregate_gp_exhausted(
        1, 0, 0, 0, 0, 0, gp_exhausted, 5.0);

    return registers_result == 107 && gp_exhausted_result == 41 ? 0 : 1;
}
