bool invalid_copy_initialization = nullptr;
bool invalid_copy_list_initialization = {nullptr};

int consume_nullptr_bool(bool value);

int main() {
    return invalid_copy_initialization || invalid_copy_list_initialization ||
           consume_nullptr_bool(nullptr);
}
