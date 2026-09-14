struct ArrayElement {
    int value;

    ArrayElement(int input) : value(input) {}
};

extern "C" int cxx_new_array_constructor(void)
{
    ArrayElement* values = new ArrayElement[3]{4, 5, 6};
    int result = values[0].value + values[1].value + values[2].value;
    delete[] values;
    return result;
}
