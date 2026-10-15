template <long long Extent>
struct DependentArrayAddOverflow {
    int elements[Extent + 9223372036854775807LL];
};

template <long long Extent>
struct DependentArraySubtractOverflow {
    int elements[Extent - 9223372036854775807LL];
};

template <long long Extent>
struct DependentArrayMultiplyOverflow {
    int elements[Extent * 9223372036854775807LL];
};

template <long long Value>
struct DependentArrayNegateOverflow {
    int elements[-Value];
};

template <long long Dividend>
struct DependentArrayDivideOverflow {
    int elements[Dividend / -1LL];
};

template <long long Dividend>
struct DependentArrayRemainderOverflow {
    int elements[Dividend % -1LL];
};

static_assert(sizeof(DependentArrayAddOverflow<1>) > 0);
static_assert(sizeof(DependentArraySubtractOverflow<-2>) > 0);
static_assert(sizeof(DependentArrayMultiplyOverflow<2>) > 0);
static_assert(sizeof(DependentArrayNegateOverflow<
    -9223372036854775807LL - 1LL>) > 0);
static_assert(sizeof(DependentArrayDivideOverflow<
    -9223372036854775807LL - 1LL>) > 0);
static_assert(sizeof(DependentArrayRemainderOverflow<
    -9223372036854775807LL - 1LL>) > 0);
