int invalid_vla_goto(int count)
{
    goto entered;
    {
        int values[count];
entered:
        return values[0];
    }
}
