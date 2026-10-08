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

template<typename T>
int invoke_global_adjust(T value) {
    return adjust_global(value);
}

struct GlobalItem {
    int value;
};

namespace payload {
int adjust(Item value) {
    return value.value + 5;
}
}

int adjust_global(GlobalItem value) {
    return value.value + 9;
}

using namespace payload;

int main() {
    Item item;
    item.value = 7;
    GlobalItem global_item;
    global_item.value = 3;
    return invoke_adjust(item) == 12 &&
                   invoke_global_adjust(global_item) == 12
        ? 0 : 1;
}
