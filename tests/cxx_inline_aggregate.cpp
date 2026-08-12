typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;
typedef unsigned int uint32_t;

typedef struct RinSliceV1 {
    uint64_t address;
    uint64_t size;
} RinSliceV1;

struct CxxPair final {
    int first;
    int second;
};

struct CxxVersioned final {
    uint32_t struct_size;
    uint32_t version;
    uint64_t payload;
};

class CxxBox final {
public:
    constexpr CxxBox() noexcept : value(0) {}
    constexpr explicit CxxBox(int input) noexcept : value(input) {}

    int value;
};

class CxxStatus final {
public:
    constexpr CxxStatus() noexcept : value_(0) {}
    constexpr explicit CxxStatus(int input) noexcept : value_(input) {}

    constexpr int code() const noexcept { return value_; }
    constexpr bool ok() const noexcept { return value_ == 0; }
    constexpr explicit operator bool() const noexcept { return ok(); }

private:
    int value_;
};

extern "C" int cxx_cleanup_close(int* value) {
    *value = *value + 1;
    return *value < 0 ? -7 : 0;
}

template<typename Handle>
class CxxUnique final {
public:
    constexpr explicit CxxUnique(Handle handle) noexcept : handle_(handle) {}
    CxxUnique(const CxxUnique&) = delete;
    CxxUnique& operator=(const CxxUnique&) = delete;

    constexpr CxxUnique(CxxUnique&& other) noexcept
        : handle_(other.release()) {}

    CxxUnique& operator=(CxxUnique&& other) noexcept {
        if (this != &other) {
            (void)close();
            handle_ = other.release();
        }
        return *this;
    }

    ~CxxUnique() {
        if (handle_ != 0) {
            (void)cxx_cleanup_close(handle_);
        }
    }

    Handle release() noexcept {
        Handle value = handle_;
        handle_ = 0;
        return value;
    }

    CxxStatus close() noexcept {
        if (handle_ == 0) return CxxStatus{};
        int result = cxx_cleanup_close(handle_);
        if (result == 0) handle_ = 0;
        return CxxStatus{result};
    }

    CxxStatus reset() noexcept { return close(); }

    Handle get() const noexcept { return handle_; }
    constexpr explicit operator bool() const noexcept {
        return handle_ != 0;
    }

private:
    Handle handle_;
};

extern "C" int cxx_cleanup_close_wide(uint64_t handle) {
    int* value = reinterpret_cast<int*>(static_cast<uintptr_t>(handle));
    *value = *value + 2;
    return 0;
}

class CxxWideUnique final {
public:
    constexpr explicit CxxWideUnique(uint64_t handle) noexcept
        : handle_(handle) {}
    CxxWideUnique(const CxxWideUnique&) = delete;
    CxxWideUnique& operator=(const CxxWideUnique&) = delete;

    constexpr CxxWideUnique(CxxWideUnique&& other) noexcept
        : handle_(other.release()) {}

    CxxWideUnique& operator=(CxxWideUnique&& other) noexcept {
        if (this != &other) {
            (void)close();
            handle_ = other.release();
        }
        return *this;
    }

    ~CxxWideUnique() {
        if (handle_ != 0) {
            (void)cxx_cleanup_close_wide(handle_);
        }
    }

    uint64_t release() noexcept {
        uint64_t value = handle_;
        handle_ = 0;
        return value;
    }

    CxxStatus close() noexcept {
        if (handle_ == 0) return CxxStatus{};
        int result = cxx_cleanup_close_wide(handle_);
        if (result == 0) handle_ = 0;
        return CxxStatus{result};
    }

    CxxStatus reset() noexcept { return close(); }

    uint64_t get() const noexcept { return handle_; }
    constexpr explicit operator bool() const noexcept {
        return handle_ != 0;
    }

private:
    uint64_t handle_;
};

template<typename T>
class CxxOutcome final {
public:
    constexpr CxxOutcome(int code, const T& value) noexcept
        : code_(code), value_(value) {}

    constexpr int code() const noexcept { return code_; }
    constexpr bool ok() const noexcept { return code_ == 0; }
    constexpr const T& value() const noexcept { return value_; }
    constexpr explicit operator bool() const noexcept { return ok(); }

private:
    int code_;
    T value_;
};

