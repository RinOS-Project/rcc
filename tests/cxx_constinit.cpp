static int global_target = 17;
constinit int mutable_global = 7;
constinit int* mutable_pointer = &global_target;
static constinit int static_global = 9;
static int read_and_update_static_local(void) {
    static constinit int local = 13;
    local += 2;
    return local;
}

int main(void) {
    if (mutable_global != 7 || mutable_pointer != &global_target ||
        static_global != 9) {
        return 1;
    }
    if (read_and_update_static_local() != 15 ||
        read_and_update_static_local() != 17) {
        return 2;
    }
    mutable_global = 23;
    return mutable_global == 23 ? 0 : 3;
}
