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

struct DefaultElement {
    int value;

    DefaultElement() : value(7) {}
};

extern "C" int cxx_new_array_default_constructor(void)
{
    DefaultElement* values = new DefaultElement[2]();
    int result = values[0].value + values[1].value;
    delete[] values;
    return result;
}

extern "C" int cxx_new_scalar_default_constructor(void)
{
    DefaultElement* value = new DefaultElement();
    int result = value->value;
    delete value;
    return result;
}
