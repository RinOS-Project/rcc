int* invalid_static_pointer = (int*)1;

int main(void)
{
    return invalid_static_pointer != 0;
}
