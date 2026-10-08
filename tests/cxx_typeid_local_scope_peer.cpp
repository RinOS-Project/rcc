extern "C" int peer_local_class_method_call() {
    class LocalMethod { public: int get() const { return 21; } };
    LocalMethod value;
    return value.get();
}

extern "C" const void* peer_local_enum_typeinfo() {
    enum LocalValue { peer_local_value };
    return &typeid(LocalValue);
}

extern "C" const void* peer_local_class_typeinfo() {
    class LocalClass { public: int value; };
    return &typeid(LocalClass);
}
