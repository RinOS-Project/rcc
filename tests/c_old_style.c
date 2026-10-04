/* C17 still accepts the obsolescent identifier-list definition form. */
int old_style_add(left, right)
int left;
int right;
{
    return left + right;
}

int old_style_default(value)
{
    return value + 1;
}

int old_style_array(values)
int values[2];
{
    return values[0] + values[1];
}

int old_style_apply(function, value)
int function(int);
int value;
{
    return function(value);
}

int old_style_plus_one(value)
int value;
{
    return value + 1;
}

int old_style_multi(left, right)
int left, right;
{
    return left - right;
}

int main(void)
{
    return old_style_add(4, 5) == 9 &&
           old_style_default(6) == 7 &&
           old_style_array((int[2]){ 2, 3 }) == 5 &&
           old_style_apply(old_style_plus_one, 8) == 9 &&
           old_style_multi(11, 4) == 7 ? 0 : 1;
}
