static int cleanup_array_destruction_count;

struct CleanupArrayElement {
    CleanupArrayElement() : value(1) {}
    ~CleanupArrayElement() {
        cleanup_array_destruction_count =
            cleanup_array_destruction_count + 1;
    }
    int value;
};

struct CleanupArrayBase {
    explicit CleanupArrayBase(int initial_value) : value(initial_value) {}
    int value;
};

int main() {
    cleanup_array_destruction_count = 0;
    {
        struct CleanupArrayDerived : CleanupArrayBase {
            using CleanupArrayBase::CleanupArrayBase;
            CleanupArrayElement elements[2];
        };
        CleanupArrayDerived object(7);

        if (object.value != 7 || object.elements[0].value != 1 ||
            object.elements[1].value != 1 ||
            cleanup_array_destruction_count != 0) {
            return 1;
        }
    }
    return cleanup_array_destruction_count == 2 ? 0 : 2;
}
