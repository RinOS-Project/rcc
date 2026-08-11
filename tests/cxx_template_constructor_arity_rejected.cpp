template<typename T>
class Outcome final {
public:
    Outcome(int code, const T& value) : code_(code), value_(value) {}

private:
    int code_;
    T value_;
};

int invalid_template_constructor_arity(int code) {
    return Outcome<int>{code}.code_;
}
