struct ExceptionProbe {
    int* destructions;

    ~ExceptionProbe() noexcept {
        ++*destructions;
    }
};

struct ExceptionAggregate {
    const ExceptionProbe& probe;
    int value;
};

static int throw_from_later_initializer() {
    throw 67;
}

int main() {
    int destructions = 0;
    int caught = 0;
    try {
        ExceptionAggregate aggregate(
            ExceptionProbe{&destructions},
            throw_from_later_initializer());
        (void)aggregate;
        return 1;
    } catch (int value) {
        caught = value;
    }
    return caught == 67 && destructions == 1 ? 0 : 2;
}
