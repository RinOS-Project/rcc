namespace member_templates {

struct MemberFunctionTemplateValue {
    int value;

    template <typename T>
    int add(T other) const {
        return value + other;
    }

    template <typename T>
    T copy(T other) const {
        return other;
    }

    template <typename T>
    static T identity(T other) {
        return other;
    }

    int choose(int other) const {
        return value + other;
    }

    template <typename T>
    int choose(T other) const {
        return other ? 1 : 0;
    }

    template <typename T>
    int choose(T* other) const {
        return other ? 2 : 0;
    }

    template <int N = 3>
        requires (N > 0)
    int add_default(int other) const {
        return value + other + N;
    }

    template <typename T>
        requires true
    int add_unconditionally(T other) const {
        return value + other;
    }

};

} // namespace member_templates

int main() {
    member_templates::MemberFunctionTemplateValue item;
    int pointed_value = 0;
    item.value = 37;
    return item.add(5) == 42 && item.add((short)2) == 39 &&
                   item.copy(6) == 6 &&
                   item.identity(9) == 9 &&
                   item.add_default(2) == 42 && item.choose(5) == 42 &&
                   item.add_unconditionally(4) == 41 &&
                   item.choose(&pointed_value) == 2
        ? 0
        : 1;
}
