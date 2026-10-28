#if __has_embed("c23_embed_resource.bin" limit())
int invalid_preprocessor_has_embed(void) {
    return 0;
}
#endif
