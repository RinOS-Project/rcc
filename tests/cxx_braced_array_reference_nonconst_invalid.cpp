int consume_array_reference(int (&values)[2]) {
    return values[0] + values[1];
}

int main() {
    return consume_array_reference({1, 2});
}
