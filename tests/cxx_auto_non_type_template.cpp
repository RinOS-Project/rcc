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

template<auto... Values>
int auto_pack_size() {
    return sizeof...(Values);
}

template<auto... Values>
int auto_pack_sum() {
    return (Values + ...);
}

template<auto... Values>
constexpr int auto_pack_sum_variable = (Values + ...);

template<auto... Values>
constexpr int auto_pack_size_variable = sizeof...(Values);

template<auto... Pointers>
int auto_pointer_pack_size() {
    return sizeof...(Pointers);
}

template<class Type, Type Value>
struct auto_constant_argument {};

template<auto... Values>
struct auto_constant_pack {};

template<class Type, auto... Values>
int auto_deduced_pack_sum(
    auto_constant_argument<Type, Values>... arguments) {
    (void)sizeof...(arguments);
    return (Values + ...);
}

template<auto... Values>
int auto_deduced_mixed_pack_sum(
    auto_constant_argument<decltype(Values), Values>... arguments) {
    (void)sizeof...(arguments);
    return (Values + ...);
}

template<auto... Values>
int auto_deduced_nested_pack_sum(auto_constant_pack<Values...>) {
    return (Values + ...);
}

template<class... Types, auto... Values>
int auto_deduced_zipped_pack_sum(
    auto_constant_argument<Types, Values>... arguments) {
    return (Values + ...) + sizeof...(arguments);
}

template<auto Pointer>
struct auto_pointer_holder {
    static constexpr int value = *Pointer;
    constexpr int read() const {
        return *Pointer;
    }
};

template<auto... Values>
struct auto_value_pack_holder {
    constexpr int read() const {
        return (Values + ...);
    }
};

auto_pointer_holder<&auto_class_pointer_target> auto_pointer_instance;
auto_value_pack_holder<5, 7u> auto_value_pack_mixed_instance;
auto_value_pack_holder<5, 7> auto_value_pack_signed_instance;
auto_value_pack_holder<5u, 7u> auto_value_pack_unsigned_instance;

extern "C" int main() {
    return auto_constant<7>() == 7 &&
                   auto_constant<7u>() == 7 &&
                   auto_value_variable<7> == 7 &&
                   auto_value_variable<7u> == 7 &&
                   auto_pack_size<7, 7>() == 2 &&
                   auto_pack_size<7u, 7>() == 2 &&
                   auto_pack_sum<5, 7u>() == 12 &&
                   auto_pack_sum_variable<5, 7u> == 12 &&
                   auto_pack_sum_variable<5, 7> == 12 &&
                   auto_pack_sum_variable<5u, 7u> == 12 &&
                   auto_pack_size_variable<> == 0 &&
                   auto_pack_size_variable<&auto_pointer_target,
                                            &auto_class_pointer_target> == 2 &&
                   auto_pointer_pack_size<&auto_pointer_target,
                                          &auto_class_pointer_target>() == 2 &&
                   auto_deduced_pack_sum(
                       auto_constant_argument<int, 5>{},
                       auto_constant_argument<int, 7>{}) == 12 &&
                   auto_deduced_pack_sum(
                       auto_constant_argument<unsigned, 5u>{},
                       auto_constant_argument<unsigned, 7u>{}) == 12 &&
                   auto_deduced_mixed_pack_sum(
                       auto_constant_argument<int, 5>{},
                       auto_constant_argument<unsigned, 7u>{}) == 12 &&
                   auto_deduced_nested_pack_sum(
                       auto_constant_pack<5, 7u>{}) == 12 &&
                   auto_deduced_zipped_pack_sum(
                       auto_constant_argument<int, 5>{},
                       auto_constant_argument<unsigned, 7u>{}) == 14 &&
                   auto_default_constant<>(3) == 7 &&
                   auto_default_constant<-2>(9) == 7 &&
                   auto_pointer_value<&auto_pointer_target>() == -19 &&
                   auto_pointer_value<auto_pointer_alias>() == -19 &&
                   auto_pointer_variable<&auto_class_pointer_target> == -23 &&
                   auto_pointer_holder<&auto_class_pointer_target>::value == -23 &&
                   auto_pointer_instance.read() == -23 &&
                   auto_value_pack_mixed_instance.read() == 12 &&
                   auto_value_pack_signed_instance.read() == 12 &&
                   auto_value_pack_unsigned_instance.read() == 12
               ? 0
               : 1;
}
