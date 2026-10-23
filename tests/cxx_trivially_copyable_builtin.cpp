#ifndef __RCC__
#include <type_traits>
#endif

enum FixedUnderlyingTypeTrait : unsigned short {
    FixedUnderlyingTypeTraitValue
};

enum class DefaultUnderlyingTypeTrait {
    DefaultUnderlyingTypeTraitValue
};

enum OpaqueUnderlyingTypeTrait : unsigned char;

template<typename T>
struct UnderlyingTypeTraitIdentity {
    using type = T;
};

using FixedUnderlyingTypeTraitResult =
    __underlying_type(FixedUnderlyingTypeTrait);
using DefaultUnderlyingTypeTraitResult =
    __underlying_type(DefaultUnderlyingTypeTrait);
using OpaqueUnderlyingTypeTraitResult =
    __underlying_type(OpaqueUnderlyingTypeTrait);
using UnderlyingTypeTraitTemplateArgument =
    UnderlyingTypeTraitIdentity<
        __underlying_type(FixedUnderlyingTypeTrait)>::type;

struct TriviallyCopyable {
    int value;
};

struct HasUserDefaultConstructor {
    HasUserDefaultConstructor() : value(0) {}
    int value;
};

struct NonTriviallyCopyable {
    NonTriviallyCopyable(const NonTriviallyCopyable& other)
        : value(other.value) {}
    int value;
};

struct DefaultedDestructor {
    ~DefaultedDestructor() = default;
    int value;
};

struct UserProvidedDestructor {
    ~UserProvidedDestructor() {}
    int value;
};

struct DefaultedDestructorWithNontrivialMember {
    ~DefaultedDestructorWithNontrivialMember() = default;
    UserProvidedDestructor member;
};

struct NonTrivialCopyWithDefaultedParameter {
    NonTrivialCopyWithDefaultedParameter(
        const NonTrivialCopyWithDefaultedParameter&, int = 0) {}
    int value;
};

struct TriviallyCopyableReferenceMember {
    int& value;
};

struct VirtualType {
    virtual void run();
};

union TriviallyCopyableUnion {
    int integer;
    unsigned bits;
};

enum TypeCategoryEnum {
    TypeCategoryEnumValue
};

class TypeCategoryClass {
};

class TypeCategoryFinalClass final {
};

class TypeCategoryAbstractClass {
public:
    virtual void run() = 0;
};

class TypeCategoryConcreteDerived : public TypeCategoryAbstractClass {
public:
    void run() override {}
};

struct IncompleteTypeCategoryClass;

struct EmptyTypeCategory {
};

struct DerivedEmptyTypeCategory : EmptyTypeCategory {
};

struct NonEmptyTypeCategory {
    int value;
};

struct DerivedNonEmptyTypeCategory : NonEmptyTypeCategory {
};

struct StaticOnlyTypeCategory {
    static int value;
};

struct ZeroWidthBitfieldTypeCategory {
    int : 0;
};

struct EmptyMemberTypeCategory {
    EmptyTypeCategory value;
};

struct TriviallyDestructibleDefaulted {
    ~TriviallyDestructibleDefaulted() = default;
};

struct NontriviallyDestructibleUserProvided {
    ~NontriviallyDestructibleUserProvided() {}
};

struct NontriviallyDestructibleDeleted {
    ~NontriviallyDestructibleDeleted() = delete;
};

struct NontriviallyDestructiblePrivate {
private:
    ~NontriviallyDestructiblePrivate() = default;
};

struct NontriviallyDestructibleProtected {
protected:
    ~NontriviallyDestructibleProtected() = default;
};

struct DeletedDefaultConstructor {
    DeletedDefaultConstructor() = delete;
    int value;
};

struct NoDefaultConstructor {
    explicit NoDefaultConstructor(int);
    int value;
};

struct ConstructorTemplateOnly {
    template<typename T>
    ConstructorTemplateOnly(T);
};

struct HasDeletedDefaultConstructorMember {
    DeletedDefaultConstructor member;
};

