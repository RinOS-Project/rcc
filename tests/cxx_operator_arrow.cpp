int arrow_proxy_order;

struct Target {
    int value;

    int read() {
        arrow_proxy_order = arrow_proxy_order * 10 + 3;
        return value;
    }
};

struct Proxy {
    Target* target;

    Target* operator->() {
        return target;
    }
};

struct ArrowProxy {
    Target* target;

    ~ArrowProxy() {
        arrow_proxy_order = arrow_proxy_order * 10 + 4;
    }

    Target* operator->() {
        arrow_proxy_order = arrow_proxy_order * 10 + 2;
        return target;
    }
};

struct NestedProxy {
    Target* target;

    ArrowProxy operator->() {
        arrow_proxy_order = arrow_proxy_order * 10 + 1;
        return ArrowProxy{target};
    }
};

int main() {
    Target target{41};
    Proxy direct{&target};
    NestedProxy nested{&target};
    arrow_proxy_order = 0;
    if (nested->read() != 41) return 1;
    if (arrow_proxy_order != 1234) return 2;
    return direct->value == 41 ? 0 : 3;
}
