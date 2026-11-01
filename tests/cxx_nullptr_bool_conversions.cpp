int main() {
    decltype(nullptr) null_value = nullptr;
    bool direct{nullptr};
    bool direct_from_lvalue{null_value};
    bool explicit_cast = static_cast<bool>(nullptr);
    int* pointer = nullptr;
    if (nullptr) return 1;
    return direct || direct_from_lvalue || explicit_cast ||
           pointer != nullptr;
}
