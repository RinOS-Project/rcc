struct Tag0 {};
struct Tag1 {};
struct Tag2 {};
struct Tag3 {};
struct Tag4 {};
struct Tag5 {};
struct Tag6 {};
struct Tag7 {};
struct Tag8 {};
struct Tag9 {};
struct Tag10 {};
struct Tag11 {};
struct Tag12 {};
struct Tag13 {};
struct Tag14 {};
struct Tag15 {};
struct Tag16 {};
struct Tag17 {};
struct Tag18 {};
struct Tag19 {};
struct Tag20 {};
struct Tag21 {};
struct Tag22 {};
struct Tag23 {};
struct Tag24 {};
struct Tag25 {};
struct Tag26 {};
struct Tag27 {};
struct Tag28 {};
struct Tag29 {};
struct Tag30 {};
struct Tag31 {};
struct Tag32 {};
struct Tag33 {};
struct Tag34 {};
struct Tag35 {};
struct Tag36 {};
struct Tag37 {};
struct Tag38 {};
struct Tag39 {};
struct Tag40 {};
struct Tag41 {};
struct Tag42 {};
struct Tag43 {};
struct Tag44 {};
struct Tag45 {};
struct Tag46 {};
struct Tag47 {};
struct Tag48 {};
struct Tag49 {};
struct Tag50 {};
struct Tag51 {};
struct Tag52 {};
struct Tag53 {};
struct Tag54 {};
struct Tag55 {};
struct Tag56 {};
struct Tag57 {};
struct Tag58 {};
struct Tag59 {};
struct Tag60 {};
struct Tag61 {};
struct Tag62 {};
struct Tag63 {};
struct Tag64 {};
struct Tag65 {};

class DeepDelegation {
public:
    DeepDelegation(Tag0) : DeepDelegation(Tag1{}) {}
    DeepDelegation(Tag1) : DeepDelegation(Tag2{}) {}
    DeepDelegation(Tag2) : DeepDelegation(Tag3{}) {}
    DeepDelegation(Tag3) : DeepDelegation(Tag4{}) {}
    DeepDelegation(Tag4) : DeepDelegation(Tag5{}) {}
    DeepDelegation(Tag5) : DeepDelegation(Tag6{}) {}
    DeepDelegation(Tag6) : DeepDelegation(Tag7{}) {}
    DeepDelegation(Tag7) : DeepDelegation(Tag8{}) {}
    DeepDelegation(Tag8) : DeepDelegation(Tag9{}) {}
    DeepDelegation(Tag9) : DeepDelegation(Tag10{}) {}
    DeepDelegation(Tag10) : DeepDelegation(Tag11{}) {}
    DeepDelegation(Tag11) : DeepDelegation(Tag12{}) {}
    DeepDelegation(Tag12) : DeepDelegation(Tag13{}) {}
    DeepDelegation(Tag13) : DeepDelegation(Tag14{}) {}
    DeepDelegation(Tag14) : DeepDelegation(Tag15{}) {}
    DeepDelegation(Tag15) : DeepDelegation(Tag16{}) {}
    DeepDelegation(Tag16) : DeepDelegation(Tag17{}) {}
    DeepDelegation(Tag17) : DeepDelegation(Tag18{}) {}
    DeepDelegation(Tag18) : DeepDelegation(Tag19{}) {}
    DeepDelegation(Tag19) : DeepDelegation(Tag20{}) {}
    DeepDelegation(Tag20) : DeepDelegation(Tag21{}) {}
    DeepDelegation(Tag21) : DeepDelegation(Tag22{}) {}
    DeepDelegation(Tag22) : DeepDelegation(Tag23{}) {}
    DeepDelegation(Tag23) : DeepDelegation(Tag24{}) {}
    DeepDelegation(Tag24) : DeepDelegation(Tag25{}) {}
    DeepDelegation(Tag25) : DeepDelegation(Tag26{}) {}
    DeepDelegation(Tag26) : DeepDelegation(Tag27{}) {}
    DeepDelegation(Tag27) : DeepDelegation(Tag28{}) {}
    DeepDelegation(Tag28) : DeepDelegation(Tag29{}) {}
    DeepDelegation(Tag29) : DeepDelegation(Tag30{}) {}
    DeepDelegation(Tag30) : DeepDelegation(Tag31{}) {}
    DeepDelegation(Tag31) : DeepDelegation(Tag32{}) {}
    DeepDelegation(Tag32) : DeepDelegation(Tag33{}) {}
    DeepDelegation(Tag33) : DeepDelegation(Tag34{}) {}
    DeepDelegation(Tag34) : DeepDelegation(Tag35{}) {}
    DeepDelegation(Tag35) : DeepDelegation(Tag36{}) {}
    DeepDelegation(Tag36) : DeepDelegation(Tag37{}) {}
    DeepDelegation(Tag37) : DeepDelegation(Tag38{}) {}
    DeepDelegation(Tag38) : DeepDelegation(Tag39{}) {}
    DeepDelegation(Tag39) : DeepDelegation(Tag40{}) {}
    DeepDelegation(Tag40) : DeepDelegation(Tag41{}) {}
    DeepDelegation(Tag41) : DeepDelegation(Tag42{}) {}
    DeepDelegation(Tag42) : DeepDelegation(Tag43{}) {}
    DeepDelegation(Tag43) : DeepDelegation(Tag44{}) {}
    DeepDelegation(Tag44) : DeepDelegation(Tag45{}) {}
    DeepDelegation(Tag45) : DeepDelegation(Tag46{}) {}
    DeepDelegation(Tag46) : DeepDelegation(Tag47{}) {}
    DeepDelegation(Tag47) : DeepDelegation(Tag48{}) {}
    DeepDelegation(Tag48) : DeepDelegation(Tag49{}) {}
    DeepDelegation(Tag49) : DeepDelegation(Tag50{}) {}
    DeepDelegation(Tag50) : DeepDelegation(Tag51{}) {}
    DeepDelegation(Tag51) : DeepDelegation(Tag52{}) {}
    DeepDelegation(Tag52) : DeepDelegation(Tag53{}) {}
    DeepDelegation(Tag53) : DeepDelegation(Tag54{}) {}
    DeepDelegation(Tag54) : DeepDelegation(Tag55{}) {}
    DeepDelegation(Tag55) : DeepDelegation(Tag56{}) {}
    DeepDelegation(Tag56) : DeepDelegation(Tag57{}) {}
    DeepDelegation(Tag57) : DeepDelegation(Tag58{}) {}
    DeepDelegation(Tag58) : DeepDelegation(Tag59{}) {}
    DeepDelegation(Tag59) : DeepDelegation(Tag60{}) {}
    DeepDelegation(Tag60) : DeepDelegation(Tag61{}) {}
    DeepDelegation(Tag61) : DeepDelegation(Tag62{}) {}
    DeepDelegation(Tag62) : DeepDelegation(Tag63{}) {}
    DeepDelegation(Tag63) : DeepDelegation(Tag64{}) {}
    DeepDelegation(Tag64) : DeepDelegation(Tag65{}) {}
    explicit DeepDelegation(Tag65) : value_(93) {}
    int value() const { return value_; }

private:
    int value_;
};

int main() {
    DeepDelegation chain(Tag0{});
    return chain.value() == 93 ? 0 : 1;
}
