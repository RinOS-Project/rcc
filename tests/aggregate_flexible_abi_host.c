struct FlexiblePrefix {
    int value;
    int tail[];
};

extern int flexible_prefix_value(struct FlexiblePrefix value);

int main(void)
{
    struct FlexiblePrefix value;
    value.value = 73;
    return flexible_prefix_value(value) == 73 ? 0 : 1;
}
