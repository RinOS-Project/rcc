struct Target {
    int value;
};

struct Proxy {
    Target* target;

    Target* operator->() {
        return target;
    }
};

int main() {
    Target target{41};
    Proxy direct{&target};
    return direct->value == 41 ? 0 : 1;
}
