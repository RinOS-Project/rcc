#include <atomic>

int atomic_float_rmw_cxx17_invalid(std::atomic<float>& value) {
    return value.fetch_add(1.0f) != 0.0f;
}
