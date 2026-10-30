extern "C" {
alignas(16384) int cxx_aligned_global_16384;
alignas(16384) int cxx_aligned_initialized_global_16384 = 13;
}

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

struct CxxMemberAligned16384 {
    char tag;
    alignas(16384) int value;
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

extern "C" int verified_cxx_local_alignment() {
    alignas(16384) int value = 9;
    return ((unsigned long)&value % 16384) == 0 && value == 9 ? 0 : 1;
}

extern "C" int legacy_cxx_aligned_stack_parameter_check(
    int first, int second, int third, int fourth, int fifth,
    int sixth, int seventh, int eighth, int ninth) {
    alignas(16384) int value = 37;
    return ((unsigned long)&value % 16384) == 0 && value == 37 &&
        first + second + third + fourth + fifth + sixth + seventh +
            eighth + ninth == 45 ? 0 : 1;
}

int main() {
    alignas(16) int value = 7;
    alignas(16384) int extended_value = 9;
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
    static_assert(alignof(CxxMemberAligned16384) == 16384,
                  "16 KiB member alignment must raise class alignment");
    static_assert(sizeof(CxxMemberAligned16384) == 32768,
                  "16 KiB member alignment must round class size");
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
    return ((unsigned long)&value % 16) == 0 && value == 7 &&
        ((unsigned long)&extended_value % 16384) == 0 &&
        extended_value == 9 && cxx_aligned_global_16384 == 0 &&
        cxx_aligned_initialized_global_16384 == 13 &&
        verified_cxx_local_alignment() == 0 &&
        legacy_cxx_aligned_stack_parameter_check(
            1, 2, 3, 4, 5, 6, 7, 8, 9) == 0 ? 0 : 1;
}
