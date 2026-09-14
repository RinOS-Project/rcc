extern "C" int cxx_new_array_values(void)
{
    int* values = new int[3]{4, 5, 6};
    int result = values[0] + values[1] + values[2];
    delete[] values;
    return result;
}
