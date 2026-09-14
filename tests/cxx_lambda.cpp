int main() {
    int base = 20;
    int direct = [](int left, int right) -> int {
        return left + right;
    }(20, 22);
    int captured = [base](int extra) -> int {
        return base + extra;
    }(22);
    return direct == 42 && captured == 42 ? 0 : 1;
}
