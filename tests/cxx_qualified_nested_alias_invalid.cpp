struct QualifiedPrivateAlias {
private:
    using value_type = int;
};

QualifiedPrivateAlias::value_type inaccessible_alias;

struct LeftQualifiedAlias {
    using value_type = int;
};

struct RightQualifiedAlias {
    using value_type = long;
};

struct AmbiguousQualifiedAlias : LeftQualifiedAlias, RightQualifiedAlias {};

AmbiguousQualifiedAlias::value_type ambiguous_alias;
