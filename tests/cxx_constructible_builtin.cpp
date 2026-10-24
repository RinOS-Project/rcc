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

struct NontrivialCopyMember {
    NontrivialCopyMember() {}
    NontrivialCopyMember(const NontrivialCopyMember&) {}
};

struct ImplicitNontrivialCopy {
    NontrivialCopyMember member;
};

struct OrdinaryConstructorWithImplicitCopy {
    explicit OrdinaryConstructorWithImplicitCopy(int) {}
    int value;
};

struct NontrivialMoveMember {
    NontrivialMoveMember() {}
    NontrivialMoveMember(NontrivialMoveMember&&) {}
};

struct ImplicitNontrivialMove {
    NontrivialMoveMember member;
};

struct DeletedCopyMember {
    DeletedCopyMember() {}
    DeletedCopyMember(const DeletedCopyMember&) = delete;
};

struct ImplicitDeletedCopy {
    DeletedCopyMember member;
};

struct MutableCopyMember {
    MutableCopyMember() {}
    MutableCopyMember(MutableCopyMember&) {}
};

struct ImplicitMutableCopy {
    MutableCopyMember member;
};

struct UserDestructorCopy {
    ~UserDestructorCopy() {}
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
static_assert(__is_constructible(ImplicitNontrivialCopy,
                                const ImplicitNontrivialCopy&),
              "implicit copy construction accepts a nontrivial member copy");
static_assert(__is_constructible(ImplicitNontrivialCopy,
                                ImplicitNontrivialCopy&&),
              "implicit move may copy a member without a move constructor");
static_assert(__is_constructible(OrdinaryConstructorWithImplicitCopy,
                                const OrdinaryConstructorWithImplicitCopy&),
              "ordinary constructors do not suppress implicit copy");
static_assert(__is_constructible(ImplicitNontrivialMove,
                                ImplicitNontrivialMove&&),
              "implicit move construction accepts a nontrivial member move");
static_assert(!__is_constructible(ImplicitNontrivialMove,
                                 const ImplicitNontrivialMove&),
              "a member move constructor deletes the implicit enclosing copy");
static_assert(!__is_constructible(ImplicitDeletedCopy,
                                 const ImplicitDeletedCopy&),
              "a deleted member copy deletes the implicit enclosing copy");
static_assert(!__is_constructible(ImplicitDeletedCopy,
                                 ImplicitDeletedCopy&&),
              "a deleted member copy also blocks implicit move construction");
static_assert(__is_constructible(ImplicitMutableCopy, ImplicitMutableCopy&),
              "implicit copy can use a non-const copy parameter");
static_assert(!__is_constructible(ImplicitMutableCopy,
                                  const ImplicitMutableCopy&),
              "non-const subobject copy prevents a const enclosing copy");
static_assert(!__is_constructible(ImplicitMutableCopy,
                                  ImplicitMutableCopy&&),
              "non-const lvalue copy cannot bind the enclosing rvalue");
static_assert(__is_constructible(UserDestructorCopy,
                                const UserDestructorCopy&),
              "a user destructor does not suppress implicit copy construction");
static_assert(__is_constructible(UserDestructorCopy, UserDestructorCopy&&),
              "a suppressed implicit move falls back to the copy constructor");
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
