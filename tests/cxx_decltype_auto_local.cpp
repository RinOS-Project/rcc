static int& decltype_auto_local_ref_source(int& value) {
    return value;
}

namespace decltype_auto_namespace {
int namespace_value = 38;
decltype(auto) namespace_copy = namespace_value;
}

extern "C" int decltype_auto_local_probe(void) {
    int value = 40;
    decltype(auto) copy = value;
    decltype(auto) reference = decltype_auto_local_ref_source(value);
    decltype(auto) parenthesized = (value);
    reference += 2;
    parenthesized += 3;
    return copy + value + decltype_auto_namespace::namespace_copy;
}
