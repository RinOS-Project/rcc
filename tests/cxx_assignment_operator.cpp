class Assignable {
public:
    int value;

    int operator=(int next) {
        value = next;
        return value;
    }

    int operator+=(int delta) {
        value += delta;
        return value;
    }
};

int main() {
    Assignable item{1};
    int assigned = (item = 4);
    int accumulated = (item += 3);
    return assigned == 4 && accumulated == 7 && item.value == 7 ? 0 : 1;
}
