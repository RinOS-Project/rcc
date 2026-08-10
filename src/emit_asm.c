/* Portable textual assembly output for the RCC -S interface. */

#include "rcc.h"
#include "codegen.h"

static bool emit_asm_bytes(FILE* file, const uint8_t* bytes, size_t size) {
    size_t offset;
    for (offset = 0u; offset < size; offset += 12u) {
        size_t count = size - offset < 12u ? size - offset : 12u;
        size_t index;
        if (fputs("    .byte ", file) == EOF) return false;
        for (index = 0u; index < count; ++index) {
            if (fprintf(file, "%s0x%02x", index ? ", " : "",
                        bytes[offset + index]) < 0) return false;
        }
        if (fputc('\n', file) == EOF) return false;
    }
    return true;
}

bool rcc_emit_asm(Module* mod, const char* outfile) {
    FILE* file;
    bool ok;
    if (!mod || !outfile) return false;
    file = fopen(outfile, "w");
    if (!file) {
        rcc_error((SourceLoc){outfile, 0, 0}, "cannot create assembly output");
        return false;
    }
    ok = fprintf(file, ".text\n.p2align 4\n.globl _rcc_entry\n_rcc_entry:\n") >= 0 &&
         emit_asm_bytes(file, mod->code.data, mod->code.size);
    if (ok && mod->data.size > 0u) {
        ok = fprintf(file, ".section .data\n.p2align 4\n_rcc_data:\n") >= 0 &&
             emit_asm_bytes(file, mod->data.data, mod->data.size);
    }
    if (fclose(file) != 0) ok = false;
    if (!ok) rcc_error((SourceLoc){outfile, 0, 0}, "cannot write assembly output");
    return ok;
}
