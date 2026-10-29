template<class Left, class Right>
struct CompoundReturnSameType {
    static constexpr bool value = false;
};

template<class Type>
struct CompoundReturnSameType<Type, Type> {
    static constexpr bool value = true;
};

template<class Left, class Right>
concept CompoundReturnSameAs = CompoundReturnSameType<Left, Right>::value;

template<class T>
requires requires(T candidate) {
    { candidate.marker + 0 } noexcept -> CompoundReturnSameAs<int>;
}
int compound_return_mismatch_friend(T& host);

class CompoundReturnMismatchHost {
public:
    int marker;

private:
    int value;

    template<class U>
    requires requires(U probe) {
        { probe.marker + 0 } noexcept -> CompoundReturnSameAs<long>;
    }
    friend int compound_return_mismatch_friend(U& host);
};

template<class V>
requires requires(V item) {
    { item.marker + 0 } noexcept -> CompoundReturnSameAs<int>;
}
int compound_return_mismatch_friend(V& host) {
    return host.value;
}

int main() {
    CompoundReturnMismatchHost host;
    return compound_return_mismatch_friend(host);
}
