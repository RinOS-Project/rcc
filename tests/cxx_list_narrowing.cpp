struct ListValues {
    int number;
    unsigned char byte;
    float fraction;
};

struct ListConstructor {
    int value;
    ListConstructor(int source) : value(source) {}
};

int main() {
    int runtime_narrow = 256;
    ListValues values{255u, 255u, 1.5};
    ListValues parenthesized(255u, runtime_narrow, 1.1);
    int scalar{255u};
    bool boolean{1};
    float exact_fraction{1.5};
    float rounded_fraction{1.1};
    float exact_integer{16777216};
    double exact_wide_integer{9007199254740992ULL};
    ListConstructor constructed{255u};
    if (values.number != scalar || values.byte != 255u ||
        values.fraction != 1.5f || exact_fraction != 1.5f) return 1;
    if (parenthesized.number != 255 || parenthesized.byte != 0 ||
        parenthesized.fraction != 1.1f) return 2;
    if (!boolean) return 3;
    if (rounded_fraction != 1.1f) return 4;
    if (exact_integer != 16777216.0f) return 5;
    if (exact_wide_integer != 9007199254740992.0) return 6;
    if (constructed.value != 255) return 7;
    return 0;
}
