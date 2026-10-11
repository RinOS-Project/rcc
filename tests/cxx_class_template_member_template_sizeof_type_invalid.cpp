template<typename T, int Size>
struct PackTagged {
    T value;
};

template<typename Owner>
struct Box {
    template<typename... Args>
    int count_tagged_type_size(PackTagged<Args, sizeof(Args)>...) const {
        return sizeof...(Args);
    }
};

int main() {
    Box<int> box;
    return box.count_tagged_type_size(PackTagged<int, 8>{1});
}
