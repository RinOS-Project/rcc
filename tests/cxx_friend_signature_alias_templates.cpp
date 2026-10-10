struct FriendSignatureAliasBase {
    int read() const { return value; }

private:
    int value;

    template<typename T>
    using Hidden = T*;

    friend Hidden<int> friend_signature_return(
        FriendSignatureAliasBase& owner);
    friend int friend_signature_parameter(
        Hidden<int> value, FriendSignatureAliasBase& owner);

    template<typename T>
    friend Hidden<T> friend_signature_template_return(T value);
    template<typename T>
    friend int friend_signature_template_parameter(Hidden<T> value);
};

struct FriendSignatureAliasDerived : public FriendSignatureAliasBase {};

FriendSignatureAliasDerived::Hidden<int> friend_signature_return(
    FriendSignatureAliasBase& owner) {
    owner.value = 1;
    return nullptr;
}

int friend_signature_parameter(
    FriendSignatureAliasDerived::Hidden<int> value,
    FriendSignatureAliasBase& owner) {
    owner.value = 2;
    return value == nullptr ? owner.value : 0;
}

template<typename T>
FriendSignatureAliasDerived::Hidden<T>
friend_signature_template_return(T value) {
    (void)value;
    return nullptr;
}

template<typename T>
int friend_signature_template_parameter(
    FriendSignatureAliasDerived::Hidden<T> value) {
    return value != nullptr ? 1 : 0;
}

int main() {
    int value = 0;
    FriendSignatureAliasDerived owner;
    if (friend_signature_return(owner) != nullptr) return 1;
    if (owner.read() != 1) return 2;
    if (friend_signature_parameter(nullptr, owner) != 2) return 3;
    if (owner.read() != 2) return 4;
    if (friend_signature_template_return(7) != nullptr) return 5;
    if (friend_signature_template_parameter<int>(&value) != 1) return 6;
    return 0;
}
