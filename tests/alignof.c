struct AlignRecord {
    char first;
    long long value;
};

_Static_assert(_Alignof(char) == 1, "char alignment");
_Static_assert(_Alignof(int) == 4, "int alignment");
_Static_assert(_Alignof(struct AlignRecord) == 8, "aggregate alignment");

int alignof_global = _Alignof(long long);

int alignof_value(void)
{
    return _Alignof(struct AlignRecord) + _Alignof(int[3]);
}
