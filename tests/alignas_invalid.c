_Alignas(3) int invalid_alignment;
_Alignas(32768) int unsupported_extended_alignment;

int main(void)
{
    return invalid_alignment;
}
