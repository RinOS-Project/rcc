int verified_typeinfo_equal()
{
    return typeid(int) == typeid(int) &&
           typeid(int) != typeid(long) &&
           typeid(1) == typeid(int);
}

int verified_typeinfo_hash()
{
    return typeid(int).hash_code() == typeid(int).hash_code() &&
           typeid(int).hash_code() != typeid(long).hash_code();
}

int verified_typeinfo_name()
{
    const char* name = typeid(int).name();
    return name != 0 && name[0] != 0;
}

int verified_typeinfo_before()
{
    return typeid(int).before(typeid(int)) == false &&
           (typeid(int).before(typeid(long)) !=
            typeid(long).before(typeid(int)));
}

int verified_typeinfo_all()
{
    return verified_typeinfo_equal() &&
           verified_typeinfo_hash() &&
           verified_typeinfo_name() &&
           verified_typeinfo_before();
}
