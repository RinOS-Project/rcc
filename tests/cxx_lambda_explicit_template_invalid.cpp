int main() {
    auto invalid = []<int... Values>(int value) {
        return value + sizeof...(Values);
    }(1);
    return invalid;
}
