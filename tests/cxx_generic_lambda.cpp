int main() {
    int captured_base = 20;
    int captured_value = [captured_base](auto value) {
        return captured_base + value;
    }(22);
    int referenced_value = [&captured_base](auto value) {
        captured_base += value;
        return captured_base;
    }(1);
    int packed_value = [](auto... values) {
        return (... + values);
    }(1, 2, 3);
    int packed_empty = [](auto... values) {
        return (10 + ... + values);
    }();
    int packed_with_seed = [](int seed, auto... values) {
        return (seed + ... + values);
    }(10, 1, 2);
    int first_pointer = 1;
    int second_pointer = 2;
    int packed_pointers = [](auto*... values) {
        return (... && values);
    }(&first_pointer, &second_pointer);
    int integer_value = [](auto value) {
        return value + 1;
    }(41);
    long long_value = [](const auto& value) {
        return value + 1;
    }((long)41);
    int pointer_value = [](auto* value) {
        return *value + 1;
    }(&integer_value);
    int forwarded = 1;
    int forwarded_result = [](auto&& value) {
        value += 40;
        return value;
    }(forwarded);
    int lvalue_result = [](auto& value) {
        value += 1;
        return value;
    }(forwarded);
    int rvalue_result = [](auto&& value) {
        return value + 1;
    }(41);
    return captured_value == 42 && referenced_value == 21 &&
           captured_base == 21 && integer_value == 42 &&
           long_value == 42 && pointer_value == 43 &&
           packed_value == 6 && packed_empty == 10 &&
           packed_with_seed == 13 &&
           packed_pointers == 1 &&
           forwarded == 42 && forwarded_result == 41 && lvalue_result == 42 &&
           rvalue_result == 42
        ? 0 : 1;
}
