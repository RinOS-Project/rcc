template<typename T>
struct ClassTemplateAliasAccessOwner {
private:
    template<typename U> using PrivateAlias = U;

protected:
    template<typename U> using ProtectedAlias = U;
};

struct ClassTemplateAliasPrivateBase {
    template<typename U> using InheritedAlias = U;
};

struct ClassTemplateAliasPrivateDerived : private ClassTemplateAliasPrivateBase {};

struct ClassTemplateAliasAmbiguousLeft {
    template<typename U> using AmbiguousAlias = U;
};

struct ClassTemplateAliasAmbiguousRight {
    template<typename U> using AmbiguousAlias = U;
};

struct ClassTemplateAliasAmbiguous
    : ClassTemplateAliasAmbiguousLeft, ClassTemplateAliasAmbiguousRight {};

struct FriendAliasPrivateBase {
private:
    template<typename U> using Hidden = U*;
};

struct FriendAliasPrivateDerived : public FriendAliasPrivateBase {
    friend struct FriendAliasPrivateDerivedAccess;
};

struct FriendAliasPrivateDerivedAccess {
    using Hidden = FriendAliasPrivateDerived::Hidden<int>;
};

struct FriendFunctionAliasOverloadOwner {
private:
    template<typename U> using Hidden = U*;

    friend int friend_function_alias_overload(int);
};

int friend_function_alias_overload(double) {
    FriendFunctionAliasOverloadOwner::Hidden<int>* pointer = nullptr;
    return pointer == nullptr ? 0 : 1;
}

ClassTemplateAliasAccessOwner<int>::template PrivateAlias<long>
    private_alias;
ClassTemplateAliasAccessOwner<int>::template ProtectedAlias<long>
    protected_alias;
ClassTemplateAliasPrivateDerived::InheritedAlias<long> private_base_alias;
ClassTemplateAliasAmbiguous::AmbiguousAlias<long> ambiguous_alias;
