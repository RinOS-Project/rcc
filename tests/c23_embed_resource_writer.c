#include <stdio.h>

int main(int argc, char** argv) {
    static const unsigned char bytes[] = {0, 65, 255};
    FILE* output;
    if (argc != 2) return 2;
    output = fopen(argv[1], "wb");
    if (!output) return 3;
    if (fwrite(bytes, sizeof(bytes), 1, output) != 1) {
        fclose(output);
        return 4;
    }
    if (fclose(output) != 0) return 5;
    return 0;
}
