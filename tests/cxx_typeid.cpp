class TypeIdBase {
public:
    virtual int value() { return 7; }
};

extern "C" int main() {
    /* The bounded frontend exposes one stable address for each complete
     * static type.  A polymorphic expression is intentionally not used here:
     * its runtime null/bad_typeid path is a separate ABI feature. */
    return (&typeid(int) == &typeid(int) &&
            &typeid(int) != &typeid(long) &&
            &typeid(TypeIdBase) == &typeid(TypeIdBase) &&
            &typeid(1) == &typeid(int)) ? 0 : 1;
}
