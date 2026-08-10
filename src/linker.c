/*
 * RLD - RinOS Linker
 * Links multiple .ro object files into .rin executable
 */

#include "linker.h"
#include "archive.h"
#include "objfile.h"
#include "rin_formats_v3.h"
#include <inttypes.h>
#include <limits.h>
#include <string.h>

/* Global linker options */
LinkerOpts g_linker_opts;

static bool linker_section_selected(const ObjSection* section);
static ObjSection* object_section_at(const ObjectFile* object, int index);

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

static bool linker_append_object(Linker* ld, ObjectFile* obj) {
    if (ld->object_count == 0 && !g_linker_opts.arch_explicit) {
        g_linker_opts.arch = obj->arch;
    } else if (obj->arch != g_linker_opts.arch) {
        fprintf(stderr,
                "rld: architecture mismatch: %s is %s, link target is %s\n",
                obj->filename, obj->arch == ARCH_X64 ? "x86_64" : "x86",
                g_linker_opts.arch == ARCH_X64 ? "x86_64" : "x86");
        objfile_free(obj);
        return false;
    }

    for (ObjSection* section = obj->sections; section;
         section = section->next) {
        if ((section->flags & SECT_FLAG_COMDAT) == 0u) continue;
        for (int previous = 0; previous < ld->object_count; previous++) {
            for (ObjSection* candidate = ld->objects[previous]->sections;
                 candidate; candidate = candidate->next) {
                if ((candidate->flags & SECT_FLAG_COMDAT) != 0u &&
                    strcmp(candidate->comdat_key, section->comdat_key) == 0) {
                    section->comdat_selected = false;
                    break;
                }
            }
            if (!section->comdat_selected) break;
        }
    }

    /* Expand object array */
    ld->objects = rcc_realloc(ld->objects, sizeof(ObjectFile*) * (ld->object_count + 1));
    ld->objects[ld->object_count++] = obj;

    if (g_linker_opts.verbose) {
        printf("  + %s: %d sections, %d symbols\n",
               obj->filename, obj->section_count, obj->symbol_count);
    }

    return true;
}

bool linker_add_object(Linker* ld, const char* filename) {
    ObjectFile* obj = objfile_read(filename);
    if (!obj) {
        fprintf(stderr, "rld: cannot read object file: %s\n", filename);
        return false;
    }
    return linker_append_object(ld, obj);
}

static bool linker_has_definition(const Linker* ld, const char* name) {
    for (int index = 0; index < ld->object_count; ++index) {
        for (ObjSymbol* symbol = ld->objects[index]->symbols; symbol;
             symbol = symbol->next) {
            if (strcmp(symbol->name, name) == 0 &&
                (symbol->type == SYM_GLOBAL || symbol->type == SYM_WEAK) &&
                (symbol->section >= 0 || symbol->binding == BIND_ABS)) {
                ObjSection* owner = symbol->section < 0 ? NULL :
                    object_section_at(ld->objects[index], symbol->section);
                if (owner && !linker_section_selected(owner)) {
                    continue;
                }
                return true;
            }
        }
    }
    return false;
}

static bool linker_symbol_is_unresolved(const Linker* ld, const char* name) {
    bool referenced = !g_linker_opts.shared && g_linker_opts.entry &&
                      strcmp(g_linker_opts.entry, name) == 0;
    if (linker_has_definition(ld, name)) return false;
    for (int index = 0; index < ld->object_count; ++index) {
        for (ObjSymbol* symbol = ld->objects[index]->symbols; symbol;
             symbol = symbol->next) {
            if (symbol->type == SYM_UNDEF &&
                strcmp(symbol->name, name) == 0) return true;
        }
    }
    return referenced;
}

static bool object_defines_symbol(const ObjectFile* obj, const char* name) {
    for (ObjSymbol* symbol = obj->symbols; symbol; symbol = symbol->next) {
        if (strcmp(symbol->name, name) == 0 &&
            (symbol->type == SYM_GLOBAL || symbol->type == SYM_WEAK) &&
            (symbol->section >= 0 || symbol->binding == BIND_ABS)) return true;
    }
    return false;
}

static ArchiveMember* archive_member_at(Archive* archive, int member_index) {
    ArchiveMember* member = archive->members;
    while (member && member_index-- > 0) member = member->next;
    return member;
}

