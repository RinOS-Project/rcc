alignas(3) int invalid_alignment;
alignas(32768) int unsupported_extended_alignment;

alignas(3) struct InvalidClassAlignment {
    int value;
};

alignas(32768) struct InvalidClassExtendedAlignment {
    int value;
};

int main() {
    return invalid_alignment;
}
