struct Bits {
    unsigned value : 3;
};

int read_bitfield_type(struct Bits* bits) {
    typeof(bits->value) arrow_value = 1;
    typeof((*bits).value) dot_value = 2;
    return (int)(arrow_value + dot_value);
}
