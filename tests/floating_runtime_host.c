extern int _rcc_entry(void);

int main(void) {
    return _rcc_entry() == 13 ? 0 : 1;
}
