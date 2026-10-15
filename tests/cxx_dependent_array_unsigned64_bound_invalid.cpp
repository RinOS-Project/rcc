template <long long Extent>
struct Unsigned64DependentArrayBound {
    int elements[static_cast<unsigned long long>(Extent)];
};

static_assert(sizeof(Unsigned64DependentArrayBound<-1>) > 0);