static const char* archive_needed_symbol(const Linker* ld,
                                         const Archive* archive,
                                         int member_index) {
    for (ArchiveSymbol* symbol = archive->symbols; symbol;
         symbol = symbol->next) {
        if (symbol->member_idx == member_index &&
            linker_symbol_is_unresolved(ld, symbol->name)) return symbol->name;
    }
    return NULL;
}

static bool linker_add_archive(Linker* ld, const char* filename) {
    Archive* archive = archive_read(filename);
    bool* selected = NULL;
    bool ok = false;
    if (!archive) {
        fprintf(stderr, "rld: cannot read archive: %s\n", filename);
        return false;
    }
    if (archive->member_count != 0) {
        selected = rcc_alloc(sizeof(bool) * (size_t)archive->member_count);
    }

    for (;;) {
        int candidate = -1;
        const char* needed = NULL;
        for (int index = 0; index < archive->member_count; ++index) {
            if (selected[index]) continue;
            needed = archive_needed_symbol(ld, archive, index);
            if (needed) {
                candidate = index;
                break;
            }
        }
        if (candidate < 0) break;

        ArchiveMember* member = archive_member_at(archive, candidate);
        size_t filename_size;
        size_t member_size;
        size_t display_size;
        char* display_name;
        ObjectFile* obj;
        if (!member) {
            fprintf(stderr, "rld: invalid archive member index in %s\n", filename);
            goto done;
        }
        filename_size = strlen(filename);
        member_size = strlen(member->name);
        if (member_size > SIZE_MAX - 3u ||
            filename_size > SIZE_MAX - member_size - 3u) {
            fprintf(stderr, "rld: archive member name is too long: %s\n", filename);
            goto done;
        }
        display_size = filename_size + member_size + 3u;
        display_name = rcc_alloc(display_size);
        snprintf(display_name, display_size, "%s(%s)", filename, member->name);
        obj = objfile_read_memory(member->data, member->size, display_name);
        rcc_free(display_name);
        if (!obj) {
            fprintf(stderr, "rld: invalid object member %s(%s)\n",
                    filename, member->name);
            goto done;
        }
        if (!object_defines_symbol(obj, needed)) {
            fprintf(stderr,
                    "rld: archive symbol '%s' is not defined by %s(%s)\n",
                    needed, filename, member->name);
            objfile_free(obj);
            goto done;
        }
        selected[candidate] = true;
        if (g_linker_opts.verbose) {
            printf("  archive %s: selecting %s for %s\n",
                   filename, member->name, needed);
        }
        if (!linker_append_object(ld, obj)) goto done;
    }

    ok = true;
done:
    rcc_free(selected);
    archive_free(archive);
    return ok;
}

static bool linker_input_magic(const char* filename, uint32_t* magic) {
    FILE* file = fopen(filename, "rb");
    uint8_t bytes[sizeof(uint32_t)];
    bool read_ok;
    bool close_ok;
    if (!file) return false;
    read_ok = fread(bytes, sizeof(bytes), 1, file) == 1;
    close_ok = fclose(file) == 0;
    if (!read_ok || !close_ok) return false;
    memcpy(magic, bytes, sizeof(*magic));
    return true;
}

