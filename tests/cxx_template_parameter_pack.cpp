template<typename... Ts>
int arity(Ts...) {
    return sizeof...(Ts);
}

template<typename... Ts>
int sum(Ts... args) {
    return (... + args);
}

template<typename... Ts>
int sum_right(Ts... args) {
    return (args + ...);
}

template<typename... Ts>
int all(Ts... args) {
    return (... && args);
}

template<typename... Ts>
int any(Ts... args) {
    return (args || ...);
}

template<typename... Ts>
int sum_left_seed(Ts... args) {
    return (10 + ... + args);
}

template<typename... Ts>
int sum_right_seed(Ts... args) {
    return (args + ... + 10);
}

int main(void) {
    return arity(1, 2, 3) == 3 && arity() == 0 &&
           arity<int, long>(1, 2) == 2 && sum(1, 2, 3) == 6 &&
           sum_right(1, 2, 3) == 6 && all(1, 1, 1) == 1 &&
           all() == 1 && any(0, 0, 1) == 1 && any() == 0 &&
           sum_left_seed() == 10 && sum_left_seed(1, 2, 3) == 16 &&
           sum_right_seed() == 10 && sum_right_seed(1, 2, 3) == 16 ? 0 : 1;
}
