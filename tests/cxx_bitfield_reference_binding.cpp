struct BitfieldReferenceValue {
    unsigned int value : 3;
};

static BitfieldReferenceValue global_object = {3};
static const int& global_snapshot = global_object.value;

enum class BitfieldReferenceCode : unsigned int {
    first = 1,
    second = 2
};

struct BitfieldReferenceEnum {
    BitfieldReferenceCode value : 2;
};

static int read_const_reference(const int& value) {
    return value;
}

extern "C" int main() {
    BitfieldReferenceValue object{};
    BitfieldReferenceValue alternate{};
    object.value = 3;
    alternate.value = 2;
    const int& snapshot = object.value;
    const unsigned int& conditional_snapshot =
        true ? object.value : alternate.value;
    const unsigned int& comma_snapshot =
        (object.value = 4, alternate.value);
    object.value = 5;
    alternate.value = 6;
    global_object.value = 5;
    BitfieldReferenceEnum enum_object{};
    enum_object.value = BitfieldReferenceCode::first;
    const BitfieldReferenceCode& enum_snapshot = enum_object.value;
    enum_object.value = BitfieldReferenceCode::second;
    BitfieldReferenceValue xvalue_object{};
    xvalue_object.value = 1;
    unsigned int&& xvalue_rvalue_snapshot =
        static_cast<BitfieldReferenceValue&&>(xvalue_object).value;
    const unsigned int& xvalue_const_snapshot =
        static_cast<BitfieldReferenceValue&&>(xvalue_object).value;
    xvalue_object.value = 2;
    return snapshot != 3 || conditional_snapshot != 3 ||
           comma_snapshot != 2 || xvalue_rvalue_snapshot != 1 ||
           xvalue_const_snapshot != 1 || global_snapshot != 3 ||
           read_const_reference(object.value) != 5 ||
           enum_snapshot != BitfieldReferenceCode::first;
}
