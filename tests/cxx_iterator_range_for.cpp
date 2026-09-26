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

int main(void) {
    Range range;
    range.values[0] = 1;
    range.values[1] = 2;
    range.values[2] = 3;
    int total = 0;
    for (auto value : range) {
        total += value;
    }
    return total == 6 ? 0 : 1;
}
