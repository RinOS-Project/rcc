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
    bool static_pointer_identity =
        &typeid(base) == &typeid(TypeIdBase*);
    TypeIdBase* null_base = 0;
    bool bad_typeid_caught = false;
    try {
        (void)typeid(*null_base);
    } catch (...) {
        bad_typeid_caught = true;
    }
    return dynamic_identity && static_pointer_identity && bad_typeid_caught
        ? 0 : 1;
}
