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
static_assert(!__is_trivially_copyable(NonTriviallyCopyable), "user copy constructors are nontrivial");
static_assert(!__is_trivially_copyable(VirtualType), "polymorphic classes are not trivially copyable");
static_assert(!__is_trivially_copyable(void), "void is not an object type");
static_assert(!__is_trivially_copyable(int()), "function types are not object types");
static_assert(!__is_trivially_copyable(int&), "reference types are not object types");
