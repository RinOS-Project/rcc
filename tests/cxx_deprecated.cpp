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

namespace block_scope_namespace {
int invoke_local_api() {
    [[deprecated("use the namespace declaration")]]
    int legacy_local_namespace_api(int);
    return legacy_local_namespace_api(3);
}

int legacy_local_namespace_api(int value) {
    return value;
}
} // namespace block_scope_namespace

int main() {
    Legacy legacy{};
    [[deprecated("use the namespace declaration")]]
    int legacy_local_api(int);
    [[deprecated("use a replacement local")]]
    int local_old_value = 5, local_old_value2 = 1;
    int result = old_api(1);
    result += legacy_local_api(2);
    result += block_scope_namespace::invoke_local_api();
    result += old_value;
    result += legacy.old_method();
    result += legacy.old_field;
    result += local_old_value;
    result += local_old_value2;
    result += modern_api(1);
    return result == 27 ? 0 : 1;
}

int legacy_local_api(int value) {
    return value;
}
