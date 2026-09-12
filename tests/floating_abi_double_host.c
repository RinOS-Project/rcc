extern double _rcc_entry(double left, double right);

int main(void) {
    double product = _rcc_entry(1.5, 4.0);
    return product == 6.0 ? 0 : 1;
}
