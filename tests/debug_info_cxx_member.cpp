struct DebugMemberObject {
    int value;
    int read() const { return value; }
};

int debug_member_object_entry(DebugMemberObject* object) {
    return object->read();
}
