template<int N = 3>
class Fixed {
public:
    int values[N];

    int extent() {
        return N;
    }
};

int main() {
    Fixed<3> explicit_value;
    Fixed<> default_value;
    return explicit_value.extent() == 3 && default_value.extent() == 3 &&
                   sizeof(explicit_value) == 12 &&
                   sizeof(default_value) == 12 ? 0 : 1;
}
