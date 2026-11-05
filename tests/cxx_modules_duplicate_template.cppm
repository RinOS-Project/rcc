export module duplicate_template;

export template<class T>
inline int duplicate_template_definition(T value) {
    return static_cast<int>(value);
}

export template<class T>
inline int duplicate_template_definition(T value) {
    return static_cast<int>(value) + 1;
}
