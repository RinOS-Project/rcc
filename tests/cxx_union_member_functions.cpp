union UnionDefaultedAssignment {
    int integer;
    unsigned int bits;
    long long wide;
    unsigned char bytes[8];

    void set(int value) { integer = value; }
    int get() const { return integer; }
    UnionDefaultedAssignment& operator=(int value) {
        integer = value;
        return *this;
    }
    UnionDefaultedAssignment& operator=(
        const union UnionDefaultedAssignment&) = default;
};

static_assert(__is_trivially_copyable(UnionDefaultedAssignment),
              "a union with defaulted trivial copy assignment remains trivial");

union UnionUserAssignment {
    int integer;
    unsigned int bits;

    void set(int value) { integer = value; }
    int get() const { return integer; }
    UnionUserAssignment& operator=(
        const union UnionUserAssignment& other) {
        integer = other.integer;
        return *this;
    }
};

union UnionForwardDeclaration;
union UnionForwardDeclaration {
    int value;
    void set(int input) { value = input; }
    int get() const { return value; }
};

union UnionDefaultMemberInitializer {
    int value = 17;
    long long other;
    int get() const { return value; }
};

union UnionDesignatedInitializer {
    unsigned integer = 11;
    float floating;
};

struct UnionContainer {
    union Nested {
        int value;
        void set(int input) { value = input; }
        int get() const { return value; }
    };
};

namespace union_namespace {
union NamespaceUnion {
    int value;
    void set(int input) { value = input; }
};
}

static_assert(sizeof(UnionForwardDeclaration) == sizeof(int),
              "union layout must use the largest variant");
static_assert(alignof(UnionForwardDeclaration) == alignof(int),
              "union layout must preserve member alignment");

static int local_union_test() {
    union LocalUnion {
        int value;
        void set(int input) { value = input; }
        int get() const { return value; }
    };
    LocalUnion local{};
    local.set(13);
    return local.get();
}

int main(void) {
    UnionDefaultedAssignment source{};
    UnionDefaultedAssignment copied{};
    UnionDefaultedAssignment defaulted{};
    source.set(23);
    source.bytes[7] = 0x5a;
    copied = source;
    defaulted = copied;
    if (defaulted.get() != 23 || defaulted.bytes[7] != 0x5a) {
        return 1;
    }
    defaulted = 29;
    if (defaulted.get() != 29) return 2;

    UnionUserAssignment user_source{};
    UnionUserAssignment user_copy{};
    user_source.set(41);
    user_copy = user_source;
    if (user_copy.get() != 41) return 3;

    UnionForwardDeclaration forward_declared{};
    forward_declared.set(53);
    if (forward_declared.get() != 53) return 4;

    UnionDefaultMemberInitializer initialized{};
    if (initialized.get() != 17) return 5;

    UnionDesignatedInitializer explicitly_selected{.floating = 2.5f};
    if (explicitly_selected.floating != 2.5f) return 6;

    UnionContainer::Nested nested{};
    nested.set(67);
    if (nested.get() != 67) return 7;

    union_namespace::NamespaceUnion namespaced{};
    namespaced.set(79);
    if (namespaced.value != 79) return 8;

    if (local_union_test() != 13) return 9;
    return 0;
}
