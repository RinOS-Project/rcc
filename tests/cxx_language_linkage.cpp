extern "C" {
typedef unsigned long linkage_size_t;
int linkage_import(int value) { return value; }
struct linkage_aggregate { int value; };
int linkage_aggregate_member(int value) {
    struct linkage_aggregate aggregate;
    aggregate.value = value;
    return aggregate.value;
}
}

extern "C" int second_linkage_import(linkage_size_t value) {
    return (int)value;
}
extern "C++" int cpp_linkage_import(int value) { return value; }

int cpp_linkage_counter = 7;
extern "C" { int c_linkage_counter = 11; }

int call_language_linkage(int value) {
    return linkage_import(value) +
           second_linkage_import((linkage_size_t)value) +
           cpp_linkage_import(value) + cpp_linkage_counter +
           c_linkage_counter;
}

namespace scoped_linkage {
extern "C" {
struct scoped_aggregate { int value; };
int scoped_c_counter = 13;
int scoped_linkage_function(int value) {
    struct scoped_aggregate aggregate;
    aggregate.value = value;
    return aggregate.value;
}
}

extern "C++" {
int scoped_cpp_counter = 17;
int scoped_cpp_import(int value) { return value + 3; }
}

int call_scoped_linkage(int value) {
    return scoped_linkage_function(value) + scoped_cpp_import(value) +
           scoped_c_counter + scoped_cpp_counter;
}
}

int call_qualified_scoped_linkage(int value) {
    return scoped_linkage::scoped_linkage_function(value) +
           scoped_linkage::scoped_c_counter;
}

int main(void) {
    return call_language_linkage(2) == 24 &&
                   linkage_aggregate_member(9) == 9 &&
                   scoped_linkage::call_scoped_linkage(2) == 37 &&
                   call_qualified_scoped_linkage(2) == 15
               ? 0
               : 1;
}

class final_class_probe final {
public:
    int value;
};
