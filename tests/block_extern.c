int shared_value = 7;

int increment_shared(void)
{
    extern int shared_value;
    ++shared_value;
    return shared_value;
}

int read_shared(void)
{
    extern int shared_value;
    return shared_value;
}

int main(void)
{
    return increment_shared() == 8 && read_shared() == 8 ? 0 : 1;
}
