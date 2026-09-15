extern int* decltype_auto_reference(int* value);
extern int decltype_auto_value(void);

int main(void) {
    int value = 7;
    int* reference = decltype_auto_reference(&value);
    if (reference != &value) return 1;
    *reference = 9;
    if (value != 9) return 2;
    return decltype_auto_value() == 42 ? 0 : 3;
}
