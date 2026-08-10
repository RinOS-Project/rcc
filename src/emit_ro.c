/*
 * RCC - RinOS C Compiler
 * Object File (.ro) Emitter
 */

#include "rcc.h"
#include "objfile.h"
#include "codegen.h"
#include <limits.h>
#include <string.h>

static bool ro_seek(FILE* file, uint64_t offset, int origin) {
    if (offset > (uint64_t)LONG_MAX) return false;
    return fseek(file, (long)offset, origin) == 0;
}

static uint64_t ro_tell(FILE* file) {
    long offset = ftell(file);
    return offset < 0 ? UINT64_MAX : (uint64_t)offset;
}

static bool ro_pad_to(FILE* file, uint64_t offset) {
    uint64_t current = ro_tell(file);
    if (current == UINT64_MAX || current > offset) return false;
    while (current++ < offset) {
        if (fputc(0, file) == EOF) return false;
    }
    return true;
}

static bool ro_range(uint64_t offset, uint64_t size, uint64_t limit) {
    return offset <= limit && size <= limit - offset;
}

static bool ro_relocation_type_valid(uint16_t type) {
    return type <= RELOC_PLT32 || type == RELOC_ABS32U ||
           type == RELOC_ABS32S;
}

static bool ro_section_policy(uint16_t arch, const RoSection* section) {
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    uint32_t allowed_flags = SECT_FLAG_WRITE | SECT_FLAG_EXEC | SECT_FLAG_ALLOC;
    if (section->type < SECT_CODE || section->type > SECT_FINI_ARRAY ||
        (section->flags & ~allowed_flags) != 0u ||
        (section->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) ==
            (SECT_FLAG_WRITE | SECT_FLAG_EXEC) ||
        (section->flags & SECT_FLAG_ALLOC) == 0u) return false;

    switch ((SectionType)section->type) {
    case SECT_CODE:
        return (section->flags & SECT_FLAG_EXEC) != 0u &&
               (section->flags & SECT_FLAG_WRITE) == 0u;
    case SECT_DATA:
    case SECT_TLS:
        return (section->flags & SECT_FLAG_WRITE) != 0u &&
               (section->flags & SECT_FLAG_EXEC) == 0u;
    case SECT_BSS:
        return section->size == 0u &&
               (section->flags & SECT_FLAG_WRITE) != 0u &&
               (section->flags & SECT_FLAG_EXEC) == 0u;
    case SECT_INIT_ARRAY:
    case SECT_FINI_ARRAY:
        return (section->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) == 0u &&
               section->size == section->memory_size &&
               section->memory_size % pointer_size == 0u;
    case SECT_RODATA:
        return (section->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) == 0u;
    case SECT_UNWIND:
        return (section->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) == 0u &&
               section->size == section->memory_size;
    default:
        return false;
    }
}

/* ═══════════════════════════════════════
 * Object File Creation
 * ═══════════════════════════════════════ */

ObjectFile* objfile_new(const char* filename, uint16_t arch) {
    ObjectFile* obj = rcc_alloc(sizeof(ObjectFile));
    obj->filename = rcc_strdup(filename);
    obj->arch = arch;
    obj->sections = NULL;
    obj->section_count = 0;
    obj->symbols = NULL;
    obj->symbol_count = 0;
    obj->strtab = rcc_alloc(256);
    obj->strtab[0] = '\0';  /* First byte is null */
    obj->strtab_size = 1;
    obj->strtab_cap = 256;
    return obj;
}

void objfile_free(ObjectFile* obj) {
    if (!obj) return;

    /* Free sections */
    ObjSection* sect = obj->sections;
    while (sect) {
        ObjSection* next = sect->next;
        rcc_free((void*)sect->name);
        rcc_free(sect->data);
        /* Free relocs */
        ObjReloc* r = sect->relocs;
        while (r) {
            ObjReloc* rn = r->next;
            rcc_free((void*)r->symbol_name);
            rcc_free(r);
            r = rn;
        }
        rcc_free(sect);
        sect = next;
    }

    /* Free symbols */
    ObjSymbol* sym = obj->symbols;
    while (sym) {
        ObjSymbol* next = sym->next;
        rcc_free((void*)sym->name);
        rcc_free(sym);
        sym = next;
    }

    rcc_free(obj->strtab);
    rcc_free((void*)obj->filename);
    rcc_free(obj);
}

