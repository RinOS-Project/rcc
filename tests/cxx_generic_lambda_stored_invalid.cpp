int main() {
    auto stored = [](auto value) {
        return value + 1;
    };
    (void)stored;
    return 0;
}
