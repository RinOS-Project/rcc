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
