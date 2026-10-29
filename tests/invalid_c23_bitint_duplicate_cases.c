typedef unsigned _BitInt(7) u7;

int duplicate_bitint_switch_cases(u7 value) {
    switch (value) {
        case -1: return 1;
        case 127: return 2;
        default: return 0;
    }
}
