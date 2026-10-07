typedef int Row;

int pointer_to_vla(int count, int values[count])
{
    int (*row)[count] = (int (*)[count])values;
    count = 1;
    if (sizeof(*row) != 3 * sizeof(int)) return -1;
    return row[1][2];
}

int typedef_pointer_to_vla(int count, int values[count])
{
    typedef int ElementRow[count];
    ElementRow *row = (ElementRow *)values;
    count = 1;
    if (sizeof(*row) != 3 * sizeof(int)) return -1;
    return row[1][2];
}

int matrix_parameter(int rows, int cols, int values[rows][cols])
{
    return values[rows - 1][cols - 1];
}

int main(void)
{
    int values[6] = {4, 5, 6, 7, 8, 9};
    int matrix[2][3] = {{1, 2, 3}, {7, 8, 9}};
    if (pointer_to_vla(3, values) != 9) return 1;
    if (typedef_pointer_to_vla(3, values) != 9) return 2;
    return matrix_parameter(2, 3, matrix) == 9 ? 0 : 3;
}
