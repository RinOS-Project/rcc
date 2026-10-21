static thread_local int initialization_count;
static thread_local int pointer_initialization_count;
static thread_local int thread_local_marker;

static int make_thread_local_value(int initial_value) {
    ++initialization_count;
    if (initialization_count == 1) throw 29;
    return initial_value;
}

static int* make_thread_local_marker_pointer() {
    ++pointer_initialization_count;
    if (pointer_initialization_count == 1) throw 31;
    return &thread_local_marker;
}

int read_function_static_thread_local(int initial_value) {
    static thread_local int value =
        make_thread_local_value(initial_value);
    return value;
}

int* read_function_static_thread_local_pointer() {
    static thread_local int* value = make_thread_local_marker_pointer();
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
    if (initialization_count != 2) return 8;

    thread_local_marker = initial_value + 101;
    try {
        (void)read_function_static_thread_local_pointer();
        return 9;
    } catch (int error) {
        if (error != 31) return 10;
    }
    if (pointer_initialization_count != 1) return 11;
    int* marker = read_function_static_thread_local_pointer();
    if (marker != &thread_local_marker ||
        *marker != initial_value + 101) return 12;
    if (pointer_initialization_count != 2) return 13;
    if (read_function_static_thread_local_pointer() != marker ||
        pointer_initialization_count != 2) return 14;
    return 0;
}

int main() {
    return verified_thread_local_static_worker(13);
}
