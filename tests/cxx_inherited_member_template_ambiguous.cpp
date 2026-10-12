struct InheritedMemberTemplateAmbiguousFirst {
    template<typename T>
    int adjust(T value) { return (int)value; }
};

struct InheritedMemberTemplateAmbiguousSecond {
    template<typename T>
    int adjust(T value) { return (int)value + 1; }
};

struct InheritedMemberTemplateAmbiguousDerived
    : InheritedMemberTemplateAmbiguousFirst,
      InheritedMemberTemplateAmbiguousSecond {};

int main() {
    InheritedMemberTemplateAmbiguousDerived value;
    return value.adjust(1);
}
