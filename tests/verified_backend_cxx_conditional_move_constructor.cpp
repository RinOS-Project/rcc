static int move_count;
static int copy_count;
static int destructor_count;

struct ConditionalMove {
    int value;

    explicit ConditionalMove(int initial) noexcept {
        value = initial;
    }

    ConditionalMove(const ConditionalMove& source) noexcept {
        value = source.value + 100;
        ++copy_count;
    }

    ConditionalMove(ConditionalMove&& source) noexcept {
        value = source.value + 200;
        ++move_count;
    }

    ~ConditionalMove() noexcept {
        ++destructor_count;
    }
};

static int consume(const ConditionalMove& value) noexcept {
    return value.value;
}

int main() {
    ConditionalMove left(7);

    int result = consume(
        true ? static_cast<ConditionalMove&&>(left)
             : ConditionalMove{80});
    if (result != 207) return 11;
    if (move_count != 1 || copy_count != 0) return 12;
    if (destructor_count != 1) return 13;

    result = consume(
        false ? static_cast<ConditionalMove&&>(left)
              : ConditionalMove{80});
    if (result != 80) return 21;
    if (move_count != 1 || copy_count != 0) return 22;
    if (destructor_count != 2) return 23;

    result = consume(
        true ? ConditionalMove{90}
             : static_cast<ConditionalMove&&>(left));
    if (result != 90) return 31;
    if (move_count != 1 || copy_count != 0) return 32;
    if (destructor_count != 3) return 33;

    result = consume(
        false ? ConditionalMove{90}
              : static_cast<ConditionalMove&&>(left));
    if (result != 207) return 41;
    if (move_count != 2 || copy_count != 0) return 42;
    if (destructor_count != 4) return 43;

    return 0;
}
