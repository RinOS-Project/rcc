alignas(3) int invalid_alignment;

alignas(3) struct InvalidClassAlignment {
    int value;
};

int main() {
    return invalid_alignment;
}
