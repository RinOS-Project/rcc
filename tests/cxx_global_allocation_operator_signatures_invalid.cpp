using size_type = __SIZE_TYPE__;

int operator new(size_type);
void* operator new(int);
void* operator new[](size_type = 1);
void* operator new[](unsigned short);
static void* operator new[](unsigned long long);

int operator delete(void*);
void operator delete[](int*);

namespace invalid_allocation_scope {
void* operator new(size_type);
}

struct InvalidClassAllocationSignatures {
    int operator new(size_type) { return 0; }
    void operator delete(int*) noexcept {}
};
