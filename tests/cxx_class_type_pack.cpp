template<typename... Ts>
struct type_pack_class {
    inline static int count = sizeof...(Ts);
};

template<typename... Ts>
struct type_pack_storage {
    int values[sizeof...(Ts) + 1];
};

int main() {
    return type_pack_class<>::count == 0 &&
           type_pack_class<int, long, char>::count == 3 &&
           sizeof(type_pack_storage<int, long>) == 12 ? 0 : 1;
}
