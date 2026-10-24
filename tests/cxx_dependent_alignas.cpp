template<unsigned Len, unsigned Align>
struct aligned_buffer {
    struct storage {
        alignas(Align) unsigned char data[Len];
    };
};

static_assert(alignof(aligned_buffer<8, 16>::storage) == 16,
              "dependent alignas is applied after substitution");