/* ═══════════════════════════════════════
 * Section Operations
 * ═══════════════════════════════════════ */

ObjSection* objfile_add_section(ObjectFile* obj, const char* name, SectionType type, uint32_t flags) {
    ObjSection* sect = rcc_alloc(sizeof(ObjSection));
    sect->name = rcc_strdup(name);
    sect->type = type;
    sect->flags = flags;
    sect->data = rcc_alloc(256);
    sect->size = 0;
    sect->memory_size = 0;
    sect->capacity = 256;
    sect->align = 1;
    sect->relocs = NULL;
    sect->next = NULL;

    /* Append to list */
    if (!obj->sections) {
        obj->sections = sect;
    } else {
        ObjSection* s = obj->sections;
        while (s->next) s = s->next;
        s->next = sect;
    }
    obj->section_count++;

    return sect;
}

ObjSection* objfile_get_section(ObjectFile* obj, const char* name) {
    for (ObjSection* s = obj->sections; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static void section_ensure_capacity(ObjSection* sect, uint64_t need) {
    uint64_t required;
    uint64_t new_cap;
    if (need > UINT64_MAX - sect->size) rcc_fatal("object section size overflow");
    required = sect->size + need;
    if (required <= sect->capacity) return;
    if (required > SIZE_MAX) rcc_fatal("object section exceeds host memory limit");

    new_cap = sect->capacity;
    while (new_cap < required) {
        if (new_cap > UINT64_MAX / 2u) {
            new_cap = required;
            break;
        }
        new_cap *= 2u;
    }
    sect->data = rcc_realloc(sect->data, (size_t)new_cap);
    sect->capacity = new_cap;
}

uint64_t section_add_data(ObjSection* sect, const void* data, uint64_t size) {
    if (sect->memory_size > sect->size) {
        uint64_t gap = sect->memory_size - sect->size;
        section_ensure_capacity(sect, gap);
        memset(sect->data + (size_t)sect->size, 0, (size_t)gap);
        sect->size = sect->memory_size;
    }
    section_ensure_capacity(sect, size);
    uint64_t offset = sect->size;
    memcpy(sect->data + (size_t)sect->size, data, (size_t)size);
    sect->size += size;
    sect->memory_size = sect->size;
    return offset;
}

uint64_t section_add_byte(ObjSection* sect, uint8_t byte) {
    return section_add_data(sect, &byte, 1u);
}

uint64_t section_add_bytes(ObjSection* sect, const uint8_t* bytes, uint64_t count) {
    return section_add_data(sect, bytes, count);
}

void section_align(ObjSection* sect, uint32_t align) {
    uint64_t mask;
    if (align <= 1) return;
    if (align > sect->align) sect->align = align;

    if (sect->memory_size > sect->size) {
        mask = (uint64_t)align - 1u;
        if (sect->memory_size > UINT64_MAX - mask) {
            rcc_fatal("object section alignment overflow");
        }
        sect->memory_size = (sect->memory_size + mask) & ~mask;
        return;
    }

    while (sect->size % align != 0) {
        section_add_byte(sect, 0);
    }
}

void section_set_memory_size(ObjSection* sect, uint64_t memory_size) {
    if (memory_size < sect->size) {
        rcc_fatal("object section memory size is smaller than file size");
    }
    sect->memory_size = memory_size;
}

/* ═══════════════════════════════════════
 * Symbol Operations
 * ═══════════════════════════════════════ */

ObjSymbol* objfile_add_symbol(ObjectFile* obj, const char* name, SymbolType type,
                              SymbolBinding binding, int section, uint64_t value,
                              uint64_t size) {
    ObjSymbol* sym = rcc_alloc(sizeof(ObjSymbol));
    sym->name = rcc_strdup(name);
    sym->value = value;
    sym->size = size;
    sym->type = type;
    sym->binding = binding;
    sym->section = section;
    sym->next = NULL;

    /* Append to list */
    if (!obj->symbols) {
        obj->symbols = sym;
    } else {
        ObjSymbol* s = obj->symbols;
        while (s->next) s = s->next;
        s->next = sym;
    }
    obj->symbol_count++;

    return sym;
}

ObjSymbol* objfile_find_symbol(ObjectFile* obj, const char* name) {
    for (ObjSymbol* s = obj->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════
 * Relocation Operations
 * ═══════════════════════════════════════ */

void objfile_add_reloc(ObjectFile* obj, int section_idx, uint64_t offset,
                       const char* symbol, RelocType type, int64_t addend) {
    /* Find section */
    int idx = 0;
    ObjSection* sect = obj->sections;
    while (sect && idx < section_idx) {
        sect = sect->next;
        idx++;
    }
    if (!sect) return;

    ObjReloc* r = rcc_alloc(sizeof(ObjReloc));
    r->offset = offset;
    r->symbol_name = rcc_strdup(symbol);
    r->symbol_idx = -1;  /* Resolved during write */
    r->type = type;
    r->addend = addend;
    r->section = section_idx;
    r->next = NULL;

    /* Append to section's reloc list */
    if (!sect->relocs) {
        sect->relocs = r;
    } else {
        ObjReloc* rp = sect->relocs;
        while (rp->next) rp = rp->next;
        rp->next = r;
    }
}

/* ═══════════════════════════════════════
 * String Table
 * ═══════════════════════════════════════ */

uint32_t objfile_add_string(ObjectFile* obj, const char* str) {
    if (!str || !str[0]) return 0;

    /* Check if already exists */
    uint32_t off = 1;
    while (off < obj->strtab_size) {
        if (strcmp(obj->strtab + off, str) == 0) {
            return off;
        }
        off += strlen(obj->strtab + off) + 1;
    }

    /* Add new string */
    size_t len = strlen(str) + 1;
    while (obj->strtab_size + len > obj->strtab_cap) {
        obj->strtab_cap *= 2;
        obj->strtab = rcc_realloc(obj->strtab, obj->strtab_cap);
    }

    uint32_t offset = obj->strtab_size;
    memcpy(obj->strtab + offset, str, len);
    obj->strtab_size += len;

    return offset;
}

/* ═══════════════════════════════════════
 * File I/O
 * ═══════════════════════════════════════ */

bool objfile_write(ObjectFile* obj, const char* filename) {
    for (ObjSection* section = obj->sections; section; section = section->next) {
        for (ObjReloc* relocation = section->relocs; relocation;
             relocation = relocation->next) {
            if (!ro_relocation_type_valid((uint16_t)relocation->type)) {
                rcc_error((SourceLoc){filename, 0, 0},
                          "unsupported .ro v2 relocation type %u",
                          (unsigned)relocation->type);
                return false;
            }
            if (relocation->type == RELOC_ABS32) {
                rcc_error((SourceLoc){filename, 0, 0},
                          "new .ro v2 objects must use ABS32U or ABS32S");
                return false;
            }
        }
    }
    FILE* f = fopen(filename, "wb");
    uint64_t* section_data_off = NULL;
    uint64_t* reloc_off = NULL;
    uint32_t* reloc_count = NULL;
    int* sym_idx_map = NULL;
    if (!f) {
        rcc_error((SourceLoc){filename, 0, 0}, "cannot open output file");
        return false;
    }

    /* Calculate offsets */
    uint64_t offset = sizeof(RoHeader);

    /* Section headers */
    uint64_t section_off = offset;
    offset += obj->section_count * sizeof(RoSection);

    /* Section data */
    section_data_off = rcc_alloc(sizeof(uint64_t) * obj->section_count);
    int sect_idx = 0;
    for (ObjSection* s = obj->sections; s; s = s->next) {
        /* Align to 16 bytes */
        offset = (offset + 15) & ~15;
        section_data_off[sect_idx++] = offset;
        offset += s->size;
    }

    /* Symbol table */
    uint64_t symbol_off = (offset + 7) & ~UINT64_C(7);
    offset = symbol_off + obj->symbol_count * sizeof(RoSymbol);

    /* Relocation tables (one per section with relocs) */
    reloc_off = rcc_alloc(sizeof(uint64_t) * obj->section_count);
    reloc_count = rcc_alloc(sizeof(uint32_t) * obj->section_count);
    sect_idx = 0;
    for (ObjSection* s = obj->sections; s; s = s->next) {
        int count = 0;
        for (ObjReloc* r = s->relocs; r; r = r->next) count++;
        reloc_count[sect_idx] = count;
        if (count > 0) {
            reloc_off[sect_idx] = offset;
            offset += count * sizeof(RoReloc);
        } else {
            reloc_off[sect_idx] = 0;
        }
        sect_idx++;
    }

    /* String table */
    uint64_t strtab_off = offset;

    /* Write header */
    RoHeader hdr = {0};
    hdr.magic = RO_MAGIC;
    hdr.version = RO_VERSION;
    hdr.arch = obj->arch;
    hdr.header_size = sizeof(RoHeader);
    hdr.flags = 0;
    hdr.section_off = section_off;
    hdr.section_count = obj->section_count;
    hdr.symbol_off = symbol_off;
    hdr.symbol_count = obj->symbol_count;
    hdr.strtab_off = strtab_off;
    hdr.strtab_size = obj->strtab_size;

    fwrite(&hdr, sizeof(hdr), 1, f);

    /* Write section headers */
    sect_idx = 0;
    for (ObjSection* s = obj->sections; s; s = s->next) {
        RoSection sh = {0};
        sh.name = objfile_add_string(obj, s->name);
        sh.type = s->type;
        sh.flags = s->flags;
        sh.offset = section_data_off[sect_idx];
        sh.size = s->size;
        sh.memory_size = s->memory_size;
        sh.align = s->align;
        sh.reloc_off = reloc_off[sect_idx];
        sh.reloc_count = reloc_count[sect_idx];
        fwrite(&sh, sizeof(sh), 1, f);
        sect_idx++;
    }

    /* Write section data */
    sect_idx = 0;
    for (ObjSection* s = obj->sections; s; s = s->next) {
        /* Pad to offset */
        if (!ro_pad_to(f, section_data_off[sect_idx])) goto write_failed;
        fwrite(s->data, s->size, 1, f);
        sect_idx++;
    }

    /* Build symbol index map */
    sym_idx_map = rcc_alloc(sizeof(int) * obj->symbol_count);
    int sym_idx = 0;
    for (ObjSymbol* s = obj->symbols; s; s = s->next) {
        sym_idx_map[sym_idx] = sym_idx;
        sym_idx++;
    }

    /* Pad to symbol table offset */
    if (!ro_pad_to(f, symbol_off)) goto write_failed;

    /* Write symbol table */
    for (ObjSymbol* s = obj->symbols; s; s = s->next) {
        RoSymbol rs = {0};
        rs.name = objfile_add_string(obj, s->name);
        rs.value = s->value;
        rs.size = s->size;
        rs.type = s->type;
        rs.binding = s->binding;
        rs.section = s->section + 1;  /* 0 = undefined, 1 = first section */
        fwrite(&rs, sizeof(rs), 1, f);
    }

    /* Write relocation tables */
    sect_idx = 0;
    for (ObjSection* s = obj->sections; s; s = s->next) {
        if (reloc_count[sect_idx] > 0) {
            for (ObjReloc* r = s->relocs; r; r = r->next) {
                /* Find symbol index */
                int sidx = 0;
                for (ObjSymbol* sym = obj->symbols; sym; sym = sym->next) {
                    if (strcmp(sym->name, r->symbol_name) == 0) {
                        break;
                    }
                    sidx++;
                }

                RoReloc rr = {0};
                rr.offset = r->offset;
                rr.symbol = sidx;
                rr.type = r->type;
                rr.addend = r->addend;
                fwrite(&rr, sizeof(rr), 1, f);
            }
        }
        sect_idx++;
    }

    /* Update string table offsets and write */
    hdr.strtab_off = ro_tell(f);
    if (hdr.strtab_off == UINT64_MAX) goto write_failed;
    hdr.strtab_size = obj->strtab_size;
    fwrite(obj->strtab, obj->strtab_size, 1, f);
    hdr.file_size = ro_tell(f);
    if (hdr.file_size == UINT64_MAX) goto write_failed;

    /* Update header with final string table offset */
    if (!ro_seek(f, 0, SEEK_SET)) goto write_failed;
    fwrite(&hdr, sizeof(hdr), 1, f);

    fclose(f);

    rcc_free(section_data_off);
    rcc_free(reloc_off);
    rcc_free(reloc_count);
    rcc_free(sym_idx_map);

    return true;

write_failed:
    fclose(f);
    rcc_free(section_data_off);
    rcc_free(reloc_off);
    rcc_free(reloc_count);
    rcc_free(sym_idx_map);
    rcc_error((SourceLoc){filename, 0, 0}, "failed to write .ro v2 object");
    return false;
}

ObjectFile* objfile_read_memory(const void* data, uint64_t size,
                                const char* display_name) {
    const uint8_t* bytes = data;
    ObjectFile* obj = NULL;
    RoSection* sections = NULL;
    ObjSymbol** sym_ptrs = NULL;
    RoHeader hdr;
    if (!bytes || size < sizeof(hdr) || size > SIZE_MAX) return NULL;
    memcpy(&hdr, bytes, sizeof(hdr));
    if (hdr.magic != RO_MAGIC || hdr.version != RO_VERSION ||
        hdr.header_size != sizeof(RoHeader) || hdr.flags != 0u ||
        hdr.file_size != size ||
        hdr.arch > ARCH_X64 || hdr.section_count > 65535u ||
        hdr.symbol_count > 1048576u || hdr.strtab_size == 0u ||
        hdr.strtab_size > UINT32_MAX ||
        !ro_range(hdr.section_off,
                  (uint64_t)hdr.section_count * sizeof(RoSection), size) ||
        !ro_range(hdr.symbol_off,
                  (uint64_t)hdr.symbol_count * sizeof(RoSymbol), size) ||
        !ro_range(hdr.strtab_off, hdr.strtab_size, size)) return NULL;

    obj = objfile_new(display_name ? display_name : "<memory>", hdr.arch);

    obj->strtab = rcc_realloc(obj->strtab, (size_t)hdr.strtab_size);
    obj->strtab_size = (uint32_t)hdr.strtab_size;
    obj->strtab_cap = (uint32_t)hdr.strtab_size;
    memcpy(obj->strtab, bytes + (size_t)hdr.strtab_off,
           (size_t)hdr.strtab_size);
    if (obj->strtab[0] != '\0') goto read_failed;

    if (hdr.section_count != 0u) {
        sections = rcc_alloc(sizeof(RoSection) * hdr.section_count);
        memcpy(sections, bytes + (size_t)hdr.section_off,
               sizeof(RoSection) * hdr.section_count);
    }

    for (uint32_t i = 0; i < hdr.section_count; i++) {
        RoSection* sh = &sections[i];
        if (sh->name >= hdr.strtab_size ||
            !memchr(obj->strtab + sh->name, '\0',
                    (size_t)hdr.strtab_size - sh->name) ||
            !ro_section_policy(hdr.arch, sh) || sh->align == 0u ||
            (sh->align & (sh->align - 1u)) != 0u ||
            sh->size > SIZE_MAX || sh->memory_size < sh->size ||
            !ro_range(sh->offset, sh->size, size) ||
            (sh->reloc_count != 0u &&
             !ro_range(sh->reloc_off,
                       (uint64_t)sh->reloc_count * sizeof(RoReloc),
                       size)) ||
            sh->reserved0 != 0u || sh->reserved1 != 0u) goto read_failed;
        const char* name = obj->strtab + sh->name;

        ObjSection* sect = objfile_add_section(obj, name, sh->type, sh->flags);
        sect->align = sh->align;

        if (sh->size > 0) {
            section_ensure_capacity(sect, sh->size);
            memcpy(sect->data, bytes + (size_t)sh->offset,
                   (size_t)sh->size);
            sect->size = sh->size;
        }
        sect->memory_size = sh->memory_size;
    }

    if (hdr.symbol_count != 0u) {
        sym_ptrs = rcc_alloc(sizeof(ObjSymbol*) * hdr.symbol_count);
    }
    for (uint32_t i = 0; i < hdr.symbol_count; i++) {
        RoSymbol rs;
        memcpy(&rs, bytes + (size_t)hdr.symbol_off +
                    (size_t)i * sizeof(rs), sizeof(rs));
        if (rs.name >= hdr.strtab_size ||
            !memchr(obj->strtab + rs.name, '\0',
                    (size_t)hdr.strtab_size - rs.name) ||
            rs.type > SYM_WEAK || rs.binding > BIND_ABS ||
            rs.section > hdr.section_count || rs.flags != 0u ||
            rs.reserved != 0u) {
            goto read_failed;
        }
        if (rs.section == 0u) {
            if (rs.type != SYM_UNDEF && rs.binding != BIND_ABS) {
                goto read_failed;
            }
        } else {
            RoSection* owner = &sections[rs.section - 1u];
            if (rs.type == SYM_UNDEF || rs.binding == BIND_ABS ||
                rs.value > owner->memory_size ||
                rs.size > owner->memory_size - rs.value) {
                goto read_failed;
            }
        }

        const char* name = obj->strtab + rs.name;
        sym_ptrs[i] = objfile_add_symbol(obj, name, rs.type, rs.binding,
                                         (int)rs.section - 1,
                                         rs.value, rs.size);
    }

    for (uint32_t i = 0; i < hdr.section_count; i++) {
        RoSection* sh = &sections[i];
        if (sh->reloc_count == 0) continue;

        for (uint32_t j = 0; j < sh->reloc_count; j++) {
            RoReloc rr;
            uint64_t width;
            memcpy(&rr, bytes + (size_t)sh->reloc_off +
                        (size_t)j * sizeof(rr), sizeof(rr));
            if (rr.symbol >= hdr.symbol_count ||
                !ro_relocation_type_valid(rr.type) || rr.flags != 0u ||
                rr.reserved != 0u) goto read_failed;
            width = rr.type == RELOC_ABS64 ? 8u :
                    rr.type == RELOC_REL8 ? 1u : 4u;
            if (rr.offset > sh->memory_size ||
                width > sh->memory_size - rr.offset) goto read_failed;

            const char* sym_name = NULL;
            if (rr.symbol < hdr.symbol_count && sym_ptrs[rr.symbol]) {
                sym_name = sym_ptrs[rr.symbol]->name;
            }

            if (sym_name) {
                objfile_add_reloc(obj, (int)i, rr.offset,
                                  sym_name, rr.type, rr.addend);
            }
        }
    }

    rcc_free(sym_ptrs);
    rcc_free(sections);
    return obj;

read_failed:
    rcc_free(sym_ptrs);
    rcc_free(sections);
    objfile_free(obj);
    return NULL;
}

ObjectFile* objfile_read(const char* filename) {
    FILE* f = fopen(filename, "rb");
    uint8_t* data = NULL;
    uint64_t size;
    ObjectFile* obj;
    if (!f) return NULL;

    if (!ro_seek(f, 0, SEEK_END) ||
        (size = ro_tell(f)) == UINT64_MAX || size > SIZE_MAX ||
        !ro_seek(f, 0, SEEK_SET)) {
        fclose(f);
        return NULL;
    }
    data = rcc_alloc((size_t)(size == 0u ? 1u : size));
    if (size != 0u && fread(data, (size_t)size, 1, f) != 1) {
        rcc_free(data);
        fclose(f);
        return NULL;
    }
    if (fclose(f) != 0) {
        rcc_free(data);
        return NULL;
    }

    obj = objfile_read_memory(data, size, filename);
    rcc_free(data);
    return obj;
}

/* ═══════════════════════════════════════
 * Convert Module to ObjectFile
 * ═══════════════════════════════════════ */

static const ModuleSymbol* module_find_symbol(const Module* mod,
                                               const char* name) {
    for (int index = 0; index < mod->symbol_count; ++index) {
        if (strcmp(mod->symbols[index].name, name) == 0) {
            return &mod->symbols[index];
        }
    }
    return NULL;
}

static char* module_scoped_symbol(const char* filename, const char* name) {
    size_t filename_length = strlen(filename);
    size_t name_length = strlen(name);
    char* scoped = rcc_alloc(filename_length + name_length + 3u);
    memcpy(scoped, filename, filename_length);
    scoped[filename_length] = ':';
    scoped[filename_length + 1u] = ':';
    memcpy(scoped + filename_length + 2u, name, name_length + 1u);
    return scoped;
}

ObjectFile* module_to_objfile(Module* mod, const char* filename) {
    ObjectFile* obj = objfile_new(filename, g_opts.target_arch);
    int next_section = 1;
    int rodata_section = -1;
    int data_section = -1;
    int bss_section = -1;

    /* Create .text section */
    ObjSection* text = objfile_add_section(obj, ".text", SECT_CODE,
                                           SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    section_add_data(text, mod->code.data, mod->code.size);

    if (mod->rodata.size > 0u) {
        ObjSection* rodata = objfile_add_section(
            obj, ".rodata", SECT_RODATA, SECT_FLAG_ALLOC);
        section_add_data(rodata, mod->rodata.data, mod->rodata.size);
        rodata_section = next_section++;
    }

    if (mod->data.size > 0) {
        ObjSection* data = objfile_add_section(obj, ".data", SECT_DATA,
                                               SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
        section_add_data(data, mod->data.data, mod->data.size);
        data_section = next_section++;
    }

    if (mod->bss.size > 0u) {
        ObjSection* bss = objfile_add_section(obj, ".bss", SECT_BSS,
                                              SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
        section_set_memory_size(bss, mod->bss.size);
        bss->align = mod->bss.align;
        bss_section = next_section++;
    }

    /* Add symbols from module */
    for (int i = 0; i < mod->symbol_count; i++) {
        ModuleSymbol* ms = &mod->symbols[i];
        const char* object_name = ms->name;
        bool referenced = ms->is_defined;
        if (!referenced) {
            for (int relocation_index = 0;
                 relocation_index < mod->reloc_count; relocation_index++) {
                const char* relocation_name =
                    mod->relocs_arr[relocation_index].symbol_name;
                if (relocation_name && strcmp(relocation_name, ms->name) == 0) {
                    referenced = true;
                    break;
                }
            }
        }
        /* A prototype is type information, not an object-file dependency.
         * Emit an undefined symbol only when generated code references it. */
        if (!referenced) continue;
        SymbolType type = ms->is_defined
            ? (ms->is_global ? SYM_GLOBAL : SYM_LOCAL)
            : SYM_UNDEF;
        SymbolBinding binding = ms->section == MODULE_SYMBOL_CODE
            ? BIND_CODE : ms->section == MODULE_SYMBOL_BSS
                ? BIND_BSS : BIND_DATA;
        int section = -1;
        if (ms->is_defined) {
            section = ms->section == MODULE_SYMBOL_CODE ? 0
                : ms->section == MODULE_SYMBOL_RODATA ? rodata_section
                : ms->section == MODULE_SYMBOL_BSS ? bss_section
                : data_section;
            if (section < 0) {
                rcc_error((SourceLoc){filename, 0, 0},
                          "defined symbol '%s' has no output section",
                          ms->name);
                objfile_free(obj);
                return NULL;
            }
        }

        if (ms->is_defined && !ms->is_global) {
            object_name = module_scoped_symbol(filename, ms->name);
        }

        objfile_add_symbol(obj, object_name, type, binding, section,
                           ms->offset, 0);
    }

    /* Add relocations */
    for (int i = 0; i < mod->reloc_count; i++) {
        ModuleReloc* mr = &mod->relocs_arr[i];
        RelocType type = mr->is_relative ? RELOC_REL32 :
            (mr->is_64bit ? RELOC_ABS64 : RELOC_ABS32U);

        /* Use symbol name directly if available */
        const char* sym_name = mr->symbol_name;
        const ModuleSymbol* module_symbol = sym_name
            ? module_find_symbol(mod, sym_name) : NULL;

        if (module_symbol && module_symbol->is_defined &&
            !module_symbol->is_global) {
            sym_name = module_scoped_symbol(filename, sym_name);
        }

        /* If no symbol name, try to find by offset */
        if (!sym_name) {
            for (int j = 0; j < mod->symbol_count; j++) {
                if (mod->symbols[j].offset == mr->target &&
                    mod->symbols[j].section != MODULE_SYMBOL_CODE) {
                    sym_name = mod->symbols[j].name;
                    break;
                }
            }
        }

        if (sym_name) {
            objfile_add_reloc(obj, 0, mr->offset, sym_name, type,
                              (int32_t)mr->target);
        }
    }

    return obj;
}

/* Emit object file from Module */
bool rcc_emit_obj(Module* mod, const char* filename) {
    ObjectFile* obj = module_to_objfile(mod, filename);
    bool ok;
    if (!obj) return false;
    ok = objfile_write(obj, filename);
    objfile_free(obj);
    return ok;
}
