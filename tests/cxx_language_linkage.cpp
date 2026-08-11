extern "C" {
typedef unsigned long linkage_size_t;
int linkage_import(int value);
}

extern "C" int second_linkage_import(linkage_size_t value);
extern "C++" int cpp_linkage_import(int value);

int cpp_linkage_counter = 7;
extern "C" int c_linkage_counter;

int call_language_linkage(int value) {
    return linkage_import(value) +
           second_linkage_import((linkage_size_t)value) +
           cpp_linkage_import(value) + cpp_linkage_counter +
           c_linkage_counter;
}

class final_class_probe final {
public:
    int value;
};
