/*
 * RLD - RinOS Linker
 * Links multiple .ro object files into .rin executable
 */

#include "linker.h"
#include "objfile.h"
#include "rin_formats_v3.h"
#include <string.h>

/* Global linker options */
LinkerOpts g_linker_opts;

/* ═══════════════════════════════════════
 * Linker Creation/Destruction
 * ═══════════════════════════════════════ */

Linker* linker_new(void) {
    Linker* ld = rcc_alloc(sizeof(Linker));
    ld->objects = NULL;
    ld->object_count = 0;
    ld->sections = NULL;
    ld->section_count = 0;
    ld->symbols = NULL;
    ld->symbol_count = 0;
    ld->relocs = NULL;
    ld->reloc_count = 0;
    ld->base_addr = 0x10000;  /* Default base address */
    ld->entry_addr = 0;
    return ld;
}

void linker_free(Linker* ld) {
    if (!ld) return;

    /* Free object files */
    for (int i = 0; i < ld->object_count; i++) {
        objfile_free(ld->objects[i]);
    }
    rcc_free(ld->objects);

    /* Free linked sections */
    LinkedSection* s = ld->sections;
    while (s) {
        LinkedSection* next = s->next;
        rcc_free((void*)s->name);
        rcc_free(s->data);
        rcc_free(s);
        s = next;
    }

    /* Free symbols */
    GlobalSymbol* sym = ld->symbols;
    while (sym) {
        GlobalSymbol* next = sym->next;
        rcc_free((void*)sym->name);
        rcc_free(sym);
        sym = next;
    }

    /* Free relocations */
    PendingReloc* rel = ld->relocs;
    while (rel) {
        PendingReloc* next = rel->next;
        rcc_free((void*)rel->symbol);
        rcc_free(rel);
        rel = next;
    }

    rcc_free(ld);
}

/* ═══════════════════════════════════════
 * Object File Loading
 * ═══════════════════════════════════════ */

bool linker_add_object(Linker* ld, const char* filename) {
    ObjectFile* obj = objfile_read(filename);
    if (!obj) {
        fprintf(stderr, "rld: cannot read object file: %s\n", filename);
        return false;
    }

    if (ld->object_count == 0 && !g_linker_opts.arch_explicit) {
        g_linker_opts.arch = obj->arch;
    } else if (obj->arch != g_linker_opts.arch) {
        fprintf(stderr,
                "rld: architecture mismatch: %s is %s, link target is %s\n",
                filename, obj->arch == ARCH_X64 ? "x86_64" : "x86",
                g_linker_opts.arch == ARCH_X64 ? "x86_64" : "x86");
        objfile_free(obj);
        return false;
    }

    /* Expand object array */
    ld->objects = rcc_realloc(ld->objects, sizeof(ObjectFile*) * (ld->object_count + 1));
    ld->objects[ld->object_count++] = obj;

    if (g_linker_opts.verbose) {
        printf("  + %s: %d sections, %d symbols\n",
               filename, obj->section_count, obj->symbol_count);
    }

    return true;
}

bool linker_add_objects(Linker* ld, char** files, int count) {
    for (int i = 0; i < count; i++) {
        if (!linker_add_object(ld, files[i])) {
            return false;
        }
    }
    return true;
}

/* ═══════════════════════════════════════
 * Section Merging
 * ═══════════════════════════════════════ */

static LinkedSection* find_or_create_section(Linker* ld, const char* name,
                                             SectionType type, uint32_t flags) {
    /* Search for existing section */
    for (LinkedSection* s = ld->sections; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }

    /* Create new section */
    LinkedSection* s = rcc_alloc(sizeof(LinkedSection));
    s->name = rcc_strdup(name);
    s->type = type;
    s->flags = flags;
    s->data = rcc_alloc(1024);
    s->size = 0;
    s->capacity = 1024;
    s->align = 1;
    s->vaddr = 0;
    s->next = NULL;

    /* Append to list */
    if (!ld->sections) {
        ld->sections = s;
    } else {
        LinkedSection* last = ld->sections;
        while (last->next) last = last->next;
        last->next = s;
    }
    ld->section_count++;

    return s;
}

static void linked_section_ensure_capacity(LinkedSection* s, uint32_t need) {
    if (s->size + need <= s->capacity) return;

    uint32_t new_cap = s->capacity * 2;
    while (new_cap < s->size + need) {
        new_cap *= 2;
    }
    s->data = rcc_realloc(s->data, new_cap);
    s->capacity = new_cap;
}

static uint32_t linked_section_add_data(LinkedSection* s, const void* data, uint32_t size) {
    linked_section_ensure_capacity(s, size);
    uint32_t offset = s->size;
    memcpy(s->data + s->size, data, size);
    s->size += size;
    return offset;
}

static void linked_section_align(LinkedSection* s, uint32_t align) {
    if (align <= 1) return;
    if (align > s->align) s->align = align;

    while (s->size % align != 0) {
        linked_section_ensure_capacity(s, 1);
        s->data[s->size++] = 0;
    }
}

