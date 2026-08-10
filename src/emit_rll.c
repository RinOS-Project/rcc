/* RCC canonical unsigned RIN v3 library emitter. */

#include "rcc.h"
#include "ast.h"
#include "codegen.h"
#include "rin_formats_v3.h"
#include <stdio.h>

bool rcc_emit_rll(Module* mod, AST* ast, const char* outfile) {
    RinHeaderV3 header;
    FILE* file;
    (void)ast;

    if (mod && mod->tls.size > 0u) {
        rcc_error((SourceLoc){outfile, 0, 0},
                  "TLS-bearing .rll output requires graph TLS layout support");
        return false;
    }

    /* The common writer produces the canonical section/relocation layout. */
    if (!rcc_emit(mod, outfile)) return false;
    file = fopen(outfile, "r+b");
    if (!file || fread(&header, 1u, sizeof(header), file) != sizeof(header) ||
        header.magic != RIN_IMAGE_MAGIC || header.version != RIN_IMAGE_VERSION_3) {
        if (file) fclose(file);
        rcc_error((SourceLoc){outfile, 0, 0}, "cannot reopen RIN v3 library stage");
        return false;
    }
    header.flags &= ~(RIN_IMAGE_EXECUTABLE | RIN_IMAGE_GUI | RIN_IMAGE_SERVICE);
    header.flags |= RIN_IMAGE_LIBRARY;
    header.entry_rva = 0u;
    if (fseek(file, 0, SEEK_SET) != 0 ||
        fwrite(&header, 1u, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        rcc_error((SourceLoc){outfile, 0, 0}, "cannot update RIN v3 library header");
        return false;
    }
    if (fclose(file) != 0) {
        rcc_error((SourceLoc){outfile, 0, 0}, "cannot finalize RIN v3 library stage");
        return false;
    }
    if (g_opts.verbose) printf("Unsigned RIN v3 library stage: %s\n", outfile);
    return true;
}
