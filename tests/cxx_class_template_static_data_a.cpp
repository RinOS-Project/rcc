template<typename T>
class SizeValue {
public:
    inline static int value = sizeof(T);
};

extern "C" int other_template_value();

extern "C" int main() {
    return SizeValue<int>::value == 4 && other_template_value() == 5 ? 0 : 1;
}
