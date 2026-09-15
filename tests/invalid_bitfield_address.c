struct invalid_bitfield_address {
    unsigned value : 3;
};

int invalid_bitfield_address(struct invalid_bitfield_address* object) {
    return (int)&object->value;
}
