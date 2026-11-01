int main() {
    decltype(nullptr) null_value = nullptr;
    bool direct{nullptr};
    bool direct_from_lvalue{null_value};
    bool direct_paren(nullptr);
    bool explicit_cast = static_cast<bool>(nullptr);
    int* pointer = nullptr;
    int source = 42;
    int copied(source);
    int* direct_pointer(nullptr);
    if (nullptr) return 1;
    return direct || direct_from_lvalue || explicit_cast ||
           direct_paren || pointer != nullptr || direct_pointer != nullptr ||
           copied != 42;
}
