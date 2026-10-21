int invalid_vla_pointer_goto(int count)
{
    goto entered;
    {
        int (*cursor)[count];
entered:
        return cursor != 0;
    }
}
