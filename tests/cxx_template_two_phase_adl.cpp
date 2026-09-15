namespace payload {
class Item {
public:
    int value;
};
}

template<typename T>
int invoke_adjust(T value) {
    return adjust(value);
}

namespace payload {
int adjust(Item value) {
    return value.value + 5;
}
}

using namespace payload;

int main() {
    Item item;
    item.value = 7;
    return invoke_adjust(item) == 12 ? 0 : 1;
}
