template <typename T>
struct ConditionalConversion {
    explicit(sizeof(T) > 4) operator bool() const { return true; }
};

struct ConditionalConversionTemplate {
    template <typename T>
    explicit(sizeof(T) > 4) operator T() const {
        return static_cast<T>(23);
    }
};

int main() {
    ConditionalConversion<long long> value;
    bool converted = value;
    ConditionalConversionTemplate templated_value;
    long long templated_converted = templated_value;
    return converted && templated_converted ? 0 : 1;
}
