_Static_assert(1. == 1.0, "trailing decimal point must form a floating literal");
_Static_assert(.5 == 0.5, "leading decimal point must form a floating literal");
_Static_assert(0x1.8p+1 == 3.0,
               "hexadecimal floating literals must use a binary exponent");
_Static_assert('\101' == 'A', "octal character escapes must consume digits");
_Static_assert('\x41' == 'A', "hexadecimal character escapes must consume digits");

int numeric_literal_values(void) {
    float half = .5f;
    double trailing = 1.;
    double hexadecimal = 0x1.8p+1;
    float hexadecimal_float = 0x1.8p+1f;
    return half == 0.5f && trailing == 1.0 &&
           hexadecimal == 3.0 && hexadecimal_float == 3.0f ? 0 : 1;
}

int main(void) {
    return numeric_literal_values();
}
