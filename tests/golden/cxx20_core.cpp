template<int N>
concept GoldenPositive = N > 0;

template<int N>
requires GoldenPositive<N>
constexpr int golden_offset()
{
    return N + 2;
}

int golden_cxx20_entry(int value)
{
    return value + golden_offset<2>();
}
