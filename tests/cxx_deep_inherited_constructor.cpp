class InheritedBase {
public:
    explicit InheritedBase(int input) : value(input) {}
    int value;
};

class InheritedLayer1 : public InheritedBase {
public:
    using InheritedBase::InheritedBase;
};

class InheritedLayer2 : public InheritedLayer1 {
public:
    using InheritedLayer1::InheritedLayer1;
};

class InheritedLayer3 : public InheritedLayer2 {
public:
    using InheritedLayer2::InheritedLayer2;
};

class InheritedLayer4 : public InheritedLayer3 {
public:
    using InheritedLayer3::InheritedLayer3;
};

class InheritedLayer5 : public InheritedLayer4 {
public:
    using InheritedLayer4::InheritedLayer4;
};

class InheritedLayer6 : public InheritedLayer5 {
public:
    using InheritedLayer5::InheritedLayer5;
};

class InheritedLayer7 : public InheritedLayer6 {
public:
    using InheritedLayer6::InheritedLayer6;
};

class InheritedLayer8 : public InheritedLayer7 {
public:
    using InheritedLayer7::InheritedLayer7;
};

class InheritedLayer9 : public InheritedLayer8 {
public:
    using InheritedLayer8::InheritedLayer8;
};

class InheritedLayer10 : public InheritedLayer9 {
public:
    using InheritedLayer9::InheritedLayer9;
};

class InheritedLayer11 : public InheritedLayer10 {
public:
    using InheritedLayer10::InheritedLayer10;
};

class InheritedLayer12 : public InheritedLayer11 {
public:
    using InheritedLayer11::InheritedLayer11;
};

class InheritedLayer13 : public InheritedLayer12 {
public:
    using InheritedLayer12::InheritedLayer12;
};

class InheritedLayer14 : public InheritedLayer13 {
public:
    using InheritedLayer13::InheritedLayer13;
};

class InheritedLayer15 : public InheritedLayer14 {
public:
    using InheritedLayer14::InheritedLayer14;
};

class InheritedLayer16 : public InheritedLayer15 {
public:
    using InheritedLayer15::InheritedLayer15;
};

class InheritedLayer17 : public InheritedLayer16 {
public:
    using InheritedLayer16::InheritedLayer16;
};

class InheritedLayer18 : public InheritedLayer17 {
public:
    using InheritedLayer17::InheritedLayer17;
};

class InheritedLayer19 : public InheritedLayer18 {
public:
    using InheritedLayer18::InheritedLayer18;
};

class InheritedLayer20 : public InheritedLayer19 {
public:
    using InheritedLayer19::InheritedLayer19;
};

class InheritedLayer21 : public InheritedLayer20 {
public:
    using InheritedLayer20::InheritedLayer20;
};

class InheritedLayer22 : public InheritedLayer21 {
public:
    using InheritedLayer21::InheritedLayer21;
};

class InheritedLayer23 : public InheritedLayer22 {
public:
    using InheritedLayer22::InheritedLayer22;
};

class InheritedLayer24 : public InheritedLayer23 {
public:
    using InheritedLayer23::InheritedLayer23;
};

class InheritedLayer25 : public InheritedLayer24 {
public:
    using InheritedLayer24::InheritedLayer24;
};

class InheritedLayer26 : public InheritedLayer25 {
public:
    using InheritedLayer25::InheritedLayer25;
};

class InheritedLayer27 : public InheritedLayer26 {
public:
    using InheritedLayer26::InheritedLayer26;
};

class InheritedLayer28 : public InheritedLayer27 {
public:
    using InheritedLayer27::InheritedLayer27;
};

class InheritedLayer29 : public InheritedLayer28 {
public:
    using InheritedLayer28::InheritedLayer28;
};

class InheritedLayer30 : public InheritedLayer29 {
public:
    using InheritedLayer29::InheritedLayer29;
};

class InheritedLayer31 : public InheritedLayer30 {
public:
    using InheritedLayer30::InheritedLayer30;
};

class InheritedLayer32 : public InheritedLayer31 {
public:
    using InheritedLayer31::InheritedLayer31;
};

