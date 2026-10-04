class RefBase {
public:
    int value;
};

class RefPrefix {
public:
    int prefix;
};

class RefDerived : public RefPrefix, public RefBase {
public:
    int extra;
};

int take_ref_base(RefBase& value);
int take_ref_derived(RefDerived& value);
int take_const_ref_base(const RefBase& value);

int call_derived_reference_overloads(RefDerived& value) {
    return take_ref_base(value) + take_ref_derived(value) +
           take_const_ref_base(value);
}

int call_const_derived_reference(const RefDerived& value) {
    return take_const_ref_base(value);
}