static CxxPair make_cxx_pair(int first, int second) {
    return CxxPair{first, second};
}

namespace rin {

template<typename T>
constexpr T versioned() noexcept {
    T value{};
    value.struct_size = sizeof(T);
    value.version = 7;
    return value;
}

inline int read_int_reference(const int& value) noexcept {
    return value;
}

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

inline RinSliceV1 copy_reference(const RinSliceV1& input) noexcept {
    return RinSliceV1{input.address, input.size};
}

inline RinSliceV1 choose(const RinSliceV1& input) noexcept {
    return RinSliceV1{input.address, input.size};
}

inline RinSliceV1 choose(const RinSliceV1* input) noexcept {
    return RinSliceV1{input->size, input->address};
}

inline int default_increment(int value, int amount = 5) noexcept {
    return value + amount;
}

inline int default_pair(int first = 3, int second = 4) noexcept {
    return first * 10 + second;
}

}

int cxx_default_source() {
    return 37;
}

int cxx_default_target(int first, int second = 7,
                       int third = cxx_default_source()) {
    return first * 100 + second * 10 + third;
}

int cxx_default_redeclared(int value = 13);

int cxx_default_redeclared(int value) {
    return value;
}

int cxx_default_accumulated(int first, int second = 19);
int cxx_default_accumulated(int first = 2, int second);

int cxx_default_accumulated(int first, int second) {
    return first * 100 + second;
}

extern "C" {

int cxx_default_c_linkage_target(int value = 17);

int cxx_default_c_linkage_target(int value) {
    return value;
}

}

RinSliceV1 mutable_slice(void* data, uint64_t size) {
    return rin::slice(data, size);
}

RinSliceV1 const_slice(const void* data, uint64_t size) {
    return rin::slice(data, size);
}

extern "C" {

int cxx_default_arguments(void) {
    return cxx_default_target(2);
}

int cxx_default_redeclaration(void) {
    return cxx_default_redeclared();
}

int cxx_default_redeclaration_accumulation(void) {
    return cxx_default_accumulated();
}

int cxx_default_c_linkage(void) {
    return cxx_default_c_linkage_target();
}

int cxx_default_namespace_inline(void) {
    return rin::default_increment(7) * 100 + rin::default_pair();
}

void* cxx_nullptr_return(void) {
    return nullptr;
}

int cxx_nullptr_context(void) {
    return nullptr ? -1 : 1;
}

int cxx_nullptr_comparisons(void* value) {
    return (nullptr == nullptr) * 1000 +
           (nullptr != nullptr) * 100 +
           (value != nullptr) * 10 +
           (nullptr == 0);
}

int cxx_nullptr_assignment(void* value) {
    void* pointer = value;
    pointer = nullptr;
    return pointer == nullptr;
}

int cxx_nullptr_auto(void* value) {
    auto first = nullptr;
    auto second = first;
    first = second;
    void* pointer = value;
    pointer = second;
    return (first == nullptr) * 1000 +
           (second == 0) * 100 +
           (!first) * 10 +
           (pointer == nullptr);
}

int cxx_nullptr_size(void) {
    auto value = nullptr;
    return sizeof(value);
}

void* cxx_nullptr_conditional(int choose, void* value) {
    auto empty = nullptr;
    return choose ? value : empty;
}

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

int cxx_versioned_template_value(void) {
    auto value = rin::versioned<CxxVersioned>();
    if (value.payload != 0) return -1;
    return value.struct_size * 100 + value.version;
}

int cxx_auto_function_call(int first, int second) {
    auto value = make_cxx_pair(first, second);
    return value.first * 100 + value.second;
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

int cxx_inline_accessor(int input) {
    CxxStatus status{input};
    return status.code() * 10 + status.ok();
}

int cxx_delegated_status_bool(int input) {
    CxxStatus status{input};
    int result = status ? 100 : 0;
    if (!status) result += 10;
    return result + (status ? 1 : 0);
}

int cxx_temporary_accessor(int input) {
    return CxxStatus{input}.code();
}

int cxx_pointer_accessor(const CxxStatus* status) {
    return status->code();
}

int cxx_template_outcome_accessor(int code, int value) {
    return CxxOutcome<int>{code, value}.code() * 1000 +
           rin::read_int_reference(
               CxxOutcome<int>{code, value}.value()) * 10 +
           CxxOutcome<int>{code, value}.ok();
}

int cxx_delegated_outcome_bool(int code, int value) {
    auto outcome = CxxOutcome<int>{code, value};
    int result = outcome ? 1000 : 100;
    if (outcome && value) result += 10;
    if (!outcome) result += 1;
    return result;
}

uint64_t cxx_template_outcome_wide_value(int code, uint64_t value) {
    return CxxOutcome<uint64_t>{code, value}.value();
}

int cxx_reference_call(uint64_t address, uint64_t size) {
    RinSliceV1 input{address, size};
    RinSliceV1 copied = rin::copy_reference(input);
    return copied.address == address && copied.size == size;
}

int cxx_reference_overload(uint64_t address, uint64_t size) {
    RinSliceV1 input{address, size};
    RinSliceV1 by_reference = rin::choose(input);
    RinSliceV1 by_pointer = rin::choose(&input);
    return by_reference.address == address &&
           by_reference.size == size &&
           by_pointer.address == size &&
           by_pointer.size == address;
}

int cxx_cleanup_block(int* value) {
    {
        auto handle = CxxUnique<int*>{value};
    }
    return *value;
}

int cxx_cleanup_return(int* value) {
    auto handle = CxxUnique<int*>{value};
    return value ? *value : 42;
}

int cxx_cleanup_wide(int* value) {
    auto handle = CxxWideUnique{
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value))};
    return *value;
}

