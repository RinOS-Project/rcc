struct InheritedDeletedExactBase {
    InheritedDeletedExactBase& operator=(int) = delete;
};

struct InheritedConvertedBase {
    InheritedConvertedBase& operator=(long) { return *this; }
};

struct InheritedDeletedExactSet : InheritedDeletedExactBase,
                                  InheritedConvertedBase {
    using InheritedDeletedExactBase::operator=;
    using InheritedConvertedBase::operator=;
};

struct InheritedDeletedExact : InheritedDeletedExactSet {
    using InheritedDeletedExactSet::operator=;
};

void assign_inherited_deleted_exact(InheritedDeletedExact& value) {
    value = 7;
}
