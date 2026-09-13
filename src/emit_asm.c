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

static const ModuleReloc* emit_asm_relocation(const Module* mod,
                                              ModuleSymbolSection section,
                                              uint32_t offset) {
    if (!mod) return NULL;
    for (int index = 0; index < mod->reloc_count; ++index) {
        const ModuleReloc* relocation = &mod->relocs_arr[index];
        if (relocation->source_section == section &&
            relocation->offset == offset) {
            return relocation;
        }
    }
    return NULL;
}

static bool emit_asm_reloc(FILE* file, const ModuleReloc* relocation) {
    uint32_t width;
    if (!file || !relocation || !relocation->symbol_name) return false;
    width = relocation->is_relative || !relocation->is_64bit ? 4u : 8u;
    if (relocation->is_tls) {
        return fprintf(file, ".long %s\n", relocation->symbol_name) >= 0;
    }
    if (relocation->is_relative) {
        return fprintf(file, ".long %s + %u - . - 4\n",
                       relocation->symbol_name, relocation->target) >= 0;
    }
    if (width == 8u) {
        if (relocation->target) {
            return fprintf(file, ".quad %s + %u\n",
                           relocation->symbol_name, relocation->target) >= 0;
        }
        return fprintf(file, ".quad %s\n", relocation->symbol_name) >= 0;
    }
    if (relocation->target) {
        return fprintf(file, ".long %s + %u\n",
                       relocation->symbol_name, relocation->target) >= 0;
    }
    return fprintf(file, ".long %s\n", relocation->symbol_name) >= 0;
}

static bool emit_asm_symbols(FILE* file, const Module* mod,
                             ModuleSymbolSection section,
                             uint32_t offset);

static bool emit_asm_reloc_bytes(FILE* file, const Module* mod,
                                 ModuleSymbolSection section,
                                 const uint8_t* data, size_t size,
                                 uint32_t base_offset) {
    size_t offset = 0u;
    while (offset < size) {
        uint64_t absolute = (uint64_t)base_offset + offset;
        if (section != MODULE_SYMBOL_CODE && absolute <= UINT32_MAX &&
            !emit_asm_symbols(file, mod, section, (uint32_t)absolute)) {
            return false;
        }
        const ModuleReloc* relocation = absolute <= UINT32_MAX
            ? emit_asm_relocation(mod, section, (uint32_t)absolute) : NULL;
        if (relocation) {
            uint32_t width = relocation->is_relative || !relocation->is_64bit
                ? 4u : 8u;
            if (width > size - offset || !emit_asm_reloc(file, relocation)) {
                return false;
            }
            offset += width;
            continue;
        }
        size_t next = size;
        for (int index = 0; index < mod->reloc_count; ++index) {
            const ModuleReloc* candidate = &mod->relocs_arr[index];
            if (candidate->source_section == section &&
                candidate->offset > absolute &&
                candidate->offset < (uint64_t)base_offset + next) {
                next = (size_t)(candidate->offset - base_offset);
            }
        }
        if (!emit_asm_bytes(file, data + offset, next - offset)) return false;
        offset = next;
    }
    return true;
}

static bool emit_asm_symbols(FILE* file, const Module* mod,
                             ModuleSymbolSection section,
                             uint32_t offset) {
    for (int index = 0; index < mod->symbol_count; ++index) {
        const ModuleSymbol* symbol = &mod->symbols[index];
        if (!symbol->is_defined || symbol->section != section ||
            symbol->offset != offset || !symbol->name) continue;
        if (symbol->is_global && fprintf(file, ".globl %s\n", symbol->name) < 0) {
            return false;
        }
        if (fprintf(file, "%s:\n", symbol->name) < 0) return false;
    }
    return true;
}

static bool emit_asm_code(FILE* file, Module* mod) {
    uint32_t offset = 0u;
    uint32_t entry = mod->entry_point;

    while (offset < mod->code.size) {
        uint32_t next = (uint32_t)mod->code.size;
        bool label_written = false;

        if (offset == entry) {
            if (fprintf(file, ".globl _rcc_entry\n_rcc_entry:\n") < 0) {
                return false;
            }
            label_written = true;
        }
        for (int index = 0; index < mod->symbol_count; ++index) {
            ModuleSymbol* symbol = &mod->symbols[index];
            if (!symbol->is_defined || symbol->section != MODULE_SYMBOL_CODE ||
                symbol->offset != offset || !symbol->name ||
                strcmp(symbol->name, "_rcc_entry") == 0) {
                continue;
            }
            if (fprintf(file, ".globl %s\n%s:\n", symbol->name,
                        symbol->name) < 0) {
                return false;
            }
            label_written = true;
        }

        for (int index = 0; index < mod->symbol_count; ++index) {
            ModuleSymbol* symbol = &mod->symbols[index];
            if (symbol->is_defined && symbol->section == MODULE_SYMBOL_CODE &&
                symbol->offset > offset && symbol->offset < next) {
                next = symbol->offset;
            }
        }
        if (next == offset) next = offset + 1u;
        if (!emit_asm_reloc_bytes(file, mod, MODULE_SYMBOL_CODE,
                                  mod->code.data + offset,
                                  (size_t)(next - offset), offset)) {
            return false;
        }
        (void)label_written;
        offset = next;
    }

    if (mod->code.size == 0u) {
        if (fprintf(file, ".globl _rcc_entry\n_rcc_entry:\n") < 0) {
            return false;
        }
    } else if (entry == mod->code.size) {
        if (fprintf(file, ".globl _rcc_entry\n_rcc_entry:\n") < 0) {
            return false;
        }
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
    ok = fprintf(file, ".text\n.p2align 4\n") >= 0 &&
         emit_asm_code(file, mod);
    if (ok && mod->rodata.size > 0u) {
        ok = fprintf(file, ".section .rodata\n.p2align 4\n") >= 0 &&
             emit_asm_reloc_bytes(file, mod, MODULE_SYMBOL_RODATA,
                                  mod->rodata.data, mod->rodata.size, 0u);
    }
    if (ok && mod->data.size > 0u) {
        ok = fprintf(file, ".section .data\n.p2align 4\n") >= 0 &&
             emit_asm_reloc_bytes(file, mod, MODULE_SYMBOL_DATA,
                                  mod->data.data, mod->data.size, 0u);
    }
    if (fclose(file) != 0) ok = false;
    if (!ok) rcc_error((SourceLoc){outfile, 0, 0}, "cannot write assembly output");
    return ok;
}
