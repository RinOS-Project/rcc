union DeletedUnionEquality {
    int value;
    friend bool operator==(const DeletedUnionEquality&,
                           const DeletedUnionEquality&) = default;
};

int main() {
    DeletedUnionEquality left{};
    DeletedUnionEquality right{};
    return left == right;
}
