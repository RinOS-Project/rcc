class TypeIdBase {
public:
    virtual int value() { return 7; }
};

extern "C" int main() {
    /* The bounded frontend exposes one stable address and hash for each
     * complete static type.  Dynamic polymorphic typeid is covered by the
     * dedicated exception/RTTI regression. */
    return (&typeid(int) == &typeid(int) &&
            &typeid(int) != &typeid(long) &&
            &typeid(TypeIdBase) == &typeid(TypeIdBase) &&
            &typeid(1) == &typeid(int) &&
            typeid(int) == typeid(int) &&
            typeid(int) != typeid(long) &&
            typeid(int).hash_code() == typeid(int).hash_code() &&
            typeid(int).hash_code() != typeid(long).hash_code()) ? 0 : 1;
}
