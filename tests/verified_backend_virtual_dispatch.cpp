class VerifiedVirtualBase {
public:
    virtual int value() const
    {
        return 17;
    }
};

class VerifiedVirtualDerived : public VerifiedVirtualBase {
public:
    int value() const override
    {
        return 29;
    }
};

class VerifiedVirtualPrefix {
public:
    int padding;
};

class VerifiedVirtualSecondaryBase {
public:
    virtual int secondary_value() const
    {
        return 31;
    }
};

class VerifiedVirtualMultiple : public VerifiedVirtualPrefix,
                               public VerifiedVirtualSecondaryBase {
public:
    int secondary_value() const override
    {
        return 47;
    }
};

VerifiedVirtualBase verified_virtual_base_instance;
VerifiedVirtualDerived verified_virtual_derived_instance;
VerifiedVirtualMultiple verified_virtual_multiple_instance;

extern "C" int verified_virtual_dispatch(VerifiedVirtualBase* object)
{
    return object->value();
}

extern "C" int verified_virtual_call_base()
{
    return verified_virtual_dispatch(&verified_virtual_base_instance);
}

extern "C" int verified_virtual_call_derived()
{
    return verified_virtual_dispatch(&verified_virtual_derived_instance);
}

extern "C" int verified_virtual_secondary_dispatch(
    VerifiedVirtualSecondaryBase* object)
{
    return object->secondary_value();
}

extern "C" int verified_virtual_call_secondary()
{
    return verified_virtual_secondary_dispatch(
        &verified_virtual_multiple_instance);
}
