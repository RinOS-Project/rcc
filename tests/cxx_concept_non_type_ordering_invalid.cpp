template<int N>
concept Positive = N > 0;

template<int N>
requires Positive<N>
int select_positive(int value) {
    return value + 1;
}

template<int N>
requires (Positive<N> && N < 10)
int select_positive(int value) {
    return value + 2;
}

int main() {
    return select_positive<0>(0);
}
