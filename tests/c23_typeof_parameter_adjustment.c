static int invoke(int value) {
    return value + 1;
}

static int adjust_parameters(int values[static 3], int callback(int)) {
    typeof(values) adjusted_values = values;
    typeof(callback) adjusted_callback = callback;
    return adjusted_values[1] + adjusted_callback(4);
}

int main(void) {
    int values[3] = { 1, 7, 2 };
    return adjust_parameters(values, invoke) == 12 ? 0 : 1;
}
