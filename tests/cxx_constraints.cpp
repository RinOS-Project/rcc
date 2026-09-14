template<int N> requires (N > 0 && N < 10)
int constrained_add(int value) {
    return value + N;
}

template<int N, int M = N * 2> requires (M >= N)
int constrained_scale(int value) {
    return value + M;
}

int main() {
    return constrained_add<3>(4) == 7 &&
                   constrained_scale<3>(4) == 10
               ? 0
               : 1;
}
