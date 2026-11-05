struct invalid_cxx_bitfield_width {
    unsigned value : -1;
};

extern "C" int main() {
    return 0;
}
