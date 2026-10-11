template<typename First, typename Second>
struct PackPair {
    First first;
    Second second;
};

template<typename T>
struct Box {
    template<typename... Args>
    int count_matching_pairs(PackPair<Args, Args>...) const {
        return sizeof...(Args);
    }
};

int main() {
    Box<int> box;
    return box.count_matching_pairs(PackPair<int, long>{1, 2});
}
