#include <atomic>

int main()
{
    std::atomic<float> float_value(1.0f);
    const float float_previous_add =
        float_value.fetch_add(0.5f, std::memory_order_acq_rel);
    const float float_after_fetch_add =
        float_value.load(std::memory_order_acquire);
    const float float_after_compound_add = (float_value += 0.25f);
    const float float_after_compound_sub = (float_value -= 0.5f);
    const float float_previous_sub =
        float_value.fetch_sub(0.25f, std::memory_order_release);

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

    alignas(8) double double_storage = 8.0;
    std::atomic_ref<double> double_reference(double_storage);
    const double double_previous_add =
        double_reference.fetch_add(1.25, std::memory_order_acq_rel);
    const double double_after_compound_add = (double_reference += 0.5);
    const double double_after_compound_sub = (double_reference -= 0.25);
    const double double_previous_sub =
        double_reference.fetch_sub(1.0, std::memory_order_release);

    return float_previous_add == 1.0f && float_after_fetch_add == 1.5f &&
            float_after_compound_add == 1.75f &&
            float_after_compound_sub == 1.25f &&
            float_previous_sub == 1.25f && float_value.load() == 1.0f &&
            previous_add == 1.25L && after_add == 3.75L &&
            previous_sub == 3.75L && value.load() == 3.0L &&
            reference_previous_add == 4.0L &&
            reference_previous_sub == 5.5L && storage == 5.0L &&
            double_previous_add == 8.0 &&
            double_after_compound_add == 9.75 &&
            double_after_compound_sub == 9.5 &&
            double_previous_sub == 9.5 && double_storage == 8.5
        ? 0 : 1;
}
