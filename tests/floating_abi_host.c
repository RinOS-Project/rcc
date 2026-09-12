extern float _rcc_entry(float left, float right);

int main(void) {
    float sum = _rcc_entry(1.25f, 2.5f);
    return sum == 3.75f ? 0 : 1;
}
