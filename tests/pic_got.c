extern int imported_data;
extern int imported_function(int value);

static int local_data = 7;

static int local_function(int value)
{
    return value + 1;
}

int pic_got_use(int value)
{
    int result = imported_data + local_data;
    if (local_function != 0) result += local_function(value);
    if (imported_function != 0) ++result;
    return result;
}
