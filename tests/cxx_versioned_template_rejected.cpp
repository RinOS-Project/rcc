typedef unsigned int uint32_t;

struct RejectedVersioned {
    uint32_t struct_size;
    uint32_t version;
    uint32_t payload;
};

template<typename T>
constexpr T unsafe_versioned() noexcept {
    T value{};
    value.struct_size = sizeof(T);
    value.version = 3;
    value.payload = 9;
    return value;
}

int rejected_versioned_template(void) {
    auto value = unsafe_versioned<RejectedVersioned>();
    return value.version;
}
