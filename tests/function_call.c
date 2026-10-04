typedef unsigned long long u64;

static unsigned accept_u8(unsigned char value) { return value; }
static int accept_s8(signed char value) { return value; }
static unsigned accept_u16(unsigned short value) { return value; }
static int accept_bool(_Bool value) { return value; }

unsigned call_fixed_u8(unsigned value) { return accept_u8(value); }
int call_fixed_s8(int value) { return accept_s8(value); }
unsigned call_fixed_u16(unsigned value) { return accept_u16(value); }
int call_fixed_bool(u64 value) { return accept_bool(value); }

static int accept_variadic(int fixed, ...) { return fixed; }

int call_variadic(u64 high_word) {
    return accept_variadic(77, (unsigned char)511, (signed char)255,
                           (unsigned short)0x12345, (_Bool)high_word,
                           5, 6, 7);
}

static int no_prototype() { return 91; }

int call_without_prototype(void) {
    return no_prototype((unsigned char)511, (signed char)255,
                        (unsigned short)0x12345, 4, 5, 6, 7);
}

int promoted_redeclaration();
int promoted_redeclaration(int value);
int promoted_redeclaration(int value) { return value; }

int call_promoted_redeclaration(void) {
    return promoted_redeclaration((unsigned char)123);
}

static int callback_value(void) { return 63; }
int call_adjusted_callback(int callback(void)) { return callback(); }

int call_function_parameter(void) {
    return call_adjusted_callback(callback_value);
}

static int nested_argument_value(void) { return 5; }

static int accept_two_arguments(int first, int second) {
    return first * 10 + second;
}

int call_nested_argument(void) {
    return accept_two_arguments(3, nested_argument_value());
}

int main(void)
{
    if (call_fixed_u8(0x1234U) != 0x34U ||
        call_fixed_s8(255) != -1 ||
        call_fixed_u16(0x12345U) != 0x2345U ||
        call_fixed_bool(0x100000000ULL) != 1 ||
        call_variadic(0x100000000ULL) != 77 ||
        call_without_prototype() != 91 ||
        call_promoted_redeclaration() != 123 ||
        call_function_parameter() != 63 ||
        call_nested_argument() != 35) {
        return 1;
    }
    return 0;
}
