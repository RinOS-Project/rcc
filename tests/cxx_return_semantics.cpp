void return_void_expression_source() {}

void return_void_expression() {
    return return_void_expression_source();
}

int main() {
    return_void_expression();
    return 0;
}
