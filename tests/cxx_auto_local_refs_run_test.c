extern int auto_local_reference_probe(void);
extern int auto_local_const_probe(void);

int main(void) {
    if (auto_local_reference_probe() != 16) return 1;
    return auto_local_const_probe() == 42 ? 0 : 2;
}
