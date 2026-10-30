using allocation_size_type = __SIZE_TYPE__;

alignas(8) static unsigned char parenthesized_new_storage[8];
static allocation_size_type parenthesized_new_size;
static int parenthesized_new_calls;

extern "C" void* rin_malloc(allocation_size_type size) {
    parenthesized_new_size = size;
    ++parenthesized_new_calls;
    return &parenthesized_new_storage[0];
}

int main() {
    int* value = new (int)(211);
    if (parenthesized_new_calls != 1 ||
        parenthesized_new_size != sizeof(int) ||
        value != reinterpret_cast<int*>(&parenthesized_new_storage[0]) ||
        *value != 211) {
        return 1;
    }
    return 0;
}
