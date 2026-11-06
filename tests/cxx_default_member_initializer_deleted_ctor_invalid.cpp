struct DeletedDefaultMemberInitializer {
    int value = 53;

    DeletedDefaultMemberInitializer() = delete;
};

extern "C" int invalid_deleted_default_member_initializer() {
    DeletedDefaultMemberInitializer value;
    return value.value;
}
