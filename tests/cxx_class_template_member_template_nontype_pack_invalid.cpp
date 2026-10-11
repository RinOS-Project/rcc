template<typename ValueType, int Tag>
struct PackTagged {
    ValueType value;
};

struct Box {
    template<int Tag, typename... Args>
    int count_tagged(PackTagged<Args, Tag>...) const {
        return sizeof...(Args);
    }
};

int main() {
    Box box;
    return box.count_tagged(
        PackTagged<int, 7>{1},
        PackTagged<long, 8>{2});
}
