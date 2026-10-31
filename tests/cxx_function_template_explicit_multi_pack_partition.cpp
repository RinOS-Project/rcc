template<int Value>
struct Constant {};

template<typename... Types, int Bias = 7, int... Values>
int choose_explicit_multi_pack_partition(Types... first,
                                         Constant<Values>... rest) {
    (static_cast<void>(first), ...);
    (static_cast<void>(rest), ...);
    return Bias * 100 + static_cast<int>(sizeof...(Types)) * 10 +
           (0 + ... + Values);
}

template<int Prefix, typename... Types, int Bias = 7, int... Values>
int choose_prefixed_explicit_multi_pack_partition(Types... first,
                                                  Constant<Values>... rest) {
    (static_cast<void>(first), ...);
    (static_cast<void>(rest), ...);
    return Prefix * 1000 + Bias * 100 +
           static_cast<int>(sizeof...(Types)) * 10 +
           (0 + ... + Values);
}

int main() {
#if defined(RCC_TEST_INVALID_PREFIXED_PARTITION)
    return choose_prefixed_explicit_multi_pack_partition<5, int, char>(
        1, Constant<3>{});
#elif defined(RCC_TEST_INVALID_EXPLICIT_PARTITION)
    return choose_explicit_multi_pack_partition<int, char>(
        1, Constant<3>{});
#else
    return choose_explicit_multi_pack_partition<int, char>(
               1, 'x', Constant<3>{}, Constant<4>{}) == 727 &&
                   choose_explicit_multi_pack_partition<>(Constant<5>{}) ==
                       705 &&
                   choose_prefixed_explicit_multi_pack_partition<5, int,
                                                                  char>(
                       1, 'x', Constant<3>{}, Constant<4>{}) == 5727 &&
                   choose_prefixed_explicit_multi_pack_partition<3>(
                       Constant<5>{}) == 3705
        ? 0 : 1;
#endif
}
