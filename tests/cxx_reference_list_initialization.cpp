static int run_reference_list_initialization() {
    int source = 17;
    int& mutable_lvalue{source};
    const int& const_lvalue{source};
    const int& integer_temporary{42};
    const int& converted_integer{42u};
    const int& copy_list_temporary = {45};
    const float& rounded_float{1.1};
    int&& mutable_rvalue{43};
    const int&& const_rvalue{44};

    mutable_lvalue = 18;
    return mutable_lvalue == 18 && const_lvalue == 18 &&
           integer_temporary == 42 && converted_integer == 42 &&
           copy_list_temporary == 45 && rounded_float == 1.1f &&
           mutable_rvalue == 43 &&
           const_rvalue == 44;
}

int main() {
    return run_reference_list_initialization() ? 0 : 1;
}
