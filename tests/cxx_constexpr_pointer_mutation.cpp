struct ConstexprPointerPair {
    int first;
    int second;
};

constexpr int mutate_pair_through_pointer() {
    ConstexprPointerPair pair{2, 3};
    ConstexprPointerPair* pointer = &pair;
    pointer->first += 4;
    pointer[0].second = (*pointer).first + 1;
    return pair.first + pair.second;
}

constexpr int mutate_array_through_pointer() {
    int values[3] = {1, 2, 3};
    int* pointer = &values[0];
    pointer[1] = pointer[0] + 5;
    *(pointer + 2) = 9;
    return values[1] + values[2];
}

static_assert(mutate_pair_through_pointer() == 13,
              "constexpr aggregate member pointer mutation failed");
static_assert(mutate_array_through_pointer() == 15,
              "constexpr array pointer mutation failed");

extern "C" int probe_constexpr_pointer_mutation(void) {
    return mutate_pair_through_pointer() == 13 &&
                   mutate_array_through_pointer() == 15
               ? 0
               : 1;
}
