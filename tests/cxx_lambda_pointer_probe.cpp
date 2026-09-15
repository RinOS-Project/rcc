extern "C" {
    int probe_lambda_pointer(void) {
        int (*function)(int) = [](int value) -> int {
            return value + 1;
        };
        return function(41);
    }
}
