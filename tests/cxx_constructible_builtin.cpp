struct ExplicitValue {
    explicit ExplicitValue(int) {}
};

struct DeletedValue {
    DeletedValue(int) = delete;
};

struct DeletedPreferredValue {
    DeletedPreferredValue(int) = delete;
    DeletedPreferredValue(long) {}
};

struct PrivatePreferredValue {
private:
    PrivatePreferredValue(int) {}

public:
    PrivatePreferredValue(long) {}
};

struct AmbiguousValue {
    AmbiguousValue(short) {}
    AmbiguousValue(unsigned) {}
};

struct PrivateValue {
private:
    explicit PrivateValue(int) {}
};

struct ReferenceValue {
    explicit ReferenceValue(int&) {}
};

struct AggregateValue {
    int value;
};

struct PublicDestructorValue {
    explicit PublicDestructorValue(int) {}
    ~PublicDestructorValue() {}
};

struct PrivateDestructorValue {
    explicit PrivateDestructorValue(int) {}

private:
    ~PrivateDestructorValue() {}
};

template <typename T, typename... Args>
struct ConstructiblePackProbe {
    static constexpr bool value = __is_constructible(T, Args...);
};

static_assert(__is_constructible(int), "scalar value initialization");
static_assert(__is_constructible(int, double), "scalar conversion");
static_assert(__is_constructible(ExplicitValue, int),
              "direct initialization may select an explicit constructor");
static_assert(!__is_constructible(DeletedValue, int),
              "deleted constructor is not viable");
static_assert(!__is_constructible(DeletedPreferredValue, int),
              "a deleted best match must block worse conversions");
static_assert(!__is_constructible(PrivatePreferredValue, int),
              "an inaccessible best match must block worse conversions");
static_assert(!__is_constructible(AmbiguousValue, int),
              "ambiguous constructor overloads are not constructible");
static_assert(!__is_constructible(PrivateValue, int),
              "private constructor is not accessible");
static_assert(__is_constructible(ReferenceValue, int&),
              "constructor reference binds an lvalue");
static_assert(!__is_constructible(ReferenceValue, int),
              "constructor reference rejects an rvalue");
static_assert(__is_constructible(AggregateValue),
              "aggregate value initialization");
static_assert(__is_constructible(AggregateValue, int),
              "C++20 parenthesized aggregate initialization");
static_assert(__is_constructible(int[2]), "bounded array value initialization");
static_assert(__is_constructible(int[2], int, int),
              "C++20 parenthesized array initialization");
static_assert(__is_constructible(PublicDestructorValue, int),
              "a public nontrivial destructor permits construction");
static_assert(!__is_constructible(PrivateDestructorValue, int),
              "an inaccessible destructor prevents construction");
static_assert(ConstructiblePackProbe<ExplicitValue, int>::value,
              "dependent type packs are substituted before evaluation");
static_assert(!ConstructiblePackProbe<DeletedValue, int>::value,
              "dependent deleted-constructor query is false");
static_assert(!ConstructiblePackProbe<DeletedPreferredValue, int>::value,
              "dependent overload resolution retains deleted best matches");
static_assert(!ConstructiblePackProbe<PrivatePreferredValue, int>::value,
              "dependent overload resolution retains inaccessible best matches");

int cxx_constructible_builtin_probe() {
    return ConstructiblePackProbe<ExplicitValue, int>::value &&
                   !ConstructiblePackProbe<DeletedValue, int>::value
               ? 0
               : 1;
}
