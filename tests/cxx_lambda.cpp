int main() {
    int base = 20;
    int direct = [](int left, int right) -> int {
        return left + right;
    }(20, 22);
    int captured = [base](int extra) -> int {
        return base + extra;
    }(22);
    int referenced = [&base](int extra) -> int {
        base += extra;
        return base;
    }(2);
    return direct == 42 && captured == 42 && referenced == 22 && base == 22
        ? 0 : 1;
}
