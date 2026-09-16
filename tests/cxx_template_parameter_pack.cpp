template<typename... Ts>
int arity(Ts...) {
    return sizeof...(Ts);
}

int main(void) {
    return arity(1, 2, 3) == 3 && arity() == 0 &&
           arity<int, long>(1, 2) == 2 ? 0 : 1;
}
