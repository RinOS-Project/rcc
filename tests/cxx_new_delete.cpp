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

int initialize_scalar() {
    int* value = new int(7);
    int result = *value;
    delete value;
    return result == 7 ? 0 : 1;
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

int main() {
    return initialize_scalar() + initialize_pair() +
           value_initialize_scalar();
}
