#include <atomic>

static_assert(__atomic_always_lock_free(sizeof(unsigned), nullptr),
              "four-byte atomics are always lock-free");
static_assert(__atomic_always_lock_free(8u, nullptr),
              "eight-byte atomics are always lock-free");
static_assert(!__atomic_always_lock_free(3u, nullptr),
              "unsupported atomic widths are not lock-free");
static_assert(ATOMIC_BOOL_LOCK_FREE == 2 && ATOMIC_CHAR_LOCK_FREE == 2 &&
                  ATOMIC_CHAR16_T_LOCK_FREE == 2 &&
                  ATOMIC_CHAR32_T_LOCK_FREE == 2 &&
                  ATOMIC_WCHAR_T_LOCK_FREE == 2 &&
                  ATOMIC_SHORT_LOCK_FREE == 2 && ATOMIC_INT_LOCK_FREE == 2 &&
                  ATOMIC_LONG_LOCK_FREE == 2 && ATOMIC_LLONG_LOCK_FREE == 2 &&
                  ATOMIC_POINTER_LOCK_FREE == 2,
              "RinOS x86 atomic lock-free macros match the supported widths");
static_assert(std::atomic<unsigned>::is_always_lock_free,
              "std::atomic exposes its lock-free property as a constant");
static_assert(std::atomic<unsigned long long>::is_always_lock_free,
              "wide atomics expose their lock-free property as a constant");

template<class T>
class InheritedStaticTemplateBase {
public:
    using value_type = T;
    static constexpr bool is_always_lock_free =
        sizeof(T) == 1 || sizeof(T) == 2 ||
        sizeof(T) == 4 || sizeof(T) == 8;

    constexpr InheritedStaticTemplateBase() noexcept : value_(0) {}
    constexpr InheritedStaticTemplateBase(T value) noexcept : value_(value) {}
    constexpr T value() const noexcept { return value_; }
    static constexpr T doubled(T value) noexcept { return value * 2; }
    static constexpr T doubled(T value, T multiplier) noexcept {
        return value * multiplier;
    }

private:
    T value_;
};

template<class T>
class InheritedStaticTemplateDerived :
    public InheritedStaticTemplateBase<T> {
public:
    constexpr InheritedStaticTemplateDerived() noexcept
        : InheritedStaticTemplateBase<T>() {}
    constexpr InheritedStaticTemplateDerived(T value) noexcept
        : InheritedStaticTemplateBase<T>(value) {}
};

template<class T>
struct CxxVolatileTemplateClass {
    T value;
};

struct AtomicRefAggregate {
    unsigned first;
    unsigned second;
};

struct AtomicRefWordAggregate {
    unsigned value;
};

struct AtomicRefOverloadedArithmetic {
    unsigned value;
};

AtomicRefOverloadedArithmetic operator+(
    AtomicRefOverloadedArithmetic left,
    AtomicRefOverloadedArithmetic right);

struct AtomicRefOverloadedBitwise {
    unsigned value;
};

AtomicRefOverloadedBitwise operator&(
    AtomicRefOverloadedBitwise left,
    AtomicRefOverloadedBitwise right);

template<class T>
concept AtomicRefHasFetchAdd = requires(std::atomic_ref<T> reference,
                                        T operand) {
    reference.fetch_add(operand);
};

template<class T>
concept AtomicRefHasFetchAnd = requires(std::atomic_ref<T> reference,
                                        T operand) {
    reference.fetch_and(operand);
};

template<class T>
concept AtomicRefHasFetchSub = requires(std::atomic_ref<T> reference,
                                        T operand) {
    reference.fetch_sub(operand);
};

template<class T>
concept AtomicRefHasFetchOr = requires(std::atomic_ref<T> reference,
                                       T operand) {
    reference.fetch_or(operand);
};

template<class T>
concept AtomicRefHasFetchXor = requires(std::atomic_ref<T> reference,
                                        T operand) {
    reference.fetch_xor(operand);
};

template<class T>
concept AtomicRefHasAddAssign = requires(std::atomic_ref<T> reference,
                                         T operand) {
    reference += operand;
};

template<class T>
concept AtomicRefHasAndAssign = requires(std::atomic_ref<T> reference,
                                         T operand) {
    reference &= operand;
};

template<class T>
concept AtomicRefHasSubAssign = requires(std::atomic_ref<T> reference,
                                         T operand) {
    reference -= operand;
};

template<class T>
concept AtomicRefHasOrAssign = requires(std::atomic_ref<T> reference,
                                        T operand) {
    reference |= operand;
};

template<class T>
concept AtomicRefHasXorAssign = requires(std::atomic_ref<T> reference,
                                         T operand) {
    reference ^= operand;
};

template<class T>
concept AtomicHasFetchAnd = requires(std::atomic<T> value, T operand) {
    value.fetch_and(operand);
};

template<class T>
concept AtomicHasFetchOr = requires(std::atomic<T> value, T operand) {
    value.fetch_or(operand);
};

