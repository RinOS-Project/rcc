class InitializerBody {
public:
    InitializerBody(int value) : first(value), second(3) {
        second += first;
    }

    int total() const {
        return first + second;
    }

private:
    int first;
    int second;
};

extern "C" int cxx_constructor_initializer_body() {
    InitializerBody local(7);
    InitializerBody* heap = new InitializerBody(5);
    int result = local.total() + heap->total();
    delete heap;
    return result == 30 ? 0 : 1;
}
