static thread_local int initialization_count;

static int make_thread_local_value(int initial_value) {
    ++initialization_count;
    if (initialization_count == 1) throw 29;
    return initial_value;
}

int read_function_static_thread_local(int initial_value) {
    static thread_local int value =
        make_thread_local_value(initial_value);
    return value;
}

extern "C" int verified_thread_local_static_worker(int initial_value) {
    if (initialization_count != 0) return 1;
    try {
        (void)read_function_static_thread_local(13);
        return 2;
    } catch (int error) {
        if (error != 29) return 3;
    }
    if (initialization_count != 1) return 4;
    if (read_function_static_thread_local(initial_value + 24) !=
        initial_value + 24) return 5;
    if (initialization_count != 2) return 6;
    if (read_function_static_thread_local(initial_value + 86) !=
        initial_value + 24) return 7;
    return initialization_count == 2 ? 0 : 8;
}

int main() {
    return verified_thread_local_static_worker(13);
}
