[[deprecated("use modern_api")]] int old_api(int value) {
    return value + 1;
}

int modern_api(int value) {
    return value + 2;
}

[[deprecated]] int old_value = 7;

class Legacy {
public:
    [[deprecated("use current_method")]] int old_method() {
        return 4;
    }
    [[deprecated("use current_field")]] int old_field;
};

int main() {
    Legacy legacy{};
    int result = old_api(1);
    result += old_value;
    result += legacy.old_method();
    result += legacy.old_field;
    result += modern_api(1);
    return result == 16 ? 0 : 1;
}
