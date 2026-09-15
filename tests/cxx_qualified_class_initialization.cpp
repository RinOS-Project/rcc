namespace payload {
class Item {
public:
    int value;
};
}

int main() {
    payload::Item item{7};
    return item.value == 7 ? 0 : 1;
}
