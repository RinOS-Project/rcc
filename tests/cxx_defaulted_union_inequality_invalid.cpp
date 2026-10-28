union DeletedUnionInequality {
    int value;
    friend bool operator==(const DeletedUnionInequality&,
                           const DeletedUnionInequality&) = default;
};

int main() {
    DeletedUnionInequality left{};
    DeletedUnionInequality right{};
    return left != right;
}
