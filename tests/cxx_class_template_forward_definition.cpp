template<class T> struct rcc_forward_template;

rcc_forward_template<int>* rcc_forward_template_saved;

template<class T> struct rcc_forward_template {
    int value;
    T* next;
};

static_assert(sizeof(rcc_forward_template<int>) == sizeof(void*) * 2);

int rcc_read_forward_template_saved() {
    return rcc_forward_template_saved->value;
}
