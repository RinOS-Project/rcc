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

struct auto_member_pointer_target {
    int field;
    int get() const {
        return field + 4;
    }
};

constexpr int auto_member_pointer_target::* auto_member_data_pointer_alias =
    &auto_member_pointer_target::field;
constexpr int (auto_member_pointer_target::*
                   auto_member_function_pointer_alias)() const =
    &auto_member_pointer_target::get;
using auto_member_function_pointer_type =
    int (auto_member_pointer_target::*)() const;
auto_member_pointer_target auto_member_pointer_object{5};

template<auto Pointer>
int auto_pointer_value() {
    return *Pointer;
}

template<auto Pointer>
int auto_member_pointer_tag() {
    return 19;
}

template<auto Pointer>
int auto_member_pointer_read() {
    return auto_member_pointer_object.*Pointer;
}

template<auto Pointer>
int auto_member_function_pointer_read() {
    return (auto_member_pointer_object.*Pointer)();
}

template<auto Pointer>
bool auto_member_pointer_is_null() {
    return Pointer == nullptr;
}

template<auto_member_function_pointer_type Pointer>
int auto_typed_member_function_pointer_tag() {
    return 37;
}

template<int auto_member_pointer_target::*Pointer>
bool auto_typed_member_pointer_is_null() {
    return Pointer == nullptr;
}

template<auto_member_function_pointer_type Pointer>
bool auto_typed_member_function_pointer_is_null() {
    return Pointer == nullptr;
}

template<auto Pointer>
inline constexpr int auto_member_pointer_variable = 23;

template<auto Pointer>
inline constexpr int auto_member_function_pointer_variable = 31;

const int* auto_member_pointer_variable_address() {
    return &auto_member_pointer_variable<
        &auto_member_pointer_target::field>;
}

const int* auto_member_function_pointer_variable_address() {
    return &auto_member_function_pointer_variable<
        &auto_member_pointer_target::get>;
}

template<auto Pointer>
struct auto_member_pointer_holder {
    int read() const {
        return auto_member_pointer_object.*Pointer;
    }
};

template<auto Pointer>
struct auto_member_function_pointer_holder {
    int read() const {
        return (auto_member_pointer_object.*Pointer)();
    }
};

template<auto... Pointers>
int auto_member_pointer_pack_sum() {
    return ((auto_member_pointer_object.*Pointers) + ...);
}

template<auto... Pointers>
int auto_member_function_pointer_pack_sum() {
    return ((auto_member_pointer_object.*Pointers)() + ...);
}

template<auto... Pointers>
bool auto_member_pointer_pack_is_null() {
    return ((Pointers == nullptr) && ...);
}

template<auto... Pointers>
struct auto_member_pointer_pack_holder {
    int read() const {
        return ((auto_member_pointer_object.*Pointers) + ...);
    }
};

template<auto... Pointers>
struct auto_member_function_pointer_pack_holder {
    int read() const {
        return ((auto_member_pointer_object.*Pointers)() + ...);
    }
};

template<auto... Pointers>
inline constexpr int auto_member_pointer_pack_variable =
    sizeof...(Pointers);

const int* auto_member_pointer_pack_variable_address() {
    return &auto_member_pointer_pack_variable<
        &auto_member_pointer_target::field,
        &auto_member_pointer_target::get>;
}

