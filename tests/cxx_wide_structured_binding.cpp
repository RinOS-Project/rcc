int main() {
    int values[33] = {0};
    values[32] = 91;
    auto [
        binding0, binding1, binding2, binding3, binding4,
        binding5, binding6, binding7, binding8, binding9,
        binding10, binding11, binding12, binding13, binding14,
        binding15, binding16, binding17, binding18, binding19,
        binding20, binding21, binding22, binding23, binding24,
        binding25, binding26, binding27, binding28, binding29,
        binding30, binding31, binding32
    ] = values;
    return binding0 == 0 && binding32 == 91 ? 0 : 1;
}
