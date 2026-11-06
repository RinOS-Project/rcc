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

class PrivateConversionWithoutUsing : private PrivateConversionBase {};

#if defined(TEST_PRIVATE_BASE_CONVERSION_WITHOUT_USING)
int main() {
    PrivateConversionWithoutUsing value;
    return take_integer(value);
}
#else
int main() {
    PublicUsingPrivateConversion value;
    return take_integer(value) == 701 ? 0 : 1;
}
#endif
