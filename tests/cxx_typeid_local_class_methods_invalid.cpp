static const void* local_class_with_method_typeinfo() {
    class LocalClass {
    public:
        int value() const;
    };
    return &typeid(LocalClass);
}

template <class T>
void local_class_inside_function_template() {
    class LocalClass {
    public:
        T value;
    };
}
