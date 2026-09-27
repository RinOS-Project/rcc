extern int auto_local_reference_probe(void);
extern int auto_local_const_probe(void);
extern int auto_local_pointer_probe(void);
extern int auto_local_direct_list_probe(void);

int main(void) {
    if (auto_local_reference_probe() != 16) return 1;
    if (auto_local_const_probe() != 42) return 2;
    if (auto_local_pointer_probe() != 8) return 3;
    return auto_local_direct_list_probe() == 13 ? 0 : 4;
}
