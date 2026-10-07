struct VerifiedMemberMethodObject {
    int value;
    unsigned long long wide_value;

    int read() const
    {
        return value;
    }

    int add(int amount) const
    {
        return this->value + amount;
    }

    bool equals_seven() const
    {
        return value == 7;
    }

    bool differs_from_seven() const
    {
        return value != 7;
    }

    const int& reference() const
    {
        return value;
    }

    unsigned long long wide() const
    {
        return wide_value;
    }

    bool wide_equals_expected() const
    {
        return wide_value == 0x1122334455667788ULL;
    }
};

struct VerifiedMemberReleaseObject {
    unsigned int value;

    unsigned int release()
    {
        unsigned int previous = value;
        value = 0;
        return previous;
    }
};

extern "C" int verified_member_method_read(
    VerifiedMemberMethodObject* object)
{
    return object->read();
}

extern "C" int verified_member_method_add(
    VerifiedMemberMethodObject* object, int amount)
{
    return object->add(amount);
}

extern "C" int verified_member_method_equals_seven(
    VerifiedMemberMethodObject* object)
{
    return object->equals_seven();
}

extern "C" int verified_member_method_differs_from_seven(
    VerifiedMemberMethodObject* object)
{
    return object->differs_from_seven();
}

extern "C" const int* verified_member_method_reference(
    VerifiedMemberMethodObject* object)
{
    return &object->reference();
}

extern "C" unsigned long long verified_member_method_wide(
    VerifiedMemberMethodObject* object)
{
    return object->wide();
}

extern "C" int verified_member_method_wide_equals_expected(
    VerifiedMemberMethodObject* object)
{
    return object->wide_equals_expected();
}

extern "C" unsigned int verified_member_method_release(
    VerifiedMemberReleaseObject* object)
{
    return object->release();
}
