typedef int **************************************** TypeIdDeepInt;
typedef long **************************************** TypeIdDeepLong;

TypeIdDeepInt type_id_deep_int;
TypeIdDeepLong type_id_deep_long;

extern "C" int main() {
    return (&typeid(type_id_deep_int) != &typeid(type_id_deep_long) &&
            &typeid(TypeIdDeepInt) == &typeid(type_id_deep_int)) ? 0 : 1;
}
