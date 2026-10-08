extern "C" const void* peer_local_enum_typeinfo() {
    enum LocalValue { peer_local_value };
    return &typeid(LocalValue);
}

extern "C" const void* peer_local_class_typeinfo() {
    class LocalClass { public: int value; };
    return &typeid(LocalClass);
}
