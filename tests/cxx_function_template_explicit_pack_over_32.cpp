template<class... Types>
int count_explicit_pack(Types... values) {
    (void)sizeof...(values);
    return sizeof...(Types);
}

template<class T>
int count_deferred_explicit_pack(T value) {
    return count_explicit_pack<
        int, int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int, int, int, int, int>(
            value, value, value, value, value, value, value, value, value,
            value, value, value, value, value, value, value, value, value,
            value, value, value, value, value, value, value, value, value,
            value, value, value, value, value, value);
}

int main() {
    return count_explicit_pack<
               int, int, int, int, int, int, int, int, int, int, int,
               int, int, int, int, int, int, int, int, int, int,
               int, int, int, int, int, int, int, int, int, int, int, int>(
               1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
               12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
               22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33) == 33 &&
                   count_explicit_pack(
                       1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                       12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                       22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33) == 33 &&
                   count_deferred_explicit_pack(7) == 33
               ? 0
               : 1;
}
