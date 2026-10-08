extern "C" const void* peer_local_enum_typeinfo();
extern "C" const void* peer_local_class_typeinfo();
extern "C" int peer_local_class_method_call();

static int local_class_member_call() {
    class LocalMethod { public: int get() const { return 9; } };
    LocalMethod value;
    return value.get();
}

static int local_class_member_call_sibling() {
    class LocalMethod { public: int get() const { return 13; } };
    LocalMethod value;
    return value.get();
}

static int local_class_constructor_call() {
    class LocalConstructor {
    public:
        int value;
        LocalConstructor(int initial) : value(initial) {}
    };
    LocalConstructor value(27);
    return value.value;
}

static const void* local_class_typeinfo_a() {
    class LocalClass { public: int value; };
    const void* first = &typeid(LocalClass);
    const void* second = &typeid(LocalClass);
    return first == second ? first : 0;
}

static const void* local_class_typeinfo_b() {
    class LocalClass { public: int value; };
    return &typeid(LocalClass);
}

static int nested_local_class_scope_identity() {
    class LocalClass { public: int value; };
    const void* outer_type = &typeid(LocalClass);
    {
        class LocalClass { public: int inner; };
        const void* inner_type = &typeid(LocalClass);
        if (outer_type == inner_type) return 1;
    }
    return outer_type == &typeid(LocalClass) ? 0 : 1;
}

static const void* local_enum_typeinfo_a() {
    enum LocalValue { local_value_a };
    const void* first = &typeid(LocalValue);
    const void* second = &typeid(LocalValue);
    return first == second ? first : 0;
}

static const void* local_enum_typeinfo_b() {
    enum LocalValue { local_value_b };
    return &typeid(LocalValue);
}

static int nested_enum_scope_identity() {
    enum LocalValue { outer_value };
    const void* outer_type = &typeid(LocalValue);
    {
        enum LocalValue { inner_value };
        const void* inner_type = &typeid(LocalValue);
        if (outer_type == inner_type) return 1;
    }
    return outer_type == &typeid(LocalValue) ? 0 : 1;
}

extern "C" int main() {
    return local_enum_typeinfo_a() != 0 &&
           local_enum_typeinfo_a() != local_enum_typeinfo_b() &&
           local_enum_typeinfo_a() != peer_local_enum_typeinfo() &&
           nested_enum_scope_identity() == 0 &&
           local_class_typeinfo_a() != 0 &&
           local_class_typeinfo_a() != local_class_typeinfo_b() &&
           local_class_typeinfo_a() != peer_local_class_typeinfo() &&
           local_class_member_call() == 9 &&
           local_class_member_call_sibling() == 13 &&
           local_class_constructor_call() == 27 &&
           peer_local_class_method_call() == 21 &&
           nested_local_class_scope_identity() == 0 ? 0 : 1;
}
