int arrow_proxy_order;
int arrow_proxy_category;

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

    Target* operator->() & {
        arrow_proxy_category = 1;
        arrow_proxy_order = arrow_proxy_order * 10 + 2;
        return target;
    }

    Target* operator->() const& {
        arrow_proxy_category = 2;
        arrow_proxy_order = arrow_proxy_order * 10 + 2;
        return target;
    }

    Target* operator->() && {
        arrow_proxy_category = 3;
        arrow_proxy_order = arrow_proxy_order * 10 + 2;
        return target;
    }
};

struct ReferenceProxy {
    ArrowProxy* target;

    ArrowProxy& operator->() {
        return *target;
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
    arrow_proxy_category = 0;
    if (nested->read() != 41) return 1;
    if (arrow_proxy_order != 1234 || arrow_proxy_category != 3) return 2;

    ArrowProxy lvalue{&target};
    arrow_proxy_category = 0;
    if (lvalue->read() != 41 || arrow_proxy_category != 1) return 3;

    const ArrowProxy const_lvalue{&target};
    arrow_proxy_category = 0;
    if (const_lvalue->read() != 41 || arrow_proxy_category != 2) return 4;

    ReferenceProxy reference{&lvalue};
    arrow_proxy_category = 0;
    if (reference->read() != 41 || arrow_proxy_category != 1) return 5;

    return direct->value == 41 ? 0 : 6;
}
