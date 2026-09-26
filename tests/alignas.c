_Alignas(16) int aligned_global;
_Alignas(long long) int aligned_type_global;

int main(void)
{
    _Alignas(16) int aligned_local = 7;
    unsigned long address = (unsigned long)&aligned_local;
    return address % 16 == 0 && aligned_local == 7 && aligned_global == 0
        ? 0 : 1;
}
