class TypeIdBase {
public:
    virtual int value() { return 7; }
};

struct TypeIdMemberOwnerA {
    int value;
};

struct TypeIdMemberOwnerB {
    int value;
};

typedef int **************************************** TypeIdDeepInt;
typedef long **************************************** TypeIdDeepLong;

int (*type_id_function_int)(int);
int (*type_id_function_long)(long);
int TypeIdMemberOwnerA::*type_id_member_a;
int TypeIdMemberOwnerB::*type_id_member_b;
TypeIdDeepInt type_id_deep_int;
TypeIdDeepLong type_id_deep_long;

extern "C" int main() {
    const char* int_name = typeid(int).name();
    const char* long_name = typeid(long).name();
    bool names_differ = false;
    bool same_before = typeid(int).before(typeid(int));
    bool opposite_before = typeid(int).before(typeid(long)) !=
                           typeid(long).before(typeid(int));
    if (int_name && long_name) {
        for (int index = 0; index < 64; ++index) {
            if (int_name[index] != long_name[index]) {
                names_differ = true;
                break;
            }
            if (int_name[index] == '\0') break;
        }
    }
    /* The bounded frontend exposes one stable address and hash for each
     * complete static type.  `name()` is a stable, non-empty implementation
     * string for the same identity and differs for distinct types.  Dynamic
     * polymorphic typeid is covered by the dedicated exception/RTTI regression. */
    return (&typeid(int) == &typeid(int) &&
            &typeid(int) != &typeid(long) &&
            &typeid(TypeIdBase) == &typeid(TypeIdBase) &&
            &typeid(1) == &typeid(int) &&
            &typeid(const int*) != &typeid(int*) &&
            &typeid(int* const) == &typeid(int*) &&
            &typeid(int&) == &typeid(int) &&
            &typeid(type_id_function_int) !=
                &typeid(type_id_function_long) &&
            &typeid(type_id_member_a) != &typeid(type_id_member_b) &&
            &typeid(type_id_deep_int) != &typeid(type_id_deep_long) &&
            typeid(int) == typeid(int) &&
            typeid(int) != typeid(long) &&
            int_name && int_name[0] != '\0' &&
            long_name && long_name[0] != '\0' && names_differ &&
            !same_before && opposite_before &&
            typeid(int).hash_code() == typeid(int).hash_code() &&
            typeid(int).hash_code() != typeid(long).hash_code()) ? 0 : 1;
}