template<int auto_member_pointer_target::*Pointer>
int auto_typed_member_pointer_tag() {
    return 29;
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

template<class Type, auto... Values>
struct variable_argument_pack {
    Type count;
};

template<class Type, auto... Values>
inline constexpr variable_argument_pack<Type, Values...>
    variable_argument_pack_value{static_cast<Type>(sizeof...(Values))};

template<class Type, auto... Values>
int deduce_variable_argument_pack(
    variable_argument_pack<Type, Values...> argument) {
    return static_cast<int>(
        argument.count + (0 + ... + static_cast<int>(Values)));
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
auto_member_pointer_holder<&auto_member_pointer_target::field>
    auto_member_pointer_instance;
auto_member_function_pointer_holder<&auto_member_pointer_target::get>
    auto_member_function_pointer_instance;
auto_member_pointer_pack_holder<
    &auto_member_pointer_target::field,
    &auto_member_pointer_target::field> auto_member_pointer_pack_instance;
auto_member_function_pointer_pack_holder<
    &auto_member_pointer_target::get,
    &auto_member_pointer_target::get>
        auto_member_function_pointer_pack_instance;
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
                   deduce_variable_argument_pack(
                       variable_argument_pack_value<unsigned,
                                                    2, 3u, 4>) == 12 &&
                   auto_default_constant<>(3) == 7 &&
                   auto_default_constant<-2>(9) == 7 &&
                   auto_pointer_value<&auto_pointer_target>() == -19 &&
                   auto_pointer_value<auto_pointer_alias>() == -19 &&
                   auto_pointer_variable<&auto_class_pointer_target> == -23 &&
                   auto_member_pointer_tag<
                       &auto_member_pointer_target::field>() == 19 &&
                   auto_member_pointer_tag<
                       auto_member_data_pointer_alias>() == 19 &&
                   auto_member_pointer_tag<
                       &auto_member_pointer_target::get>() == 19 &&
                   auto_member_pointer_tag<static_cast<
                       int auto_member_pointer_target::*>(nullptr)>() == 19 &&
                   auto_typed_member_pointer_tag<
                       &auto_member_pointer_target::field>() == 29 &&
                   auto_typed_member_pointer_tag<nullptr>() == 29 &&
                   auto_member_pointer_is_null<static_cast<
                       int auto_member_pointer_target::*>(nullptr)>() &&
                   auto_member_pointer_is_null<
                       static_cast<auto_member_function_pointer_type>(
                           nullptr)>() &&
                   auto_typed_member_function_pointer_tag<
                       &auto_member_pointer_target::get>() == 37 &&
                   auto_typed_member_function_pointer_tag<
                       auto_member_function_pointer_alias>() == 37 &&
                   auto_typed_member_pointer_is_null<nullptr>() &&
                   auto_typed_member_function_pointer_is_null<nullptr>() &&
                   auto_member_pointer_variable<
                       &auto_member_pointer_target::field> == 23 &&
                   *auto_member_pointer_variable_address() == 23 &&
                   auto_member_function_pointer_variable<
                       &auto_member_pointer_target::get> == 31 &&
                   *auto_member_function_pointer_variable_address() == 31 &&
                   auto_member_pointer_read<
                       &auto_member_pointer_target::field>() == 5 &&
                   auto_member_pointer_read<
                       auto_member_data_pointer_alias>() == 5 &&
                   auto_member_function_pointer_read<
                       &auto_member_pointer_target::get>() == 9 &&
                   auto_member_function_pointer_read<
                       auto_member_function_pointer_alias>() == 9 &&
                   auto_member_pointer_instance.read() == 5 &&
                   auto_member_function_pointer_instance.read() == 9 &&
                   auto_member_pointer_pack_sum<
                       &auto_member_pointer_target::field,
                       &auto_member_pointer_target::field>() == 10 &&
                   auto_member_function_pointer_pack_sum<
                       &auto_member_pointer_target::get,
                       &auto_member_pointer_target::get>() == 18 &&
                   auto_member_pointer_pack_is_null<
                       static_cast<int auto_member_pointer_target::*>(nullptr),
                       static_cast<auto_member_function_pointer_type>(
                           nullptr)>() &&
                   auto_member_pointer_pack_instance.read() == 10 &&
                   auto_member_function_pointer_pack_instance.read() == 18 &&
                   auto_member_pointer_pack_variable<
                       &auto_member_pointer_target::field,
                       &auto_member_pointer_target::get> == 2 &&
                   *auto_member_pointer_pack_variable_address() == 2 &&
                   auto_pointer_holder<&auto_class_pointer_target>::value == -23 &&
                   auto_pointer_instance.read() == -23 &&
                   auto_value_pack_mixed_instance.read() == 12 &&
                   auto_value_pack_signed_instance.read() == 12 &&
                   auto_value_pack_unsigned_instance.read() == 12
               ? 0
               : 1;
}
