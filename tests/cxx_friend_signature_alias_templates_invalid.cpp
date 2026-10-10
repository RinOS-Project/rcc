struct FriendSignatureAliasMismatchBase {
private:
    template<typename T>
    using Hidden = T*;

    friend Hidden<int> friend_signature_mismatch(int value);
};

struct FriendSignatureAliasMismatchDerived
    : public FriendSignatureAliasMismatchBase {};

FriendSignatureAliasMismatchDerived::Hidden<int>
friend_signature_mismatch(double value) {
    (void)value;
    return nullptr;
}

struct FriendSignatureTemplateMismatchBase {
private:
    template<typename T>
    using Hidden = T*;

    template<typename T>
    friend Hidden<T> friend_signature_template_mismatch(T value);
};

struct FriendSignatureTemplateMismatchDerived
    : public FriendSignatureTemplateMismatchBase {};

template<typename T>
FriendSignatureTemplateMismatchDerived::Hidden<T>
friend_signature_template_mismatch(T* value) {
    (void)value;
    return nullptr;
}
