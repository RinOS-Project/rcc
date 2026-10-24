struct InheritedMultipleBaseTemplate {
    template <typename T>
    explicit InheritedMultipleBaseTemplate(T value) { (void)value; }
};

struct InheritedAdditionalBase {
    InheritedAdditionalBase() {}
};

struct InheritedMultipleBaseDerived
    : InheritedMultipleBaseTemplate, InheritedAdditionalBase {
    using InheritedMultipleBaseTemplate::InheritedMultipleBaseTemplate;
};

InheritedMultipleBaseDerived inherited_multiple_base_value{1};
