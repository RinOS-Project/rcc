typedef unsigned long property_word_t;

struct PropertyPair {
    int left;
    int right;
};

static int property_bias(int value)
{
    return value + 1;
}

int property_corpus_c(int value)
{
    struct PropertyPair pair = {.left = value, .right = 2};
    return _Generic(value, int: property_bias(pair.left), default: 0) +
           (int)(property_word_t)pair.right;
}
