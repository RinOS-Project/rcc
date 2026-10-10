struct FriendSignatureConstrainedAliasBase {
private:
    template<typename T>
    using Hidden = T*;

    template<typename T>
    requires (sizeof(T) > 1)
    friend Hidden<T> friend_signature_constrained_alias(T* value)
        noexcept(sizeof(T) > 1);
};

struct FriendSignatureConstrainedAliasDerived
    : public FriendSignatureConstrainedAliasBase {};

template<typename U>
requires (sizeof(U) > 1)
FriendSignatureConstrainedAliasDerived::Hidden<U>
friend_signature_constrained_alias(U* value) noexcept(sizeof(U) > 1) {
    return value;
}

int main() {
    int value = 7;
    return friend_signature_constrained_alias(&value) == &value ? 0 : 1;
}
