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

struct ThrowingCleanupProbe {
    int* destructions;
    int value;

    ~ThrowingCleanupProbe() noexcept(false) {
        ++*destructions;
        throw 79;
    }
};

struct ThrowingCleanupAggregate {
    const ThrowingCleanupProbe& probe;
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
    if (caught != 67 || destructions != 1) {
        return 2;
    }

    int incomplete_destructions = 0;
    caught = 0;
    try {
        ThrowingCleanupAggregate aggregate(
            ThrowingCleanupProbe{
                &incomplete_destructions, throw_from_later_initializer()},
            81);
        (void)aggregate;
        return 3;
    } catch (int value) {
        caught = value;
    }
    if (caught != 67 || incomplete_destructions != 0) {
        return 4;
    }

    int throwing_destructions = 0;
    caught = 0;
    try {
        ThrowingCleanupAggregate aggregate(
            ThrowingCleanupProbe{&throwing_destructions, 0}, 83);
        (void)aggregate;
        return 5;
    } catch (int value) {
        caught = value;
    }
    return caught == 79 && throwing_destructions == 1 ? 0 : 6;
}
