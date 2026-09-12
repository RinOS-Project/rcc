template<typename T>
class box {
public:
    T value;
};

box<int> use_box() {
    box<int> result{};
    result.value = 7;
    return result;
}
