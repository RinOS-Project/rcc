namespace TypeIdScopeA {
enum Value { value_a };
struct Payload { int value; };
}
namespace TypeIdScopeB {
enum Value { value_b };
struct Payload { int value; };
}
template <class T> struct TypeIdBox { T value; };

TypeIdBox<TypeIdScopeA::Payload> type_id_box_a;
TypeIdBox<TypeIdScopeB::Payload> type_id_box_b;

struct TypeIdAnonymousScopeClass { int value; };
enum TypeIdAnonymousScopeEnum { global_scope_value };

static int local_class_scope_identity_is_distinct() {
    class TypeIdAnonymousScopeClass { public: int local_value; };
    const void* local_type = &typeid(TypeIdAnonymousScopeClass);
    if (local_type == &typeid(::TypeIdAnonymousScopeClass)) return 1;
    {
        class TypeIdAnonymousScopeClass { public: int nested_value; };
        if (local_type == &typeid(TypeIdAnonymousScopeClass)) return 1;
    }
    return local_type == &typeid(TypeIdAnonymousScopeClass) ? 0 : 1;
}

namespace {
struct TypeIdAnonymousScopeClass { int value; };
enum TypeIdAnonymousScopeEnum { anonymous_scope_value };

int anonymous_scope_typeinfo_is_distinct() {
    return (&typeid(TypeIdAnonymousScopeClass) !=
                &typeid(::TypeIdAnonymousScopeClass) &&
            &typeid(anonymous_scope_value) !=
                &typeid(global_scope_value)) ? 0 : 1;
}
}

extern "C" int main() {
    return (&typeid(type_id_box_a) != &typeid(type_id_box_b) &&
            &typeid(TypeIdScopeA::Value) != &typeid(TypeIdScopeB::Value) &&
            local_class_scope_identity_is_distinct() == 0 &&
            anonymous_scope_typeinfo_is_distinct() == 0)
        ? 0 : 1;
}
