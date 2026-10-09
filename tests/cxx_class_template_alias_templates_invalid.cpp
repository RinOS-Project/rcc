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

ClassTemplateAliasAccessOwner<int>::template PrivateAlias<long>
    private_alias;
ClassTemplateAliasAccessOwner<int>::template ProtectedAlias<long>
    protected_alias;
ClassTemplateAliasPrivateDerived::InheritedAlias<long> private_base_alias;
ClassTemplateAliasAmbiguous::AmbiguousAlias<long> ambiguous_alias;
