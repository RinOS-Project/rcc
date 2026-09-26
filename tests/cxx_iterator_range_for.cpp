struct Iterator {
    int* current;
    int* last;

    int operator*() const {
        return *current;
    }

    Iterator& operator++() {
        ++current;
        return *this;
    }

    bool operator!=(const Iterator& other) const {
        return current != other.current;
    }
};

struct Range {
    int values[3];

    Iterator begin() {
        return Iterator{values, values + 3};
    }

    Iterator end() {
        return Iterator{values + 3, values + 3};
    }
};

struct PointerRange {
    int values[2];

    int* begin() {
        return values;
    }

    int* end() {
        return values + 2;
    }
};

int main(void) {
    Range range;
    range.values[0] = 1;
    range.values[1] = 2;
    range.values[2] = 3;
    int total = 0;
    for (auto value : range) {
        total += value;
    }
    PointerRange pointer_range;
    pointer_range.values[0] = 4;
    pointer_range.values[1] = 5;
    for (const auto& value : pointer_range) {
        total += value;
    }
    return total == 15 ? 0 : 1;
}
