int main(void)
{
    _Atomic unsigned char value8 = 250u;
    _Atomic unsigned short value16 = 65530u;
    _Atomic signed char signed8 = -120;
    _Atomic signed short signed16 = -30000;
    int elements[8] = {0};
    _Atomic(int *) cursor = elements;
    int *old_pointer;
    int *compound_pointer;
    int *subtracted_pointer;
    int *prefix_pointer;
    int *decremented_pointer;
    unsigned char old8 = value8++;
    unsigned short old16 = value16++;
    unsigned char added8 = (value8 += 10u);
    unsigned short added16 = (value16 += 10u);
    signed char old_signed8 = signed8++;
    signed short old_signed16 = signed16++;
    signed char added_signed8 = (signed8 += 7);
    signed short added_signed16 = (signed16 += 20);
    old_pointer = cursor++;
    if (old_pointer != &elements[0] || cursor != &elements[1]) return 3;
    compound_pointer = (cursor += 3);
    if (compound_pointer != &elements[4] || cursor != &elements[4]) return 4;
    subtracted_pointer = (cursor -= 2);
    if (subtracted_pointer != &elements[2] || cursor != &elements[2]) return 5;
    prefix_pointer = ++cursor;
    if (prefix_pointer != &elements[3] || cursor != &elements[3]) return 6;
    decremented_pointer = --cursor;

    return old8 == 250u && value8 == 5u && added8 == 5u &&
        old16 == 65530u && value16 == 5u && added16 == 5u &&
        old_signed8 == -120 && signed8 == -112 && added_signed8 == -112 &&
        old_signed16 == -30000 && signed16 == -29979 &&
        added_signed16 == -29979 &&
        decremented_pointer == &elements[2] && cursor == &elements[2]
            ? 0 : 1;
}
