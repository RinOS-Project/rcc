int main() {
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
    return integer_value == 42 && long_value == 42 && pointer_value == 43 &&
           forwarded == 42 && forwarded_result == 41 && lvalue_result == 42 &&
           rvalue_result == 42
        ? 0 : 1;
}