int cxx_cleanup_release(int* value) {
    auto handle = CxxUnique<int*>{value};
    int* released = handle.release();
    return released == value;
}

int cxx_cleanup_wide_release(int* value) {
    auto handle = CxxWideUnique{
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value))};
    uint64_t released = handle.release();
    return released != 0;
}

int cxx_cleanup_get(int* value) {
    auto handle = CxxUnique<int*>{value};
    return handle.get() == value;
}

int cxx_cleanup_close_call(int* value) {
    auto handle = CxxUnique<int*>{value};
    auto result = handle.close();
    return result.code() * 100 + (!handle) * 10 + result.ok();
}

int cxx_cleanup_close_failure(int* value) {
    auto handle = CxxUnique<int*>{value};
    auto result = handle.close();
    int retained = handle.get() == value && handle;
    (void)handle.release();
    return (result.code() == -7) * 100 + retained * 10 +
           (*value == -9);
}

int cxx_cleanup_close_invalid(void) {
    auto handle = CxxUnique<int*>{0};
    auto result = handle.close();
    return result.code() * 100 + (!handle) * 10 + result.ok();
}

int cxx_cleanup_reset_call(int* value) {
    auto handle = CxxUnique<int*>{value};
    auto result = handle.reset();
    return result.code() * 100 + (!handle) * 10 + result.ok();
}

int cxx_cleanup_reset_failure(int* value) {
    auto handle = CxxUnique<int*>{value};
    auto result = handle.reset();
    int retained = handle.get() == value && handle;
    (void)handle.release();
    return (result.code() == -7) * 100 + retained * 10 +
           (*value == -9);
}

int cxx_cleanup_wide_reset_call(int* value) {
    auto handle = CxxWideUnique{
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value))};
    auto result = handle.reset();
    return result.code() * 100 + (!handle) * 10 + result.ok();
}

int cxx_cleanup_wide_close_call(int* value) {
    auto handle = CxxWideUnique{
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value))};
    auto result = handle.close();
    return result.code() * 100 + (!handle) * 10 + result.ok();
}

uint64_t cxx_cleanup_wide_get(uint64_t value) {
    auto handle = CxxWideUnique{value};
    uint64_t result = handle.get();
    (void)handle.release();
    return result;
}

int cxx_cleanup_break(int* value) {
    while (*value < 1000) {
        auto handle = CxxUnique<int*>{value};
        break;
    }
    return *value;
}

int cxx_cleanup_continue(int* value) {
    int remaining = 2;
    while (remaining > 0) {
        auto handle = CxxUnique<int*>{value};
        --remaining;
        continue;
    }
    return *value;
}

int cxx_cleanup_for_break(int* value) {
    for (auto handle = CxxUnique<int*>{value}; ; ) {
        break;
    }
    return *value;
}

