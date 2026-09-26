struct CxxAlignedRecord {
    char tag;
    long long value;
};

int main() {
    alignas(16) int value = 7;
    static_assert(alignof(long long) == 8, "long long alignment");
    static_assert(alignof(CxxAlignedRecord) == 8, "record alignment");
    return ((unsigned long)&value % 16) == 0 && value == 7 ? 0 : 1;
}
