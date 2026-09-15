struct DecltypePair {
    int left;
    int right;
};

extern "C" int probe_decltype(void) {
    int value = 41;
    int* pointer = &value;
    DecltypePair pair = { 7, 9 };
    decltype(value) copy = value + 1;
    decltype((value)) reference = copy;
    decltype(*pointer) dereferenced = value;
    decltype(pair.left) member = pair.right;
    reference += 1;
    dereferenced += 1;
    return value == 42 && copy == 43 && member == 9 && dereferenced == 42
               ? 0
               : 1;
}
