#define RCC_MAYBE_UNUSED_ATTRIBUTE maybe_unused

#ifdef __RCC__
#if !defined(__has_c_attribute)
#error "C23 __has_c_attribute is unavailable"
#endif
#if __has_c_attribute(RCC_MAYBE_UNUSED_ATTRIBUTE) != 202311L
#error "maybe_unused attribute version is incorrect"
#endif
#endif

[[maybe_unused]] static int unused_global;
typedef int MaybeUnusedInteger [[maybe_unused]];

struct [[maybe_unused]] MaybeUnusedRecord {
    int field [[maybe_unused]];
};

union [[maybe_unused]] MaybeUnusedUnion {
    int field;
};

enum [[maybe_unused]] MaybeUnusedEnum {
    maybe_unused_enumerator [[maybe_unused]] = 7
};

[[maybe_unused]] static int maybe_unused_function(
    [[maybe_unused]] int parameter) {
    [[maybe_unused]] int local = parameter;
    [[maybe_unused]] unused_label: return local;
}

[[maybe_unused]] static int maybe_unused_case_label(int value) {
    switch (value) {
        [[maybe_unused]] case 0: return 1;
        [[maybe_unused]] default: return 2;
    }
}

int maybe_unused_attribute_runtime(void) {
    struct MaybeUnusedRecord record = { 3 };
    MaybeUnusedInteger value = record.field;
    return value + maybe_unused_enumerator;
}
