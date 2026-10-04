class TypeIdBase {
public:
    virtual int value() { return 7; }
};

class TypeIdDerived : public TypeIdBase {
public:
    virtual int value() { return 9; }
};

extern "C" int main() {
    TypeIdDerived object;
    TypeIdBase* base = &object;
    bool dynamic_identity =
        &typeid(*base) == &typeid(TypeIdDerived);
    const char* dynamic_name = typeid(*base).name();
    bool dynamic_name_valid = dynamic_name && dynamic_name[0] != '\0';
    bool static_pointer_identity =
        &typeid(base) == &typeid(TypeIdBase*);
    TypeIdBase* null_base = 0;
    bool bad_typeid_caught = false;
    try {
        (void)typeid(*null_base);
    } catch (...) {
        bad_typeid_caught = true;
    }
    return dynamic_identity && dynamic_name_valid &&
           static_pointer_identity && bad_typeid_caught
        ? 0 : 1;
}
