template<int N>
concept Positive = N > 0;

template<int N>
concept MoreThanOne = Positive<N + 0> && N < 10;

template<int N>
requires Positive<N + 0>
int select_positive(int value) {
    return value + 1;
}

template<int N>
requires MoreThanOne<N>
int select_positive(int value) {
    return value + 2;
}

int main() {
    return select_positive<0>(0);
}
