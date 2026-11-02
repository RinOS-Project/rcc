struct DependentAlignofRecord {
    char tag;
    long long value;
};

template<typename T>
constexpr unsigned dependent_alignof() {
    return alignof(T);
}

template<typename T>
constexpr unsigned dependent_reference_alignof() {
    return alignof(T&);
}

template<typename T>
constexpr unsigned dependent_sizeof() {
    return sizeof(T);
}

template<typename T>
constexpr unsigned dependent_reference_sizeof() {
    return sizeof(T&);
}

static_assert(dependent_alignof<int>() == alignof(int));
static_assert(dependent_alignof<DependentAlignofRecord>() ==
              alignof(DependentAlignofRecord));
static_assert(dependent_reference_alignof<int>() == alignof(int));
static_assert(dependent_sizeof<int>() == sizeof(int));
static_assert(dependent_sizeof<DependentAlignofRecord>() ==
              sizeof(DependentAlignofRecord));
static_assert(dependent_reference_sizeof<int>() == sizeof(int));
