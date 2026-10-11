template<typename ValueType, int Tag>
struct PackTagged {
    ValueType value;
};

struct Box {
    template<typename... Args>
    int count_tagged(PackTagged<Args, 7>...) const {
        return sizeof...(Args);
    }
};

int main() {
    Box box;
    return box.count_tagged(PackTagged<int, 8>{1});
}
