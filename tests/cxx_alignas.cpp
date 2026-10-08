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

struct CxxMemberAligned32 {
    char tag;
    alignas(32) long long value;
    char tail;
};

alignas(32) struct CxxClassAligned {
    int value;
};

struct CxxClassAlignedHolder {
    char tag;
    CxxClassAligned value;
};

namespace cxx_alignas_namespace {
alignas(8) struct CxxNamespaceAligned {
    int value;
};
}

template<typename T>
alignas(16) struct CxxTemplateAligned {
    T value;
};

using CxxTemplateAlignedInt = CxxTemplateAligned<int>;

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
    static_assert(alignof(CxxMemberAligned32) == 32,
                  "over-aligned member must raise class alignment");
    static_assert(sizeof(CxxMemberAligned32) == 64,
                  "over-aligned member must round class size");
    static_assert(alignof(CxxClassAligned) == 32,
                  "class alignas must raise class alignment");
    static_assert(sizeof(CxxClassAligned) == 32,
                  "class alignas must round class size");
    static_assert(sizeof(CxxClassAlignedHolder) == 64,
                  "containing layout must preserve class alignment padding");
    static_assert(alignof(cxx_alignas_namespace::CxxNamespaceAligned) == 8,
                  "namespace class alignas must be retained");
    static_assert(sizeof(cxx_alignas_namespace::CxxNamespaceAligned) == 8,
                  "namespace class alignas must round class size");
    static_assert(alignof(CxxTemplateAlignedInt) == 16,
                  "class-template alignas must survive instantiation");
    static_assert(sizeof(CxxTemplateAlignedInt) == 16,
                  "class-template alignas must round instantiated size");
    return ((unsigned long)&value % 16) == 0 && value == 7 ? 0 : 1;
}
