struct RestrictFields {
    int *restrict member;
};

typedef int *restrict RestrictIntPointer;

static int add_restrict(int *restrict left, int *const restrict right)
{
    *left += *right;
    return *left;
}

static int read_array(int values[static restrict 1])
{
    return values[0];
}

int main(void)
{
    int left = 2;
    int right = 3;
    RestrictIntPointer alias = &left;
    int *restrict direct = &right;
    void *restrict opaque = &left;
    int **restrict nested = &alias;
    struct RestrictFields fields;

    fields.member = alias;
    return add_restrict(direct, fields.member) == 5 &&
                   read_array(&left) == 2 && opaque != 0 && nested != 0
               ? 0
               : 1;
}
