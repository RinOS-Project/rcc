extern "C" int unsafe_assignment_close(int* value);
extern "C" void unsafe_assignment_observe(void);

class UnsafeAssignmentStatus final {
public:
    constexpr UnsafeAssignmentStatus() noexcept : code_(0) {}
    constexpr explicit UnsafeAssignmentStatus(int code) noexcept
        : code_(code) {}

private:
    int code_;
};

class UnsafeMoveAssignment final {
public:
    constexpr explicit UnsafeMoveAssignment(int* value) noexcept
        : value_(value) {}
    UnsafeMoveAssignment(const UnsafeMoveAssignment&) = delete;
    UnsafeMoveAssignment& operator=(const UnsafeMoveAssignment&) = delete;

    constexpr UnsafeMoveAssignment(UnsafeMoveAssignment&& other) noexcept
        : value_(other.release()) {}

    UnsafeMoveAssignment& operator=(UnsafeMoveAssignment&& other) noexcept {
        if (this != &other) {
            (void)close();
            value_ = other.release();
        }
        return *this;
    }

    ~UnsafeMoveAssignment() {
        if (value_ != 0) {
            (void)unsafe_assignment_close(value_);
        }
    }

    int* release() noexcept {
        int* value = value_;
        value_ = 0;
        return value;
    }

    UnsafeAssignmentStatus close() noexcept {
        if (value_ == 0) return UnsafeAssignmentStatus{};
        int result = unsafe_assignment_close(value_);
        unsafe_assignment_observe();
        if (result == 0) value_ = 0;
        return UnsafeAssignmentStatus{result};
    }

private:
    int* value_;
};

int unsafe_move_assignment_rejected(int* old_value, int* new_value) {
    auto source = UnsafeMoveAssignment{new_value};
    auto target = UnsafeMoveAssignment{old_value};
    target = static_cast<UnsafeMoveAssignment&&>(source);
    return target.release() == new_value;
}

int unsafe_copy_assignment_rejected(int* old_value, int* new_value) {
    auto source = UnsafeMoveAssignment{new_value};
    auto target = UnsafeMoveAssignment{old_value};
    target = source;
    return target.release() == new_value;
}

int unsafe_close_call_rejected(int* value) {
    auto handle = UnsafeMoveAssignment{value};
    auto result = handle.close();
    (void)handle.release();
    return 0;
}
