template<typename... Ts>
int arity(Ts...) {
    return sizeof...(Ts);
}

template<typename... Ts>
int sum(Ts... args) {
    return (... + args);
}

int main(void) {
    return arity(1, 2, 3) == 3 && arity() == 0 &&
           arity<int, long>(1, 2) == 2 && sum(1, 2, 3) == 6 ? 0 : 1;
}
