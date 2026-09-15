class ArrayDefaultMemberInitializer {
public:
    int value = 3;
};

extern "C" int invalid_default_member_initializer_array() {
    ArrayDefaultMemberInitializer* values =
        new ArrayDefaultMemberInitializer[2]{};
    return values[0].value;
}
