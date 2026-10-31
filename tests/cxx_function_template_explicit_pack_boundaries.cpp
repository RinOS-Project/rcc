template<typename... ExplicitPrefix, typename Middle>
int choose_explicit_prefix_pack_boundary(ExplicitPrefix..., Middle middle) {
    return static_cast<int>(sizeof...(ExplicitPrefix)) * 100 + middle;
}

template<typename... ExplicitPrefix, typename Middle,
         typename... DeducedSuffix>
int choose_explicit_prefix_and_suffix_pack_boundary(
    ExplicitPrefix..., Middle middle, DeducedSuffix...) {
    return static_cast<int>(sizeof...(ExplicitPrefix)) * 100 + middle * 10 +
           static_cast<int>(sizeof...(DeducedSuffix));
}

int main() {
    return choose_explicit_prefix_pack_boundary<>(7) == 7 &&
                   choose_explicit_prefix_pack_boundary<int>(5, 7) == 107 &&
                   choose_explicit_prefix_pack_boundary<int, short>(
                       5, 6, 7) == 207 &&
                   choose_explicit_prefix_and_suffix_pack_boundary<>(7) ==
                       70 &&
                   choose_explicit_prefix_and_suffix_pack_boundary<int>(
                       5, 7, 9L) == 171 &&
                   choose_explicit_prefix_and_suffix_pack_boundary<int, short>(
                       5, 6, 7, 9L, 11.0) == 272
        ? 0 : 1;
}
