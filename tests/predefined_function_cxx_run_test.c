extern int probe_cxx_predefined_function(void);
extern int probe_cxx_nested_predefined_function(void);

int main(void) {
    return probe_cxx_predefined_function() +
           probe_cxx_nested_predefined_function();
}
