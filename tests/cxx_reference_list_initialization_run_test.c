int rcc_generated_main(void);

int main(void) {
    /* The host CRT runs the emitted .init_array before entering main. */
    return rcc_generated_main();
}
