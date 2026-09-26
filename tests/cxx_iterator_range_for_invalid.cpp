struct UnsupportedRange {
    int value;
};

int main(void) {
    UnsupportedRange range;
    for (auto value : range) {
        (void)value;
    }
    return 0;
}