template<class T>
concept AtomicHasFetchXor = requires(std::atomic<T> value, T operand) {
    value.fetch_xor(operand);
};

template<class T>
concept AtomicHasAndAssign = requires(std::atomic<T> value, T operand) {
    value &= operand;
};

template<class T>
concept AtomicHasOrAssign = requires(std::atomic<T> value, T operand) {
    value |= operand;
};

template<class T>
concept AtomicHasXorAssign = requires(std::atomic<T> value, T operand) {
    value ^= operand;
};

template<class T>
concept VolatileAtomicHasFetchAnd =
    requires(volatile std::atomic<T>* value, T operand) {
        value->fetch_and(operand);
    };

template<class T>
concept VolatileAtomicHasAndAssign =
    requires(volatile std::atomic<T>* value, T operand) {
        *value &= operand;
    };

struct AtomicRefNonTrivial {
    explicit AtomicRefNonTrivial(unsigned initial) : value(initial) {}
    AtomicRefNonTrivial(const AtomicRefNonTrivial& other)
        : value(other.value) {}
    unsigned value;
};

static_assert(AtomicRefHasFetchAdd<unsigned>);
static_assert(AtomicRefHasFetchAdd<float>);
static_assert(!AtomicRefHasFetchAdd<AtomicRefAggregate>);
static_assert(!AtomicRefHasFetchAdd<AtomicRefOverloadedArithmetic>);
static_assert(AtomicRefHasFetchSub<unsigned>);
static_assert(AtomicRefHasFetchSub<float>);
static_assert(!AtomicRefHasFetchSub<AtomicRefAggregate>);
static_assert(AtomicRefHasFetchAnd<unsigned>);
static_assert(!AtomicRefHasFetchAnd<float>);
static_assert(!AtomicRefHasFetchAnd<AtomicRefAggregate>);
static_assert(!AtomicRefHasFetchAnd<AtomicRefOverloadedBitwise>);
static_assert(AtomicRefHasFetchOr<unsigned>);
static_assert(!AtomicRefHasFetchOr<float>);
static_assert(!AtomicRefHasFetchOr<AtomicRefAggregate>);
static_assert(AtomicRefHasFetchXor<unsigned>);
static_assert(!AtomicRefHasFetchXor<float>);
static_assert(!AtomicRefHasFetchXor<AtomicRefAggregate>);
static_assert(AtomicRefHasAddAssign<unsigned>);
static_assert(AtomicRefHasAddAssign<float>);
static_assert(!AtomicRefHasAddAssign<AtomicRefAggregate>);
static_assert(!AtomicRefHasAddAssign<AtomicRefOverloadedArithmetic>);
static_assert(AtomicRefHasSubAssign<unsigned>);
static_assert(AtomicRefHasSubAssign<float>);
static_assert(!AtomicRefHasSubAssign<AtomicRefAggregate>);
static_assert(AtomicRefHasAndAssign<unsigned>);
static_assert(!AtomicRefHasAndAssign<float>);
static_assert(!AtomicRefHasAndAssign<AtomicRefOverloadedBitwise>);
static_assert(AtomicRefHasOrAssign<unsigned>);
static_assert(!AtomicRefHasOrAssign<float>);
static_assert(!AtomicRefHasOrAssign<AtomicRefAggregate>);
static_assert(AtomicRefHasXorAssign<unsigned>);
static_assert(!AtomicRefHasXorAssign<float>);
static_assert(!AtomicRefHasXorAssign<AtomicRefAggregate>);
static_assert(AtomicHasFetchAnd<unsigned>);
static_assert(!AtomicHasFetchAnd<float>);
static_assert(!AtomicHasFetchAnd<AtomicRefAggregate>);
static_assert(!AtomicHasFetchAnd<AtomicRefOverloadedBitwise>);
static_assert(AtomicHasFetchOr<unsigned>);
static_assert(!AtomicHasFetchOr<float>);
static_assert(!AtomicHasFetchOr<AtomicRefAggregate>);
static_assert(AtomicHasFetchXor<unsigned>);
static_assert(!AtomicHasFetchXor<float>);
static_assert(!AtomicHasFetchXor<AtomicRefAggregate>);
static_assert(AtomicHasAndAssign<unsigned>);
static_assert(!AtomicHasAndAssign<float>);
static_assert(!AtomicHasAndAssign<AtomicRefOverloadedBitwise>);
static_assert(AtomicHasOrAssign<unsigned>);
static_assert(!AtomicHasOrAssign<float>);
static_assert(AtomicHasXorAssign<unsigned>);
static_assert(!AtomicHasXorAssign<float>);
static_assert(VolatileAtomicHasFetchAnd<unsigned>);
static_assert(!VolatileAtomicHasFetchAnd<float>);
static_assert(VolatileAtomicHasAndAssign<unsigned>);
static_assert(!VolatileAtomicHasAndAssign<float>);

