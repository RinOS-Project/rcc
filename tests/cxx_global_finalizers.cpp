/* Validated static-storage cleanup is emitted into .fini_array. */

extern "C" int global_cleanup_count = 0;
extern "C" int global_cleanup_sequence = 0;
extern "C" unsigned int global_array_cleanup_count = 0;
extern "C" unsigned int global_array_expected_index = 4101;
extern "C" int global_array_cleanup_order_error = 0;
extern "C" unsigned int global_array_finalizer_phase = 0;

extern "C" int global_cleanup_close(int* value) {
    if (!value) return -1;
    ++global_cleanup_count;
    global_cleanup_sequence = global_cleanup_sequence * 10 + *value;
    *value = 0;
    return 0;
}

class GlobalHandle final {
public:
    constexpr explicit GlobalHandle(int* value) noexcept : value_(value) {}

    ~GlobalHandle() {
        if (value_ != 0) {
            (void)global_cleanup_close(value_);
        }
    }

private:
    int* value_;
};

extern "C" int global_storage = 9;
extern "C" int second_storage = 7;
GlobalHandle global_handle{&global_storage};
GlobalHandle second_handle{&second_storage};

struct GlobalArrayElement {
    unsigned int index;
    unsigned int kind;

    ~GlobalArrayElement() {
        unsigned int expected_kind = global_array_finalizer_phase == 0 ? 2 : 1;
        if (kind != expected_kind || index != global_array_expected_index) {
            global_array_cleanup_order_error = 1;
        }
        if (global_array_expected_index != 0) --global_array_expected_index;
        ++global_array_cleanup_count;
        if (global_array_expected_index == 0) {
            if (global_array_finalizer_phase == 0) {
                global_array_finalizer_phase = 1;
                global_array_expected_index = 4101;
            } else if (global_array_finalizer_phase == 1) {
                global_array_finalizer_phase = 2;
            } else {
                global_array_cleanup_order_error = 1;
            }
        }
    }
};

GlobalArrayElement zero_initialized_array[4101];
GlobalArrayElement explicitly_initialized_array[4101] = {};

int main(void) {
    if (global_storage != 9 || global_cleanup_count != 0 ||
        zero_initialized_array[0].index != 0 ||
        zero_initialized_array[4100].index != 0 ||
        explicitly_initialized_array[0].index != 0 ||
        explicitly_initialized_array[4100].index != 0) {
        return 1;
    }
    for (unsigned int index = 0; index < 4101; ++index) {
        zero_initialized_array[index].index = index + 1;
        zero_initialized_array[index].kind = 1;
        explicitly_initialized_array[index].index = index + 1;
        explicitly_initialized_array[index].kind = 2;
    }
    return global_array_cleanup_count == 0 &&
                   global_array_expected_index == 4101 &&
                   global_array_finalizer_phase == 0
               ? 0
               : 2;
}
