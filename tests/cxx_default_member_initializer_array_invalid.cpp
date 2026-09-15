class NonScalarDefaultMember {
public:
    int value = 3;
};

class ArrayDefaultMemberInitializer {
public:
    NonScalarDefaultMember nested{};
};

extern "C" int invalid_default_member_initializer_array() {
    ArrayDefaultMemberInitializer* values =
        new ArrayDefaultMemberInitializer[2]{};
    return values[0].nested.value;
}