template<class T>
T cxx_volatile_class_template_deduction(
    volatile CxxVolatileTemplateClass<T>* object) {
    return object->value;
}

template<class T>
T cxx_volatile_atomic_template_deduction(
    volatile std::atomic<T>* object) {
    return object->load();
}

extern "C" int cpp_atomic_always_lock_free_value() {
    return __atomic_always_lock_free(8, nullptr) &&
           __atomic_is_lock_free(4, nullptr);
}

extern "C" int cpp_atomic_is_lock_free_value(unsigned count, int* cursor) {
    return __atomic_is_lock_free(count, cursor++);
}

extern "C" int cpp_signed_parameter_value(signed value) {
    return value;
}

extern "C" void cpp_atomic_generic_load(const unsigned* value,
                                          unsigned* result) {
    __atomic_load(value, result, __ATOMIC_ACQUIRE);
}

extern "C" void cpp_atomic_generic_store(unsigned* value,
                                           unsigned* desired) {
    __atomic_store(value, desired, __ATOMIC_RELEASE);
}

extern "C" unsigned cpp_atomic_generic_exchange(unsigned* value,
                                                  unsigned* desired) {
    unsigned previous;
    __atomic_exchange(value, desired, &previous, __ATOMIC_ACQ_REL);
    return previous;
}