struct HasNontriviallyDestructibleMember {
    NontriviallyDestructibleUserProvided member;
};

struct HasInaccessibleMemberDestructor {
    NontriviallyDestructiblePrivate member;
};

struct HasNontrivialBaseDestructor : NontriviallyDestructibleUserProvided {
};

struct HasProtectedBaseDestructor : NontriviallyDestructibleProtected {
};

struct VirtualDestructor {
    virtual ~VirtualDestructor() = default;
};

struct InheritedVirtualDestructor : VirtualDestructor {
};

struct VirtualMethodOnly {
    virtual void run();
};

struct AggregateTypeTrait {
    int value;
};

struct AggregateWithDefaultMemberInitializer {
    int value = 1;
};

struct AggregateWithPublicBase : AggregateTypeTrait {
    int extra;
};

struct NonAggregatePrivateMember {
private:
    int value;
};

struct NonAggregateDefaultedConstructor {
    NonAggregateDefaultedConstructor() = default;
};

struct NonAggregateVirtualFunction {
    virtual void run();
};

union AggregateTypeTraitUnion {
    int integer;
    unsigned bits;
};

struct BaseOfRoot {
};

struct BaseOfPrivateDerived : private BaseOfRoot {
};

struct BaseOfLeft : BaseOfRoot {
};

struct BaseOfRight : BaseOfRoot {
};

struct BaseOfAmbiguousDerived : BaseOfLeft, BaseOfRight {
};

template<typename T>
struct DeferredTraitResults {
#ifdef __RCC__
    static constexpr bool trivial = __is_trivial(T);
    static constexpr bool class_type = __is_class(T);
    static constexpr bool copyable = __is_trivially_copyable(T);
    static constexpr bool destructible = __is_trivially_destructible(T);
    static constexpr bool virtual_destructor = __has_virtual_destructor(T);
    static constexpr bool aggregate = __is_aggregate(T);
    static constexpr bool enum_type = __is_enum(T);
    static constexpr bool union_type = __is_union(T);
    static constexpr bool final_type = __is_final(T);
    static constexpr bool abstract_type = __is_abstract(T);
    static constexpr bool empty = __is_empty(T);
    static constexpr bool polymorphic = __is_polymorphic(T);
#else
    static constexpr bool trivial = std::is_trivial_v<T>;
    static constexpr bool class_type = std::is_class_v<T>;
    static constexpr bool copyable = std::is_trivially_copyable_v<T>;
    static constexpr bool destructible = std::is_trivially_destructible_v<T>;
    static constexpr bool virtual_destructor = std::has_virtual_destructor_v<T>;
    static constexpr bool aggregate = std::is_aggregate_v<T>;
    static constexpr bool enum_type = std::is_enum_v<T>;
    static constexpr bool union_type = std::is_union_v<T>;
    static constexpr bool final_type = std::is_final_v<T>;
    static constexpr bool abstract_type = std::is_abstract_v<T>;
    static constexpr bool empty = std::is_empty_v<T>;
    static constexpr bool polymorphic = std::is_polymorphic_v<T>;
#endif
};

template<typename Base, typename Derived>
struct DeferredBaseTraitResult {
#ifdef __RCC__
    static constexpr bool value = __is_base_of(Base, Derived);
#else
    static constexpr bool value = std::is_base_of_v<Base, Derived>;
#endif
};

#ifdef __RCC__
template<typename T>
requires __is_class(T)
int deferred_trait_constraint(T*);

template<typename T>
long deferred_trait_constraint(T*);
#else
template<typename T>
requires std::is_class_v<T>
int deferred_trait_constraint(T*);

template<typename T>
long deferred_trait_constraint(T*);
#endif

#ifdef __RCC__
template<typename T>
requires __is_trivial(T)
int deferred_trivial_constraint(T*);
#else
template<typename T>
requires std::is_trivial_v<T>
int deferred_trivial_constraint(T*);
#endif
template<typename T>
long deferred_trivial_constraint(T*);

