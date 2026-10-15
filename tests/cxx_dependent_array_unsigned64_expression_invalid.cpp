template <long long Extent>
struct Unsigned64AddedDependentArrayBound {
    int elements[static_cast<unsigned long long>(Extent) + 0ULL];
};

template <long long Extent>
struct Unsigned64SubtractedDependentArrayBound {
    int elements[static_cast<unsigned long long>(Extent) - 1ULL];
};

template <long long Extent>
struct Unsigned64WrappedDependentArrayBound {
    int elements[static_cast<unsigned long long>(Extent) + 1ULL];
};

static_assert(sizeof(Unsigned64AddedDependentArrayBound<-1>) > 0);
static_assert(sizeof(Unsigned64SubtractedDependentArrayBound<-1>) > 0);
static_assert(sizeof(Unsigned64WrappedDependentArrayBound<-1>) > 0);
