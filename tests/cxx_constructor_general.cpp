class GeneralConstructor {
public:
    int value;

    GeneralConstructor(int input) : value(input) {
        int delta = input + 1;
        if (delta > 0) {
            value += delta;
        } else {
            value = 0;
        }
    }
};

class TemplateConstructor {
public:
    int value;

    template <typename T>
    TemplateConstructor(T input) {
        int adjusted = input + 2;
        if (adjusted > 0) {
            value = adjusted;
        } else {
            value = 0;
        }
    }
};

class NormalizedTemplateConstructor {
public:
    int value;

    template <typename T>
    NormalizedTemplateConstructor(T input) {
        value = input;
    }
};

static_assert(__is_constructible(TemplateConstructor, int),
              "constructor-template body does not affect viability");

int main() {
    GeneralConstructor object(3);
    TemplateConstructor templated{5};
    NormalizedTemplateConstructor normalized{9};
    TemplateConstructor parenthesized(4);
    NormalizedTemplateConstructor normalized_parenthesized(11);
    TemplateConstructor functional_cast = TemplateConstructor(2);
    return object.value == 7 && templated.value == 7 &&
                   normalized.value == 9 && parenthesized.value == 6 &&
                   normalized_parenthesized.value == 11 &&
                   functional_cast.value == 4
        ? 0
        : 1;
}