static_assert(__is_trivially_copyable(int), "scalar types are trivially copyable");
static_assert(__is_trivial(int), "scalar types are trivial");
static_assert(__is_trivially_copyable(int*), "object pointers are trivially copyable");
static_assert(__is_trivially_copyable(int[3]), "bounded arrays are trivially copyable");
static_assert(__is_trivially_copyable(const TriviallyCopyable), "cv-qualified objects retain the property");
static_assert(__is_trivially_copyable(HasUserDefaultConstructor), "ordinary constructors do not affect trivial copying");
static_assert(__is_trivially_copyable(TriviallyCopyableUnion), "trivial unions are trivially copyable");
static_assert(__is_trivially_copyable(TriviallyCopyableReferenceMember), "reference members do not make copy construction nontrivial");
static_assert(__is_trivially_copyable(DefaultedDestructor), "an explicitly defaulted trivial destructor preserves trivial copying");
static_assert(!__is_trivially_copyable(UserProvidedDestructor), "a user-provided destructor prevents trivial copying");
static_assert(!__is_trivially_copyable(DefaultedDestructorWithNontrivialMember), "a defaulted destructor is nontrivial when a member destructor is nontrivial");
static_assert(!__is_trivially_copyable(NonTrivialCopyWithDefaultedParameter), "copy constructors may have additional defaulted parameters");
static_assert(!__is_trivially_copyable(NonTriviallyCopyable), "user copy constructors are nontrivial");
static_assert(!__is_trivially_copyable(VirtualType), "polymorphic classes are not trivially copyable");
static_assert(!__is_trivially_copyable(void), "void is not an object type");
static_assert(!__is_trivially_copyable(int()), "function types are not object types");
static_assert(!__is_trivially_copyable(int&), "reference types are not object types");

static_assert(__is_enum(TypeCategoryEnum), "enum types are enums");
static_assert(!__is_enum(TriviallyCopyable), "class types are not enums");
static_assert(!__is_enum(TypeCategoryEnum*), "pointers to enums are not enums");
static_assert(__is_class(TriviallyCopyable), "struct types are class types");
static_assert(__is_class(TypeCategoryClass), "class-key class types are classes");
static_assert(__is_class(IncompleteTypeCategoryClass), "incomplete class types retain their category");
static_assert(!__is_class(TriviallyCopyableUnion), "union types are not class types");
static_assert(!__is_class(TypeCategoryEnum), "enum types are not class types");
static_assert(__is_union(TriviallyCopyableUnion), "union types are unions");
static_assert(!__is_union(TriviallyCopyable), "struct types are not unions");
static_assert(!__is_union(TypeCategoryEnum), "enum types are not unions");

static_assert(__is_final(TypeCategoryFinalClass), "final classes are final");
static_assert(!__is_final(TypeCategoryClass), "ordinary classes are not final");
static_assert(!__is_final(TypeCategoryClass*), "pointers to final classes are not final types");
static_assert(__is_abstract(TypeCategoryAbstractClass), "pure virtual classes are abstract");
static_assert(!__is_abstract(TypeCategoryConcreteDerived), "overridden pure virtual functions make the derived class concrete");
static_assert(!__is_abstract(TypeCategoryClass), "ordinary classes are not abstract");
static_assert(__is_polymorphic(TypeCategoryAbstractClass), "classes with virtual methods are polymorphic");
static_assert(__is_polymorphic(TypeCategoryConcreteDerived), "polymorphic bases propagate polymorphism");
static_assert(!__is_polymorphic(TypeCategoryClass), "classes without virtual methods are not polymorphic");
static_assert(!__is_polymorphic(TriviallyCopyableUnion), "unions are not polymorphic");

