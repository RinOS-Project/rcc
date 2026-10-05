struct CxxAlignedRecord {
    char tag;
    long long value;
};

struct CxxMemberAlignedRecord {
    char tag;
    alignas(16) int value;
    char tail;
};

struct CxxMemberAlignedSmall {
    alignas(8) int value;
};

int main() {
    alignas(16) int value = 7;
    static_assert(alignof(long long) == 8, "long long alignment");
    static_assert(alignof(CxxAlignedRecord) == 8, "record alignment");
    static_assert(alignof(CxxMemberAlignedRecord) == 16,
                  "member alignas must raise class alignment");
    static_assert(sizeof(CxxMemberAlignedRecord) == 32,
                  "member alignas must preserve trailing class padding");
    static_assert(alignof(CxxMemberAlignedSmall) == 8,
                  "small member alignas must raise class alignment");
    static_assert(sizeof(CxxMemberAlignedSmall) == 8,
                  "small member alignas must round class size");
    return ((unsigned long)&value % 16) == 0 && value == 7 ? 0 : 1;
}
