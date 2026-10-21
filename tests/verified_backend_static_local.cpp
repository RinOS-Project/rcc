static int initialization_count;
static int enum_initialization_count;
static int floating_initialization_count;
static int volatile_initialization_count;

enum class StaticKind : unsigned char { expected = 23 };

static int make_static_value() {
    ++initialization_count;
    if (initialization_count == 1) throw 29;
    return 37;
}

static StaticKind make_static_kind() {
    ++enum_initialization_count;
    return StaticKind::expected;
}

static double make_static_floating_value() {
    ++floating_initialization_count;
    return 12.5;
}

static int make_static_volatile_value() {
    ++volatile_initialization_count;
    return 41;
}

int read_function_static_value() {
    static int value = make_static_value();
    return value;
}

int function_static_initialization_count() {
    return initialization_count;
}

int read_function_static_enum() {
    static StaticKind value = make_static_kind();
    return static_cast<int>(value);
}

double read_function_static_floating() {
    static double value = make_static_floating_value();
    return value;
}

int read_function_static_volatile() {
    static volatile int value = make_static_volatile_value();
    return value;
}

int main() {
    if (function_static_initialization_count() != 0) return 1;
    try {
        (void)read_function_static_value();
        return 2;
    } catch (int error) {
        if (error != 29) return 3;
    }
    if (function_static_initialization_count() != 1) return 4;
    if (read_function_static_value() != 37) return 5;
    if (function_static_initialization_count() != 2) return 6;
    if (read_function_static_value() != 37) return 7;
    if (function_static_initialization_count() != 2) return 8;
    if (read_function_static_enum() != 23) return 9;
    if (read_function_static_enum() != 23 ||
        enum_initialization_count != 1) return 10;
    if (read_function_static_floating() != 12.5) return 11;
    if (read_function_static_floating() != 12.5 ||
        floating_initialization_count != 1) return 12;
    if (read_function_static_volatile() != 41) return 13;
    if (read_function_static_volatile() != 41 ||
        volatile_initialization_count != 1) return 14;
    return 0;
}
