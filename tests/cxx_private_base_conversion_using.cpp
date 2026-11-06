int take_integer(int value) {
    return value;
}

class PrivateConversionBase {
public:
    operator int() const & {
        return 701;
    }
};

class PublicUsingPrivateConversion : private PrivateConversionBase {
public:
    using PrivateConversionBase::operator int;
};

class PrivateUsingPrivateConversion : private PrivateConversionBase {
private:
    using PrivateConversionBase::operator int;

public:
    int convert_inside() const {
        return static_cast<int>(*this);
    }
};

class PrivateConversionWithoutUsing : private PrivateConversionBase {};

#if defined(TEST_PRIVATE_BASE_CONVERSION_WITHOUT_USING)
int main() {
    PrivateConversionWithoutUsing value;
    return take_integer(value);
}
#elif defined(TEST_PRIVATE_USING_CONVERSION_OUTSIDE)
int main() {
    PrivateUsingPrivateConversion value;
    return take_integer(value);
}
#else
int main() {
    PublicUsingPrivateConversion public_value;
    PrivateUsingPrivateConversion private_value;
    return take_integer(public_value) == 701 &&
                   private_value.convert_inside() == 701
        ? 0 : 1;
}
#endif
