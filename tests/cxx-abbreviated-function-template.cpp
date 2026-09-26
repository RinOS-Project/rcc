int add(auto left, auto right) {
    return left + right;
}

int pointed(auto* value) {
    return *value;
}

int forwarded(auto&& value) {
    return value;
}

int reference_add(const auto& left, auto right) {
    return left + right;
}

int main() {
        int value = 4;
        return add(2, 3) == 5 && pointed(&value) == 4 &&
               forwarded(value) == 4 && forwarded(6) == 6 &&
               reference_add(6, 4) == 10 ? 0 : 1;
}
