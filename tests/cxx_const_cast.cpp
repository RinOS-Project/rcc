extern "C" int main() {
    const int value = 17;
    const int* read_only = &value;
    int* writable = const_cast<int*>(read_only);
    return *writable == 17 ? 0 : 1;
}
