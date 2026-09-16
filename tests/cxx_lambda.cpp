struct LambdaPoint {
    int x;
    int y;

    int capture_this(int extra) {
        return [this](int value) {
            return this->x + value;
        }(extra);
    }

    int capture_this_by_default(int extra) {
        return [=](int value) {
            return this->y + value;
        }(extra);
    }
};

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
    int mixed_referenced = [=, &base](int extra) -> int {
        base += extra;
        return base;
    }(4);
    int mixed_copied = [&, base](int extra) -> int {
        return base + extra;
    }(5);
    double inferred_float = [](double value) {
        return value + 0.5;
    }(1.0);
    [&]() {
        base += 0;
    }();
    LambdaPoint point;
    point.x = 30;
    point.y = 40;
    return direct == 42 && captured == 42 && default_captured == 42 &&
           referenced == 22 && default_referenced == 25 &&
           mixed_referenced == 29 && mixed_copied == 34 && base == 29 &&
           inferred_float == 1.5 &&
           point.capture_this(12) == 42 &&
           point.capture_this_by_default(2) == 42
        ? 0 : 1;
}