class InheritedLayer33 : public InheritedLayer32 {
public:
    using InheritedLayer32::InheritedLayer32;
};

class InheritedLayer34 : public InheritedLayer33 {
public:
    using InheritedLayer33::InheritedLayer33;
};

class InheritedLayer35 : public InheritedLayer34 {
public:
    using InheritedLayer34::InheritedLayer34;
};

class InheritedLayer36 : public InheritedLayer35 {
public:
    using InheritedLayer35::InheritedLayer35;
};

class InheritedLayer37 : public InheritedLayer36 {
public:
    using InheritedLayer36::InheritedLayer36;
};

class InheritedLayer38 : public InheritedLayer37 {
public:
    using InheritedLayer37::InheritedLayer37;
};

class InheritedLayer39 : public InheritedLayer38 {
public:
    using InheritedLayer38::InheritedLayer38;
};

class InheritedLayer40 : public InheritedLayer39 {
public:
    using InheritedLayer39::InheritedLayer39;
};

class InheritedLayer41 : public InheritedLayer40 {
public:
    using InheritedLayer40::InheritedLayer40;
};

class InheritedLayer42 : public InheritedLayer41 {
public:
    using InheritedLayer41::InheritedLayer41;
};

class InheritedLayer43 : public InheritedLayer42 {
public:
    using InheritedLayer42::InheritedLayer42;
};

class InheritedLayer44 : public InheritedLayer43 {
public:
    using InheritedLayer43::InheritedLayer43;
};

class InheritedLayer45 : public InheritedLayer44 {
public:
    using InheritedLayer44::InheritedLayer44;
};

class InheritedLayer46 : public InheritedLayer45 {
public:
    using InheritedLayer45::InheritedLayer45;
};

class InheritedLayer47 : public InheritedLayer46 {
public:
    using InheritedLayer46::InheritedLayer46;
};

class InheritedLayer48 : public InheritedLayer47 {
public:
    using InheritedLayer47::InheritedLayer47;
};

class InheritedLayer49 : public InheritedLayer48 {
public:
    using InheritedLayer48::InheritedLayer48;
};

class InheritedLayer50 : public InheritedLayer49 {
public:
    using InheritedLayer49::InheritedLayer49;
};

class InheritedLayer51 : public InheritedLayer50 {
public:
    using InheritedLayer50::InheritedLayer50;
};

class InheritedLayer52 : public InheritedLayer51 {
public:
    using InheritedLayer51::InheritedLayer51;
};

class InheritedLayer53 : public InheritedLayer52 {
public:
    using InheritedLayer52::InheritedLayer52;
};

class InheritedLayer54 : public InheritedLayer53 {
public:
    using InheritedLayer53::InheritedLayer53;
};

class InheritedLayer55 : public InheritedLayer54 {
public:
    using InheritedLayer54::InheritedLayer54;
};

class InheritedLayer56 : public InheritedLayer55 {
public:
    using InheritedLayer55::InheritedLayer55;
};

class InheritedLayer57 : public InheritedLayer56 {
public:
    using InheritedLayer56::InheritedLayer56;
};

class InheritedLayer58 : public InheritedLayer57 {
public:
    using InheritedLayer57::InheritedLayer57;
};

class InheritedLayer59 : public InheritedLayer58 {
public:
    using InheritedLayer58::InheritedLayer58;
};

class InheritedLayer60 : public InheritedLayer59 {
public:
    using InheritedLayer59::InheritedLayer59;
};

class InheritedLayer61 : public InheritedLayer60 {
public:
    using InheritedLayer60::InheritedLayer60;
};

class InheritedLayer62 : public InheritedLayer61 {
public:
    using InheritedLayer61::InheritedLayer61;
};

class InheritedLayer63 : public InheritedLayer62 {
public:
    using InheritedLayer62::InheritedLayer62;
};

class InheritedLayer64 : public InheritedLayer63 {
public:
    using InheritedLayer63::InheritedLayer63;
};

class InheritedLayer65 : public InheritedLayer64 {
public:
    using InheritedLayer64::InheritedLayer64;
};

int main() {
    InheritedLayer65 object(91);
    return object.value == 91 ? 0 : 1;
}
