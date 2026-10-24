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

struct InheritedConstructorTemplateBase {
    template <typename T>
    InheritedConstructorTemplateBase(T value) { (void)value; }

    template <typename T>
    InheritedConstructorTemplateBase(T*) = delete;
};

struct InheritedConstructorTemplateValue : InheritedConstructorTemplateBase {
    using InheritedConstructorTemplateBase::InheritedConstructorTemplateBase;
};

struct InheritedConstructorTemplateMiddle
    : InheritedConstructorTemplateBase {
    using InheritedConstructorTemplateBase::InheritedConstructorTemplateBase;
};

struct InheritedConstructorTemplateLeaf
    : InheritedConstructorTemplateMiddle {
    using InheritedConstructorTemplateMiddle::
        InheritedConstructorTemplateMiddle;
};

struct InheritedConstrainedConstructorTemplateBase {
    template <typename T>
        requires (sizeof(T) == sizeof(int))
    InheritedConstrainedConstructorTemplateBase(T value) { (void)value; }
};

struct InheritedConstrainedConstructorTemplateValue
    : InheritedConstrainedConstructorTemplateBase {
    using InheritedConstrainedConstructorTemplateBase::
        InheritedConstrainedConstructorTemplateBase;
};

struct InheritedPrivateBestBase {
    InheritedPrivateBestBase(int value) { (void)value; }

private:
    InheritedPrivateBestBase(short value) { (void)value; }
};

struct InheritedPrivateBest : InheritedPrivateBestBase {
    using InheritedPrivateBestBase::InheritedPrivateBestBase;
};

struct InheritedDeletedBestBase {
    InheritedDeletedBestBase(int value) { (void)value; }
    InheritedDeletedBestBase(short) = delete;
};

struct InheritedDeletedBest : InheritedDeletedBestBase {
    using InheritedDeletedBestBase::InheritedDeletedBestBase;
};

struct InheritedBaseCopyBase {
    InheritedBaseCopyBase(int value) { (void)value; }
    InheritedBaseCopyBase(const InheritedBaseCopyBase&) = delete;
};

struct InheritedBaseCopyValue : InheritedBaseCopyBase {
    using InheritedBaseCopyBase::InheritedBaseCopyBase;
};

struct AssignableFromInt {
    AssignableFromInt& operator=(int value) { (void)value; return *this; }
};

struct DeletedCopyAssignment {
    DeletedCopyAssignment& operator=(const DeletedCopyAssignment&) = delete;
};

struct PrivateIntAssignment {
private:
    PrivateIntAssignment& operator=(int value) { (void)value; return *this; }
};

struct ImplicitlyAssignable {
    int value;
};

struct ConstMemberNotAssignable {
    const int value;
};

struct ReferenceMemberNotAssignable {
    int& value;
};

struct LvalueOnlyCopyAssignment {
    LvalueOnlyCopyAssignment& operator=(LvalueOnlyCopyAssignment&);
};

struct LvalueOnlyCopySubobject {
    LvalueOnlyCopyAssignment value;
};

struct DefaultedConstCopyAssignment {
    const int value;
    DefaultedConstCopyAssignment& operator=(
        const DefaultedConstCopyAssignment&) = default;
};

struct ImplicitMoveAssignment {
    int value;
};

struct RvalueQualifiedAssignment {
    RvalueQualifiedAssignment& operator=(
        const RvalueQualifiedAssignment&) &&;
};

struct AssignmentBase {};

struct DeletedBestAssignment : AssignmentBase {
    DeletedBestAssignment& operator=(
        const DeletedBestAssignment&) = delete;
    DeletedBestAssignment& operator=(const AssignmentBase&);
};

struct NontrivialUnionMemberAssignment {
    NontrivialUnionMemberAssignment& operator=(
        const NontrivialUnionMemberAssignment&);
};

struct TemplateAssignment {
    template <typename T>
    TemplateAssignment& operator=(T&&);
};

using IntArray = int[2];
using FunctionType = int();

template <typename Target, typename Source>
struct StandardAssignmentProbe : std::is_assignable<Target, Source> {};

union ScalarAssignmentUnion {
    int value;
    double other;
};

