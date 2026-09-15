namespace library {
int helper(int value) {
    return value + 1;
}

template<typename T>
int apply(T value) {
    return helper(value);
}
}

int main(void) {
    return library::apply<int>(4) == 5 ? 0 : 1;
}
