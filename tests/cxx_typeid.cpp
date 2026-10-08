namespace TypeIdCrossTU {
struct Payload { int value; };
}
template <class T> struct TypeIdCrossTUBox { T value; };
TypeIdCrossTUBox<TypeIdCrossTU::Payload> type_id_cross_tu_box;
namespace {
struct TypeIdInternalPayload { int value; };
}
extern "C" const void* cxx_typeid_peer_int_pointer();
extern "C" const void* cxx_typeid_peer_template_box();
extern "C" const void* cxx_typeid_peer_internal_payload();

extern "C" int main() {
    return (&typeid(int) == &typeid(int) &&
            &typeid(int) != &typeid(long) &&
            &typeid(1) == &typeid(int) &&
            &typeid(const int*) != &typeid(int*) &&
            &typeid(int* const) == &typeid(int*) &&
            &typeid(int&) == &typeid(int) &&
            &typeid(int*) == cxx_typeid_peer_int_pointer() &&
            &typeid(type_id_cross_tu_box) ==
                cxx_typeid_peer_template_box() &&
            &typeid(TypeIdInternalPayload) !=
                cxx_typeid_peer_internal_payload()) ? 0 : 1;
}