union NontrivialAssignmentUnion {
    NontrivialUnionMemberAssignment value;
    int other;
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

int nonthrowing_trait_default() noexcept;
int throwing_trait_default();

namespace nothrow_trait_calls {
int qualified_nonthrowing() noexcept;
int qualified_throwing();
int select_overload(int) noexcept;
int select_overload(long);
}

static_assert(noexcept(nonthrowing_trait_default()),
              "declared noexcept call remains visible to the expression parser");
static_assert(!noexcept(throwing_trait_default()),
              "unannotated call remains potentially throwing");

struct NothrowDefaultArgumentValue {
    NothrowDefaultArgumentValue(
        int value = nonthrowing_trait_default()) noexcept {
        (void)value;
    }
};

struct ThrowingDefaultArgumentValue {
    ThrowingDefaultArgumentValue(
        int value = throwing_trait_default()) noexcept {
        (void)value;
    }
};

struct NothrowDefaultMemberInitializerValue {
    int value = nonthrowing_trait_default();
};

struct ThrowingDefaultMemberInitializerValue {
    int value = throwing_trait_default();
};

struct NothrowOverloadedDefaultArgumentValue {
    NothrowOverloadedDefaultArgumentValue(
        int value = nothrow_trait_calls::select_overload(0)) noexcept {
        (void)value;
    }
};

struct ThrowingOverloadedDefaultArgumentValue {
    ThrowingOverloadedDefaultArgumentValue(
        int value = nothrow_trait_calls::select_overload(0L)) noexcept {
        (void)value;
    }
};

struct NothrowQualifiedDefaultMemberInitializerValue {
    int value = nothrow_trait_calls::qualified_nonthrowing();
};

struct ThrowingQualifiedDefaultMemberInitializerValue {
    int value = nothrow_trait_calls::qualified_throwing();
};

struct NothrowDefaultValue {
    NothrowDefaultValue() noexcept {}
};

struct ThrowingDefaultValue {
    ThrowingDefaultValue() {}
};

struct DefaultedNothrowMember {
    NothrowDefaultValue value;
};

struct DefaultedThrowingMember {
    ThrowingDefaultValue value;
};

struct ThrowingDestructorValue {
    ThrowingDestructorValue() noexcept {}
    ~ThrowingDestructorValue() noexcept(false) {}
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

template <typename T>
struct NothrowConstructibleTraitProbe {
    static constexpr bool value = std::is_nothrow_constructible<T>::value;
};

template <typename T, typename... Args>
struct NothrowConstructiblePackTraitProbe {
    static constexpr bool value =
        std::is_nothrow_constructible<T, Args...>::value;
};

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
static_assert(std::is_nothrow_constructible<NothrowValue, int>::value,
              "direct construction must honor a noexcept constructor");
static_assert(!std::is_nothrow_constructible<ThrowingValue, int>::value,
              "direct construction must honor a throwing constructor");
static_assert(
    std::is_nothrow_constructible<NothrowDefaultArgumentValue>::value,
    "libcxx trait must fold a non-throwing call in a default argument");
static_assert(
    !std::is_nothrow_constructible<ThrowingDefaultArgumentValue>::value,
    "libcxx trait must fold a throwing call in a default argument");
static_assert(std::is_nothrow_constructible<
                  NothrowDefaultMemberInitializerValue>::value,
              "libcxx trait must fold a non-throwing call in a DMI");
static_assert(!std::is_nothrow_constructible<
                  ThrowingDefaultMemberInitializerValue>::value,
              "libcxx trait must fold a throwing call in a DMI");
static_assert(std::is_nothrow_constructible<
                  NothrowOverloadedDefaultArgumentValue>::value,
              "libcxx trait must follow the selected non-throwing overload");
static_assert(!std::is_nothrow_constructible<
                  ThrowingOverloadedDefaultArgumentValue>::value,
              "libcxx trait must follow the selected throwing overload");
static_assert(std::is_nothrow_constructible<
                  NothrowQualifiedDefaultMemberInitializerValue>::value,
              "libcxx trait must resolve a qualified noexcept call in a DMI");
static_assert(!std::is_nothrow_constructible<
                  ThrowingQualifiedDefaultMemberInitializerValue>::value,
              "libcxx trait must resolve a qualified throwing call in a DMI");
static_assert(NothrowConstructibleTraitProbe<NothrowDefaultValue>::value,
              "dependent libcxx nothrow constructibility retains true");
static_assert(!NothrowConstructibleTraitProbe<ThrowingDefaultValue>::value,
              "dependent libcxx nothrow constructibility retains false");
static_assert(NothrowConstructiblePackTraitProbe<NothrowValue, int>::value,
              "dependent libcxx nothrow packs retain true");
static_assert(!NothrowConstructiblePackTraitProbe<ThrowingValue, int>::value,
              "dependent libcxx nothrow packs retain false");
static_assert(std::is_nothrow_constructible<DefaultedNothrowMember>::value,
              "implicit aggregate construction propagates member noexcept");
static_assert(!std::is_nothrow_constructible<DefaultedThrowingMember>::value,
              "implicit aggregate construction propagates member throws");
static_assert(!std::is_nothrow_constructible<ThrowingDestructorValue>::value,
              "temporary destruction participates in nothrow construction");
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
static_assert(!std::is_constructible<InheritedPrivateBest, short>::value,
              "an inaccessible inherited best match blocks weaker overloads");
static_assert(!std::is_constructible<InheritedDeletedBest, short>::value,
              "a deleted inherited best match blocks weaker overloads");
static_assert(std::is_constructible<InheritedPrivateBest, int>::value,
              "a public inherited overload remains usable");
static_assert(std::is_constructible<InheritedDeletedBest, int>::value,
              "a non-deleted inherited overload remains usable");
static_assert(!std::is_constructible<InheritedBaseCopyValue,
                                     const InheritedBaseCopyBase&>::value,
              "base copy constructors are not inherited");
static_assert(std::is_constructible<InheritedConstructorTemplateValue,
                                   int>::value,
              "inherited constructor templates deduce their arguments");
static_assert(!std::is_constructible<InheritedConstructorTemplateValue,
                                     int*>::value,
              "a deleted inherited template best match remains deleted");
static_assert(std::is_constructible<InheritedConstructorTemplateLeaf,
                                   int>::value,
              "constructor templates inherit through a public base chain");
static_assert(!std::is_constructible<InheritedConstructorTemplateLeaf,
                                     int*>::value,
              "deleted overloads remain deleted through an inherited chain");
static_assert(std::is_constructible<
                  InheritedConstrainedConstructorTemplateValue,
                  int>::value,
              "inherited constructor-template constraints are substituted");
static_assert(!std::is_constructible<
                  InheritedConstrainedConstructorTemplateValue,
                  double>::value,
              "unsatisfied inherited constructor constraints are rejected");
static_assert(std::is_assignable<int&, int>::value,
              "scalar lvalue assignment");
static_assert(!std::is_assignable<int, int>::value,
              "a scalar prvalue is not assignable");
static_assert(!std::is_assignable<const int&, int>::value,
              "a const scalar lvalue is not assignable");
static_assert(std::is_assignable<int&, short>::value,
              "scalar assignment conversion");
static_assert(std::is_assignable<AssignableFromInt&, int>::value,
              "user-defined assignment overload");
static_assert(!std::is_assignable<DeletedCopyAssignment&,
                                  const DeletedCopyAssignment&>::value,
              "deleted assignment overload");
static_assert(!std::is_assignable<PrivateIntAssignment&, int>::value,
              "inaccessible assignment overload");
static_assert(std::is_assignable<ImplicitlyAssignable&,
                                 const ImplicitlyAssignable&>::value,
              "implicit copy assignment");
static_assert(!std::is_assignable<ConstMemberNotAssignable&,
                                  const ConstMemberNotAssignable&>::value,
              "const subobjects delete implicit copy assignment");
static_assert(!std::is_assignable<ReferenceMemberNotAssignable&,
                                  const ReferenceMemberNotAssignable&>::value,
              "reference subobjects delete implicit copy assignment");
static_assert(std::is_assignable<LvalueOnlyCopyAssignment&,
                                 LvalueOnlyCopyAssignment&>::value,
              "non-const copy assignment remains viable for mutable sources");
static_assert(!std::is_assignable<LvalueOnlyCopyAssignment&,
                                  const LvalueOnlyCopyAssignment&>::value,
              "non-const copy assignment rejects const sources");
static_assert(std::is_assignable<LvalueOnlyCopySubobject&,
                                 LvalueOnlyCopySubobject&>::value,
              "implicit copy assignment propagates a non-const source");
static_assert(!std::is_assignable<LvalueOnlyCopySubobject&,
                                  const LvalueOnlyCopySubobject&>::value,
              "implicit copy signature follows non-const subobject copies");
static_assert(!std::is_assignable<DefaultedConstCopyAssignment&,
                                  const DefaultedConstCopyAssignment&>::value,
              "defaulted copy assignment is deleted by a const subobject");
static_assert(std::is_assignable<ImplicitMoveAssignment&,
                                 ImplicitMoveAssignment>::value,
              "implicit move assignment accepts a class prvalue");
static_assert(std::is_assignable<ImplicitMoveAssignment,
                                 const ImplicitMoveAssignment&>::value,
              "class xvalues can be assignment targets");
static_assert(!std::is_assignable<RvalueQualifiedAssignment&,
                                  const RvalueQualifiedAssignment&>::value,
              "rvalue-qualified assignment rejects lvalue targets");
static_assert(std::is_assignable<RvalueQualifiedAssignment,
                                 const RvalueQualifiedAssignment&>::value,
              "rvalue-qualified assignment accepts xvalue targets");
static_assert(!std::is_assignable<DeletedBestAssignment&,
                                  const DeletedBestAssignment&>::value,
              "a deleted best assignment blocks a weaker base overload");
static_assert(std::is_assignable<ScalarAssignmentUnion&,
                                 const ScalarAssignmentUnion&>::value,
              "trivial scalar union assignment is available");
static_assert(!std::is_assignable<NontrivialAssignmentUnion&,
                                  const NontrivialAssignmentUnion&>::value,
              "a non-trivial union member deletes union assignment");
static_assert(std::is_assignable<TemplateAssignment&, int>::value,
              "a member assignment template participates in overload resolution");
static_assert(std::is_assignable<volatile int&, int>::value,
              "a volatile scalar lvalue can be assigned");
static_assert(std::is_assignable<int&, volatile int&>::value,
              "a volatile scalar source can be converted for assignment");
static_assert(!std::is_assignable<IntArray&, IntArray&>::value,
              "built-in arrays are not assignable");
static_assert(!std::is_assignable<FunctionType, FunctionType>::value,
              "function types are not assignable");
static_assert(!std::is_assignable<void, int>::value,
              "void is not an assignment target");
static_assert(StandardAssignmentProbe<int&, int>::value,
              "dependent std::is_assignable is evaluated after substitution");
static_assert(!StandardAssignmentProbe<const int&, int>::value,
              "dependent non-assignability survives trait substitution");
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
