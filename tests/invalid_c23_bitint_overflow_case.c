typedef _BitInt(7) s7;

int overflowed_case(int selector) {
    switch (selector) {
        case (s7)-64 - (s7)63:
            return 1;
        default:
            return 0;
    }
}
