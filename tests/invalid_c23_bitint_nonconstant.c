int invalid_bitint_width(int width)
{
    typedef unsigned _BitInt(width) nonconstant_bitint;
    return (int)sizeof(nonconstant_bitint);
}
