int main(void)
{
    int count = 3;
    int values[count];

    values[0] = 7;
    values[2] = 9;

    return count == 3 && sizeof(int[count]) == 12 &&
        values[0] == 7 && values[2] == 9 ? 0 : 1;
}
