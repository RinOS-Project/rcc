extern int decltype_auto_local_probe(void);

int main(void) {
    return decltype_auto_local_probe() == 120 ? 0 : 1;
}
