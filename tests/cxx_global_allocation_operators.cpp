using size_type = __SIZE_TYPE__;

extern "C" {
[[noreturn]] void allocation_operator_noreturn_probe() {
    for (;;) {}
}
}

void* operator new(size_type size) {
    (void)size;
    return (void*)0;
}

void* operator new[](size_type size) {
    (void)size;
    return (void*)0;
}

void operator delete(void* pointer) noexcept {
    (void)pointer;
}

void operator delete[](void* pointer) noexcept {
    (void)pointer;
}

void operator delete(void* pointer, size_type size) noexcept {
    (void)pointer;
    (void)size;
}

void operator delete[](void* pointer, size_type size) noexcept {
    (void)pointer;
    (void)size;
}

void* operator new(size_type size, void* storage) noexcept {
    (void)size;
    return storage;
}

void* operator new[](size_type size, void* storage) noexcept {
    (void)size;
    return storage;
}

void operator delete(void* pointer, void* storage) noexcept {
    (void)pointer;
    (void)storage;
}

void operator delete[](void* pointer, void* storage) noexcept {
    (void)pointer;
    (void)storage;
}
