extern int decltype_auto_local_probe(void);

int main(void) {
    return decltype_auto_local_probe() == 123 ? 0 : 1;
}