bool linker_add_objects(Linker* ld, char** files, int count) {
    for (int i = 0; i < count; i++) {
        uint32_t magic;
        if (!linker_input_magic(files[i], &magic)) {
            fprintf(stderr, "rld: cannot read input file: %s\n", files[i]);
            return false;
        }
        if (magic == RO_MAGIC) {
            if (!linker_add_object(ld, files[i])) return false;
        } else if (magic == RA_MAGIC) {
            if (!linker_add_archive(ld, files[i])) return false;
        } else {
            fprintf(stderr, "rld: unsupported input format: %s\n", files[i]);
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
            if (s->type != type || s->flags != flags) {
                fprintf(stderr,
                        "rld: section '%s' has conflicting type or flags\n",
                        name);
                return NULL;
            }
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
    s->memory_size = 0;
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

static void linked_section_ensure_capacity(LinkedSection* s,
                                           uint64_t required) {
    uint64_t new_cap;
    if (required <= s->capacity) return;
    if (required > SIZE_MAX) rcc_fatal("linked section exceeds host memory limit");
    new_cap = s->capacity;
    while (new_cap < required) {
        if (new_cap > UINT64_MAX / 2u) {
            new_cap = required;
            break;
        }
        new_cap *= 2u;
    }
    s->data = rcc_realloc(s->data, (size_t)new_cap);
    s->capacity = new_cap;
}

static uint64_t linked_section_add_data(LinkedSection* s, const void* data,
                                        uint64_t size) {
    uint64_t offset = s->memory_size;
    uint64_t required;
    if (size > UINT64_MAX - offset) rcc_fatal("linked section size overflow");
    required = offset + size;
    linked_section_ensure_capacity(s, required);
    if (s->size < offset) {
        memset(s->data + (size_t)s->size, 0, (size_t)(offset - s->size));
    }
    memcpy(s->data + (size_t)offset, data, (size_t)size);
    s->size = required;
    s->memory_size = required;
    return offset;
}

static void linked_section_align(LinkedSection* s, uint32_t align) {
    if (align <= 1) return;
    if (align > s->align) s->align = align;

    uint64_t mask = (uint64_t)align - 1u;
    if (s->memory_size > UINT64_MAX - mask) {
        rcc_fatal("linked section alignment overflow");
    }
    s->memory_size = (s->memory_size + mask) & ~mask;
}

static uint64_t linked_section_add_object_section(LinkedSection* linked,
                                                  const ObjSection* input) {
    uint64_t offset;
    uint64_t file_end;
    uint64_t memory_end;
    linked_section_align(linked, input->align);
    offset = linked->memory_size;
    if (input->size > UINT64_MAX - offset ||
        input->memory_size > UINT64_MAX - offset) {
        rcc_fatal("linked section size overflow");
    }
    file_end = offset + input->size;
    memory_end = offset + input->memory_size;
    if (input->size != 0u) {
        linked_section_ensure_capacity(linked, file_end);
        if (linked->size < offset) {
            memset(linked->data + (size_t)linked->size, 0,
                   (size_t)(offset - linked->size));
        }
        memcpy(linked->data + (size_t)offset, input->data,
               (size_t)input->size);
        linked->size = file_end;
    }
    linked->memory_size = memory_end;
    return offset;
}

/* COMDAT ANY keeps the first input object that contributes a group key.  All
 * sections with that key in the winning object are retained as one group. */
static bool linker_section_selected(const ObjSection* section) {
    return (section->flags & SECT_FLAG_COMDAT) == 0u ||
           section->comdat_selected;
}

static ObjSection* object_section_at(const ObjectFile* object, int index) {
    ObjSection* section = object->sections;
    while (section && index-- > 0) section = section->next;
    return section;
}

bool linker_merge_sections(Linker* ld) {
    if (g_linker_opts.verbose) {
        printf("Merging sections...\n");
    }

    /* Track offsets for relocations */
    typedef struct {
        int obj_idx;
        int sect_idx;
        uint64_t offset;  /* Offset in linked section */
    } SectionOffset;

    SectionOffset* offsets = NULL;
    int offset_count = 0;

    /* Process each object file */
    for (int obj_idx = 0; obj_idx < ld->object_count; obj_idx++) {
        ObjectFile* obj = ld->objects[obj_idx];

        int sect_idx = 0;
        for (ObjSection* sect = obj->sections; sect; sect = sect->next, sect_idx++) {
            uint32_t output_flags;
            if (!linker_section_selected(sect)) {
                if (g_linker_opts.verbose) {
                    printf("    %s:%s discarded (COMDAT %s)\n",
                           obj->filename, sect->name, sect->comdat_key);
                }
                continue;
            }
            output_flags = sect->flags & ~SECT_FLAG_COMDAT;
            /* Find or create linked section */
            LinkedSection* linked = find_or_create_section(ld, sect->name,
                                                           sect->type,
                                                           output_flags);
            if (!linked) {
                rcc_free(offsets);
                return false;
            }

            /* Record offset for relocation adjustment */
            offsets = rcc_realloc(offsets, sizeof(SectionOffset) * (offset_count + 1));
            offsets[offset_count].obj_idx = obj_idx;
            offsets[offset_count].sect_idx = sect_idx;
            offsets[offset_count].offset =
                linked_section_add_object_section(linked, sect);
            offset_count++;

            if (g_linker_opts.verbose) {
                printf("    %s:%s -> %s (+%" PRIu64 " bytes at %" PRIu64 ")\n",
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

static void add_symbol(Linker* ld, const char* name, uint64_t value, uint64_t size,
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
    uint64_t** sect_offsets = rcc_alloc(sizeof(uint64_t*) * ld->object_count);

    /* Calculate section offsets */
    for (int obj_idx = 0; obj_idx < ld->object_count; obj_idx++) {
        ObjectFile* obj = ld->objects[obj_idx];
        sect_offsets[obj_idx] = rcc_alloc(sizeof(uint64_t) * obj->section_count);

        int sect_idx = 0;
        for (ObjSection* sect = obj->sections; sect; sect = sect->next, sect_idx++) {
            if (!linker_section_selected(sect)) continue;
            /* Find this section's offset in the linked output */
            LinkedSection* linked = NULL;

            for (LinkedSection* ls = ld->sections; ls; ls = ls->next) {
                if (strcmp(ls->name, sect->name) == 0) {
                    linked = ls;
                    break;
                }
            }

            if (linked) {
                uint64_t offset = 0;
                for (int i = 0; i <= obj_idx; i++) {
                    ObjectFile* prev = ld->objects[i];
                    for (ObjSection* ps = prev->sections; ps; ps = ps->next) {
                        if (linker_section_selected(ps) &&
                            strcmp(ps->name, sect->name) == 0) {
                            uint64_t mask = ps->align - 1u;
                            offset = (offset + mask) & ~mask;
                            if (i == obj_idx && ps == sect) {
                                sect_offsets[obj_idx][sect_idx] = offset;
                            }
                            offset += ps->memory_size;
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
            uint64_t value;
            int linked_sect = -1;
            ObjSection* owner = NULL;

            /* Skip undefined symbols on first pass */
            if (sym->type == SYM_UNDEF) continue;

            if (sym->section >= 0) {
                owner = object_section_at(obj, sym->section);
                if (!owner || !linker_section_selected(owner)) {
                    continue;
                }
            }

            value = sym->value;
            if (sym->section >= 0) {
                const char* sect_name = owner->name;
                int idx = 0;
                value = sect_offsets[obj_idx][sym->section] + sym->value;
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
                    for (int i = 0; i < ld->object_count; i++) {
                        rcc_free(sect_offsets[i]);
                    }
                    rcc_free(sect_offsets);
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
                printf("    %s: %s = 0x%" PRIx64 " (sect %d)\n",
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
    if (!data) return false;
    for (LinkedSection* section = ld->sections; section && section != data;
         section = section->next) data_section++;

    for (import_index = 0; import_index < g_linker_opts.import_count;
         import_index++) {
        LinkImportSpec* import = &g_linker_opts.imports[import_index];
        uint64_t zero = 0u;
        uint64_t slot;
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
            bool referenced = false;
            bool retained_reference = false;
            if (sym->type != SYM_UNDEF) continue;

            for (ObjSection* section = obj->sections; section;
                 section = section->next) {
                for (ObjReloc* relocation = section->relocs; relocation;
                     relocation = relocation->next) {
                    if (strcmp(relocation->symbol_name, sym->name) != 0) {
                        continue;
                    }
                    referenced = true;
                    if (linker_section_selected(section)) {
                        retained_reference = true;
                    }
                }
            }
            if (referenced && !retained_reference) continue;

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

static bool linker_align_address(uint64_t value, uint32_t alignment,
                                 uint64_t* result) {
    uint64_t mask = (uint64_t)alignment - 1u;
    if (!alignment || (alignment & (alignment - 1u)) != 0u ||
        value > UINT64_MAX - mask) return false;
    *result = (value + mask) & ~mask;
    return true;
}

bool linker_layout(Linker* ld, uint64_t base_addr) {
    if (g_linker_opts.verbose) {
        printf("Layout at base 0x%" PRIx64 "...\n", base_addr);
    }

    ld->base_addr = base_addr;
    uint64_t addr = base_addr;

    /* Standard section order: .text, .rodata, .data, .bss */
    const char* order[] = {".text", ".rodata", ".data", ".bss", NULL};

    for (int i = 0; order[i]; i++) {
        for (LinkedSection* s = ld->sections; s; s = s->next) {
            if (strcmp(s->name, order[i]) != 0) continue;

            /* Align to section alignment */
            if (!linker_align_address(addr, s->align, &addr) ||
                s->memory_size > UINT64_MAX - addr) {
                fprintf(stderr, "rld: section layout overflow\n");
                return false;
            }

            s->vaddr = addr;
            addr += s->memory_size;

            if (g_linker_opts.verbose) {
                printf("    %s: 0x%" PRIx64 " - 0x%" PRIx64
                       " (%" PRIu64 " bytes)\n",
                       s->name, s->vaddr, s->vaddr + s->memory_size,
                       s->memory_size);
            }
        }
    }

    /* Handle any remaining sections */
    for (LinkedSection* s = ld->sections; s; s = s->next) {
        if (s->vaddr != 0) continue;  /* Already placed */

        if (!linker_align_address(addr, s->align, &addr) ||
            s->memory_size > UINT64_MAX - addr) {
            fprintf(stderr, "rld: section layout overflow\n");
            return false;
        }
        s->vaddr = addr;
        addr += s->memory_size;

        if (g_linker_opts.verbose) {
            printf("    %s: 0x%" PRIx64 " - 0x%" PRIx64
                   " (%" PRIu64 " bytes)\n",
                   s->name, s->vaddr, s->vaddr + s->memory_size,
                   s->memory_size);
        }
    }

    if (g_linker_opts.arch == ARCH_X86 && addr >= UINT64_C(0xC0000000)) {
        fprintf(stderr, "rld: x86 image layout must remain below 3 GiB\n");
        return false;
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
            printf("  Entry point: %s = 0x%" PRIx64 "\n",
                   entry_name, ld->entry_addr);
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

        uint8_t* patch = sect->data + (size_t)r->offset;
        uint64_t target;
        if (r->type == RELOC_TLSOFF32S) {
            LinkedSection* tls = NULL;
            uint64_t local_offset;
            idx = 0;
            for (LinkedSection* candidate = ld->sections; candidate;
                 candidate = candidate->next, idx++) {
                if (idx == sym->section) {
                    tls = candidate;
                    break;
                }
            }
            if (sym->binding != BIND_TLS || !tls || tls->type != SECT_TLS ||
                sym->value < tls->vaddr) {
                fprintf(stderr,
                        "rld: TLSOFF32S requires a TLS symbol: '%s'\n",
                        r->symbol);
                return false;
            }
            local_offset = sym->value - tls->vaddr;
            if (r->addend >= 0) {
                if ((uint64_t)r->addend > UINT64_MAX - local_offset) {
                    fprintf(stderr, "rld: TLS offset overflow for '%s'\n",
                            r->symbol);
                    return false;
                }
                local_offset += (uint64_t)r->addend;
            } else {
                uint64_t magnitude = UINT64_C(0) - (uint64_t)r->addend;
                if (magnitude > local_offset) {
                    fprintf(stderr, "rld: TLS offset underflow for '%s'\n",
                            r->symbol);
                    return false;
                }
                local_offset -= magnitude;
            }
            if (local_offset >= tls->memory_size || local_offset > UINT32_MAX) {
                fprintf(stderr, "rld: TLS offset outside template for '%s'\n",
                        r->symbol);
                return false;
            }
            {
                uint32_t value = (uint32_t)local_offset;
                memcpy(patch, &value, sizeof(value));
            }
            continue;
        }
        if (r->addend >= 0) {
            if ((uint64_t)r->addend > UINT64_MAX - sym->value) {
                fprintf(stderr, "rld: relocation target overflow for '%s'\n",
                        r->symbol);
                return false;
            }
            target = sym->value + (uint64_t)r->addend;
        } else {
            uint64_t magnitude = UINT64_C(0) - (uint64_t)r->addend;
            if (magnitude > sym->value) {
                fprintf(stderr, "rld: relocation target underflow for '%s'\n",
                        r->symbol);
                return false;
            }
            target = sym->value - magnitude;
        }

        switch (r->type) {
            case RELOC_ABS32: {
                uint32_t value;
                if (target > INT32_MAX) {
                    fprintf(stderr,
                            "rld: legacy ABS32 relocation is ambiguous for '%s'\n",
                            r->symbol);
                    return false;
                }
                value = (uint32_t)target;
                memcpy(patch, &value, sizeof(value));
                break;
            }
            case RELOC_ABS32U: {
                uint32_t value;
                if (target > UINT32_MAX) {
                    fprintf(stderr, "rld: ABS32U relocation overflow for '%s'\n",
                            r->symbol);
                    return false;
                }
                value = (uint32_t)target;
                memcpy(patch, &value, sizeof(value));
                break;
            }
            case RELOC_ABS32S: {
                int32_t value;
                if (target > INT32_MAX) {
                    fprintf(stderr, "rld: ABS32S relocation overflow for '%s'\n",
                            r->symbol);
                    return false;
                }
                value = (int32_t)target;
                memcpy(patch, &value, sizeof(value));
                break;
            }
            case RELOC_ABS64: {
                memcpy(patch, &target, sizeof(target));
                break;
            }
            case RELOC_REL32: {
                /* 32-bit PC-relative (relative to next instruction) */
                uint64_t pc = sect->vaddr + r->offset + 4u;
                int32_t delta;
                if (target >= pc) {
                    uint64_t distance = target - pc;
                    if (distance > INT32_MAX) goto rel32_overflow;
                    delta = (int32_t)distance;
                } else {
                    uint64_t distance = pc - target;
                    if (distance > UINT64_C(2147483648)) goto rel32_overflow;
                    delta = distance == UINT64_C(2147483648)
                        ? INT32_MIN : -(int32_t)distance;
                }
                memcpy(patch, &delta, sizeof(delta));
                break;
rel32_overflow:
                fprintf(stderr, "rld: REL32 relocation overflow for '%s'\n",
                        r->symbol);
                return false;
            }
            case RELOC_REL8: {
                /* 8-bit PC-relative */
                uint64_t pc = sect->vaddr + r->offset + 1u;
                int64_t delta;
                if (target >= pc) {
                    uint64_t distance = target - pc;
                    if (distance > INT8_MAX) goto rel8_overflow;
                    delta = (int64_t)distance;
                } else {
                    uint64_t distance = pc - target;
                    if (distance > UINT64_C(128)) goto rel8_overflow;
                    delta = -(int64_t)distance;
                }
                {
                    int8_t value = (int8_t)delta;
                    memcpy(patch, &value, sizeof(value));
                }
                break;
rel8_overflow:
                {
                    fprintf(stderr, "rld: 8-bit relocation overflow for '%s'\n", r->symbol);
                    return false;
                }
            }
            default:
                fprintf(stderr, "rld: unsupported relocation type %d\n", r->type);
                return false;
        }

        if (g_linker_opts.verbose) {
            printf("    %s+0x%" PRIx64 " -> %s = 0x%" PRIx64 "\n",
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

static bool linker_image_section_type(SectionType type) {
    return type >= SECT_CODE && type <= SECT_FINI_ARRAY;
}

static bool linker_alias_section_type(SectionType type) {
    return type >= SECT_TLS && type <= SECT_FINI_ARRAY;
}

static SectionType linker_alias_owner_type(SectionType type) {
    return type == SECT_TLS ? SECT_DATA : SECT_RODATA;
}

static uint16_t linker_rin_section_type(SectionType type) {
    switch (type) {
    case SECT_CODE: return RIN_IMAGE_SECTION_CODE;
    case SECT_RODATA: return RIN_IMAGE_SECTION_RODATA;
    case SECT_DATA: return RIN_IMAGE_SECTION_DATA;
    case SECT_BSS: return RIN_IMAGE_SECTION_BSS;
    case SECT_TLS: return RIN_IMAGE_SECTION_TLS;
    case SECT_UNWIND: return RIN_IMAGE_SECTION_UNWIND;
    case SECT_INIT_ARRAY: return RIN_IMAGE_SECTION_INIT_ARRAY;
    case SECT_FINI_ARRAY: return RIN_IMAGE_SECTION_FINI_ARRAY;
    default: return RIN_IMAGE_SECTION_INVALID;
    }
}

static uint16_t linker_rin_section_flags(SectionType type) {
    switch (type) {
    case SECT_CODE:
        return RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_EXECUTE;
    case SECT_DATA:
    case SECT_BSS:
        return RIN_IMAGE_SECTION_READ | RIN_IMAGE_SECTION_WRITE;
    default:
        return RIN_IMAGE_SECTION_READ;
    }
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
    bool uses_tls = false;
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
        if (!linker_image_section_type(linked->type) ||
            linked->memory_size == 0u) {
            continue;
        }
        load_section_count += linker_alias_section_type(linked->type) ? 2u : 1u;
        if (linked->type == SECT_CODE) ++code_count;
        if (linked->type == SECT_TLS) uses_tls = true;
        string_capacity += strlen(linked->name) + 1u;
        if (linker_alias_section_type(linked->type)) {
            string_capacity += strlen(linked->name) + 1u;
        }
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
        if (pending->type == RELOC_ABS32 ||
            pending->type == RELOC_ABS32U ||
            pending->type == RELOC_ABS32S ||
            pending->type == RELOC_ABS64 ||
            pending->type == RELOC_TLSOFF32S) {
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
    if (library && uses_tls) {
        fprintf(stderr,
                "rld: TLS-bearing .rll output is disabled until graph TLS layout is available\n");
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
        RinSectionV3* alias = NULL;
        uint64_t rva;
        size_t name_length;
        if (!linker_image_section_type(linked->type) ||
            linked->memory_size == 0u) {
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
        section->type = linker_rin_section_type(
            linker_alias_section_type(linked->type)
                ? linker_alias_owner_type(linked->type) : linked->type);
        section->flags = linker_rin_section_flags(
            linker_alias_section_type(linked->type)
                ? linker_alias_owner_type(linked->type) : linked->type);
        section->alignment = linker_power_of_two(linked->align) ? linked->align : 1u;
        section->virtual_address = rva;
        section->memory_size = linked->memory_size;
        name_length = strlen(linked->name) + 1u;
        section->name_offset = string_size;
        memcpy(strings + string_size, linked->name, name_length);
        string_size += (uint32_t)name_length;
        if (linker_alias_section_type(linked->type)) {
            alias = &sections[section_index++];
            alias->type = linker_rin_section_type(linked->type);
            alias->flags = RIN_IMAGE_SECTION_READ;
            alias->alignment = section->alignment;
            alias->virtual_address = rva;
            alias->memory_size = linked->memory_size;
            alias->name_offset = string_size;
            memcpy(strings + string_size, linked->name, name_length);
            string_size += (uint32_t)name_length;
        }
        if (rva + linked->memory_size > image_size) {
            image_size = rva + linked->memory_size;
        }
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
            if (pending->type != RELOC_ABS32 &&
                pending->type != RELOC_ABS32U &&
                pending->type != RELOC_ABS32S &&
                pending->type != RELOC_ABS64 &&
                pending->type != RELOC_TLSOFF32S) continue;
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
            relocations[relocation_index].type =
                pending->type == RELOC_TLSOFF32S
                ? RIN_IMAGE_RELOCATION_TLSOFF32S
                : width == 8u ? RIN_IMAGE_RELOCATION_ABS64
                : pending->type == RELOC_ABS32U
                    ? RIN_IMAGE_RELOCATION_ABS32U
                    : RIN_IMAGE_RELOCATION_ABS32S;
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
        if (!linker_image_section_type(linked->type) ||
            linked->memory_size == 0u) continue;
        section = &sections[section_index++];
        if (linked->type == SECT_BSS) continue;
        cursor = linker_align_u64(cursor, section->alignment);
        section->file_offset = cursor;
        section->file_size = linked->size;
        if (linker_alias_section_type(linked->type)) {
            RinSectionV3* alias = &sections[section_index++];
            alias->file_offset = cursor;
            alias->file_size = linked->size;
        }
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
                   RIN_IMAGE_RELOCATABLE | RIN_IMAGE_ASLR |
                   (uses_tls ? RIN_IMAGE_USES_TLS : 0u);
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
        if (!linker_image_section_type(linked->type) ||
            linked->memory_size == 0u) continue;
        section = &sections[section_index++];
        if (linked->type != SECT_BSS) memcpy(output + section->file_offset, linked->data, linked->size);
        if (linker_alias_section_type(linked->type)) ++section_index;
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
