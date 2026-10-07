class DebugMemberObject {
private:
    int secret : 4;
    int secret_value() const { return secret; }
protected:
    int protected_value;
    int protected_read() const { return protected_value; }
public:
    int value;
    int read() const { return value; }
    static int create_value(int seed) { return seed + 1; }
};

int debug_member_object_entry(DebugMemberObject* object) {
    return object->read() + DebugMemberObject::create_value(object->value);
}
