template<typename T>
int read_const_reference(const T& value) {
    return value;
}

template<typename T>
int update_lvalue(T& value) {
    value += 3;
    return value;
}

template<typename T>
int read_rvalue(T&& value) {
    return value;
}

template<typename T>
T copy_from_const_pointer(const T* value) {
    T copy = *value;
    copy += 1;
    return copy;
}

int double_value(int value) {
    return value + value;
}

int update_rvalue_reference_local(int&& value) {
    int&& local = static_cast<int&&>(value);
    local += 2;
    return value;
}

int& return_lvalue_reference(int& value) {
    return static_cast<int&>(value);
}

int&& return_rvalue_reference(int&& value) {
    return static_cast<int&&>(value);
}

int& select_conditional_lvalue(bool choose_first, int& first, int& second) {
    return choose_first ? first : second;
}

int&& select_conditional_xvalue(bool choose_first, int&& first, int&& second) {
    return choose_first ? static_cast<int&&>(first)
                        : static_cast<int&&>(second);
}

struct ReferenceMemberValue {
    int value;
};

int&& return_member_xvalue(ReferenceMemberValue&& object) {
    return static_cast<ReferenceMemberValue&&>(object).value;
}

decltype(auto) return_member_xvalue_auto(ReferenceMemberValue&& object) {
    return (static_cast<ReferenceMemberValue&&>(object).value);
}

decltype(auto) select_conditional_decltype_xvalue(
        bool choose_first, int&& first, int&& second) {
    return (choose_first ? static_cast<int&&>(first)
                         : static_cast<int&&>(second));
}

template<typename R, typename A>
R invoke(R (*function)(A), A value) {
    return function(value);
}

template<typename T, typename U = T>
U default_type_copy(T value) {
    U result{};
    result = value;
    return result;
}

template<typename T>
int read_array_element(T* value) {
    return value[1];
}

int main() {
    const int constant = 7;
    int mutable_value = 4;
    int values[2] = {5, 6};
    if (read_const_reference(constant) != 7) return 1;
    if (update_lvalue(mutable_value) != 7) return 2;
    if (mutable_value != 7) return 3;
    if (update_rvalue_reference_local(
            static_cast<int&&>(mutable_value)) != 9) return 4;
    if (mutable_value != 9) return 5;
    int& lvalue_result = return_lvalue_reference(mutable_value);
    lvalue_result += 1;
    if (mutable_value != 10) return 6;
    int&& rvalue_result = return_rvalue_reference(
            static_cast<int&&>(mutable_value));
    rvalue_result += 2;
    if (mutable_value != 12) return 7;
    int other_value = 20;
    int& selected_lvalue = select_conditional_lvalue(
            false, mutable_value, other_value);
    selected_lvalue += 1;
    if (mutable_value != 12 || other_value != 21) return 8;
    char small_first = 'a';
    char small_second = 'b';
    (true ? small_first : small_second) = 'z';
    if (small_first != 'z' || small_second != 'b') return 9;
    int&& selected_xvalue = select_conditional_xvalue(
            true, static_cast<int&&>(mutable_value),
            static_cast<int&&>(other_value));
    selected_xvalue += 2;
    if (mutable_value != 14 || other_value != 21) return 10;
    decltype(auto) deduced_xvalue = select_conditional_decltype_xvalue(
            false, static_cast<int&&>(mutable_value),
            static_cast<int&&>(other_value));
    deduced_xvalue += 3;
    if (mutable_value != 14 || other_value != 24) return 11;
    ReferenceMemberValue member_value{};
    member_value.value = 25;
    int&& member_result = return_member_xvalue(
            static_cast<ReferenceMemberValue&&>(member_value));
    member_result += 4;
    if (member_value.value != 29) return 12;
    decltype(auto) member_auto_result = return_member_xvalue_auto(
            static_cast<ReferenceMemberValue&&>(member_value));
    member_auto_result += 2;
    if (member_value.value != 31) return 13;
    if (read_rvalue(9) != 9) return 14;
    if (read_rvalue(mutable_value) != 14) return 15;
    if (invoke(double_value, 6) != 12) return 16;
    if (copy_from_const_pointer(&constant) != 8) return 17;
    if (default_type_copy(13) != 13) return 18;
    if (read_array_element(values) != 6) return 19;
    return 0;
}
