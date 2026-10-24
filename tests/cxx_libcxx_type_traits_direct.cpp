#include "../../../libs/libcxx/type_traits.h"

static_assert(alignof(max_align_t) >= alignof(long long),
              "RinOS max_align_t must cover supported scalar alignment");
static_assert(alignof(max_align_t) >= alignof(double),
              "RinOS max_align_t must cover floating scalar alignment");

struct ExplicitOnly {
    explicit ExplicitOnly(int) {}
};

struct NothrowValue {
    NothrowValue(int) noexcept {}
};

struct ThrowingValue {
    ThrowingValue(int) {}
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

void accept_nothrow_value(NothrowValue);

static_assert(std::is_convertible<int, long>::value,
              "arithmetic conversion must be detected");
static_assert(!std::is_convertible<int, ExplicitOnly>::value,
              "explicit constructors are not implicit conversions");
static_assert(std::is_convertible<int, NothrowValue>::value,
              "nothrow constructor must still be an implicit conversion");
static_assert(noexcept(NothrowValue(0)),
              "direct nothrow constructor expression must be non-throwing");
static_assert(std::is_nothrow_convertible<int, NothrowValue>::value,
              "nothrow constructor conversion must be detected");
static_assert(!std::is_nothrow_convertible<int, ThrowingValue>::value,
              "throwing constructor conversion must not be nothrow");
static_assert(std::is_convertible<void, void>::value,
              "void specialisation must remain available");
static_assert(std::is_constructible<int>::value,
              "scalar value-initialization must be constructible");
static_assert(std::is_constructible<int, double>::value,
              "scalar conversions must be constructible");
static_assert(std::is_constructible<ExplicitOnly, int>::value,
              "direct construction may select an explicit constructor");
static_assert(!std::is_constructible<DeletedValue, int>::value,
              "deleted constructors are not constructible");
static_assert(!std::is_constructible<DeletedPreferredValue, int>::value,
              "a deleted best match blocks worse conversions");
static_assert(!std::is_constructible<PrivatePreferredValue, int>::value,
              "an inaccessible best match blocks worse conversions");
static_assert(!std::is_constructible<AmbiguousValue, int>::value,
              "ambiguous overloads are not constructible");
static_assert(!std::is_constructible<PrivateValue, int>::value,
              "private constructors are inaccessible to the trait");
static_assert(std::is_constructible<ReferenceValue, int&>::value,
              "reference parameters must bind lvalue trait arguments");
static_assert(!std::is_constructible<ReferenceValue, int>::value,
              "reference parameters must reject non-binding rvalues");
static_assert(std::is_constructible<AggregateValue>::value,
              "default aggregate initialization must be constructible");
static_assert(std::is_constructible<AggregateValue, int>::value,
              "C++20 parenthesized aggregate initialization must be modeled");
static_assert(std::is_constructible<int[2]>::value,
              "bounded array value initialization must be constructible");
static_assert(std::is_constructible<int[2], int, int>::value,
              "C++20 parenthesized array initialization must be modeled");
static_assert(std::is_constructible<PublicDestructorValue, int>::value,
              "a public nontrivial destructor permits construction");
static_assert(!std::is_constructible<PrivateDestructorValue, int>::value,
              "an inaccessible destructor prevents construction");
static_assert(std::is_constructible<ImplicitNontrivialCopy,
                                   const ImplicitNontrivialCopy&>::value,
              "implicit copy construction accepts a nontrivial member copy");
static_assert(std::is_constructible<ImplicitNontrivialCopy,
                                   ImplicitNontrivialCopy&&>::value,
              "implicit move may copy a member without a move constructor");
static_assert(std::is_constructible<OrdinaryConstructorWithImplicitCopy,
                                   const OrdinaryConstructorWithImplicitCopy&>::value,
              "ordinary constructors do not suppress implicit copy");
static_assert(std::is_constructible<ImplicitNontrivialMove,
                                   ImplicitNontrivialMove&&>::value,
              "implicit move accepts a nontrivial member move");
static_assert(!std::is_constructible<ImplicitNontrivialMove,
                                    const ImplicitNontrivialMove&>::value,
              "a member move constructor deletes the implicit enclosing copy");
static_assert(!std::is_constructible<ImplicitDeletedCopy,
                                    const ImplicitDeletedCopy&>::value,
              "a deleted member copy deletes the implicit enclosing copy");
static_assert(!std::is_constructible<ImplicitDeletedCopy,
                                    ImplicitDeletedCopy&&>::value,
              "a deleted member copy also blocks implicit move construction");
static_assert(std::is_constructible<ImplicitMutableCopy,
                                   ImplicitMutableCopy&>::value,
              "implicit copy can use a non-const copy parameter");
static_assert(!std::is_constructible<ImplicitMutableCopy,
                                    const ImplicitMutableCopy&>::value,
              "non-const subobject copy prevents a const enclosing copy");
static_assert(!std::is_constructible<ImplicitMutableCopy,
                                    ImplicitMutableCopy&&>::value,
              "non-const lvalue copy cannot bind the enclosing rvalue");
static_assert(std::is_constructible<UserDestructorCopy,
                                   const UserDestructorCopy&>::value,
              "a user destructor does not suppress implicit copy construction");
static_assert(std::is_constructible<UserDestructorCopy,
                                   UserDestructorCopy&&>::value,
              "a suppressed implicit move falls back to the copy constructor");

int cxx_libcxx_type_traits_direct_probe() {
    accept_nothrow_value(0);
    return std::is_convertible<int, long>::value &&
                   !std::is_convertible<int, ExplicitOnly>::value &&
                   std::is_nothrow_convertible<int, NothrowValue>::value &&
                   !std::is_nothrow_convertible<int, ThrowingValue>::value &&
                   std::is_constructible<ExplicitOnly, int>::value &&
                   !std::is_constructible<DeletedValue, int>::value
               ? 0
               : 1;
}
