int main() {
    unsigned int decimal = 1'000'000u;
    unsigned int binary = 0b1010'0101u;
    unsigned int hexadecimal = 0xAB'CDu;
    double fraction = 1'2.5'0;
    double hexadecimal_fraction = 0x1'0.8'0p+1;

    return decimal == 1000000u && binary == 165u &&
                   hexadecimal == 0xABCDu && fraction == 12.5 &&
                   hexadecimal_fraction == 33.0 ? 0 : 1;
}
