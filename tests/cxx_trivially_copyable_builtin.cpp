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
