struct EmptyInitializerPair {
    int first;
    int second;
};

int empty_initializer_scalar = {};
int empty_initializer[2] = {};
struct EmptyInitializerPair empty_initializer_pair = {};

int main(void)
{
    return empty_initializer_scalar + empty_initializer[0] +
           empty_initializer[1] + empty_initializer_pair.first +
           empty_initializer_pair.second;
}
