int main() {
    int value = 41;
    int explicit_value = []<typename T>(T input) {
        return input + 1;
    }(value);
    int explicit_pointer = []<class T>(T* input) {
        return *input + 1;
    }(&value);
    int explicit_pack = []<typename... T>(T... values) {
        return (0 + ... + values);
    }(1, 2, 3);
    return explicit_value == 42 && explicit_pointer == 42 &&
           explicit_pack == 6 ? 0 : 1;
}
