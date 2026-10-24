typedef struct RccCxxNamedAliasTag {
    long long value;
} RccCxxNamedAliasType;

typedef struct {
    long long value;
} RccCxxAnonymousAliasType;

struct ClassTypedefScope {
    typedef int value_type;
    typedef int* pointer_type, const_value_type;

    value_type value;
};

template<typename T>
struct ClassTypedefRemoveReference {
    typedef T type;
};

template<typename T>
struct ClassTypedefRemoveReference<T&> {
    typedef T type;
};

ClassTypedefRemoveReference<int&>::type
cxx_class_typedef_partial_specialization_probe() {
    return 17;
}

ClassTypedefScope::value_type cxx_class_typedef_member_probe(
    ClassTypedefScope::pointer_type pointer) {
    return pointer ? *pointer : 23;
}
