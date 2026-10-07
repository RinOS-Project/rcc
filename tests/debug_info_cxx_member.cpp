class DebugMemberObject {
private:
    int secret : 4;
protected:
    int protected_value;
public:
    int value;
    int read() const { return value; }
    static int create_value(int seed) { return seed + 1; }
};

int debug_member_object_entry(DebugMemberObject* object) {
    return object->read() + DebugMemberObject::create_value(object->value);
}
