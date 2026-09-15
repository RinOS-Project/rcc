int main(void) {
    const int value = 1;
    long* invalid = const_cast<long*>(&value);
    return *invalid;
}
