extern "C" {
typedef unsigned long linkage_size_t;
int linkage_import(int value);
struct linkage_aggregate { int value; };
int linkage_aggregate_member(int value) {
    struct linkage_aggregate aggregate;
    aggregate.value = value;
    return aggregate.value;
}
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

namespace scoped_linkage {
extern "C" {
struct scoped_aggregate { int value; };
int scoped_linkage_function(int value) {
    struct scoped_aggregate aggregate;
    aggregate.value = value;
    return aggregate.value;
}
}

extern "C++" {
int scoped_cpp_import(int value);
}

int call_scoped_linkage(int value) {
    return scoped_linkage_function(value) + scoped_cpp_import(value);
}
}

int call_qualified_scoped_linkage(int value) {
    return scoped_linkage::scoped_linkage_function(value);
}

class final_class_probe final {
public:
    int value;
};
