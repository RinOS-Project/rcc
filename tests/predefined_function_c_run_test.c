extern int probe_c_predefined_function(void);
extern int probe_c_predefined_location(void);

int main(void) {
    return probe_c_predefined_function() + probe_c_predefined_location();
}
