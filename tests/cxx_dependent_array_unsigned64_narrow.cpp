template <long long Extent>
struct Unsigned64NarrowedDependentArrayBound {
    int elements[static_cast<unsigned char>(
        static_cast<unsigned long long>(Extent))];
};

template <long long Extent>
struct Unsigned64NegatedDependentArrayBound {
    int elements[-static_cast<unsigned long long>(Extent)];
};

template <long long Extent>
struct Unsigned64ComparedDependentArrayBound {
    int elements[static_cast<unsigned long long>(Extent) > 0ULL];
};

template <long long Extent>
struct Unsigned64NarrowedArithmeticArrayBound {
    int elements[static_cast<unsigned char>(
                     static_cast<unsigned long long>(Extent)) + 1];
};

static_assert(sizeof(Unsigned64NarrowedDependentArrayBound<-1>) ==
              255u * sizeof(int));
static_assert(sizeof(Unsigned64NegatedDependentArrayBound<-1>) == sizeof(int));
static_assert(sizeof(Unsigned64ComparedDependentArrayBound<-1>) == sizeof(int));
static_assert(sizeof(Unsigned64NarrowedArithmeticArrayBound<-1>) ==
              256u * sizeof(int));
