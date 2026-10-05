struct EmptyBase {};

struct EmptyBaseOptimized : EmptyBase {
    int value;
};

struct SameTypeBaseAndMember : EmptyBase {
    EmptyBase value;
};

static_assert(sizeof(EmptyBaseOptimized) == sizeof(int),
              "an empty non-polymorphic base uses EBO");
static_assert(sizeof(SameTypeBaseAndMember) == 2,
              "same-type base and member cannot overlap");

int main() {
    EmptyBaseOptimized value;
    value.value = 42;
    return value.value == 42 ? 0 : 1;
}
