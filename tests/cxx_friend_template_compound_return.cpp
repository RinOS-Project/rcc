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
int reveal_compound_return_friend(T& host);

class CompoundReturnFriendHost {
public:
    int marker;

private:
    int value = 0;

    template<class U>
    requires requires(U probe) {
        { probe.marker + 0 } noexcept -> CompoundReturnSameAs<int>;
    }
    friend int reveal_compound_return_friend(U& host);
};

template<class V>
requires requires(V item) {
    { item.marker + 0 } noexcept -> CompoundReturnSameAs<int>;
}
int reveal_compound_return_friend(V& host) {
    return host.value;
}

int main() {
    CompoundReturnFriendHost host;
    return reveal_compound_return_friend(host) == 0 ? 0 : 1;
}
