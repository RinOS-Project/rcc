int main() {
    int value = 1;
    auto invalid = []<int... Values>(int (&...inputs)[Values]) {
        return (... + Values);
    }(value);
    return invalid;
}
