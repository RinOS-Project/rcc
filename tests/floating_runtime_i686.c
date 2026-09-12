float add_float(float left, float right);
double multiply_double(double left, double right);

int main(void) {
    float value = 1.5f;
    value += 2.25f;
    value *= 2.0f;
    value -= 1.5f;
    value /= 2.0f;
    float old = value++;
    double product = multiply_double((double)value, 2.0);
    int ordered = value > 3.5f;
    int truth = !!value;
    return (int)old + (int)product + ordered + truth;
}

float add_float(float left, float right) {
    return left + right;
}

double multiply_double(double left, double right) {
    return left * right;
}
