typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;

typedef struct RinSliceV1 {
    uint64_t address;
    uint64_t size;
} RinSliceV1;

struct CxxPair final {
    int first;
    int second;
};

class CxxBox final {
public:
    constexpr CxxBox() noexcept : value(0) {}
    constexpr explicit CxxBox(int input) noexcept : value(input) {}

    int value;
};

static CxxPair make_cxx_pair(int first, int second) {
    return CxxPair{first, second};
}

namespace rin {

inline RinSliceV1 slice(void* data, uint64_t size) noexcept {
    return RinSliceV1{static_cast<uint64_t>(
                          reinterpret_cast<uintptr_t>(data)),
                      size};
}

inline RinSliceV1 slice(const void* data, uint64_t size) noexcept {
    return RinSliceV1{static_cast<uint64_t>(
                          reinterpret_cast<uintptr_t>(data)),
                      size};
}

}

RinSliceV1 mutable_slice(void* data, uint64_t size) {
    return rin::slice(data, size);
}

RinSliceV1 const_slice(const void* data, uint64_t size) {
    return rin::slice(data, size);
}

extern "C" {

int cxx_direct_value_init(void) {
    return RinSliceV1{}.address == 0 && RinSliceV1{}.size == 0;
}

int cxx_local_value_init(void) {
    RinSliceV1 value{};
    return value.address == 0 && value.size == 0;
}

int cxx_scalar_value_init(void) {
    uint64_t value{};
    return value == 0;
}

int cxx_class_aggregate_init(int first, int second) {
    CxxPair local{first, second};
    CxxPair returned = make_cxx_pair(second, first);
    return local.first * 1000 + local.second * 100 +
           returned.first * 10 + returned.second;
}

int cxx_lowered_constructor_init(int input) {
    CxxBox zero{};
    CxxBox value{input};
    return value.value * 10 + zero.value;
}

}
