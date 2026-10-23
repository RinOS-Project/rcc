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

static_assert(__is_trivially_copyable(int), "scalar types are trivially copyable");
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
