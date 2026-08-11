typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;

typedef struct RinSliceV1 {
    uint64_t address;
    uint64_t size;
} RinSliceV1;

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
