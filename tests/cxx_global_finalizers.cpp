/* Validated static-storage cleanup is emitted into .fini_array. */

extern "C" int global_cleanup_count = 0;
extern "C" int global_cleanup_sequence = 0;

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

int main(void) {
    return global_storage == 9 && global_cleanup_count == 0 ? 0 : 1;
}
