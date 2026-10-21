struct CircularArrow {
    CircularArrow operator->() const {
        return {};
    }
};

int main() {
    CircularArrow arrow;
    return arrow->value;
}
