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

struct ConditionalMoveHolder {
    ConditionalMove member;

    const ConditionalMove& get() const noexcept {
        return member;
    }

    ConditionalMove&& get_xvalue() noexcept {
        return static_cast<ConditionalMove&&>(member);
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

    result = consume(
        true ? *(&left) : ConditionalMove{70});
    if (result != 107) return 51;
    if (move_count != 2 || copy_count != 1) return 52;
    if (destructor_count != 5) return 53;

    result = consume(
        false ? *(&left) : ConditionalMove{70});
    if (result != 70) return 61;
    if (move_count != 2 || copy_count != 1) return 62;
    if (destructor_count != 6) return 63;

    ConditionalMoveHolder holder{ConditionalMove{11}};
    result = consume(true ? holder.member : ConditionalMove{70});
    if (result != 111) return 71;
    if (move_count != 2 || copy_count != 2) return 72;
    if (destructor_count != 7) return 73;

    result = consume(false ? holder.member : ConditionalMove{70});
    if (result != 70) return 81;
    if (move_count != 2 || copy_count != 2) return 82;
    if (destructor_count != 8) return 83;

    result = consume(true ? holder.get() : ConditionalMove{70});
    if (result != 111) return 91;
    if (move_count != 2 || copy_count != 3) return 92;
    if (destructor_count != 9) return 93;

    result = consume(false ? holder.get() : ConditionalMove{70});
    if (result != 70) return 101;
    if (move_count != 2 || copy_count != 3) return 102;
    if (destructor_count != 10) return 103;

    result = consume(true ? holder.get_xvalue() : ConditionalMove{70});
    if (result != 211) return 111;
    if (move_count != 3 || copy_count != 3) return 112;
    if (destructor_count != 11) return 113;

    result = consume(false ? holder.get_xvalue() : ConditionalMove{70});
    if (result != 70) return 121;
    if (move_count != 3 || copy_count != 3) return 122;
    if (destructor_count != 12) return 123;

    return 0;
}
