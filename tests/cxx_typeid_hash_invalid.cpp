extern "C" int main() {
    return (int)typeid(int).hash_code(1) +
           (int)typeid(int).name(1) +
           (typeid(int) < typeid(long));
}
