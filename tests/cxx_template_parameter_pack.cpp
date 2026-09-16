template<typename... Ts>
int arity(Ts...) {
    return sizeof...(Ts);
}

int add_three(int first, int second, int third) {
    return first + second + third;
}

template<typename... Ts>
int forward_three(Ts... args) {
    return add_three(args...);
}

template<int... Ns>
int value_sum() {
    return (10 + ... + Ns);
}

template<int... Ns>
int value_size() {
    return sizeof...(Ns);
}

template<int... Ns>
struct value_pack_class {
    inline static int value = (0 + ... + Ns);
};

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
           forward_three(1, 2, 3) == 6 &&
           value_sum<>() == 10 && value_sum<1, 2, 3>() == 16 &&
           value_size<1, 2, 3>() == 3 && value_size<>() == 0 &&
           value_pack_class<>::value == 0 &&
           value_pack_class<1, 2, 3>::value == 6 &&
           sum_right(1, 2, 3) == 6 && all(1, 1, 1) == 1 &&
           all() == 1 && any(0, 0, 1) == 1 && any() == 0 &&
           sum_left_seed() == 10 && sum_left_seed(1, 2, 3) == 16 &&
           sum_right_seed() == 10 && sum_right_seed(1, 2, 3) == 16 ? 0 : 1;
}
