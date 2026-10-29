// Candidate sets must not be truncated at the former 32-overload boundary.
template <int Index>
struct CandidateOverloadArgument {};

template <typename T>
int choose_candidate(CandidateOverloadArgument<0>) {
    return 0;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<1>) {
    return 1;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<2>) {
    return 2;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<3>) {
    return 3;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<4>) {
    return 4;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<5>) {
    return 5;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<6>) {
    return 6;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<7>) {
    return 7;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<8>) {
    return 8;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<9>) {
    return 9;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<10>) {
    return 10;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<11>) {
    return 11;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<12>) {
    return 12;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<13>) {
    return 13;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<14>) {
    return 14;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<15>) {
    return 15;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<16>) {
    return 16;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<17>) {
    return 17;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<18>) {
    return 18;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<19>) {
    return 19;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<20>) {
    return 20;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<21>) {
    return 21;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<22>) {
    return 22;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<23>) {
    return 23;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<24>) {
    return 24;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<25>) {
    return 25;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<26>) {
    return 26;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<27>) {
    return 27;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<28>) {
    return 28;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<29>) {
    return 29;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<30>) {
    return 30;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<31>) {
    return 31;
}

template <typename T>
int choose_candidate(CandidateOverloadArgument<32>) {
    return 32;
}

namespace adl_candidate_set {
struct Payload {};
template <int Index>
struct Tag {};

template <typename T>
int choose_adl_candidate(T, Tag<0>) {
    return 0;
}

template <typename T>
int choose_adl_candidate(T, Tag<1>) {
    return 1;
}

template <typename T>
int choose_adl_candidate(T, Tag<2>) {
    return 2;
}

template <typename T>
int choose_adl_candidate(T, Tag<3>) {
    return 3;
}

template <typename T>
int choose_adl_candidate(T, Tag<4>) {
    return 4;
}

template <typename T>
int choose_adl_candidate(T, Tag<5>) {
    return 5;
}

template <typename T>
int choose_adl_candidate(T, Tag<6>) {
    return 6;
}

template <typename T>
int choose_adl_candidate(T, Tag<7>) {
    return 7;
}

template <typename T>
int choose_adl_candidate(T, Tag<8>) {
    return 8;
}

template <typename T>
int choose_adl_candidate(T, Tag<9>) {
    return 9;
}

template <typename T>
int choose_adl_candidate(T, Tag<10>) {
    return 10;
}

template <typename T>
int choose_adl_candidate(T, Tag<11>) {
    return 11;
}

template <typename T>
int choose_adl_candidate(T, Tag<12>) {
    return 12;
}

template <typename T>
int choose_adl_candidate(T, Tag<13>) {
    return 13;
}

template <typename T>
int choose_adl_candidate(T, Tag<14>) {
    return 14;
}

template <typename T>
int choose_adl_candidate(T, Tag<15>) {
    return 15;
}

template <typename T>
int choose_adl_candidate(T, Tag<16>) {
    return 16;
}

template <typename T>
int choose_adl_candidate(T, Tag<17>) {
    return 17;
}

template <typename T>
int choose_adl_candidate(T, Tag<18>) {
    return 18;
}

template <typename T>
int choose_adl_candidate(T, Tag<19>) {
    return 19;
}

template <typename T>
int choose_adl_candidate(T, Tag<20>) {
    return 20;
}

template <typename T>
int choose_adl_candidate(T, Tag<21>) {
    return 21;
}

template <typename T>
int choose_adl_candidate(T, Tag<22>) {
    return 22;
}

template <typename T>
int choose_adl_candidate(T, Tag<23>) {
    return 23;
}

template <typename T>
int choose_adl_candidate(T, Tag<24>) {
    return 24;
}

template <typename T>
int choose_adl_candidate(T, Tag<25>) {
    return 25;
}

template <typename T>
int choose_adl_candidate(T, Tag<26>) {
    return 26;
}

template <typename T>
int choose_adl_candidate(T, Tag<27>) {
    return 27;
}

template <typename T>
int choose_adl_candidate(T, Tag<28>) {
    return 28;
}

template <typename T>
int choose_adl_candidate(T, Tag<29>) {
    return 29;
}

template <typename T>
int choose_adl_candidate(T, Tag<30>) {
    return 30;
}

template <typename T>
int choose_adl_candidate(T, Tag<31>) {
    return 31;
}

template <typename T>
int choose_adl_candidate(T, Tag<32>) {
    return 32;
}
}

namespace operator_candidate_set {
template <int Index>
struct Tag {};

#define DECLARE_OPERATOR_TEMPLATE_CANDIDATE(Index) \
    template <typename T> int operator+(T, Tag<Index>) { return Index; }
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(0)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(1)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(2)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(3)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(4)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(5)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(6)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(7)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(8)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(9)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(10)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(11)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(12)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(13)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(14)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(15)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(16)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(17)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(18)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(19)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(20)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(21)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(22)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(23)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(24)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(25)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(26)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(27)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(28)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(29)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(30)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(31)
DECLARE_OPERATOR_TEMPLATE_CANDIDATE(32)
#undef DECLARE_OPERATOR_TEMPLATE_CANDIDATE
}

template <typename T>
int defer_candidate(T value) {
    return choose_candidate<void>(value);
}

int main() {
    CandidateOverloadArgument<32> value;
    return choose_candidate<void>(value) == 32 &&
                   defer_candidate(value) == 32 &&
                   choose_adl_candidate(
                       adl_candidate_set::Payload{},
                       adl_candidate_set::Tag<32>{}) == 32 &&
                   7 + operator_candidate_set::Tag<32>{} == 32
               ? 0
               : 1;
}
