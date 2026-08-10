int out_of_bounds[2] = {[2] = 1};
int field_on_array[1] = {.member = 1};
int duplicate_array[2] = {1, [0] = 2};

struct InvalidDesignator {
    int member;
};

struct InvalidDesignator missing_field = {.missing = 1};
struct InvalidDesignator index_on_struct = {[0] = 1};
struct InvalidDesignator duplicate_field = {1, .member = 2};

int main(void)
{
    return 0;
}
