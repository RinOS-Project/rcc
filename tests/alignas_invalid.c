_Alignas(3) int invalid_alignment;
_Alignas(8192) int unsupported_extended_alignment;

int main(void)
{
    return invalid_alignment;
}
