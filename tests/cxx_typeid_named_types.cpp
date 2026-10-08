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

extern "C" int main() {
    return &typeid(type_id_box_a) != &typeid(type_id_box_b) ? 0 : 1;
}
