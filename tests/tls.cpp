thread_local int tls_cpp_counter = 11;
thread_local void* tls_cpp_null_pointer = nullptr;
void* cpp_null_pointer = nullptr;

int main() {
    ++tls_cpp_counter;
    return tls_cpp_counter + (tls_cpp_null_pointer != nullptr) +
           (cpp_null_pointer != nullptr);
}
