template<typename... Ts>
struct type_pack_class {
    inline static int count = sizeof...(Ts);
};

int main() {
    return type_pack_class<>::count == 0 &&
           type_pack_class<int, long, char>::count == 3 ? 0 : 1;
}
