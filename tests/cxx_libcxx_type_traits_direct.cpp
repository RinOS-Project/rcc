#include "../../../libs/libcxx/type_traits.h"

static_assert(alignof(max_align_t) >= alignof(long long),
              "RinOS max_align_t must cover supported scalar alignment");
static_assert(alignof(max_align_t) >= alignof(double),
              "RinOS max_align_t must cover floating scalar alignment");

struct ExplicitOnly {
    explicit ExplicitOnly(int) {}
};

struct TemplateConstructorValue {
    template <typename T>
    TemplateConstructorValue(T&&) {}
};

struct ConstrainedTemplateConstructorValue {
    template <typename T>
        requires (sizeof(T) == sizeof(int))
    ConstrainedTemplateConstructorValue(T&&) {}
};

struct DefaultedTemplateConstructorValue {
    template <typename T>
    DefaultedTemplateConstructorValue(T&&, int = 0) {}
};

struct PartialOrderTemplateConstructorValue {
    template <typename T>
    PartialOrderTemplateConstructorValue(T*) = delete;

    template <typename T>
    PartialOrderTemplateConstructorValue(T) {}
};

struct InheritedConstructorBase {
    explicit InheritedConstructorBase(int value) { (void)value; }
    InheritedConstructorBase(int left, int right) {
        (void)left;
        (void)right;
    }
};

struct InheritedConstructorValue : InheritedConstructorBase {
    using InheritedConstructorBase::InheritedConstructorBase;
};

struct DeletedTemplateCopyMember {
    DeletedTemplateCopyMember(const DeletedTemplateCopyMember&) = delete;
};

struct TemplateConstructorWithDeletedImplicitCopy {
    DeletedTemplateCopyMember member;

    template <typename T>
    TemplateConstructorWithDeletedImplicitCopy(T&&) {}
};

struct DeletedForwardingConstructorWithSuppressedMove {
    ~DeletedForwardingConstructorWithSuppressedMove() {}

    template <typename T>
    DeletedForwardingConstructorWithSuppressedMove(T&&) = delete;
};

struct DeletedTemplatePreferredValue {
    DeletedTemplatePreferredValue(long) {}

    template <typename T>
    DeletedTemplatePreferredValue(T&&) = delete;
};

struct PrivateTemplatePreferredValue {
    PrivateTemplatePreferredValue(long) {}

private:
    template <typename T>
    PrivateTemplatePreferredValue(T&&) {}
};

struct NonTemplatePreferredValue {
    NonTemplatePreferredValue(int) {}

    template <typename T>
    NonTemplatePreferredValue(T&&) = delete;
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

struct NontrivialCopyBase {
    NontrivialCopyBase() {}
    NontrivialCopyBase(const NontrivialCopyBase&) {}
};

struct DerivedWithImplicitCopy : NontrivialCopyBase {};

struct ImplicitArrayCopy {
    NontrivialCopyMember members[2];
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

struct DeletedArrayCopy {
    DeletedCopyMember members[2];
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
static_assert(std::is_constructible<TemplateConstructorValue, int>::value,
              "constructor templates deduce an rvalue-reference parameter");
static_assert(std::is_constructible<TemplateConstructorValue, int&>::value,
              "constructor templates deduce lvalue-reference collapsing");
static_assert(std::is_constructible<ConstrainedTemplateConstructorValue,
                                    int>::value,
              "constructor-template constraints are evaluated after deduction");
static_assert(!std::is_constructible<ConstrainedTemplateConstructorValue,
                                     long long>::value,
              "a non-satisfied constructor-template constraint removes the candidate");
static_assert(std::is_constructible<DefaultedTemplateConstructorValue,
                                    int>::value,
              "constructor-template default arguments affect viable arity");
static_assert(!std::is_constructible<PartialOrderTemplateConstructorValue,
                                    int*>::value,
              "constructor-template partial ordering selects the pointer pattern");
static_assert(std::is_constructible<InheritedConstructorValue, int>::value,
              "inherited constructors participate in constructibility");
static_assert(std::is_constructible<InheritedConstructorValue,
                                    int, int>::value,
              "inherited constructor overloads retain their arity");
static_assert(!std::is_constructible<InheritedConstructorValue,
                                     int, int, int>::value,
              "inherited constructors reject excess arguments");
static_assert(std::is_constructible<TemplateConstructorValue,
                                    const TemplateConstructorValue&>::value,
              "the implicit copy constructor remains available beside a template");
static_assert(!std::is_constructible<
                  TemplateConstructorWithDeletedImplicitCopy,
                  const TemplateConstructorWithDeletedImplicitCopy&>::value,
              "a deleted implicit copy constructor is not bypassed by a template");
static_assert(!std::is_constructible<
                  DeletedForwardingConstructorWithSuppressedMove,
                  DeletedForwardingConstructorWithSuppressedMove&&>::value,
              "a forwarding template beats copy when a destructor suppresses move");
static_assert(!std::is_constructible<DeletedTemplatePreferredValue,
                                    int>::value,
              "a deleted constructor-template best match blocks conversion fallback");
static_assert(!std::is_constructible<PrivateTemplatePreferredValue,
                                    int>::value,
              "an inaccessible constructor-template best match is rejected");
static_assert(std::is_constructible<NonTemplatePreferredValue, int>::value,
              "a non-template constructor wins an otherwise equal tie");
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
static_assert(std::is_constructible<DerivedWithImplicitCopy,
                                   const DerivedWithImplicitCopy&>::value,
              "implicit copy construction checks nontrivial base copies");
static_assert(std::is_constructible<ImplicitArrayCopy,
                                   const ImplicitArrayCopy&>::value,
              "implicit copy construction checks array element copies");
static_assert(!std::is_constructible<DeletedArrayCopy,
                                    const DeletedArrayCopy&>::value,
              "a deleted array-element copy deletes the enclosing copy");
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
