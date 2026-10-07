int vla_for_initializer_scope(int count)
{
    for (int values[count]; count > 0; --count) {
        values[0] = count;
        break;
    }
    return values[0];
}
