extern "C" const void* cxx_typeid_peer_int_pointer() {
    return &typeid(int*);
}

namespace TypeIdCrossTU {
struct Payload { int value; };
}
template <class T> struct TypeIdCrossTUBox { T value; };

TypeIdCrossTUBox<int> type_id_peer_ordering_probe;
TypeIdCrossTUBox<TypeIdCrossTU::Payload> type_id_peer_template_box;

extern "C" const void* cxx_typeid_peer_template_box() {
    return &typeid(type_id_peer_template_box);
}
