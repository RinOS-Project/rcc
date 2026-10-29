#ifdef __RCC__
#if !defined(__has_c_attribute)
#error "C23 __has_c_attribute is unavailable"
#endif
#if __has_c_attribute(reproducible) != 202311L
#error "reproducible attribute version is incorrect"
#endif
#if __has_c_attribute(unsequenced) != 202311L
#error "unsequenced attribute version is incorrect"
#endif
#endif

typedef int c23_reproducible_function(int) [[reproducible]];
typedef int (*c23_unsequenced_pointer)(int) [[unsequenced]];
typedef int (**c23_nested_function_pointer)(int) [[reproducible]];
typedef int (*c23_function_pointer_array[2])(int) [[unsequenced]];
typedef int c23_plain_function(int),
            c23_comma_attributed_function(int) [[reproducible]];

struct c23_function_type_member {
    int (*callback)(int) [[unsequenced]];
};

typedef int c23_function_type(int);
c23_function_type [[reproducible]] *c23_attributed_type_specifier;

_Static_assert(__builtin_types_compatible_p(
                   int(int) [[reproducible]], int(int)),
               "function type attributes do not change compatibility");
_Static_assert(__builtin_types_compatible_p(
                   int (*)(int) [[unsequenced]], int (*)(int)),
               "pointer function attributes do not change compatibility");

static int c23_identity(int value) [[reproducible]] {
    return value;
}

static int c23_add_one(int value) [[unsequenced]] {
    return value + 1;
}

static int c23_apply_function_pointer(
    int (*callback)(int) [[reproducible]], int value) {
    return callback(value);
}

int c23_redeclared_function(int value) [[reproducible]];
int c23_redeclared_function(int value) {
    return value;
}

extern int (*c23_redeclared_pointer)(int) [[unsequenced]];
int (*c23_redeclared_pointer)(int) = c23_add_one;

int main(void) {
    c23_unsequenced_pointer callback = c23_add_one;
    struct c23_function_type_member member = {c23_add_one};
    c23_attributed_type_specifier = c23_identity;
    if (c23_identity(4) != 4 || c23_add_one(4) != 5 ||
        c23_redeclared_function(7) != 7 || callback(8) != 9 ||
        c23_redeclared_pointer(10) != 11 || member.callback(11) != 12 ||
        c23_apply_function_pointer(c23_identity, 13) != 13 ||
        c23_attributed_type_specifier(12) != 12) {
        return 1;
    }
    return 0;
}
