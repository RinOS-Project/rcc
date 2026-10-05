union AggregateUnionAbi {
    double floating;
    int integer;
};

#if defined(_WIN32) && defined(__GNUC__)
#define RCC_SYSV_ABI __attribute__((sysv_abi))
#else
#define RCC_SYSV_ABI
#endif

extern int aggregate_union_integer(union AggregateUnionAbi value) RCC_SYSV_ABI;

int main(void)
{
    union AggregateUnionAbi value;
    value.integer = 73;
    return aggregate_union_integer(value) == 73 ? 0 : 1;
}
