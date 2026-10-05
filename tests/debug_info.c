int debug_global_data = 7;
extern int debug_global_data;
static int debug_file_static;
const int debug_const_data = 4;
volatile int debug_volatile_data;
int * restrict debug_restrict_data;
_Atomic int debug_atomic_data;

struct debug_aggregate {
    int first;
    int second;
};

struct debug_aggregate debug_aggregate_data = {1, 2};
int debug_array_data[2] = {8, 9};
struct debug_recursive {
    int value;
    struct debug_recursive* next;
};
struct debug_recursive* debug_recursive_root;
struct debug_bits {
    unsigned first : 3;
    unsigned second : 5;
};
struct debug_bits debug_bits_data;
enum debug_enum {
    DEBUG_ENUM_NEGATIVE = -2,
    DEBUG_ENUM_POSITIVE = 6
};
enum debug_enum debug_enum_data = DEBUG_ENUM_POSITIVE;
static int debug_aggregate_sum(void)
{
    struct debug_aggregate local = {3, 4};
    return local.first + local.second;
}

static int debug_line_helper(void)
{
    return 3;
}

inline int debug_declared_inline(int value)
{
    return value + 5;
}

int debug_line_entry(void)
{
    static int debug_line_static;
    return debug_line_helper() + debug_declared_inline(1) +
           debug_global_data + debug_file_static + debug_line_static;
}

int debug_info_parameters(int left, int right)
{
    int sum = left + right;
    int* pointer = &sum;
    {
        int nested = *pointer;
        return nested + debug_aggregate_sum() +
               debug_aggregate_data.first + debug_array_data[0];
    }
}
