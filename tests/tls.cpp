thread_local int tls_cpp_counter = 11;
thread_local void* tls_cpp_null_pointer = nullptr;
void* cpp_null_pointer = nullptr;

namespace tls_namespace {
thread_local int counter = 13;
}

int main() {
    ++tls_cpp_counter;
    ++tls_namespace::counter;
    return tls_cpp_counter + (tls_cpp_null_pointer != nullptr) +
           (cpp_null_pointer != nullptr) + tls_namespace::counter;
}
