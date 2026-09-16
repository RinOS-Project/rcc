extern "C" int may_throw(int value) {
    return value;
}

extern "C" int always_safe(int value) noexcept {
    return value;
}

int conditionally_safe(int value) noexcept(1) {
    return value;
}

int conditionally_throwing(int value) noexcept(0) {
    return value;
}

struct NoexceptBox {
    int value;

    int read() const noexcept(1) {
        return value;
    }
};

struct ThrowingBox {
    int value;

    int read() const noexcept(0) {
        return value;
    }
};

static_assert(noexcept(always_safe(1)),
              "plain noexcept function call must be non-throwing");
static_assert(!noexcept(may_throw(1)),
              "ordinary function call must remain potentially throwing");
static_assert(noexcept(noexcept(may_throw(1))),
              "the noexcept operator itself must be non-throwing");
static_assert(noexcept(conditionally_safe(1)),
              "a true conditional noexcept specification must be non-throwing");
static_assert(!noexcept(conditionally_throwing(1)),
              "a false conditional noexcept specification must remain throwing");
static_assert(noexcept(1 + 2 * 3),
              "scalar expressions must be non-throwing");

extern "C" int probe_noexcept_expression(void) {
    NoexceptBox safe{7};
    ThrowingBox unsafe{9};
    int result = 0;

    if (!noexcept(always_safe(1))) result = 1;
    if (noexcept(may_throw(1))) result = 2;
    if (!noexcept(noexcept(may_throw(1)))) result = 3;
    if (!noexcept((1 + 2) * 3)) result = 4;
    if (!noexcept(conditionally_safe(1))) result = 5;
    if (noexcept(conditionally_throwing(1))) result = 6;
    if (!noexcept(safe.read())) result = 7;
    if (noexcept(unsafe.read())) result = 8;
    return result + safe.value - 7 + unsafe.value - 9;
}
