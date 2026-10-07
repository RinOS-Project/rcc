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

struct VerifiedVirtualLargeResult {
    int first;
    int second;
    int third;
    int fourth;
    int fifth;
};

class VerifiedVirtualSretBase {
public:
    virtual VerifiedVirtualLargeResult result(int seed) const
    {
        VerifiedVirtualLargeResult value;
        value.first = seed;
        value.second = seed + 1;
        value.third = seed + 2;
        value.fourth = seed + 3;
        value.fifth = seed + 4;
        return value;
    }
};

class VerifiedVirtualSretDerived : public VerifiedVirtualSretBase {
public:
    VerifiedVirtualLargeResult result(int seed) const override
    {
        VerifiedVirtualLargeResult value;
        value.first = seed + 100;
        value.second = seed + 101;
        value.third = seed + 102;
        value.fourth = seed + 103;
        value.fifth = seed + 104;
        return value;
    }
};

VerifiedVirtualBase verified_virtual_base_instance;
VerifiedVirtualDerived verified_virtual_derived_instance;
VerifiedVirtualMultiple verified_virtual_multiple_instance;
VerifiedVirtualSretBase verified_virtual_sret_base_instance;
VerifiedVirtualSretDerived verified_virtual_sret_derived_instance;

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

extern "C" VerifiedVirtualLargeResult verified_virtual_sret_dispatch(
    VerifiedVirtualSretBase* object, int seed)
{
    return object->result(seed);
}

extern "C" int verified_virtual_sret_call_base()
{
    VerifiedVirtualLargeResult value = verified_virtual_sret_dispatch(
        &verified_virtual_sret_base_instance, 10);
    return value.first + value.fifth;
}

extern "C" int verified_virtual_sret_call_derived()
{
    VerifiedVirtualLargeResult value = verified_virtual_sret_dispatch(
        &verified_virtual_sret_derived_instance, 10);
    return value.first + value.fifth;
}
