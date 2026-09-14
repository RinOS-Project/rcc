template<int N> requires (N > 0 && N < 10)
int constrained_add_invalid(int value) {
    return value + N;
}

int invalid_constraint_use() {
    return constrained_add_invalid<0>(4);
}