int cxx_cleanup_for_continue(int* value) {
    int iteration = 0;
    for (auto outer = CxxUnique<int*>{value}; iteration < 2; ++iteration) {
        auto inner = CxxUnique<int*>{value};
        continue;
    }
    return *value;
}

int cxx_cleanup_zero_for(int* value) {
    for (auto handle = CxxUnique<int*>{value}; 0; ) {
        *value = 1000;
    }
    return *value;
}

int cxx_cleanup_switch(int* value, int selector) {
    auto outer = CxxUnique<int*>{value};
    switch (selector) {
        case 0: {
            auto inner = CxxUnique<int*>{value};
            break;
        }
        case 1: {
            auto first = CxxUnique<int*>{value};
        }
        case 2: {
            auto second = CxxUnique<int*>{value};
            break;
        }
        default:
            break;
    }
    return *value;
}

int cxx_cleanup_goto_exit(int* value) {
    {
        auto handle = CxxUnique<int*>{value};
        goto done;
    }
done:
    return *value;
}

int cxx_cleanup_goto_backward(int* value) {
    int remaining = 2;
retry:
    {
        auto handle = CxxUnique<int*>{value};
        --remaining;
        if (remaining > 0) {
            goto retry;
        }
    }
    return *value;
}

int cxx_cleanup_goto_same_scope(int* value) {
    auto handle = CxxUnique<int*>{value};
    goto done;
done:
    return *value;
}

int cxx_cleanup_goto_for_init(int* value) {
    for (auto handle = CxxUnique<int*>{value}; ; ) {
        goto done;
    }
done:
    return *value;
}

int cxx_cleanup_contextual_bool(int* value) {
    auto handle = CxxUnique<int*>{value};
    int conditional = handle ? 1 : 0;
    int conjunction = handle && conditional;
    int negated_before = !handle;
    (void)handle.release();
    int after_release = handle ? 1 : 0;
    int negated_after = !handle;
    return conditional * 1000 + conjunction * 100 +
           negated_before * 10 + after_release + negated_after;
}

int cxx_cleanup_wide_contextual_bool(int* value) {
    auto handle = CxxWideUnique{
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value))};
    int before = handle ? 1 : 0;
    (void)handle.release();
    int after = !handle;
    return before * 10 + after;
}

int cxx_cleanup_move(int* value) {
    auto source = CxxUnique<int*>{value};
    auto target = CxxUnique<int*>{
        static_cast<CxxUnique<int*>&&>(source)};
    return (!source) * 10 + (target ? 1 : 0);
}

int cxx_cleanup_wide_move(int* value) {
    auto source = CxxWideUnique{
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value))};
    auto target = CxxWideUnique{
        static_cast<CxxWideUnique&&>(source)};
    return (!source) * 10 + (target ? 1 : 0);
}

int cxx_cleanup_move_assignment(int* old_value, int* new_value) {
    auto source = CxxUnique<int*>{new_value};
    auto target = CxxUnique<int*>{old_value};
    int* moved = (target = static_cast<CxxUnique<int*>&&>(source)).get();
    int result = (!source) * 100 + (moved == new_value) * 10 +
                 (target ? 1 : 0);
    (void)target.release();
    return result;
}

int cxx_cleanup_move_self_assignment(int* value) {
    auto target = CxxUnique<int*>{value};
    target = static_cast<CxxUnique<int*>&&>(target);
    int result = target.get() == value && target;
    (void)target.release();
    return result;
}

int cxx_cleanup_wide_move_assignment(int* old_value, int* new_value) {
    auto source = CxxWideUnique{
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new_value))};
    auto target = CxxWideUnique{
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(old_value))};
    target = static_cast<CxxWideUnique&&>(source);
    int result = (!source) * 100 + (target ? 1 : 0);
    (void)target.release();
    return result;
}

int cxx_cleanup_contextual_control(int* value) {
    auto handle = CxxUnique<int*>{value};
    int result = 0;
    if (handle) {
        result += 1;
    }
    int once = 1;
    while (handle && once) {
        result += 10;
        once = 0;
    }
    do {
        result += 100;
    } while (!handle);
    for (; handle; ) {
        result += 1000;
        break;
    }
    (void)handle.release();
    if (!handle) {
        result += 10000;
    }
    return result;
}

}