bool linker_merge_sections(Linker* ld) {
    if (g_linker_opts.verbose) {
        printf("Merging sections...\n");
    }

    /* Track offsets for relocations */
    typedef struct {
        int obj_idx;
        int sect_idx;
        uint32_t offset;  /* Offset in linked section */
    } SectionOffset;

    SectionOffset* offsets = NULL;
    int offset_count = 0;

    /* Process each object file */
    for (int obj_idx = 0; obj_idx < ld->object_count; obj_idx++) {
        ObjectFile* obj = ld->objects[obj_idx];

        int sect_idx = 0;
        for (ObjSection* sect = obj->sections; sect; sect = sect->next, sect_idx++) {
            /* Find or create linked section */
            LinkedSection* linked = find_or_create_section(ld, sect->name,
                                                           sect->type, sect->flags);

            /* Align section */
            linked_section_align(linked, sect->align);

            /* Record offset for relocation adjustment */
            offsets = rcc_realloc(offsets, sizeof(SectionOffset) * (offset_count + 1));
            offsets[offset_count].obj_idx = obj_idx;
            offsets[offset_count].sect_idx = sect_idx;
            offsets[offset_count].offset = linked->size;
            offset_count++;

            /* Copy section data */
            linked_section_add_data(linked, sect->data, sect->size);

            if (g_linker_opts.verbose) {
                printf("    %s:%s -> %s (+%u bytes at %u)\n",
                       obj->filename, sect->name, linked->name,
                       sect->size, offsets[offset_count-1].offset);
            }

            /* Copy relocations with adjusted offsets */
            for (ObjReloc* r = sect->relocs; r; r = r->next) {
                PendingReloc* pr = rcc_alloc(sizeof(PendingReloc));
                pr->offset = offsets[offset_count-1].offset + r->offset;
                pr->symbol = rcc_strdup(r->symbol_name);
                pr->type = r->type;
                pr->addend = r->addend;

                /* Find linked section index */
                int idx = 0;
                for (LinkedSection* ls = ld->sections; ls; ls = ls->next, idx++) {
                    if (ls == linked) break;
                }
                pr->section = idx;
                pr->source = obj->filename;
                pr->next = NULL;

                /* Append to relocation list */
                if (!ld->relocs) {
                    ld->relocs = pr;
                } else {
                    PendingReloc* last = ld->relocs;
                    while (last->next) last = last->next;
                    last->next = pr;
                }
                ld->reloc_count++;
            }
        }
    }

    rcc_free(offsets);

    if (g_linker_opts.verbose) {
        printf("  %d linked sections\n", ld->section_count);
    }

    return true;
}

/* ═══════════════════════════════════════
 * Symbol Collection and Resolution
 * ═══════════════════════════════════════ */

