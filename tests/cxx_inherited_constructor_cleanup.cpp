static int inherited_cleanup_events[64];
static int inherited_cleanup_event_count;
static int inherited_cleanup_throw_after;

static void inherited_cleanup_construct(int id) {
    inherited_cleanup_events[inherited_cleanup_event_count++] = id;
    if (inherited_cleanup_throw_after == id) throw id + 100;
}

static void inherited_cleanup_destroy(int id) {
    inherited_cleanup_events[inherited_cleanup_event_count++] = -id;
}

struct InheritedCleanupLeafPrefix {
    InheritedCleanupLeafPrefix() { inherited_cleanup_construct(1); }
    ~InheritedCleanupLeafPrefix() { inherited_cleanup_destroy(1); }
};

struct InheritedCleanupMiddlePrefix {
    InheritedCleanupMiddlePrefix() { inherited_cleanup_construct(2); }
    ~InheritedCleanupMiddlePrefix() { inherited_cleanup_destroy(2); }
};

struct InheritedCleanupSelectedBase {
    template <typename T>
    explicit InheritedCleanupSelectedBase(T value) {
        (void)value;
        inherited_cleanup_construct(3);
    }

    ~InheritedCleanupSelectedBase() { inherited_cleanup_destroy(3); }
};

struct InheritedCleanupEmptySelectedBase {
    template <typename T>
    explicit InheritedCleanupEmptySelectedBase(T value) {}

    ~InheritedCleanupEmptySelectedBase() { inherited_cleanup_destroy(8); }
};

struct InheritedCleanupMiddleSuffix {
    InheritedCleanupMiddleSuffix() { inherited_cleanup_construct(4); }
    ~InheritedCleanupMiddleSuffix() { inherited_cleanup_destroy(4); }
};

struct InheritedCleanupMiddleMember {
    InheritedCleanupMiddleMember() { inherited_cleanup_construct(5); }
    ~InheritedCleanupMiddleMember() { inherited_cleanup_destroy(5); }
};

struct InheritedCleanupLeafMember {
    InheritedCleanupLeafMember() { inherited_cleanup_construct(7); }
    ~InheritedCleanupLeafMember() { inherited_cleanup_destroy(7); }
};

struct InheritedCleanupMiddle : InheritedCleanupMiddlePrefix,
                                InheritedCleanupSelectedBase,
                                InheritedCleanupMiddleSuffix {
    using InheritedCleanupSelectedBase::InheritedCleanupSelectedBase;
    InheritedCleanupMiddleMember middle_member{};
};

struct InheritedCleanupLeafSuffix {
    InheritedCleanupLeafSuffix() { inherited_cleanup_construct(6); }
    ~InheritedCleanupLeafSuffix() { inherited_cleanup_destroy(6); }
};

struct InheritedCleanupLeaf : InheritedCleanupLeafPrefix,
                              InheritedCleanupMiddle,
                              InheritedCleanupLeafSuffix {
    using InheritedCleanupMiddle::InheritedCleanupMiddle;
    InheritedCleanupLeafMember leaf_member{};
};

struct InheritedCleanupThrowingMember {
    InheritedCleanupThrowingMember() { inherited_cleanup_construct(9); }
};

struct InheritedCleanupEmptyDerived : InheritedCleanupEmptySelectedBase {
    using InheritedCleanupEmptySelectedBase::InheritedCleanupEmptySelectedBase;
    InheritedCleanupThrowingMember member{};
};

static int run_inherited_cleanup_case(int throw_after, int exception_value,
                                      const int* expected,
                                      int expected_count) {
    int caught = 0;
    inherited_cleanup_event_count = 0;
    inherited_cleanup_throw_after = throw_after;
    try {
        InheritedCleanupLeaf object(17);
        (void)object;
    } catch (int value) {
        caught = value;
    }
    if (caught != exception_value ||
        inherited_cleanup_event_count != expected_count) {
        return 1;
    }
    for (int index = 0; index < expected_count; ++index) {
        if (inherited_cleanup_events[index] != expected[index]) return 1;
    }
    return 0;
}

static int run_inherited_empty_base_cleanup_case() {
    int caught = 0;
    inherited_cleanup_event_count = 0;
    inherited_cleanup_throw_after = 9;
    try {
        InheritedCleanupEmptyDerived object(17);
        (void)object;
    } catch (int value) {
        caught = value;
    }
    return caught == 109 && inherited_cleanup_event_count == 2 &&
                   inherited_cleanup_events[0] == 9 &&
                   inherited_cleanup_events[1] == -8
        ? 0 : 1;
}

int main() {
    static const int selected_base_throws[] = {1, 2, 3, -2, -1};
    static const int middle_suffix_throws[] = {1, 2, 3, 4, -3, -2, -1};
    static const int middle_member_throws[] = {
        1, 2, 3, 4, 5, -4, -3, -2, -1};
    static const int leaf_suffix_throws[] = {
        1, 2, 3, 4, 5, 6, -5, -4, -3, -2, -1};
    static const int leaf_member_throws[] = {
        1, 2, 3, 4, 5, 6, 7, -6, -5, -4, -3, -2, -1};

    return (run_inherited_cleanup_case(
                3, 103, selected_base_throws,
                (int)(sizeof(selected_base_throws) /
                      sizeof(selected_base_throws[0]))) << 0) |
        (run_inherited_cleanup_case(
             4, 104, middle_suffix_throws,
             (int)(sizeof(middle_suffix_throws) /
                   sizeof(middle_suffix_throws[0]))) << 1) |
        (run_inherited_cleanup_case(
             5, 105, middle_member_throws,
             (int)(sizeof(middle_member_throws) /
                   sizeof(middle_member_throws[0]))) << 2) |
        (run_inherited_cleanup_case(
             6, 106, leaf_suffix_throws,
             (int)(sizeof(leaf_suffix_throws) /
                   sizeof(leaf_suffix_throws[0]))) << 3) |
        (run_inherited_cleanup_case(
             7, 107, leaf_member_throws,
             (int)(sizeof(leaf_member_throws) /
                   sizeof(leaf_member_throws[0]))) << 4) |
        (run_inherited_empty_base_cleanup_case() << 5);
}
