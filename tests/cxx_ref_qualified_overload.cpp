struct RefQualifiedProbe {
    int select() & {
        return 11;
    }

    int select() const & {
        return 22;
    }

    int select() && {
        return 33;
    }

    int select() const && {
        return 44;
    }

    int const_ref_only() const & {
        return 55;
    }
};

struct RefQualifiedVirtualBase {
    virtual int select_virtual() & {
        return 51;
    }

    virtual int select_virtual() && {
        return 52;
    }
};

struct RefQualifiedVirtualDerived : RefQualifiedVirtualBase {
    int select_virtual() & override {
        return 61;
    }

    int select_virtual() && override {
        return 62;
    }
};

template <typename T>
struct RefQualifiedTemplateProbe {
    int select_template() & {
        return 71;
    }

    int select_template() && {
        return 72;
    }
};

int main() {
    RefQualifiedProbe mutable_value;
    const RefQualifiedProbe const_value{};
    RefQualifiedVirtualDerived derived;
    RefQualifiedVirtualBase& base_lvalue = derived;
    RefQualifiedTemplateProbe<int> templated;
    return mutable_value.select() == 11 &&
                   const_value.select() == 22 &&
                   static_cast<RefQualifiedProbe&&>(mutable_value).select() == 33 &&
                   RefQualifiedProbe{}.select() == 33 &&
                   static_cast<const RefQualifiedProbe&&>(const_value).select() == 44 &&
                   RefQualifiedProbe{}.const_ref_only() == 55 &&
                   base_lvalue.select_virtual() == 61 &&
                   static_cast<RefQualifiedVirtualBase&&>(derived)
                           .select_virtual() == 62 &&
                   templated.select_template() == 71 &&
                   static_cast<RefQualifiedTemplateProbe<int>&&>(templated)
                           .select_template() == 72
               ? 0
               : 1;
}
