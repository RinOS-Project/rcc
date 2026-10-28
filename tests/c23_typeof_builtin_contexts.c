struct TypeofBuiltinInner {
    short prefix;
    int values[4];
};

struct TypeofBuiltinRecord {
    char prefix;
    struct TypeofBuiltinInner inner;
};

struct TypeofBuiltinNestedMember {
    typeof("abc") values;
};

static struct TypeofBuiltinRecord typeof_builtin_record;

_Static_assert(__builtin_types_compatible_p(
                   typeof(typeof_builtin_record),
                   struct TypeofBuiltinRecord),
               "types_compatible_p resolves aggregate typeof operands");
_Static_assert(__builtin_types_compatible_p(
                   typeof(typeof_builtin_record.inner.values), int[4]),
               "types_compatible_p resolves array-member typeof operands");
_Static_assert(__builtin_types_compatible_p(
                   typeof(&typeof_builtin_record),
                   struct TypeofBuiltinRecord*),
               "types_compatible_p resolves pointer typeof operands");
_Static_assert(!__builtin_types_compatible_p(
                   typeof(&typeof_builtin_record), int),
               "types_compatible_p does not compare the typeof placeholder");
_Static_assert(__builtin_types_compatible_p(
                   typeof(((struct TypeofBuiltinNestedMember*)0)->values),
                   char[4]),
               "types_compatible_p resolves typeof nested in a member type");
_Static_assert(__builtin_offsetof(typeof(typeof_builtin_record),
                                  inner.values[2]) ==
                   __builtin_offsetof(struct TypeofBuiltinRecord,
                                      inner.values[2]),
               "offsetof resolves nested typeof member and array designators");
_Static_assert(__builtin_offsetof(struct TypeofBuiltinNestedMember,
                                  values[2]) == 2,
               "offsetof resolves typeof nested in an aggregate member");

int main(void) {
    return __builtin_types_compatible_p(
               typeof(&typeof_builtin_record),
               struct TypeofBuiltinRecord*) &&
           __builtin_offsetof(typeof(typeof_builtin_record),
                              inner.values[2]) ==
               __builtin_offsetof(struct TypeofBuiltinRecord,
                                  inner.values[2]) &&
           __builtin_offsetof(struct TypeofBuiltinNestedMember,
                              values[2]) == 2
        ? 0 : 1;
}
