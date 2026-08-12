int consume_integer(int value);

int reject_nullptr_integer_conversion() {
    return consume_integer(nullptr);
}
