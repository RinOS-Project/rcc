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
    object.value = 3;
    const int& snapshot = object.value;
    object.value = 5;
    global_object.value = 5;
    BitfieldReferenceEnum enum_object{};
    enum_object.value = BitfieldReferenceCode::first;
    const BitfieldReferenceCode& enum_snapshot = enum_object.value;
    enum_object.value = BitfieldReferenceCode::second;
    return snapshot != 3 || global_snapshot != 3 ||
           read_const_reference(object.value) != 5 ||
           enum_snapshot != BitfieldReferenceCode::first;
}
