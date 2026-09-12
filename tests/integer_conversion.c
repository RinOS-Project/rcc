typedef unsigned long long u64;

unsigned char cast_u8(u64 value) { return (unsigned char)value; }
signed char cast_s8(u64 value) { return (signed char)value; }
unsigned short cast_u16(u64 value) { return (unsigned short)value; }
_Bool cast_bool(u64 value) { return (_Bool)value; }
u64 cast_signed_char_to_u64(signed char value) { return (u64)value; }

unsigned assign_u8(unsigned char* output, unsigned value) {
    return (*output = value);
}

int assign_s8(signed char* output, int value) {
    return (*output = value);
}

int assign_bool(_Bool* output, u64 value) {
    return (*output = value);
}

unsigned char return_u8(unsigned value) { return value; }
_Bool return_bool(u64 value) { return value; }
u64 return_widen_signed(int value) { return value; }
u64 return_widen_unsigned(unsigned value) { return value; }
int return_truncate_wide(u64 value) { return value; }

static u64 accept_u64(u64 value) { return value; }
static unsigned accept_u8(unsigned char value) { return value; }
static int accept_bool(_Bool value) { return value; }

u64 call_widen_signed(int value) { return accept_u64(value); }
u64 call_widen_unsigned(unsigned value) { return accept_u64(value); }
unsigned call_truncate_u8(unsigned value) { return accept_u8(value); }
int call_bool(u64 value) { return accept_bool(value); }

int initialize_bool(u64 value) {
    _Bool converted = value;
    return converted;
}

u64 unsigned_add_then_widen(unsigned left, unsigned right) {
    return left + right;
}

u64 unsigned_multiply_then_widen(unsigned left, unsigned right) {
    return left * right;
}
