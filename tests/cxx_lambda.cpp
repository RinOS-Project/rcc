int main() {
    return [](int left, int right) -> int { return left + right; }(20, 22) == 42
        ? 0 : 1;
}
