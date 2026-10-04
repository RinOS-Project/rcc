class DynamicType {
public:
    virtual int value() { return 1; }
};

extern "C" DynamicType make_dynamic();

extern "C" int probe() {
    /* A polymorphic prvalue needs a temporary lifetime/runtime path that is
     * not part of the bounded glvalue implementation. */
    return &typeid(make_dynamic()) != 0;
}
