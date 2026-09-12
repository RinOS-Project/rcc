float add_float(float left, float right);
double multiply_double(double left, double right);

int main(void) {
    float sum = add_float(1.5f, 2.25f);
    double product = multiply_double(2.0, 4.0);
    int ordered = add_float(1.0f, 2.0f) < 4.0f;
    int unordered = add_float(1.0f, 2.0f) != 4.0f;
    return (int)sum + (int)product + ordered + unordered;
}

float add_float(float left, float right) {
    return left + right;
}

double multiply_double(double left, double right) {
    return left * right;
}
