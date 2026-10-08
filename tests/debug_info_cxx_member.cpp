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

int debug_reference_type_entry(int& lvalue, int&& rvalue) {
    int& local_lvalue = lvalue;
    return local_lvalue + rvalue;
}

extern "C" int debug_cxx_for_initializer_scope(int limit) {
    int outer_value = 0;
    for (int loop_index = 0; loop_index < limit; ++loop_index) {
        outer_value += loop_index;
    }
    return outer_value;
}
