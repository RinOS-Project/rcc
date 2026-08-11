extern "C" {
typedef unsigned long linkage_size_t;
int linkage_import(int value);
}

extern "C" int second_linkage_import(linkage_size_t value);

int call_language_linkage(int value) {
    return linkage_import(value) +
           second_linkage_import((linkage_size_t)value);
}

class final_class_probe final {
public:
    int value;
};
