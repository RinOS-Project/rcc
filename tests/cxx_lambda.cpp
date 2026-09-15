int main() {
    int base = 20;
    int direct = [](int left, int right) -> int {
        return left + right;
    }(20, 22);
    int captured = [base](int extra) -> int {
        return base + extra;
    }(22);
    int default_captured = [=](int extra) -> int {
        return base + extra;
    }(22);
    int referenced = [&base](int extra) -> int {
        base += extra;
        return base;
    }(2);
    int default_referenced = [&](int extra) -> int {
        base += extra;
        return base;
    }(3);
    return direct == 42 && captured == 42 && default_captured == 42 &&
           referenced == 22 && default_referenced == 25 && base == 25
        ? 0 : 1;
}
