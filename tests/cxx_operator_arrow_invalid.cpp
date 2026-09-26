struct InvalidProxy {
    int operator->() {
        return 0;
    }
};

int main() {
    InvalidProxy value{};
    return value->missing;
}
