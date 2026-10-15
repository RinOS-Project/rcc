template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentArrayBound {
    int elements[(Value == 18446744073709551615ULL) ? 3 : 1];
};

template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentWrapArrayBound {
    int elements[((Value + 1ULL) == 0ULL) ? 1 : 2];
};

static_assert(sizeof(DirectUnsigned64TemplateArgumentArrayBound<
                  18446744073709551615ULL>) == 3u * sizeof(int));
static_assert(sizeof(DirectUnsigned64TemplateArgumentWrapArrayBound<
                  18446744073709551615ULL>) == sizeof(int));
