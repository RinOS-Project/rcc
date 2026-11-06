static int copy_count;
static int destructor_count;

struct ConditionalCopy {
    int value;

    explicit ConditionalCopy(int initial) noexcept {
        value = initial;
    }

    ConditionalCopy(const ConditionalCopy& source) noexcept {
        value = source.value + 100;
        ++copy_count;
    }

    ~ConditionalCopy() noexcept {
        ++destructor_count;
    }
};

static int consume(const ConditionalCopy& value) noexcept {
    return value.value;
}

int main() {
    ConditionalCopy left(7);
    ConditionalCopy right(9);

    int result = consume(true ? left : ConditionalCopy{80});
    if (result != 107) return 11;
    if (copy_count != 1) return 12;
    if (destructor_count != 1) return 13;

    result = consume(false ? left : ConditionalCopy{80});
    if (result != 80) return 21;
    if (copy_count != 1) return 22;
    if (destructor_count != 2) return 23;

    result = consume(true ? ConditionalCopy{90} : right);
    if (result != 90) return 31;
    if (copy_count != 1) return 32;
    if (destructor_count != 3) return 33;

    result = consume(false ? ConditionalCopy{90} : right);
    if (result != 109) return 41;
    if (copy_count != 2) return 42;
    if (destructor_count != 4) return 43;

    return 0;
}
