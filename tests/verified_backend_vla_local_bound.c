int main(void)
{
    int count = 3;
    int values[count];

    values[0] = 7;
    values[2] = 9;

    return count == 3 && values[0] == 7 && values[2] == 9 ? 0 : 1;
}
