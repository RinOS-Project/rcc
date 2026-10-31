constexpr int count_down(int value) {
    return value == 0 ? 0 : count_down(value - 1) + 1;
}

static_assert(count_down(100) == 100);

int main() {
    return count_down(100) == 100 ? 0 : 1;
}
