void* rcc_test_consume(void*);

int rcc_test_const_array_to_void_pointer() {
    const int rows[2][3] = {{0}};
    rcc_test_consume(rows);
    return 0;
}