static_assert(__is_empty(EmptyTypeCategory), "classes without non-static data are empty");
static_assert(__is_empty(DerivedEmptyTypeCategory), "classes with only empty bases are empty");
static_assert(__is_empty(StaticOnlyTypeCategory), "static data members do not make a class nonempty");
static_assert(__is_empty(ZeroWidthBitfieldTypeCategory), "zero-width bitfields do not make a class nonempty");
static_assert(!__is_empty(NonEmptyTypeCategory), "classes with non-static data are nonempty");
static_assert(!__is_empty(DerivedNonEmptyTypeCategory), "nonempty bases make a derived class nonempty");
static_assert(!__is_empty(EmptyMemberTypeCategory), "empty data members still make a class nonempty");
static_assert(!__is_empty(TriviallyCopyableUnion), "unions are not empty classes");
static_assert(!__is_empty(EmptyTypeCategory*), "pointers to empty classes are not empty classes");

static_assert(DeferredTraitResults<TypeCategoryClass>::class_type, "dependent class traits are resolved after specialization");
static_assert(!DeferredTraitResults<int>::class_type, "dependent class traits retain false results after specialization");
static_assert(DeferredTraitResults<TriviallyCopyable>::trivial, "dependent triviality is resolved after specialization");
static_assert(!DeferredTraitResults<HasUserDefaultConstructor>::trivial, "dependent triviality rejects user-provided default constructors");
static_assert(!DeferredTraitResults<NoDefaultConstructor>::trivial, "dependent triviality requires a default constructor");
static_assert(!DeferredTraitResults<ConstructorTemplateOnly>::trivial, "constructor templates suppress the implicit default constructor");
static_assert(DeferredTraitResults<DeletedDefaultConstructor>::trivial, "dependent triviality preserves deleted default-constructor classification");
static_assert(DeferredTraitResults<HasDeletedDefaultConstructorMember>::trivial, "dependent triviality recurses through deleted-default subobjects");
static_assert(DeferredTraitResults<TriviallyCopyableReferenceMember>::trivial, "reference members preserve trivial class classification");
static_assert(!DeferredTraitResults<AggregateWithDefaultMemberInitializer>::trivial, "default member initializers make default construction nontrivial");
static_assert(DeferredTraitResults<DerivedEmptyTypeCategory>::trivial, "triviality includes trivial base subobjects");
static_assert(DeferredTraitResults<DefaultedDestructor>::trivial, "defaulted trivial destructors preserve triviality");
static_assert(DeferredTraitResults<NonAggregateDefaultedConstructor>::trivial, "defaulted default constructors preserve triviality");
static_assert(!DeferredTraitResults<UserProvidedDestructor>::trivial, "user-provided destructors prevent triviality");
static_assert(DeferredTraitResults<TriviallyCopyableUnion>::trivial, "trivial unions retain triviality");
static_assert(DeferredTraitResults<TypeCategoryEnum>::trivial, "enumerations are trivial types");
static_assert(!DeferredTraitResults<TypeCategoryAbstractClass>::trivial, "polymorphic classes are not trivial");
static_assert(DeferredTraitResults<int[3]>::trivial, "triviality propagates through arrays");
static_assert(!DeferredTraitResults<int&>::trivial, "references are not trivial types");
static_assert(sizeof(deferred_trivial_constraint(static_cast<TriviallyCopyable*>(nullptr))) == sizeof(int), "dependent triviality participates in constrained overload resolution");
static_assert(sizeof(deferred_trivial_constraint(static_cast<HasUserDefaultConstructor*>(nullptr))) == sizeof(long), "false dependent triviality removes constrained overloads");
static_assert(DeferredTraitResults<TriviallyCopyable>::copyable, "dependent copyability traits are resolved after specialization");
static_assert(!DeferredTraitResults<NonTriviallyCopyable>::copyable, "dependent copyability traits preserve false results");
static_assert(DeferredTraitResults<TriviallyDestructibleDefaulted>::destructible, "dependent destructor traits are resolved after specialization");
static_assert(!DeferredTraitResults<NontriviallyDestructibleUserProvided>::destructible, "dependent destructor traits preserve false results");
static_assert(DeferredTraitResults<VirtualDestructor>::virtual_destructor, "dependent virtual-destructor traits are resolved after specialization");
static_assert(!DeferredTraitResults<VirtualMethodOnly>::virtual_destructor, "dependent virtual-method traits distinguish virtual destructors");
static_assert(DeferredTraitResults<AggregateWithDefaultMemberInitializer>::aggregate, "dependent aggregate traits are resolved after specialization");
static_assert(DeferredTraitResults<TypeCategoryEnum>::enum_type, "dependent enum traits are resolved after specialization");
static_assert(!DeferredTraitResults<TypeCategoryClass>::enum_type, "dependent enum traits preserve false results");
static_assert(DeferredTraitResults<TriviallyCopyableUnion>::union_type, "dependent union traits are resolved after specialization");
static_assert(DeferredTraitResults<TypeCategoryFinalClass>::final_type, "dependent final traits are resolved after specialization");
static_assert(!DeferredTraitResults<TypeCategoryClass>::final_type, "dependent final traits preserve false results");
static_assert(DeferredTraitResults<TypeCategoryAbstractClass>::abstract_type, "dependent abstract traits are resolved after specialization");
static_assert(!DeferredTraitResults<TypeCategoryConcreteDerived>::abstract_type, "dependent abstract traits reevaluate overridden virtual functions");
static_assert(DeferredTraitResults<EmptyTypeCategory>::empty, "dependent emptiness traits are resolved after specialization");
static_assert(DeferredTraitResults<TypeCategoryAbstractClass>::polymorphic, "dependent polymorphism traits are resolved after specialization");
static_assert(!DeferredTraitResults<TypeCategoryClass*>::class_type, "dependent traits traverse pointer type operands");
static_assert(DeferredTraitResults<TypeCategoryClass*>::copyable, "dependent pointer traits are evaluated after substitution");
static_assert(DeferredBaseTraitResult<BaseOfRoot, BaseOfPrivateDerived>::value, "dependent base-of traits are resolved after specialization");
static_assert(!DeferredBaseTraitResult<BaseOfPrivateDerived, BaseOfRoot>::value, "dependent base-of traits preserve operand order");
static_assert(sizeof(deferred_trait_constraint(static_cast<TypeCategoryClass*>(nullptr))) == sizeof(int), "dependent type traits participate in constrained overload resolution");
static_assert(sizeof(deferred_trait_constraint(static_cast<int*>(nullptr))) == sizeof(long), "false dependent traits remove constrained overloads");

