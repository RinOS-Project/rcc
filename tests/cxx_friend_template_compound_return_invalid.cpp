#ifndef __RCC__
#include <concepts>
#endif

template<class T>
requires requires(T candidate) {
    { candidate.marker + 0 } noexcept -> std::same_as<int>;
}
int compound_return_mismatch_friend(T& host);

class CompoundReturnMismatchHost {
public:
    int marker;

private:
    int value;

    template<class U>
    requires requires(U probe) {
        { probe.marker + 0 } noexcept -> std::same_as<long>;
    }
    friend int compound_return_mismatch_friend(U& host);
};

template<class V>
requires requires(V item) {
    { item.marker + 0 } noexcept -> std::same_as<int>;
}
int compound_return_mismatch_friend(V& host) {
    return host.value;
}

int main() {
    CompoundReturnMismatchHost host;
    return compound_return_mismatch_friend(host);
}
