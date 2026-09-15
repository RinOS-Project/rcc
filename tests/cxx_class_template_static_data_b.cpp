template<typename T>
class SizeValue {
public:
    inline static int value = sizeof(T);
};

extern "C" int other_template_value() {
    return SizeValue<char>::value + SizeValue<int>::value;
}
