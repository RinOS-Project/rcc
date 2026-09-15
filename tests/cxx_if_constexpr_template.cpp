template<int N>
int select_positive(int value) {
    if constexpr (N > 0) {
        return value + N;
    } else {
        return no_such_symbol(value);
    }
}

template<int N>
int select_nonpositive(int value) {
    if constexpr (N <= 0) {
        return value - N;
    } else {
        return another_missing_symbol(value);
    }
}

int main(void) {
    return select_positive<3>(4) == 7 &&
                   select_nonpositive<-2>(5) == 7
               ? 0
               : 1;
}
