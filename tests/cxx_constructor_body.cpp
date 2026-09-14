class Pair {
public:
    int first;
    int second;

    Pair(int left, int right)
    {
        first = left;
        this->second = right;
    }
};

int cxx_constructor_body()
{
    Pair* value = new Pair(7, 11);
    int result = value->first + value->second;
    delete value;
    return result == 18 ? 0 : 1;
}
