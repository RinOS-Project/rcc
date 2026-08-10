int out_of_bounds[2] = {[2] = 1};
int field_on_array[1] = {.member = 1};

struct InvalidDesignator {
    int member;
};

struct InvalidDesignator missing_field = {.missing = 1};
struct InvalidDesignator index_on_struct = {[0] = 1};

struct NestedInvalidDesignator {
    struct InvalidDesignator nested;
    int values[2];
};

struct NestedInvalidDesignator missing_nested = {.nested.missing = 1};
struct NestedInvalidDesignator out_of_bounds_nested = {.values[2] = 1};

int main(void)
{
    return 0;
}
