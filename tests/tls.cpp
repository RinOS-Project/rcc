thread_local int tls_cpp_counter = 11;

int main() {
    ++tls_cpp_counter;
    return tls_cpp_counter;
}
