struct FlexiblePrefix {
    int value;
    int tail[];
};

#if defined(_WIN32) && defined(__GNUC__)
#define RCC_SYSV_ABI __attribute__((sysv_abi))
#else
#define RCC_SYSV_ABI
#endif

extern int flexible_prefix_value(struct FlexiblePrefix value) RCC_SYSV_ABI;

int main(void)
{
    struct FlexiblePrefix value;
    value.value = 73;
    return flexible_prefix_value(value) == 73 ? 0 : 1;
}