#ifdef __RCC__
static_assert(__is_trivially_destructible(int), "scalar types are trivially destructible");
static_assert(__is_trivially_destructible(int&), "references are trivially destructible");
static_assert(__is_trivially_destructible(TriviallyDestructibleDefaulted), "defaulted trivial destructors retain the property");
static_assert(__is_trivially_destructible(TriviallyDestructibleDefaulted[3]), "bounded arrays retain the element property");
static_assert(!__is_trivially_destructible(NontriviallyDestructibleUserProvided), "user-provided destructors are nontrivial");
static_assert(!__is_trivially_destructible(NontriviallyDestructibleDeleted), "deleted destructors are not destructible");
static_assert(!__is_trivially_destructible(NontriviallyDestructiblePrivate), "private destructors are inaccessible");
static_assert(!__is_trivially_destructible(NontriviallyDestructibleProtected), "protected destructors are inaccessible to callers");
static_assert(!__is_trivially_destructible(HasNontriviallyDestructibleMember), "nontrivial member destructors propagate");
static_assert(!__is_trivially_destructible(HasInaccessibleMemberDestructor), "inaccessible member destructors propagate");
static_assert(!__is_trivially_destructible(HasNontrivialBaseDestructor), "nontrivial base destructors propagate");
static_assert(__is_trivially_destructible(HasProtectedBaseDestructor), "derived classes can destroy protected base subobjects");
static_assert(!__is_trivially_destructible(VirtualDestructor), "virtual destructors are nontrivial");
static_assert(!__is_trivially_destructible(void), "void is not destructible");
static_assert(!__is_trivially_destructible(int()), "function types are not destructible");
static_assert(__has_virtual_destructor(VirtualDestructor), "virtual destructors are detected");
static_assert(__has_virtual_destructor(InheritedVirtualDestructor), "virtual destructors are inherited");
static_assert(!__has_virtual_destructor(VirtualMethodOnly), "unrelated virtual methods do not make the destructor virtual");
static_assert(!__has_virtual_destructor(TriviallyCopyableUnion), "unions do not have virtual destructors");
static_assert(!__has_virtual_destructor(VirtualDestructor*), "pointers do not have virtual destructors");
static_assert(__is_aggregate(AggregateTypeTrait), "plain classes are aggregates");
static_assert(__is_aggregate(AggregateWithDefaultMemberInitializer), "default member initializers are allowed in C++20 aggregates");
static_assert(__is_aggregate(AggregateWithPublicBase), "public non-virtual bases are allowed in C++20 aggregates");
static_assert(__is_aggregate(AggregateTypeTraitUnion), "plain unions are aggregates");
static_assert(__is_aggregate(AggregateTypeTrait[2]), "bounded arrays are aggregates");
static_assert(!__is_aggregate(NonAggregatePrivateMember), "private data members disqualify aggregates");
static_assert(!__is_aggregate(NonAggregateDefaultedConstructor), "user-declared constructors disqualify aggregates");
static_assert(!__is_aggregate(NonAggregateVirtualFunction), "virtual functions disqualify aggregates");
static_assert(!__is_aggregate(AggregateTypeTrait*), "pointers are not aggregates");
static_assert(!__is_aggregate(TypeCategoryEnum), "enumerations are not aggregates");
static_assert(__is_base_of(BaseOfRoot, BaseOfRoot), "a class is its own base");
static_assert(__is_base_of(BaseOfRoot, BaseOfPrivateDerived), "private bases still satisfy base_of");
static_assert(__is_base_of(BaseOfRoot, BaseOfAmbiguousDerived), "ambiguous bases still satisfy base_of");
static_assert(!__is_base_of(BaseOfPrivateDerived, BaseOfRoot), "base_of direction is derived from its second type");
static_assert(!__is_base_of(BaseOfRoot, int), "non-class derived types are not related");
static_assert(!__is_base_of(int, int), "identical non-class types are not base classes");
static_assert(!__is_base_of(AggregateTypeTraitUnion, AggregateTypeTraitUnion), "unions are not base classes");
static_assert(sizeof(FixedUnderlyingTypeTraitResult) == sizeof(unsigned short), "fixed enum underlying type is preserved");
static_assert(sizeof(DefaultUnderlyingTypeTraitResult) == sizeof(int), "scoped enums default to int");
static_assert(sizeof(OpaqueUnderlyingTypeTraitResult) == sizeof(unsigned char), "opaque enums retain their fixed underlying type");
static_assert(sizeof(UnderlyingTypeTraitTemplateArgument) == sizeof(unsigned short), "underlying_type works in template arguments");
#else
static_assert(std::is_trivially_destructible_v<int>, "scalar types are trivially destructible");
static_assert(std::is_trivially_destructible_v<int&>, "references are trivially destructible");
static_assert(std::is_trivially_destructible_v<TriviallyDestructibleDefaulted>, "defaulted trivial destructors retain the property");
static_assert(std::is_trivially_destructible_v<TriviallyDestructibleDefaulted[3]>, "bounded arrays retain the element property");
static_assert(!std::is_trivially_destructible_v<NontriviallyDestructibleUserProvided>, "user-provided destructors are nontrivial");
static_assert(!std::is_trivially_destructible_v<NontriviallyDestructibleDeleted>, "deleted destructors are not destructible");
static_assert(!std::is_trivially_destructible_v<NontriviallyDestructiblePrivate>, "private destructors are inaccessible");
static_assert(!std::is_trivially_destructible_v<NontriviallyDestructibleProtected>, "protected destructors are inaccessible to callers");
static_assert(!std::is_trivially_destructible_v<HasNontriviallyDestructibleMember>, "nontrivial member destructors propagate");
static_assert(!std::is_trivially_destructible_v<HasInaccessibleMemberDestructor>, "inaccessible member destructors propagate");
static_assert(!std::is_trivially_destructible_v<HasNontrivialBaseDestructor>, "nontrivial base destructors propagate");
static_assert(std::is_trivially_destructible_v<HasProtectedBaseDestructor>, "derived classes can destroy protected base subobjects");
static_assert(!std::is_trivially_destructible_v<VirtualDestructor>, "virtual destructors are nontrivial");
static_assert(!std::is_trivially_destructible_v<void>, "void is not destructible");
static_assert(!std::is_trivially_destructible_v<int()>, "function types are not destructible");
static_assert(std::has_virtual_destructor_v<VirtualDestructor>, "virtual destructors are detected");
static_assert(std::has_virtual_destructor_v<InheritedVirtualDestructor>, "virtual destructors are inherited");
static_assert(!std::has_virtual_destructor_v<VirtualMethodOnly>, "unrelated virtual methods do not make the destructor virtual");
static_assert(!std::has_virtual_destructor_v<TriviallyCopyableUnion>, "unions do not have virtual destructors");
static_assert(!std::has_virtual_destructor_v<VirtualDestructor*>, "pointers do not have virtual destructors");
static_assert(std::is_aggregate_v<AggregateTypeTrait>, "plain classes are aggregates");
static_assert(std::is_aggregate_v<AggregateWithDefaultMemberInitializer>, "default member initializers are allowed in C++20 aggregates");
static_assert(std::is_aggregate_v<AggregateWithPublicBase>, "public non-virtual bases are allowed in C++20 aggregates");
static_assert(std::is_aggregate_v<AggregateTypeTraitUnion>, "plain unions are aggregates");
static_assert(std::is_aggregate_v<AggregateTypeTrait[2]>, "bounded arrays are aggregates");
static_assert(!std::is_aggregate_v<NonAggregatePrivateMember>, "private data members disqualify aggregates");
static_assert(!std::is_aggregate_v<NonAggregateDefaultedConstructor>, "user-declared constructors disqualify aggregates");
static_assert(!std::is_aggregate_v<NonAggregateVirtualFunction>, "virtual functions disqualify aggregates");
static_assert(!std::is_aggregate_v<AggregateTypeTrait*>, "pointers are not aggregates");
static_assert(!std::is_aggregate_v<TypeCategoryEnum>, "enumerations are not aggregates");
static_assert(std::is_base_of_v<BaseOfRoot, BaseOfRoot>, "a class is its own base");
static_assert(std::is_base_of_v<BaseOfRoot, BaseOfPrivateDerived>, "private bases still satisfy base_of");
static_assert(std::is_base_of_v<BaseOfRoot, BaseOfAmbiguousDerived>, "ambiguous bases still satisfy base_of");
static_assert(!std::is_base_of_v<BaseOfPrivateDerived, BaseOfRoot>, "base_of direction is derived from its second type");
static_assert(!std::is_base_of_v<BaseOfRoot, int>, "non-class derived types are not related");
static_assert(!std::is_base_of_v<int, int>, "identical non-class types are not base classes");
static_assert(!std::is_base_of_v<AggregateTypeTraitUnion, AggregateTypeTraitUnion>, "unions are not base classes");
static_assert(std::is_same_v<FixedUnderlyingTypeTraitResult, unsigned short>, "fixed enum underlying type is preserved");
static_assert(std::is_same_v<DefaultUnderlyingTypeTraitResult, int>, "scoped enums default to int");
static_assert(std::is_same_v<OpaqueUnderlyingTypeTraitResult, unsigned char>, "opaque enums retain their fixed underlying type");
static_assert(std::is_same_v<UnderlyingTypeTraitTemplateArgument, unsigned short>, "underlying_type works in template arguments");
#endif
