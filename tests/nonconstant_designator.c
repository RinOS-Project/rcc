int designator_value;
int non_constant[2] = {[designator_value] = 1};

int main(void)
{
    return non_constant[0];
}
