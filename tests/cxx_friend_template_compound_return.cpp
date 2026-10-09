#ifndef __RCC__
#include <concepts>
#endif

template<class T>
requires requires(T candidate) {
    { candidate.marker + 0 } noexcept -> std::same_as<int>;
}
int reveal_compound_return_friend(T& host);

class CompoundReturnFriendHost {
public:
    int marker;

private:
    int value = 0;

    template<class U>
    requires requires(U probe) {
        { probe.marker + 0 } noexcept -> std::same_as<int>;
    }
    friend int reveal_compound_return_friend(U& host);
};

template<class V>
requires requires(V item) {
    { item.marker + 0 } noexcept -> std::same_as<int>;
}
int reveal_compound_return_friend(V& host) {
    return host.value;
}

int main() {
    CompoundReturnFriendHost host;
    return reveal_compound_return_friend(host) == 0 ? 0 : 1;
}
