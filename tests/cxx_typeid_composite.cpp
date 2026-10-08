class TypeIdBase {
public:
    virtual int value() { return 7; }
};

struct TypeIdMemberOwnerA { int value; };
struct TypeIdMemberOwnerB { int value; };
int TypeIdMemberOwnerA::* member_a;
int TypeIdMemberOwnerB::* member_b;
int (*function_int)(int);
int (*function_long)(long);

extern "C" int main() {
    return (&typeid(TypeIdBase) == &typeid(TypeIdBase) &&
            &typeid(member_a) != &typeid(member_b) &&
            &typeid(function_int) != &typeid(function_long)) ? 0 : 1;
}
