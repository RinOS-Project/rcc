struct PrivateClassAliasTemplate {
private:
    template<typename T> using Hidden = T;
};

PrivateClassAliasTemplate::template Hidden<int> private_alias;

struct ProtectedClassAliasTemplate {
protected:
    template<typename T> using Hidden = T;
};

ProtectedClassAliasTemplate::template Hidden<int> protected_alias;

struct ProtectedBaseThroughPrivateInheritance {
protected:
    template<typename T> using Hidden = T;
};

struct PrivateIntermediate : private ProtectedBaseThroughPrivateInheritance {
    using Inside =
        ProtectedBaseThroughPrivateInheritance::template Hidden<int>;
};

struct FurtherDerived : PrivateIntermediate {
    using Outside =
        ProtectedBaseThroughPrivateInheritance::template Hidden<int>;
};
