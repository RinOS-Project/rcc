int reject_vla_generic_association(int length)
{
    return _Generic(0, int[length]: 1, default: 0);
}

int reject_pointer_to_vla_generic_association(int length)
{
    return _Generic(0, int (*)[length]: 2, default: 0);
}
