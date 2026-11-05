template<auto N>
int auto_constant() {
    return N;
}

template<auto N = 4>
int auto_default_constant(int value) {
    return value + N;
}

signed char auto_pointer_target = -19;
extern constexpr int auto_class_pointer_target = -23;
constexpr signed char* auto_pointer_alias = &auto_pointer_target;

template<auto Pointer>
int auto_pointer_value() {
    return *Pointer;
}

template<auto Pointer>
constexpr int auto_pointer_variable = *Pointer;

template<auto Value>
constexpr int auto_value_variable = Value;

template<auto Pointer>
struct auto_pointer_holder {
    static constexpr int value = *Pointer;
    constexpr int read() const {
        return *Pointer;
    }
};

auto_pointer_holder<&auto_class_pointer_target> auto_pointer_instance;

extern "C" int main() {
    return auto_constant<7>() == 7 &&
                   auto_constant<7u>() == 7 &&
                   auto_value_variable<7> == 7 &&
                   auto_value_variable<7u> == 7 &&
                   auto_default_constant<>(3) == 7 &&
                   auto_default_constant<-2>(9) == 7 &&
                   auto_pointer_value<&auto_pointer_target>() == -19 &&
                   auto_pointer_value<auto_pointer_alias>() == -19 &&
                   auto_pointer_variable<&auto_class_pointer_target> == -23 &&
                   auto_pointer_holder<&auto_class_pointer_target>::value == -23 &&
                   auto_pointer_instance.read() == -23
               ? 0
               : 1;
}