extern "C" bool cpp_atomic_generic_compare(unsigned* value,
                                            unsigned* expected,
                                            unsigned* desired) {
    return __atomic_compare_exchange(value, expected, desired, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

static unsigned test_atomic_ref() {
    unsigned storage = 5u;
    std::atomic_ref<unsigned> reference{storage};
    unsigned expected = 9u;

    static_assert(std::atomic_ref<unsigned>::is_always_lock_free,
                  "atomic_ref exposes the underlying lock-free width");
    static_assert(std::atomic_ref<unsigned>::required_alignment >=
                      alignof(unsigned),
                  "atomic_ref reports an adequate object alignment");
    if (!reference.is_lock_free() || reference.load() != 5u) return 120u;
    reference.store(7u, std::memory_order_release);
    if (reference.load(std::memory_order_acquire) != 7u ||
        reference.exchange(9u) != 7u)
        return 121u;
    if (!reference.compare_exchange_weak(
            expected, 10u, std::memory_order_acq_rel,
            std::memory_order_acquire) || reference.load() != 10u)
        return 122u;
    if (reference.fetch_add(3u) != 10u || reference.fetch_sub(1u) != 13u ||
        reference.fetch_and(10u) != 12u || reference.fetch_or(1u) != 8u ||
        reference.fetch_xor(3u) != 9u || reference.load() != 10u)
        return 123u;
    reference = 6u;
    unsigned converted = reference;
    if (converted != 6u || reference.load() != 6u) return 126u;
    if ((reference += 3u) != 9u || (reference -= 2u) != 7u ||
        (reference &= 3u) != 3u || (reference |= 8u) != 11u ||
        (reference ^= 1u) != 10u || reference.load() != 10u)
        return 129u;
    if (reference++ != 10u || ++reference != 12u ||
        reference-- != 12u || --reference != 10u ||
        reference.load() != 10u)
        return 130u;

    int signed_storage = 2147483647;
    std::atomic_ref<int> signed_reference{signed_storage};
    if (signed_reference.fetch_add(1) != 2147483647 ||
        signed_reference.load() != (-2147483647 - 1))
        return 127u;

    float floating_storage = 1.25f;
    std::atomic_ref<float> floating_reference{floating_storage};
    if (floating_reference.fetch_add(0.75f) != 1.25f ||
        floating_reference.fetch_sub(0.5f) != 2.0f ||
        floating_reference.load() != 1.5f ||
        (floating_reference += 0.5f) != 2.0f ||
        (floating_reference -= 0.25f) != 1.75f)
        return 128u;

    int values[5];
    int* pointer = values;
    std::atomic_ref<int*> pointer_reference{pointer};
    if (!pointer_reference.is_lock_free() ||
        pointer_reference.fetch_add(2) != values || pointer != values + 2)
        return 124u;
    int* pointer_expected = values + 2;
    if (!pointer_reference.compare_exchange_strong(
            pointer_expected, values + 4, std::memory_order_release,
            std::memory_order_relaxed) ||
        pointer_reference.fetch_sub(1) != values + 4 || pointer != values + 3 ||
        (pointer_reference += 1) != values + 4 ||
        pointer_reference++ != values + 4 || pointer != values + 5 ||
        --pointer_reference != values + 4 ||
        (pointer_reference -= 2) != values + 2 || pointer != values + 2)
        return 125u;

    alignas(8) AtomicRefAggregate aggregate_storage{2u, 3u};
    std::atomic_ref<AtomicRefAggregate> aggregate_reference{aggregate_storage};
    AtomicRefAggregate aggregate_value = aggregate_reference.load();
    if (aggregate_value.first != 2u || aggregate_value.second != 3u)
        return 135u;
    AtomicRefAggregate aggregate_stored{5u, 8u};
    aggregate_reference.store(aggregate_stored, std::memory_order_release);
    AtomicRefAggregate aggregate_exchanged{13u, 21u};
    aggregate_value = aggregate_reference.exchange(aggregate_exchanged);
    if (aggregate_value.first != 5u || aggregate_value.second != 8u)
        return 136u;
    AtomicRefAggregate aggregate_expected{13u, 21u};
    AtomicRefAggregate aggregate_desired{34u, 55u};
    if (!aggregate_reference.compare_exchange_strong(
            aggregate_expected, aggregate_desired, std::memory_order_acq_rel,
            std::memory_order_acquire))
        return 137u;
    aggregate_expected = AtomicRefAggregate{1u, 1u};
    AtomicRefAggregate aggregate_mismatch_desired{89u, 144u};
    if (aggregate_reference.compare_exchange_weak(
            aggregate_expected, aggregate_mismatch_desired,
            std::memory_order_relaxed) ||
        aggregate_expected.first != 34u || aggregate_expected.second != 55u)
        return 138u;

    alignas(4) AtomicRefWordAggregate word_storage{7u};
    std::atomic_ref<AtomicRefWordAggregate> word_reference{word_storage};
    AtomicRefWordAggregate word_exchanged{9u};
    AtomicRefWordAggregate word_value = word_reference.exchange(word_exchanged);
    if (word_value.value != 7u || word_storage.value != 9u)
        return 139u;
    AtomicRefWordAggregate word_expected{9u};
    AtomicRefWordAggregate word_desired{11u};
    if (!word_reference.compare_exchange_strong(
            word_expected, word_desired, std::memory_order_acq_rel,
            std::memory_order_acquire) || word_reference.load().value != 11u)
        return 140u;

    AtomicRefNonTrivial nontrivial_object{17u};
    AtomicRefNonTrivial* nontrivial_pointer = &nontrivial_object;
    std::atomic<AtomicRefNonTrivial*> atomic_pointer{nontrivial_pointer};
    std::atomic_ref<AtomicRefNonTrivial*> nontrivial_pointer_reference{
        nontrivial_pointer};
    if (atomic_pointer.load() != &nontrivial_object ||
        nontrivial_pointer_reference.load() != &nontrivial_object)
        return 141u;
    return 0u;
}

extern "C" unsigned cpp_standard_atomic_operations() {
    unsigned atomic_ref_status = test_atomic_ref();
    if (atomic_ref_status != 0u) return atomic_ref_status;

    std::atomic_bool alias_bool(true);
    std::atomic_char alias_char('A');
    std::atomic_schar alias_schar(-2);
    std::atomic_uchar alias_uchar(3u);
    std::atomic_short alias_short(-4);
    std::atomic_ushort alias_ushort(5u);
    std::atomic_int alias_int(-6);
    std::atomic_uint alias_uint(7u);
    std::atomic_long alias_long(-8L);
    std::atomic_ulong alias_ulong(9UL);
    std::atomic_llong alias_llong(-10LL);
    std::atomic_ullong alias_ullong(11ULL);
    std::atomic_char16_t alias_char16(u'B');
    std::atomic_char32_t alias_char32(U'C');
    std::atomic_wchar_t alias_wchar(L'D');
#if defined(__cpp_char8_t)
    std::atomic_char8_t alias_char8(u8'E');
#endif
    std::atomic<unsigned> zero;
    std::atomic<unsigned> compatibility_init = ATOMIC_VAR_INIT(1u);
    std::atomic<unsigned> value(3u);
    std::atomic<unsigned long long> wide(4ULL);
    int pointer_values[5];
    std::atomic<int*> pointer;
    std::atomic<int*> initialized_pointer(pointer_values + 1);
    std::atomic<int*>::difference_type offset = 2;
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    InheritedStaticTemplateDerived<unsigned>::value_type inherited_value = 3u;
    InheritedStaticTemplateDerived<unsigned> inherited(inherited_value);
    int* pointer_expected;
    std::atomic<float> floating;
    std::atomic<unsigned>::value_type atomic_value_type = 1u;
    float floating_expected = 9.0f;
    unsigned expected = 9u;
    unsigned api_expected = 3u;
    volatile CxxVolatileTemplateClass<unsigned> volatile_template_value =
        { 17u };
    std::atomic_init(&compatibility_init, 2u);
    if (std::atomic_load(&compatibility_init) != 2u) return 103u;
    std::atomic_thread_fence(std::memory_order_acq_rel);
    std::atomic_signal_fence(std::memory_order_acquire);
    if (!alias_bool.load() || alias_char.load() != 'A' ||
        alias_schar.load() != -2 || alias_uchar.load() != 3u ||
        alias_short.load() != -4 || alias_ushort.load() != 5u ||
        alias_int.load() != -6 || alias_uint.load() != 7u ||
        alias_long.load() != -8L || alias_ulong.load() != 9UL ||
        alias_llong.load() != -10LL || alias_ullong.load() != 11ULL ||
        alias_char16.load() != u'B' || alias_char32.load() != U'C' ||
        alias_wchar.load() != L'D')
        return 109u;
#if defined(__cpp_char8_t)
    if (alias_char8.load() != u8'E') return 110u;
#endif
    if (std::kill_dependency(7u) != 7u ||
        std::kill_dependency(pointer_values) != pointer_values)
        return 106u;
    if (cxx_volatile_class_template_deduction(
            &volatile_template_value) != 17u)
        return 108u;
    if (!zero.is_always_lock_free) return 80u;
    if (!pointer.is_always_lock_free) return 81u;
#ifndef RCC_STDLIB_REFERENCE
    if (!flag.is_always_lock_free) return 82u;
#endif
    if (!InheritedStaticTemplateDerived<unsigned>::is_always_lock_free ||
        !inherited.is_always_lock_free || inherited.value() != 3u ||
        InheritedStaticTemplateDerived<unsigned>::doubled(3u) != 6u ||
        InheritedStaticTemplateDerived<unsigned>::doubled(3u, 2u) != 6u ||
        inherited.doubled(3u) != 6u || inherited.doubled(3u, 2u) != 6u)
        return 83u;
    if (zero.load() != 0u || !zero.is_always_lock_free ||
        !zero.is_lock_free()) return 20u;
    if (!std::atomic_is_lock_free(&zero) ||
        std::atomic_load(&zero) != 0u)
        return 90u;
    std::atomic_store_explicit(&zero, 2u, std::memory_order_relaxed);
    if (std::atomic_load_explicit(&zero, std::memory_order_acquire) != 2u ||
        std::atomic_exchange(&zero, 3u) != 2u ||
        std::atomic_load(&zero) != 3u)
        return 91u;
    api_expected = 3u;
    if (!std::atomic_compare_exchange_weak_explicit(
            &zero, &api_expected, 4u, std::memory_order_acq_rel,
            std::memory_order_acquire) || std::atomic_load(&zero) != 4u)
        return 92u;
    api_expected = 2u;
    if (std::atomic_compare_exchange_strong(&zero, &api_expected, 5u) ||
        api_expected != 4u || std::atomic_load(&zero) != 4u)
        return 93u;
    if (std::atomic_fetch_add(&zero, 1u) != 4u ||
        std::atomic_load(&zero) != 5u ||
        std::atomic_fetch_add_explicit(
            &zero, 2u, std::memory_order_relaxed) != 5u ||
        std::atomic_load(&zero) != 7u ||
        std::atomic_fetch_sub(&zero, 1u) != 7u ||
        std::atomic_load(&zero) != 6u ||
        std::atomic_fetch_sub_explicit(
            &zero, 2u, std::memory_order_acq_rel) != 6u ||
        std::atomic_load(&zero) != 4u)
        return 115u;
    if (std::atomic_fetch_and(&zero, 6u) != 4u ||
        std::atomic_load(&zero) != 4u ||
        std::atomic_fetch_and_explicit(
            &zero, 3u, std::memory_order_relaxed) != 4u ||
        std::atomic_load(&zero) != 0u ||
        std::atomic_fetch_or(&zero, 2u) != 0u ||
        std::atomic_load(&zero) != 2u ||
        std::atomic_fetch_or_explicit(
            &zero, 4u, std::memory_order_release) != 2u ||
        std::atomic_load(&zero) != 6u ||
        std::atomic_fetch_xor(&zero, 3u) != 6u ||
        std::atomic_load(&zero) != 5u ||
        std::atomic_fetch_xor_explicit(
            &zero, 1u, std::memory_order_relaxed) != 5u ||
        std::atomic_load(&zero) != 4u)
        return 116u;
    if (atomic_value_type != 1u) return 79u;
    if (wide.fetch_add(3ULL) != 4ULL || wide.load() != 7ULL) return 21u;
    if (initialized_pointer.load() != pointer_values + 1) return 29u;
    pointer.store(pointer_values);
    std::atomic_init(&pointer, pointer_values + 1);
    if (std::atomic_load(&pointer) != pointer_values + 1) return 105u;
    pointer.store(pointer_values);
    if (!std::atomic_is_lock_free(&pointer) ||
        std::atomic_load(&pointer) != pointer_values)
        return 100u;
    if (std::atomic_exchange_explicit(
            &pointer, pointer_values + 1,
            std::memory_order_seq_cst) != pointer_values)
        return 101u;
    pointer_expected = pointer_values + 1;
    if (!std::atomic_compare_exchange_strong_explicit(
            &pointer, &pointer_expected, pointer_values + 2,
            std::memory_order_acq_rel, std::memory_order_acquire) ||
        std::atomic_load(&pointer) != pointer_values + 2)
        return 102u;
    if (std::atomic_fetch_add(&pointer, 2) != pointer_values + 2 ||
        std::atomic_load(&pointer) != pointer_values + 4 ||
        std::atomic_fetch_sub_explicit(
            &pointer, 1, std::memory_order_relaxed) != pointer_values + 4 ||
        std::atomic_load(&pointer) != pointer_values + 3)
        return 117u;
    pointer.store(pointer_values);
    if (flag.test(std::memory_order_relaxed)) return 30u;
    if (flag.test_and_set(std::memory_order_acquire)) return 31u;
    if (!flag.test()) return 32u;
    if (!flag.test_and_set()) return 33u;
    flag.clear(std::memory_order_release);
    if (flag.test()) return 34u;
    if (flag.test_and_set()) return 35u;
    flag.clear();
    if (std::atomic_flag_test_and_set_explicit(
            &flag, std::memory_order_acquire))
        return 94u;
    if (!std::atomic_flag_test_and_set(&flag)) return 95u;
    std::atomic_flag_clear_explicit(&flag, std::memory_order_release);
    if (std::atomic_flag_test_and_set(&flag)) return 96u;
    std::atomic_flag_clear(&flag);
    if (std::atomic_flag_test(&flag) ||
        std::atomic_flag_test_explicit(&flag, std::memory_order_relaxed))
        return 111u;
    if (std::atomic_flag_test_and_set(&flag) ||
        !std::atomic_flag_test_explicit(&flag, std::memory_order_acquire))
        return 112u;
    std::atomic_flag_clear(&flag);
    if (floating.load() != 0.0f) return 36u;
    floating.store(1.5f);
    if (floating.load(std::memory_order_relaxed) != 1.5f) return 48u;
    if (floating.fetch_add(2.0f, std::memory_order_acq_rel) != 1.5f ||
        floating.load() != 3.5f) return 37u;
    if (floating.fetch_sub(0.5f) != 3.5f || floating.load() != 3.0f)
        return 38u;
    if (floating.compare_exchange_strong(
            floating_expected, 6.0f, std::memory_order_acq_rel,
            std::memory_order_acquire) || floating_expected != 3.0f)
        return 39u;
    floating_expected = 3.0f;
    if (!floating.compare_exchange_weak(
            floating_expected, 4.0f, std::memory_order_release) ||
        floating.load() != 4.0f) return 40u;
    if (floating.exchange(5.5f, std::memory_order_relaxed) != 4.0f ||
        floating.load() != 5.5f) return 41u;
    if ((floating = 7.0f) != 7.0f || floating.load() != 7.0f) return 42u;
    {
        std::atomic<double> floating_wide;
        double floating_wide_expected = 3.0;
        if (!floating_wide.is_always_lock_free ||
            !floating_wide.is_lock_free()) return 77u;
        if (floating_wide.load() != 0.0) return 47u;
        floating_wide.store(2.25);
        if (floating_wide.fetch_add(1.5) != 2.25 ||
            floating_wide.load() != 3.75) return 43u;
        if (floating_wide.fetch_sub(0.75, std::memory_order_relaxed) !=
                3.75 || floating_wide.load() != 3.0) return 44u;
        if (floating_wide.exchange(4.0, std::memory_order_acq_rel) != 3.0 ||
            floating_wide.load() != 4.0) return 49u;
        floating_wide_expected = 4.0;
        if (!floating_wide.compare_exchange_strong(
                floating_wide_expected, 4.5, std::memory_order_acq_rel,
                std::memory_order_acquire) || floating_wide.load() != 4.5)
            return 45u;
        if ((floating_wide = 6.25) != 6.25 || floating_wide.load() != 6.25)
            return 46u;
    }
    {
        volatile std::atomic<unsigned> volatile_value(2u);
        volatile std::atomic<unsigned> volatile_compatibility_init(0u);
        unsigned volatile_expected = 100u;
        if (cxx_volatile_atomic_template_deduction(&volatile_value) != 2u)
            return 107u;
        std::atomic_init(&volatile_compatibility_init, 3u);
        if (std::atomic_load(&volatile_compatibility_init) != 3u)
            return 104u;
        if (!volatile_value.is_lock_free()) return 50u;
        if (!std::atomic_is_lock_free(&volatile_value) ||
            std::atomic_load(&volatile_value) != 2u)
            return 97u;
        std::atomic_store(&volatile_value, 4u);
        if (std::atomic_exchange_explicit(
                &volatile_value, 5u, std::memory_order_acq_rel) != 4u)
            return 98u;
        volatile_expected = 5u;
        if (!std::atomic_compare_exchange_strong(
                &volatile_value, &volatile_expected, 6u) ||
            std::atomic_load_explicit(
                &volatile_value, std::memory_order_relaxed) != 6u)
            return 99u;
        std::atomic_store(&volatile_value, 2u);
        if (volatile_value.load() != 2u) return 75u;
        unsigned volatile_converted = volatile_value;
        if (volatile_converted != 2u) return volatile_converted;
        volatile_value.store(5u, std::memory_order_release);
        if (volatile_value.exchange(7u, std::memory_order_acq_rel) != 5u)
            return 51u;
        if (volatile_value.compare_exchange_strong(
                volatile_expected, 9u, std::memory_order_acq_rel,
                std::memory_order_acquire) || volatile_expected != 7u)
            return 52u;
        volatile_expected = 7u;
        if (!volatile_value.compare_exchange_weak(
                volatile_expected, 8u, std::memory_order_release) ||
            volatile_value.load() != 8u) return 53u;
        if (volatile_value.fetch_add(2u) != 8u ||
            volatile_value.fetch_sub(1u, std::memory_order_relaxed) != 10u)
            return 54u;
        if (volatile_value.fetch_and(7u) != 9u ||
            volatile_value.fetch_or(8u, std::memory_order_relaxed) != 1u ||
            volatile_value.fetch_xor(3u) != 9u) return 55u;
        if ((volatile_value &= 6u) != 2u ||
            (volatile_value |= 1u) != 3u ||
            (volatile_value ^= 2u) != 1u)
            return 89u;
        volatile_value.store(10u);
        if (++volatile_value != 11u || volatile_value++ != 11u ||
            --volatile_value != 11u || volatile_value-- != 11u)
            return 56u;
        if ((volatile_value += 4u) != 14u ||
            (volatile_value -= 3u) != 11u ||
            (volatile_value = 12u) != 12u)
            return 57u;
        volatile_value.store(12u);
        volatile_expected = 12u;
        if (!volatile_value.compare_exchange_strong(
                volatile_expected, 13u)) return 67u;
        volatile_expected = 13u;
        if (!volatile_value.compare_exchange_weak(
                volatile_expected, 14u)) return 72u;
        volatile_expected = 14u;
        if (!volatile_value.compare_exchange_strong(
                volatile_expected, 15u, std::memory_order_seq_cst) ||
            volatile_value.load() != 15u) return 73u;
    }
    {
        int volatile_pointer_values[5];
        volatile std::atomic<int*> volatile_pointer(
            volatile_pointer_values);
        int* volatile_pointer_expected = volatile_pointer_values + 2;
        if (volatile_pointer.load() != volatile_pointer_values ||
            volatile_pointer.fetch_add(2) != volatile_pointer_values ||
            volatile_pointer.fetch_sub(1, std::memory_order_relaxed) !=
                volatile_pointer_values + 2)
            return 58u;
        if (volatile_pointer.compare_exchange_strong(
                volatile_pointer_expected, volatile_pointer_values + 3) ||
            volatile_pointer_expected != volatile_pointer_values + 1)
            return 59u;
        volatile_pointer_expected = volatile_pointer_values + 1;
        if (!volatile_pointer.compare_exchange_weak(
                volatile_pointer_expected, volatile_pointer_values + 2,
                std::memory_order_acq_rel) ||
            volatile_pointer++ != volatile_pointer_values + 2 ||
            volatile_pointer.load() != volatile_pointer_values + 3)
            return 60u;
        if ((volatile_pointer -= 2) != volatile_pointer_values + 1 ||
            (volatile_pointer = volatile_pointer_values + 4) !=
                volatile_pointer_values + 4 ||
            volatile_pointer.exchange(volatile_pointer_values) !=
                volatile_pointer_values + 4)
            return 61u;
        volatile_pointer.store(volatile_pointer_values + 1);
        volatile_pointer_expected = volatile_pointer_values + 1;
        if (!volatile_pointer.compare_exchange_strong(
                volatile_pointer_expected, volatile_pointer_values + 2,
                std::memory_order_seq_cst))
            return 68u;
        volatile_pointer_expected = volatile_pointer_values + 2;
        if (!volatile_pointer.compare_exchange_weak(
                volatile_pointer_expected, volatile_pointer_values + 3) ||
            volatile_pointer.load() != volatile_pointer_values + 3)
            return 74u;
    }
    {
        volatile std::atomic<float> volatile_floating(1.0f);
        float volatile_float_expected = 2.0f;
        if (volatile_floating.fetch_add(1.5f) != 1.0f ||
            volatile_floating.fetch_sub(0.5f,
                std::memory_order_relaxed) != 2.5f)
            return 62u;
        if (!volatile_floating.compare_exchange_strong(
                volatile_float_expected, 4.0f,
                std::memory_order_acq_rel,
                std::memory_order_acquire) ||
            volatile_floating.exchange(5.0f) != 4.0f ||
            (volatile_floating = 6.0f) != 6.0f ||
            volatile_floating.load() != 6.0f)
            return 63u;
        volatile_floating.store(7.0f, std::memory_order_release);
        if (volatile_floating.load(std::memory_order_acquire) != 7.0f)
            return 69u;
    }
    {
        volatile std::atomic<double> volatile_floating_wide(2.0);
        double volatile_double_expected = 2.0;
        if (!volatile_floating_wide.is_always_lock_free ||
            !volatile_floating_wide.is_lock_free()) return 78u;
        if (!volatile_floating_wide.compare_exchange_weak(
                volatile_double_expected, 3.0,
                std::memory_order_acq_rel,
                std::memory_order_acquire) ||
            volatile_floating_wide.fetch_add(0.5) != 3.0 ||
            volatile_floating_wide.fetch_sub(0.25) != 3.5 ||
            (volatile_floating_wide = 4.0) != 4.0 ||
            volatile_floating_wide.load() != 4.0)
            return 64u;
        if (volatile_floating_wide.exchange(4.5) != 4.0 ||
            volatile_floating_wide.load() != 4.5) return 76u;
        volatile_floating_wide.store(5.0);
        if (volatile_floating_wide.load() != 5.0) return 70u;
    }
    {
        volatile std::atomic_flag volatile_flag;
        if (volatile_flag.test() || volatile_flag.test_and_set() ||
            !volatile_flag.test(std::memory_order_relaxed))
            return 65u;
        volatile_flag.clear(std::memory_order_release);
        if (volatile_flag.test_and_set(std::memory_order_acquire))
            return 66u;
        volatile_flag.clear();
    }
    {
        const volatile std::atomic<unsigned> const_volatile_value(17u);
        const volatile std::atomic_flag const_volatile_flag;
        if (!const_volatile_value.is_lock_free() ||
            const_volatile_value.load(std::memory_order_acquire) != 17u ||
            !std::atomic_is_lock_free(&const_volatile_value) ||
            std::atomic_load_explicit(
                &const_volatile_value, std::memory_order_acquire) != 17u ||
            const_volatile_flag.test())
            return 71u;
    }
    if (value.load(std::memory_order_relaxed) != 3u) return 1u;
    value.store(7u, std::memory_order_release);
    if (value.exchange(9u, std::memory_order_acq_rel) != 7u) return 2u;
    if (!value.compare_exchange_strong(
            expected, 13u, std::memory_order_acq_rel,
            std::memory_order_acquire)) return 3u;
    if (value.fetch_add(2u, std::memory_order_relaxed) != 13u) return 4u;
    if (value.fetch_and(7u) != 15u) return 5u;
    if (value.fetch_or(8u) != 7u) return 6u;
    if (value.fetch_xor(3u) != 15u) return 7u;
    if ((value &= 10u) != 8u || value.load() != 8u ||
        (value |= 5u) != 13u || value.load() != 13u ||
        (value ^= 3u) != 14u || value.load() != 14u)
        return 87u;
    value.store(12u);
    {
        std::atomic<unsigned char> byte_bits(0xf0u);
        unsigned wide_bitwise_operand = 0x101u;
        if (byte_bits.fetch_and(0x0fu) != 0xf0u || byte_bits.load() != 0u)
            return 84u;
        byte_bits.store(0x10u);
        if (byte_bits.fetch_or(wide_bitwise_operand) != 0x10u ||
            byte_bits.load() != 0x11u)
            return 85u;
        if (byte_bits.fetch_xor(wide_bitwise_operand) != 0x11u ||
            byte_bits.load() != 0x10u)
            return 86u;
        if ((byte_bits &= 0x11u) != 0x10u ||
            byte_bits.load() != 0x10u ||
            (byte_bits |= 0x104u) != 0x14u ||
            byte_bits.load() != 0x14u ||
            (byte_bits ^= 0x105u) != 0x11u ||
            byte_bits.load() != 0x11u)
            return 88u;
    }
    if (value++ != 12u || value.load() != 13u) return 8u;
    if (--value != 12u || value.load() != 12u) return 9u;
    expected = 0u;
    if (value.compare_exchange_strong(expected, 99u) || expected != 12u)
        return 10u;
    expected = 12u;
    if (!value.compare_exchange_weak(
            expected, 14u, std::memory_order_release)) return 12u;
    value.store(12u);
    if ((value = 5u) != 5u || value.load() != 5u) return 11u;
    value.store(12u);
    if (pointer.fetch_add(offset) != pointer_values ||
        pointer.load() != pointer_values + 2) return 22u;
    if (pointer++ != pointer_values + 2 ||
        pointer.load() != pointer_values + 3) return 23u;
    if (pointer.fetch_sub(2, std::memory_order_relaxed) !=
            pointer_values + 3 || pointer.load() != pointer_values + 1)
        return 24u;
    if ((pointer += 2) != pointer_values + 3 ||
        pointer.load() != pointer_values + 3) return 25u;
    if (--pointer != pointer_values + 2 ||
        pointer.load() != pointer_values + 2) return 26u;
    pointer_expected = pointer_values + 2;
    if (!pointer.compare_exchange_strong(
            pointer_expected, pointer_values + 4) ||
        pointer.load() != pointer_values + 4) return 27u;
    pointer_expected = pointer_values;
    if (pointer.compare_exchange_strong(
            pointer_expected, pointer_values) ||
        pointer_expected != pointer_values + 4) return 28u;
    return value.load();
}
