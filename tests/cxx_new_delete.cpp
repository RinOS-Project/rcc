int allocate_scalar() {
    int* value = new int;
    delete value;
    return 0;
}

int allocate_array() {
    int* values = new int[3];
    delete[] values;
    return 0;
}

struct Pair {
    int left;
    int right;
    Pair(int first, int second) : left(first), right(second) {}
};

int next_array_count = 0;

int array_count() {
    ++next_array_count;
    return 3;
}

int initialize_scalar() {
    int* value = new int(7);
    int result = *value;
    delete value;
    return result == 7 ? 0 : 1;
}

extern "C" void cleanup_pointer(int* value) {
    if (value) *value = *value + 1;
}

struct OwnedPointer {
    int* value;

    OwnedPointer(int* input) : value(input) {}

    ~OwnedPointer() {
        if (value != 0) (void)cleanup_pointer(value);
    }
};

int initialize_and_delete_class() {
    int value = 9;
    OwnedPointer* object = new OwnedPointer(&value);
    delete object;
    return value == 10 ? 0 : 1;
}

int delete_null_class() {
    OwnedPointer* object = nullptr;
    delete object;
    return 0;
}

int initialize_pair() {
    Pair* value = new Pair(11, 31);
    int result = value->left + value->right;
    delete value;
    return result == 42 ? 0 : 1;
}

int value_initialize_scalar() {
    int* value = new int();
    int result = *value;
    delete value;
    return result == 0 ? 0 : 1;
}

int value_initialize_array() {
    int* values = new int[3]();
    int result = values[0] + values[1] + values[2];
    delete[] values;
    return result == 0 ? 0 : 1;
}

int value_initialize_dynamic_array() {
    int* values = new int[array_count()]();
    int result = values[0] + values[1] + values[2];
    delete[] values;
    return next_array_count == 1 && result == 0 ? 0 : 1;
}

int main() {
    return initialize_scalar() + initialize_pair() +
           value_initialize_scalar() + value_initialize_array() +
           value_initialize_dynamic_array() +
           initialize_and_delete_class() + delete_null_class();
}
