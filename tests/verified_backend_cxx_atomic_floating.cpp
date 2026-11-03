#include <atomic>

int main()
{
    std::atomic<long double> value;
    if (value.load(std::memory_order_relaxed) != 0.0L) return 1;
    value.store(1.25L, std::memory_order_release);
    const long double previous_add =
        value.fetch_add(2.5L, std::memory_order_acq_rel);
    const long double after_add = value.load(std::memory_order_acquire);
    const long double previous_sub =
        value.fetch_sub(0.75L, std::memory_order_release);

    alignas(8) long double storage = 4.0L;
    std::atomic_ref<long double> reference(storage);
    const long double reference_previous_add =
        reference.fetch_add(1.5L, std::memory_order_acq_rel);
    const long double reference_previous_sub =
        reference.fetch_sub(0.5L, std::memory_order_release);

    return previous_add == 1.25L && after_add == 3.75L &&
            previous_sub == 3.75L && value.load() == 3.0L &&
            reference_previous_add == 4.0L &&
            reference_previous_sub == 5.5L && storage == 5.0L
        ? 0 : 1;
}
