int main(void) {
    typeof(const int) value = 1;
    typeof(const int * const) pointer = &value;
    value = 2;
    *pointer = 3;
    return value;
}
