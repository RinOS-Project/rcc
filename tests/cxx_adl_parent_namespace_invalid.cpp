namespace associated_parent {
namespace nested {
struct Argument {
    operator int() const;
};
}

int parent_only(int) {
    return 0;
}
}

int main() {
    associated_parent::nested::Argument argument;
    return parent_only(argument);
}
