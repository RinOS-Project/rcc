extern "C" int main() {
    const char* int_name = typeid(int).name();
    const char* long_name = typeid(long).name();
    bool names_differ = false;
    bool opposite_before = typeid(int).before(typeid(long)) !=
                           typeid(long).before(typeid(int));
    for (int index = 0; int_name && long_name && index < 64; ++index) {
        if (int_name[index] != long_name[index]) {
            names_differ = true;
            break;
        }
        if (int_name[index] == '\0') break;
    }
    return (int_name && int_name[0] != '\0' &&
            long_name && long_name[0] != '\0' && names_differ &&
            !typeid(int).before(typeid(int)) && opposite_before &&
            typeid(int).hash_code() == typeid(int).hash_code() &&
            typeid(int).hash_code() != typeid(long).hash_code()) ? 0 : 1;
}
