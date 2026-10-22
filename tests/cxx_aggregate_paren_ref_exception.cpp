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

#if !defined(TEST_VERIFIED_BACKEND)
struct ThrowingCleanupProbe {
    int* destructions;

    ~ThrowingCleanupProbe() noexcept(false) {
        ++*destructions;
        throw 79;
    }
};

struct ThrowingCleanupAggregate {
    const ThrowingCleanupProbe& probe;
    int value;
};
#endif

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

#if !defined(TEST_VERIFIED_BACKEND)
    int throwing_destructions = 0;
    caught = 0;
    try {
        ThrowingCleanupAggregate aggregate(
            ThrowingCleanupProbe{&throwing_destructions}, 83);
        (void)aggregate;
        return 3;
    } catch (int value) {
        caught = value;
    }
    return caught == 79 && throwing_destructions == 1 ? 0 : 4;
#else
    return 0;
#endif
}
