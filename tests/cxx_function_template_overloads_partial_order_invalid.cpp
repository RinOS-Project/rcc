template<typename T>
int select_template(T first, int second) {
    return (int)first + (int)second;
}

template<typename T>
long select_template(int first, T second) {
    return (long)first + (long)second + 100;
}

template<typename T>
int select_incomparable_reference_template(T marker, const int& value) {
    return (int)marker + value;
}

template<typename T>
int select_incomparable_reference_template(T marker, volatile int& value) {
    return (int)marker + value + 100;
}

template<typename T>
int select_by_value_or_const_reference(T value) {
    return (int)value;
}

template<typename T>
int select_by_value_or_const_reference(const T& value) {
    return (int)value + 100;
}

template<typename T>
int select_by_value_or_lvalue_reference(T value) {
    return (int)value;
}

template<typename T>
int select_by_value_or_lvalue_reference(T& value) {
    return (int)value + 200;
}

template<typename T>
int select_by_value_or_rvalue_reference(T value) {
    return (int)value;
}

template<typename T>
int select_by_value_or_rvalue_reference(T&& value) {
    return (int)value + 300;
}

template<typename T>
int select_by_value_or_pointer_lvalue_reference(T* value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_pointer_lvalue_reference(T*& value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_const_pointer_reference(T* value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_const_pointer_reference(T* const& value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_volatile_pointer_reference(T* value) {
    return value != nullptr;
}

template<typename T>
int select_by_value_or_volatile_pointer_reference(T* volatile& value) {
    return value != nullptr;
}

template<typename T> requires (sizeof(T) > 0)
int select_incomparable_constraints(T) {
    return 1;
}

template<typename T> requires (sizeof(T) >= 1)
int select_incomparable_constraints(T) {
    return 2;
}

template<typename Element, int Extent>
int select_crossed_array_pointer_extent(Element (*)[Extent],
                                        Element (*)[1]) {
    return 3;
}

template<typename Element, int Extent>
int select_crossed_array_pointer_extent(Element (*)[1],
                                        Element (*)[Extent]) {
    return 4;
}

template<int First, int Second>
struct IncomparableRepeatedNontypeTag {};

template<int Value, int First, int Second>
int select_incomparable_repeated_nontype_order(
    IncomparableRepeatedNontypeTag<Value, Value>,
    IncomparableRepeatedNontypeTag<First, Second>) {
    return 5;
}

template<int First, int Second, int Value>
int select_incomparable_repeated_nontype_order(
    IncomparableRepeatedNontypeTag<First, Second>,
    IncomparableRepeatedNontypeTag<Value, Value>) {
    return 6;
}

template<int First, int Second>
struct IncomparableFixedNontypeTag {};

template<int First, int Second>
int select_incomparable_fixed_nontype_order(
    IncomparableFixedNontypeTag<1, First>,
    IncomparableFixedNontypeTag<Second, 1>) {
    return 7;
}

template<int First, int Second>
int select_incomparable_fixed_nontype_order(
    IncomparableFixedNontypeTag<First, 1>,
    IncomparableFixedNontypeTag<1, Second>) {
    return 8;
}

template<typename First, typename... Rest>
int select_crossed_pointer_pack_order(First*, Rest...) {
    return 9;
}

template<typename First, typename... Rest>
int select_crossed_pointer_pack_order(First, Rest*...) {
    return 10;
}

template<typename... Rest>
int select_nontrailing_function_pack_too_many(Rest..., int) {
    return 11;
}

int main() {
    int value = 3;
    int* pointer = &value;
    char character = 0;
    int array[1] = { value };
    return (int)select_template((short)1, (short)2) +
           select_incomparable_reference_template(1, value) +
           select_by_value_or_const_reference(value) +
           select_by_value_or_lvalue_reference(value) +
           select_by_value_or_rvalue_reference(3) +
           select_by_value_or_pointer_lvalue_reference(pointer) +
           select_by_value_or_const_pointer_reference(pointer) +
           select_by_value_or_volatile_pointer_reference(pointer) +
           select_incomparable_constraints(value) +
           select_crossed_array_pointer_extent(&array, &array) +
           select_incomparable_repeated_nontype_order(
               IncomparableRepeatedNontypeTag<1, 1>{},
               IncomparableRepeatedNontypeTag<2, 2>{}) +
           select_incomparable_fixed_nontype_order(
               IncomparableFixedNontypeTag<1, 1>{},
               IncomparableFixedNontypeTag<1, 1>{}) +
           select_crossed_pointer_pack_order(pointer, &character) +
           select_nontrailing_function_pack_too_many(value, value + 1);
}
