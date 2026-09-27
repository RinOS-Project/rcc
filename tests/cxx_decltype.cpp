struct DecltypePair {
    int left;
    int right;
};

extern "C" int decltype_factory(void) {
    return 42;
}

extern "C" int probe_decltype(void) {
    int value = 41;
    int* pointer = &value;
    DecltypePair pair = { 7, 9 };
    decltype(value) copy = value + 1;
    decltype((value)) reference = copy;
    decltype(*pointer) dereferenced = value;
    decltype(pair.left) member = pair.right;
    decltype(decltype_factory()) returned = decltype_factory();
    decltype((decltype_factory())) grouped_return = decltype_factory();
    decltype(value + 1) arithmetic = value + 1;
    decltype(value == 41) comparison = false;
    decltype(value ? value : copy) conditional = value;
    reference += 1;
    dereferenced += 1;
    member += 2;
    return value == 42 && copy == 43 && member == 11 && dereferenced == 42 &&
                   arithmetic == 42 && !comparison && conditional == 41 &&
                   pair.right == 11 && returned == 42 && grouped_return == 42
               ? 0
               : 1;
}
