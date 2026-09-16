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

int main(void) {
    return arity(1, 2, 3) == 3 && arity() == 0 &&
           arity<int, long>(1, 2) == 2 && sum(1, 2, 3) == 6 &&
           sum_right(1, 2, 3) == 6 && all(1, 1, 1) == 1 &&
           all() == 1 && any(0, 0, 1) == 1 && any() == 0 ? 0 : 1;
}
