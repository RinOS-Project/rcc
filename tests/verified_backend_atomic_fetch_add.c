int main(void)
{
    _Atomic unsigned char value8 = 250u;
    _Atomic unsigned short value16 = 65530u;
    _Atomic signed char signed8 = -120;
    _Atomic signed short signed16 = -30000;
    unsigned char old8 = value8++;
    unsigned short old16 = value16++;
    unsigned char added8 = (value8 += 10u);
    unsigned short added16 = (value16 += 10u);
    signed char old_signed8 = signed8++;
    signed short old_signed16 = signed16++;
    signed char added_signed8 = (signed8 += 7);
    signed short added_signed16 = (signed16 += 20);

    return old8 == 250u && value8 == 5u && added8 == 5u &&
        old16 == 65530u && value16 == 5u && added16 == 5u &&
        old_signed8 == -120 && signed8 == -112 && added_signed8 == -112 &&
        old_signed16 == -30000 && signed16 == -29979 &&
        added_signed16 == -29979 ? 0 : 1;
}
