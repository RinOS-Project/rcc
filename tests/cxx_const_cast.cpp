extern "C" int main() {
    int value = 17;
    const int& read_only_ref = value;
    int& writable_ref = const_cast<int&>(read_only_ref);
    writable_ref = 19;
    const int* read_only = &value;
    int* writable = const_cast<int*>(read_only);
    return *writable == 19 && writable_ref == 19 ? 0 : 1;
}
