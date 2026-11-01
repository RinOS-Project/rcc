struct SharedInlineStaticData {
    inline static int value = 73;
};

#if defined(TEST_INLINE_STATIC_DATA_MAIN)
extern "C" int* inline_static_data_address_a();
extern "C" int* inline_static_data_address_b();

int main() {
    int* first = inline_static_data_address_a();
    int* second = inline_static_data_address_b();
    return first == second && *first == 73 && *second == 73 ? 0 : 1;
}
#elif defined(TEST_INLINE_STATIC_DATA_B)
extern "C" int* inline_static_data_address_b() {
    return &SharedInlineStaticData::value;
}
#else
extern "C" int* inline_static_data_address_a() {
    return &SharedInlineStaticData::value;
}
#endif
