int missing_from_unused(void);

/* A correct archive link never extracts this conflicting member. */
int main(void) {
    return missing_from_unused();
}
