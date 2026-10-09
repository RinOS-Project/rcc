template<typename T>
struct ClassTemplateAliasAccessOwner {
private:
    template<typename U> using PrivateAlias = U;

protected:
    template<typename U> using ProtectedAlias = U;
};

ClassTemplateAliasAccessOwner<int>::template PrivateAlias<long>
    private_alias;
ClassTemplateAliasAccessOwner<int>::template ProtectedAlias<long>
    protected_alias;
