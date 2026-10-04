template <int Bias>
int property_add(int value)
{
    return value + Bias;
}

struct PropertyValue {
    int value;
};

int property_corpus_cpp(int value)
{
    PropertyValue property{value};
    return property_add<2>(property.value);
}
