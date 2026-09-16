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
    return integer_value == 42 && long_value == 42 && pointer_value == 43
        ? 0 : 1;
}
