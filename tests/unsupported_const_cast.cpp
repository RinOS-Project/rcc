int main(void) {
    const int value = 1;
    int* writable = const_cast<int*>(&value);
    return *writable;
}