static GlobalSymbol* find_symbol(Linker* ld, const char* name) {
    for (GlobalSymbol* s = ld->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static int linker_dependency_index(const char* name) {
    int index;
    for (index = 0; index < g_linker_opts.dependency_count; index++) {
        if (strcmp(g_linker_opts.dependencies[index], name) == 0) return index;
    }
    return -1;
}

static void add_symbol(Linker* ld, const char* name, uint32_t value, uint32_t size,
                       SymbolType type, SymbolBinding binding, int section,
                       const char* source, bool resolved) {
    GlobalSymbol* sym = rcc_alloc(sizeof(GlobalSymbol));
    sym->name = rcc_strdup(name);
    sym->value = value;
    sym->size = size;
    sym->type = type;
    sym->binding = binding;
    sym->section = section;
    sym->source = source;
    sym->resolved = resolved;
    sym->next = NULL;

    if (!ld->symbols) {
        ld->symbols = sym;
    } else {
        GlobalSymbol* last = ld->symbols;
        while (last->next) last = last->next;
        last->next = sym;
    }
    ld->symbol_count++;
}

bool linker_collect_symbols(Linker* ld) {
    if (g_linker_opts.verbose) {
        printf("Collecting symbols...\n");
    }

    /* Track section offsets per object */
    /* For now, we need to recalculate these */
    uint32_t** sect_offsets = rcc_alloc(sizeof(uint32_t*) * ld->object_count);

    /* Calculate section offsets */
    for (int obj_idx = 0; obj_idx < ld->object_count; obj_idx++) {
        ObjectFile* obj = ld->objects[obj_idx];
        sect_offsets[obj_idx] = rcc_alloc(sizeof(uint32_t) * obj->section_count);

        int sect_idx = 0;
        for (ObjSection* sect = obj->sections; sect; sect = sect->next, sect_idx++) {
            /* Find this section's offset in the linked output */
            LinkedSection* linked = NULL;

            for (LinkedSection* ls = ld->sections; ls; ls = ls->next) {
                if (strcmp(ls->name, sect->name) == 0) {
                    linked = ls;
                    break;
                }
            }

            if (linked) {
                uint32_t offset = 0;
                for (int i = 0; i <= obj_idx; i++) {
                    ObjectFile* prev = ld->objects[i];
                    for (ObjSection* ps = prev->sections; ps; ps = ps->next) {
                        if (strcmp(ps->name, sect->name) == 0) {
                            uint32_t mask = ps->align - 1u;
                            offset = (offset + mask) & ~mask;
                            if (i == obj_idx && ps == sect) {
                                sect_offsets[obj_idx][sect_idx] = offset;
                            }
                            offset += ps->size;
                        }
                    }
                }
            }
        }
    }

    /* Collect symbols from all objects */
    for (int obj_idx = 0; obj_idx < ld->object_count; obj_idx++) {
        ObjectFile* obj = ld->objects[obj_idx];

        for (ObjSymbol* sym = obj->symbols; sym; sym = sym->next) {
            uint32_t value;
            int linked_sect = -1;

            /* Skip undefined symbols on first pass */
            if (sym->type == SYM_UNDEF) continue;

            value = sym->value;
            if (sym->section >= 0) {
                const char* sect_name = NULL;
                int idx = 0;
                value = sect_offsets[obj_idx][sym->section] + sym->value;
                for (ObjSection* s = obj->sections; s; s = s->next, idx++) {
                    if (idx == sym->section) {
                        sect_name = s->name;
                        break;
                    }
                }
                if (sect_name) {
                    idx = 0;
                    for (LinkedSection* ls = ld->sections; ls;
                         ls = ls->next, idx++) {
                        if (strcmp(ls->name, sect_name) == 0) {
                            linked_sect = idx;
                            break;
                        }
                    }
                }
            }

            /* Check for duplicate global symbols */
            GlobalSymbol* existing = find_symbol(ld, sym->name);
            if (existing) {
                if (existing->type == SYM_GLOBAL && sym->type == SYM_GLOBAL) {
                    fprintf(stderr, "rld: multiple definition of '%s'\n", sym->name);
                    fprintf(stderr, "     first defined in %s\n", existing->source);
                    fprintf(stderr, "     also defined in %s\n", obj->filename);
                    return false;
                }
                /* Weak symbols can be overridden */
                if (existing->type == SYM_WEAK && sym->type == SYM_GLOBAL) {
                    existing->value = value;
                    existing->size = sym->size;
                    existing->binding = sym->binding;
                    existing->section = linked_sect;
                    existing->source = obj->filename;
                    existing->type = SYM_GLOBAL;
                    existing->resolved = true;
                }
                continue;
            }

            add_symbol(ld, sym->name, value, sym->size, sym->type, sym->binding,
                      linked_sect, obj->filename, true);

            if (g_linker_opts.verbose) {
                printf("    %s: %s = 0x%x (sect %d)\n",
                       obj->filename, sym->name, value, linked_sect);
            }
        }
    }

    /* Free section offsets */
    for (int i = 0; i < ld->object_count; i++) {
        rcc_free(sect_offsets[i]);
    }
    rcc_free(sect_offsets);

    if (g_linker_opts.verbose) {
        printf("  %d symbols collected\n", ld->symbol_count);
    }

    return true;
}

static bool linker_materialize_import_slots(Linker* ld) {
    LinkedSection* data;
    int data_section = 0;
    int import_index;
    if (g_linker_opts.import_count == 0) return true;
    if (!g_linker_opts.shared) {
        fprintf(stderr, "rld: imports are supported only for shared .rll output\n");
        return false;
    }
    data = find_or_create_section(ld, ".data", SECT_DATA,
                                  SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
    for (LinkedSection* section = ld->sections; section && section != data;
         section = section->next) data_section++;

    for (import_index = 0; import_index < g_linker_opts.import_count;
         import_index++) {
        LinkImportSpec* import = &g_linker_opts.imports[import_index];
        uint64_t zero = 0u;
        uint32_t slot;
        int previous;
        if (linker_dependency_index(import->dependency) < 0) {
            fprintf(stderr, "rld: import %s references undeclared dependency %s\n",
                    import->symbol, import->dependency);
            return false;
        }
        for (previous = 0; previous < import_index; previous++) {
            if (strcmp(g_linker_opts.imports[previous].symbol,
                       import->symbol) == 0) {
                fprintf(stderr, "rld: duplicate import %s\n", import->symbol);
                return false;
            }
        }
        if (find_symbol(ld, import->symbol)) {
            fprintf(stderr, "rld: import %s conflicts with a linked definition\n",
                    import->symbol);
            return false;
        }
        linked_section_align(data, 8u);
        slot = linked_section_add_data(data, &zero, sizeof(zero));
        add_symbol(ld, import->symbol, slot, sizeof(zero), SYM_LOCAL,
                   import->kind == RIN_SYMBOL_FUNCTION ? BIND_CODE : BIND_DATA,
                   data_section, "<import-slot>", true);
    }
    return true;
}

bool linker_resolve_symbols(Linker* ld) {
    if (g_linker_opts.verbose) {
        printf("Resolving symbols...\n");
    }

    int unresolved = 0;

    /* Check undefined symbols */
    for (int obj_idx = 0; obj_idx < ld->object_count; obj_idx++) {
        ObjectFile* obj = ld->objects[obj_idx];

        for (ObjSymbol* sym = obj->symbols; sym; sym = sym->next) {
            if (sym->type != SYM_UNDEF) continue;

            /* Look for definition */
            GlobalSymbol* def = find_symbol(ld, sym->name);
            if (!def || !def->resolved) {
                fprintf(stderr, "rld: undefined reference to '%s'\n", sym->name);
                fprintf(stderr, "     referenced from %s\n", obj->filename);
                unresolved++;
            }
        }
    }

    if (unresolved > 0) {
        fprintf(stderr, "rld: %d unresolved symbol(s)\n", unresolved);
        return false;
    }

    return true;
}

/* ═══════════════════════════════════════
 * Memory Layout
 * ═══════════════════════════════════════ */

bool linker_layout(Linker* ld, uint32_t base_addr) {
    if (g_linker_opts.verbose) {
        printf("Layout at base 0x%x...\n", base_addr);
    }

    ld->base_addr = base_addr;
    uint32_t addr = base_addr;

    /* Standard section order: .text, .rodata, .data, .bss */
    const char* order[] = {".text", ".rodata", ".data", ".bss", NULL};

    for (int i = 0; order[i]; i++) {
        for (LinkedSection* s = ld->sections; s; s = s->next) {
            if (strcmp(s->name, order[i]) != 0) continue;

            /* Align to section alignment */
            while (addr % s->align != 0) addr++;

            s->vaddr = addr;
            addr += s->size;

            if (g_linker_opts.verbose) {
                printf("    %s: 0x%x - 0x%x (%u bytes)\n",
                       s->name, s->vaddr, s->vaddr + s->size, s->size);
            }
        }
    }

    /* Handle any remaining sections */
    for (LinkedSection* s = ld->sections; s; s = s->next) {
        if (s->vaddr != 0) continue;  /* Already placed */

        while (addr % s->align != 0) addr++;
        s->vaddr = addr;
        addr += s->size;

        if (g_linker_opts.verbose) {
            printf("    %s: 0x%x - 0x%x (%u bytes)\n",
                   s->name, s->vaddr, s->vaddr + s->size, s->size);
        }
    }

    /* Update symbol values with final addresses */
    for (GlobalSymbol* sym = ld->symbols; sym; sym = sym->next) {
        if (sym->section < 0) continue;

        /* Find section by index */
        int idx = 0;
        for (LinkedSection* s = ld->sections; s; s = s->next, idx++) {
            if (idx == sym->section) {
                sym->value += s->vaddr;
                break;
            }
        }
    }

    /* Find entry point */
    const char* entry_name = g_linker_opts.entry ? g_linker_opts.entry : "main";
    GlobalSymbol* entry = find_symbol(ld, entry_name);
    if (entry) {
        ld->entry_addr = entry->value;
        if (g_linker_opts.verbose) {
            printf("  Entry point: %s = 0x%x\n", entry_name, ld->entry_addr);
        }
    } else if (!g_linker_opts.shared) {
        fprintf(stderr, "rld: warning: entry point '%s' not found\n", entry_name);
        ld->entry_addr = base_addr;
    }

    return true;
}

/* ═══════════════════════════════════════
 * Relocation Application
 * ═══════════════════════════════════════ */

bool linker_apply_relocations(Linker* ld) {
    if (g_linker_opts.verbose) {
        printf("Applying %d relocations...\n", ld->reloc_count);
    }

    for (PendingReloc* r = ld->relocs; r; r = r->next) {
        /* Find target symbol */
        GlobalSymbol* sym = find_symbol(ld, r->symbol);
        if (!sym) {
            fprintf(stderr, "rld: relocation to undefined symbol '%s'\n", r->symbol);
            return false;
        }

        /* Find section to patch */
        int idx = 0;
        LinkedSection* sect = NULL;
        for (LinkedSection* s = ld->sections; s; s = s->next, idx++) {
            if (idx == r->section) {
                sect = s;
                break;
            }
        }

        uint32_t relocation_width = r->type == RELOC_ABS64 ? 8u :
                                    r->type == RELOC_REL8 ? 1u : 4u;
        if (!sect || r->offset > sect->size ||
            relocation_width > sect->size - r->offset) {
            fprintf(stderr, "rld: invalid relocation offset\n");
            return false;
        }

        uint8_t* patch = sect->data + r->offset;
        uint32_t target = sym->value + r->addend;

        switch (r->type) {
            case RELOC_ABS32: {
                /* 32-bit absolute address */
                *(uint32_t*)patch = target;
                break;
            }
            case RELOC_ABS64: {
                *(uint64_t*)patch = (uint64_t)target;
                break;
            }
            case RELOC_REL32: {
                /* 32-bit PC-relative (relative to next instruction) */
                uint32_t pc = sect->vaddr + r->offset + 4;
                *(int32_t*)patch = (int32_t)(target - pc);
                break;
            }
            case RELOC_REL8: {
                /* 8-bit PC-relative */
                uint32_t pc = sect->vaddr + r->offset + 1;
                int32_t delta = (int32_t)(target - pc);
                if (delta < -128 || delta > 127) {
                    fprintf(stderr, "rld: 8-bit relocation overflow for '%s'\n", r->symbol);
                    return false;
                }
                *(int8_t*)patch = (int8_t)delta;
                break;
            }
            default:
                fprintf(stderr, "rld: unsupported relocation type %d\n", r->type);
                return false;
        }

        if (g_linker_opts.verbose) {
            printf("    %s+0x%x -> %s = 0x%x\n",
                   sect->name, r->offset, r->symbol, target);
        }
    }

    return true;
}

/* ═══════════════════════════════════════
 * Output Generation
 * ═══════════════════════════════════════ */

static uint64_t linker_align_u64(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static int linker_relocation_compare(const void* left, const void* right) {
    const RinRelocationV3* a = (const RinRelocationV3*)left;
    const RinRelocationV3* b = (const RinRelocationV3*)right;
    return a->virtual_address < b->virtual_address ? -1 :
           a->virtual_address > b->virtual_address ? 1 : 0;
}

static int linker_power_of_two(uint32_t value) {
    return value && (value & (value - 1u)) == 0u;
}

static bool linker_emit_image_v3(Linker* ld, const char* filename, bool library) {
    RinHeaderV3 header;
    RinSectionV3* sections;
    RinDependencyV3* dependencies = NULL;
    RinRelocationV3* relocations = NULL;
    RinImportV3* imports = NULL;
    RinExportV3* exports = NULL;
    uint32_t load_section_count = 0u;
    uint32_t absolute_relocation_count = 0u;
    uint32_t export_count = 0u;
    uint32_t dependency_count = (uint32_t)g_linker_opts.dependency_count;
    uint32_t import_count = (uint32_t)g_linker_opts.import_count;
    uint32_t section_count;
    uint32_t section_index = 0u;
    uint32_t relocation_index = 0u;
    uint32_t export_index = 0u;
    uint32_t import_index = 0u;
    uint32_t code_count = 0u;
    uint64_t image_size = 0u;
    uint64_t section_table_offset = sizeof(RinHeaderV3);
    uint64_t dependency_table_offset;
    uint64_t string_table_offset;
    uint64_t cursor;
    uint64_t unsigned_size;
    size_t string_capacity = 1u;
    uint32_t string_size = 1u;
    char* strings;
    uint8_t* output;
    FILE* file;
    LinkedSection* linked;
    PendingReloc* pending;
    GlobalSymbol* symbol;

    for (linked = ld->sections; linked; linked = linked->next) {
        if (linked->type < SECT_CODE || linked->type > SECT_BSS || linked->size == 0u) {
            continue;
        }
        ++load_section_count;
        if (linked->type == SECT_CODE) ++code_count;
        string_capacity += strlen(linked->name) + 1u;
    }
    for (int index = 0; index < g_linker_opts.dependency_count; index++) {
        int prior;
        string_capacity += strlen(g_linker_opts.dependencies[index]) + 1u;
        for (prior = 0; prior < index; prior++) {
            if (strcmp(g_linker_opts.dependencies[prior],
                       g_linker_opts.dependencies[index]) == 0) {
                fprintf(stderr, "rld: duplicate dependency %s\n",
                        g_linker_opts.dependencies[index]);
                return false;
            }
        }
    }
    for (int index = 0; index < g_linker_opts.import_count; index++) {
        string_capacity += strlen(g_linker_opts.imports[index].symbol) + 1u;
    }
    for (pending = ld->relocs; pending; pending = pending->next) {
        if (pending->type == RELOC_ABS32 || pending->type == RELOC_ABS64) {
            ++absolute_relocation_count;
        }
    }
    if (library) {
        for (symbol = ld->symbols; symbol; symbol = symbol->next) {
            if (symbol->resolved &&
                (symbol->type == SYM_GLOBAL || symbol->type == SYM_WEAK) &&
                (symbol->binding == BIND_CODE || symbol->binding == BIND_DATA ||
                 symbol->binding == BIND_BSS)) {
                ++export_count;
                string_capacity += strlen(symbol->name) + 1u;
            }
        }
    }
    if (absolute_relocation_count) string_capacity += sizeof(".reloc");
    if (import_count) string_capacity += sizeof(".imports");
    if (export_count) string_capacity += sizeof(".exports");
    if (code_count != 1u || load_section_count == 0u || string_capacity > UINT32_MAX) {
        fprintf(stderr, "rld: canonical RIN v3 requires exactly one non-empty code section\n");
        return false;
    }
    section_count = load_section_count + (absolute_relocation_count ? 1u : 0u) +
                    (import_count ? 1u : 0u) +
                    (export_count ? 1u : 0u);
    sections = rcc_alloc((size_t)section_count * sizeof(RinSectionV3));
    if (dependency_count) {
        dependencies = rcc_alloc((size_t)dependency_count *
                                 sizeof(RinDependencyV3));
    }
    strings = rcc_alloc(string_capacity);
    strings[0] = '\0';

    for (uint32_t index = 0u; index < dependency_count; index++) {
        size_t name_length = strlen(g_linker_opts.dependencies[index]) + 1u;
        dependencies[index].name_offset = string_size;
        dependencies[index].minimum_abi_major = RIN_IMAGE_ABI_MAJOR;
        dependencies[index].minimum_abi_minor = RIN_IMAGE_ABI_MINOR;
        memcpy(strings + string_size, g_linker_opts.dependencies[index],
               name_length);
        string_size += (uint32_t)name_length;
    }

    for (linked = ld->sections; linked; linked = linked->next) {
        RinSectionV3* section;
        uint64_t rva;
        size_t name_length;
        if (linked->type < SECT_CODE || linked->type > SECT_BSS || linked->size == 0u) {
            continue;
        }
        if (linked->vaddr < ld->base_addr) {
            fprintf(stderr, "rld: section %s precedes the preferred base\n", linked->name);
            rcc_free(dependencies);
            rcc_free(sections);
            rcc_free(strings);
            return false;
        }
        rva = linked->vaddr - ld->base_addr;
        section = &sections[section_index++];
        section->type = linked->type == SECT_CODE ? RIN_IMAGE_SECTION_CODE :
                        linked->type == SECT_RODATA ? RIN_IMAGE_SECTION_RODATA :
                        linked->type == SECT_DATA ? RIN_IMAGE_SECTION_DATA :
                                                   RIN_IMAGE_SECTION_BSS;
        section->flags = linked->type == SECT_CODE
            ? RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_EXECUTE
            : linked->type == SECT_RODATA ? RIN_IMAGE_SECTION_READ
            : RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_WRITE;
        section->alignment = linker_power_of_two(linked->align) ? linked->align : 1u;
        section->virtual_address = rva;
        section->memory_size = linked->size;
        name_length = strlen(linked->name) + 1u;
        section->name_offset = string_size;
        memcpy(strings + string_size, linked->name, name_length);
        string_size += (uint32_t)name_length;
        if (rva + linked->size > image_size) image_size = rva + linked->size;
    }
    if (absolute_relocation_count) {
        RinSectionV3* section = &sections[section_index++];
        size_t length = sizeof(".reloc");
        section->type = RIN_IMAGE_SECTION_RELOCATIONS;
        section->flags = RIN_IMAGE_SECTION_DISCARDABLE;
        section->alignment = 8u;
        section->file_size =
            (uint64_t)absolute_relocation_count * sizeof(RinRelocationV3);
        section->name_offset = string_size;
        memcpy(strings + string_size, ".reloc", length);
        string_size += (uint32_t)length;
        relocations = rcc_alloc((size_t)section->file_size);
        for (pending = ld->relocs; pending; pending = pending->next) {
            LinkedSection* target_section;
            int index = 0;
            uint64_t width;
            if (pending->type != RELOC_ABS32 && pending->type != RELOC_ABS64) continue;
            target_section = ld->sections;
            while (target_section && index++ < pending->section) target_section = target_section->next;
            width = pending->type == RELOC_ABS64 ? 8u : 4u;
            if (!target_section || target_section->vaddr < ld->base_addr ||
                pending->offset > target_section->size ||
                width > target_section->size - pending->offset ||
                (g_linker_opts.arch == ARCH_X86 && width != 4u)) {
                fprintf(stderr, "rld: invalid absolute relocation in canonical output\n");
                rcc_free(dependencies);
                rcc_free(relocations);
                rcc_free(sections);
                rcc_free(strings);
                return false;
            }
            relocations[relocation_index].virtual_address =
                target_section->vaddr - ld->base_addr + pending->offset;
            relocations[relocation_index].type = width == 8u
                ? RIN_IMAGE_RELOCATION_ABS64 : RIN_IMAGE_RELOCATION_ABS32U;
            ++relocation_index;
        }
        qsort(relocations, absolute_relocation_count, sizeof(RinRelocationV3),
              linker_relocation_compare);
        for (relocation_index = 1u; relocation_index < absolute_relocation_count;
             ++relocation_index) {
            uint64_t prior_width = relocations[relocation_index - 1u].type ==
                RIN_IMAGE_RELOCATION_ABS64 ? 8u : 4u;
            if (relocations[relocation_index - 1u].virtual_address + prior_width >
                relocations[relocation_index].virtual_address) {
                fprintf(stderr, "rld: overlapping absolute relocations\n");
                rcc_free(dependencies);
                rcc_free(relocations);
                rcc_free(sections);
                rcc_free(strings);
                return false;
            }
        }
    }
    if (import_count) {
        RinSectionV3* section = &sections[section_index++];
        size_t section_name_length = sizeof(".imports");
        section->type = RIN_IMAGE_SECTION_IMPORTS;
        section->flags = RIN_IMAGE_SECTION_DISCARDABLE;
        section->alignment = 8u;
        section->file_size = (uint64_t)import_count * sizeof(RinImportV3);
        section->name_offset = string_size;
        memcpy(strings + string_size, ".imports", section_name_length);
        string_size += (uint32_t)section_name_length;
        imports = rcc_alloc((size_t)section->file_size);
        for (import_index = 0u; import_index < import_count; import_index++) {
            const LinkImportSpec* spec = &g_linker_opts.imports[import_index];
            GlobalSymbol* slot = find_symbol(ld, spec->symbol);
            int dependency_index = linker_dependency_index(spec->dependency);
            size_t symbol_length = strlen(spec->symbol) + 1u;
            if (!slot || !slot->resolved || dependency_index < 0 ||
                image_size < 8u || slot->value < ld->base_addr ||
                (uint64_t)slot->value - ld->base_addr > image_size - 8u) {
                fprintf(stderr, "rld: invalid import slot for %s\n", spec->symbol);
                rcc_free(imports); rcc_free(dependencies); rcc_free(relocations);
                rcc_free(sections); rcc_free(strings);
                return false;
            }
            imports[import_index].name_offset = string_size;
            imports[import_index].dependency_index = (uint32_t)dependency_index;
            imports[import_index].target_rva = slot->value - ld->base_addr;
            imports[import_index].kind = spec->kind;
            memcpy(strings + string_size, spec->symbol, symbol_length);
            string_size += (uint32_t)symbol_length;
        }
    }
    if (export_count) {
        RinSectionV3* section = &sections[section_index++];
        size_t length = sizeof(".exports");
        section->type = RIN_IMAGE_SECTION_EXPORTS;
        section->flags = RIN_IMAGE_SECTION_DISCARDABLE;
        section->alignment = 8u;
        section->file_size = (uint64_t)export_count * sizeof(RinExportV3);
        section->name_offset = string_size;
        memcpy(strings + string_size, ".exports", length);
        string_size += (uint32_t)length;
        exports = rcc_alloc((size_t)section->file_size);
        for (symbol = ld->symbols; symbol; symbol = symbol->next) {
            size_t length;
            if (!symbol->resolved ||
                (symbol->type != SYM_GLOBAL && symbol->type != SYM_WEAK) ||
                (symbol->binding != BIND_CODE && symbol->binding != BIND_DATA &&
                 symbol->binding != BIND_BSS)) continue;
            if (symbol->value < ld->base_addr ||
                (uint64_t)symbol->value - ld->base_addr >= image_size) {
                fprintf(stderr, "rld: export %s lies outside the image\n", symbol->name);
                rcc_free(exports); rcc_free(imports); rcc_free(dependencies);
                rcc_free(relocations); rcc_free(sections); rcc_free(strings);
                return false;
            }
            length = strlen(symbol->name) + 1u;
            exports[export_index].name_offset = string_size;
            memcpy(strings + string_size, symbol->name, length);
            string_size += (uint32_t)length;
            exports[export_index].kind = symbol->binding == BIND_CODE
                ? RIN_SYMBOL_FUNCTION : RIN_SYMBOL_DATA;
            exports[export_index].virtual_address = symbol->value - ld->base_addr;
            exports[export_index].size = symbol->size;
            ++export_index;
        }
    }

    dependency_table_offset = section_table_offset +
        (uint64_t)section_count * sizeof(RinSectionV3);
    string_table_offset = dependency_table_offset +
        (uint64_t)dependency_count * sizeof(RinDependencyV3);
    cursor = linker_align_u64(string_table_offset + string_size, 16u);
    section_index = 0u;
    for (linked = ld->sections; linked; linked = linked->next) {
        RinSectionV3* section;
        if (linked->type < SECT_CODE || linked->type > SECT_BSS || linked->size == 0u) continue;
        section = &sections[section_index++];
        if (linked->type == SECT_BSS) continue;
        cursor = linker_align_u64(cursor, section->alignment);
        section->file_offset = cursor;
        section->file_size = linked->size;
        cursor += linked->size;
    }
    if (absolute_relocation_count) {
        RinSectionV3* section = &sections[section_index++];
        cursor = linker_align_u64(cursor, section->alignment);
        section->file_offset = cursor;
        cursor += section->file_size;
    }
    if (import_count) {
        RinSectionV3* section = &sections[section_index++];
        cursor = linker_align_u64(cursor, section->alignment);
        section->file_offset = cursor;
        cursor += section->file_size;
    }
    if (export_count) {
        RinSectionV3* section = &sections[section_index++];
        cursor = linker_align_u64(cursor, section->alignment);
        section->file_offset = cursor;
        cursor += section->file_size;
    }
    unsigned_size = cursor;
    image_size = linker_align_u64(image_size, 4096u);
    if (unsigned_size > SIZE_MAX || image_size == 0u ||
        (g_linker_opts.arch == ARCH_X86 &&
         ((uint64_t)ld->base_addr + image_size >= UINT64_C(0xC0000000)))) {
        fprintf(stderr, "rld: canonical image exceeds target limits\n");
        rcc_free(exports); rcc_free(imports); rcc_free(dependencies);
        rcc_free(relocations); rcc_free(sections); rcc_free(strings);
        return false;
    }

    memset(&header, 0, sizeof(header));
    header.magic = RIN_IMAGE_MAGIC;
    header.version = RIN_IMAGE_VERSION_3;
    header.header_size = sizeof(RinHeaderV3);
    header.architecture = g_linker_opts.arch == ARCH_X64
        ? RIN_ARCH_X86_64 : RIN_ARCH_X86;
    header.abi_major = RIN_IMAGE_ABI_MAJOR;
    header.abi_minor = RIN_IMAGE_ABI_MINOR;
    header.flags = (library ? RIN_IMAGE_LIBRARY : RIN_IMAGE_EXECUTABLE | RIN_IMAGE_GUI) |
                   RIN_IMAGE_RELOCATABLE | RIN_IMAGE_ASLR;
    header.section_count = section_count;
    header.dependency_count = dependency_count;
    header.entry_rva = library ? 0u : ld->entry_addr - ld->base_addr;
    header.preferred_base = ld->base_addr;
    header.image_size = image_size;
    header.section_table_offset = section_table_offset;
    header.dependency_table_offset = dependency_table_offset;
    header.string_table_offset = string_table_offset;
    header.string_table_size = string_size;

    output = rcc_alloc((size_t)unsigned_size);
    memcpy(output, &header, sizeof(header));
    memcpy(output + section_table_offset, sections,
           (size_t)section_count * sizeof(RinSectionV3));
    if (dependency_count) {
        memcpy(output + dependency_table_offset, dependencies,
               (size_t)dependency_count * sizeof(RinDependencyV3));
    }
    memcpy(output + string_table_offset, strings, string_size);
    section_index = 0u;
    for (linked = ld->sections; linked; linked = linked->next) {
        RinSectionV3* section;
        if (linked->type < SECT_CODE || linked->type > SECT_BSS || linked->size == 0u) continue;
        section = &sections[section_index++];
        if (linked->type != SECT_BSS) memcpy(output + section->file_offset, linked->data, linked->size);
    }
    if (absolute_relocation_count) {
        memcpy(output + sections[section_index++].file_offset, relocations,
               (size_t)absolute_relocation_count * sizeof(RinRelocationV3));
    }
    if (import_count) {
        memcpy(output + sections[section_index++].file_offset, imports,
               (size_t)import_count * sizeof(RinImportV3));
    }
    if (export_count) {
        memcpy(output + sections[section_index++].file_offset, exports,
               (size_t)export_count * sizeof(RinExportV3));
    }

    file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "rld: cannot create output file: %s\n", filename);
        rcc_free(output); rcc_free(exports); rcc_free(imports);
        rcc_free(dependencies); rcc_free(relocations);
        rcc_free(sections); rcc_free(strings);
        return false;
    }
    size_t written = fwrite(output, 1u, (size_t)unsigned_size, file);
    int close_result = fclose(file);
    rcc_free(output); rcc_free(exports); rcc_free(imports);
    rcc_free(dependencies); rcc_free(relocations);
    rcc_free(sections); rcc_free(strings);
    if (written != (size_t)unsigned_size || close_result != 0) {
        fprintf(stderr, "rld: cannot write output file: %s\n", filename);
        return false;
    }
    if (g_linker_opts.verbose) {
        printf("Unsigned RIN v3 %s stage: %s (%u sections)\n",
               library ? "library" : "image", filename, section_count);
    }
    return true;
}

bool linker_emit_rin(Linker* ld, const char* filename) {
    return linker_emit_image_v3(ld, filename, false);
}

/* ═══════════════════════════════════════
 * High-level Link Function
 * ═══════════════════════════════════════ */

bool rld_link(char** input_files, int count, const char* output) {
    Linker* ld = linker_new();

    /* Load all object files */
    if (g_linker_opts.verbose) {
        printf("Loading %d object files...\n", count);
    }
    if (!linker_add_objects(ld, input_files, count)) {
        linker_free(ld);
        return false;
    }

    /* Merge sections */
    if (!linker_merge_sections(ld)) {
        linker_free(ld);
        return false;
    }

    /* Collect symbols */
    if (!linker_collect_symbols(ld)) {
        linker_free(ld);
        return false;
    }

    if (!linker_materialize_import_slots(ld)) {
        linker_free(ld);
        return false;
    }

    /* Resolve symbols */
    if (!linker_resolve_symbols(ld)) {
        linker_free(ld);
        return false;
    }

    /* Layout */
    if (!linker_layout(ld, g_linker_opts.base_addr)) {
        linker_free(ld);
        return false;
    }

    /* Apply relocations */
    if (!linker_apply_relocations(ld)) {
        linker_free(ld);
        return false;
    }

    /* Emit output */
    bool ok;
    if (g_linker_opts.shared) {
        ok = linker_emit_rll(ld, output);
    } else {
        ok = linker_emit_rin(ld, output);
    }

    linker_free(ld);
    return ok;
}

bool linker_emit_rll(Linker* ld, const char* filename) {
    return linker_emit_image_v3(ld, filename, true);
}
