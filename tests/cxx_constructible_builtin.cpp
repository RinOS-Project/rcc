struct ExplicitValue {
    explicit ExplicitValue(int) {}
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

template <typename T, typename... Args>
struct ConstructiblePackProbe {
    static constexpr bool value = __is_constructible(T, Args...);
};

static_assert(__is_constructible(int), "scalar value initialization");
static_assert(__is_constructible(int, double), "scalar conversion");
static_assert(__is_constructible(ExplicitValue, int),
              "direct initialization may select an explicit constructor");
static_assert(__is_constructible(TemplateConstructorValue, int),
              "constructor templates deduce an rvalue-reference parameter");
static_assert(__is_constructible(TemplateConstructorValue, int&),
              "constructor templates deduce lvalue-reference collapsing");
static_assert(__is_constructible(ConstrainedTemplateConstructorValue, int),
              "constructor-template constraints are evaluated after deduction");
static_assert(!__is_constructible(ConstrainedTemplateConstructorValue,
                                 long long),
              "a non-satisfied constructor-template constraint removes the candidate");
static_assert(__is_constructible(DefaultedTemplateConstructorValue, int),
              "constructor-template default arguments affect viable arity");
static_assert(!__is_constructible(PartialOrderTemplateConstructorValue,
                                 int*),
              "constructor-template partial ordering selects the pointer pattern");
static_assert(__is_constructible(InheritedConstructorValue, int),
              "inherited constructors participate in constructibility");
static_assert(__is_constructible(InheritedConstructorValue, int, int),
              "inherited constructor overloads retain their arity");
static_assert(!__is_constructible(InheritedConstructorValue, int, int, int),
              "inherited constructors reject excess arguments");
static_assert(__is_constructible(TemplateConstructorValue,
                                const TemplateConstructorValue&),
              "the implicit copy constructor remains available beside a template");
static_assert(!__is_constructible(
                  TemplateConstructorWithDeletedImplicitCopy,
                  const TemplateConstructorWithDeletedImplicitCopy&),
              "a deleted implicit copy constructor is not bypassed by a template");
static_assert(!__is_constructible(
                  DeletedForwardingConstructorWithSuppressedMove,
                  DeletedForwardingConstructorWithSuppressedMove&&),
              "a forwarding template beats copy when a destructor suppresses move");
static_assert(!__is_constructible(DeletedTemplatePreferredValue, int),
              "a deleted constructor-template best match blocks conversion fallback");
static_assert(!__is_constructible(PrivateTemplatePreferredValue, int),
              "an inaccessible constructor-template best match is rejected");
static_assert(__is_constructible(NonTemplatePreferredValue, int),
              "a non-template constructor wins an otherwise equal tie");
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
static_assert(__is_constructible(DerivedWithImplicitCopy,
                                const DerivedWithImplicitCopy&),
              "implicit copy construction checks nontrivial base copies");
static_assert(__is_constructible(ImplicitArrayCopy, const ImplicitArrayCopy&),
              "implicit copy construction checks array element copies");
static_assert(!__is_constructible(DeletedArrayCopy, const DeletedArrayCopy&),
              "a deleted array-element copy deletes the enclosing copy");
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
