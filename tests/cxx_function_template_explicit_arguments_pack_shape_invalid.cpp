template<typename... Types>
int mixed_parameter_pack_shape(int value) {
    return value + 10;
}

template<int Base, typename... Types>
int mixed_parameter_pack_shape(int value) {
    return value + Base + 20;
}

int main() {
    return mixed_parameter_pack_shape<7, short, 9>(1);
}
