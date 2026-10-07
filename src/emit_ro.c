/*
 * RCC - RinOS C Compiler
 * Object File (.ro) Emitter
 */

#include "rcc.h"
#include "objfile.h"
#include "codegen.h"
#include <limits.h>
#include <stdlib.h>
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
           type == RELOC_ABS32S || type == RELOC_TLSOFF32S;
}

static bool ro_section_policy(uint16_t arch, const RoSection* section) {
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    uint32_t allowed_flags = SECT_FLAG_WRITE | SECT_FLAG_EXEC |
                             SECT_FLAG_ALLOC | SECT_FLAG_COMDAT;
    if (section->type < SECT_CODE || section->type > SECT_DEBUG_RANGES ||
        (section->flags & ~allowed_flags) != 0u ||
        (section->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) ==
            (SECT_FLAG_WRITE | SECT_FLAG_EXEC) ||
        (!(section->type >= SECT_DEBUG_LINE &&
           section->type <= SECT_DEBUG_RANGES) &&
         (section->flags & SECT_FLAG_ALLOC) == 0u)) return false;

    if (section->type >= SECT_DEBUG_LINE &&
        section->type <= SECT_DEBUG_RANGES) {
        return section->flags == 0u &&
               section->size == section->memory_size;
    }

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
        rcc_free((void*)sect->comdat_key);
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
    sect->comdat_selection = 0u;
    sect->comdat_key = NULL;
    sect->comdat_selected = true;
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

bool objfile_set_comdat(ObjSection* sect, const char* key,
                        uint32_t selection) {
    if (!sect || !key || key[0] == '\0' ||
        selection != RO_COMDAT_SELECT_ANY) return false;
    rcc_free((void*)sect->comdat_key);
    sect->comdat_key = rcc_strdup(key);
    sect->comdat_selection = selection;
    sect->flags |= SECT_FLAG_COMDAT;
    return true;
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
        bool is_comdat = (section->flags & SECT_FLAG_COMDAT) != 0u;
        if ((is_comdat &&
             (section->comdat_selection != RO_COMDAT_SELECT_ANY ||
              !section->comdat_key || section->comdat_key[0] == '\0')) ||
            (!is_comdat &&
             (section->comdat_selection != 0u || section->comdat_key))) {
            rcc_error((SourceLoc){filename, 0, 0},
                      "invalid .ro v2 section metadata for '%s'",
                      section->name);
            return false;
        }
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
        if ((s->flags & SECT_FLAG_COMDAT) != 0u) {
            sh.reserved0 = s->comdat_selection;
            sh.reserved1 = objfile_add_string(obj, s->comdat_key);
        }
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
        bool is_comdat = (sh->flags & SECT_FLAG_COMDAT) != 0u;
        bool comdat_valid = !is_comdat
            ? sh->reserved0 == 0u && sh->reserved1 == 0u
            : sh->reserved0 == RO_COMDAT_SELECT_ANY &&
              sh->reserved1 < hdr.strtab_size &&
              obj->strtab[sh->reserved1] != '\0' &&
              memchr(obj->strtab + (size_t)sh->reserved1, '\0',
                     (size_t)(hdr.strtab_size - sh->reserved1)) != NULL;
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
                       size)) || !comdat_valid) goto read_failed;
        const char* name = obj->strtab + sh->name;

        ObjSection* sect = objfile_add_section(obj, name, sh->type, sh->flags);
        sect->align = sh->align;
        if (is_comdat &&
            !objfile_set_comdat(sect,
                                obj->strtab + (size_t)sh->reserved1,
                                sh->reserved0)) goto read_failed;

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
            rs.type > SYM_WEAK || rs.binding > BIND_TLS ||
            rs.section > hdr.section_count || rs.flags != 0u ||
            rs.reserved != 0u) {
            goto read_failed;
        }
        if (rs.section == 0u) {
            if (rs.type != SYM_UNDEF && rs.type != SYM_WEAK &&
                rs.binding != BIND_ABS) {
                goto read_failed;
            }
        } else {
            RoSection* owner = &sections[rs.section - 1u];
            if (rs.type == SYM_UNDEF || rs.binding == BIND_ABS ||
                ((rs.binding == BIND_TLS) !=
                 (owner->type == SECT_TLS)) ||
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

static void debug_line_u16(ObjSection* section, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    section_add_bytes(section, bytes, sizeof(bytes));
}

static void debug_line_u32(ObjSection* section, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8),
                        (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    section_add_bytes(section, bytes, sizeof(bytes));
}

static void debug_line_address(ObjSection* section, uint64_t value,
                               int address_size) {
    for (int byte = 0; byte < address_size; ++byte) {
        section_add_byte(section, (uint8_t)(value >> (byte * 8)));
    }
}

static void debug_line_patch_u32(ObjSection* section, uint64_t offset,
                                 uint32_t value) {
    if (!section || offset > section->size ||
        sizeof(uint32_t) > section->size - offset) {
        rcc_fatal("DWARF line table patch is outside its section");
    }
    section->data[offset] = (uint8_t)value;
    section->data[offset + 1u] = (uint8_t)(value >> 8);
    section->data[offset + 2u] = (uint8_t)(value >> 16);
    section->data[offset + 3u] = (uint8_t)(value >> 24);
}

static void debug_line_uleb(ObjSection* section, uint64_t value) {
    do {
        uint8_t byte = (uint8_t)(value & 0x7fu);
        value >>= 7u;
        if (value != 0u) byte |= 0x80u;
        section_add_byte(section, byte);
    } while (value != 0u);
}

static void debug_line_sleb(ObjSection* section, int64_t value) {
    bool more;
    do {
        uint8_t byte = (uint8_t)((uint64_t)value & 0x7fu);
        value >>= 7;
        more = !((value == 0 && (byte & 0x40u) == 0u) ||
                 (value == -1 && (byte & 0x40u) != 0u));
        if (more) byte |= 0x80u;
        section_add_byte(section, byte);
    } while (more);
}

static uint32_t debug_str_add(ObjSection* strings, const char* value);
static void debug_expr_member_location(ObjSection* info, int offset);

typedef struct DebugTypeEntry {
    const Type* type;
    Type* qualifier_base;
    uint32_t offset;
    bool collecting;
    bool recursive;
} DebugTypeEntry;

typedef struct DebugTypeContext {
    DebugTypeEntry* entries;
    size_t count;
    size_t capacity;
} DebugTypeContext;

typedef struct DebugTypePatch {
    uint64_t offset;
    DebugTypeEntry* target;
} DebugTypePatch;

typedef struct DebugTypePatchContext {
    DebugTypePatch* patches;
    size_t count;
    size_t capacity;
} DebugTypePatchContext;

static void debug_type_patch_add(DebugTypePatchContext* context,
                                 uint64_t offset, DebugTypeEntry* target) {
    size_t capacity;
    if (!context || !target) return;
    if (context->count == context->capacity) {
        capacity = context->capacity == 0u ? 16u : context->capacity * 2u;
        if (capacity < context->count ||
            capacity > SIZE_MAX / sizeof(*context->patches)) {
            rcc_fatal("DWARF type patch table is too large");
        }
        context->patches = rcc_realloc(
            context->patches, capacity * sizeof(*context->patches));
        context->capacity = capacity;
    }
    context->patches[context->count].offset = offset;
    context->patches[context->count].target = target;
    ++context->count;
}

static void debug_type_ref(ObjSection* info,
                           DebugTypePatchContext* patches,
                           DebugTypeEntry* target) {
    uint64_t offset;
    if (!info) return;
    offset = info->size;
    debug_line_u32(info, target && target->offset != 0u
                         ? target->offset : 0u);
    if (target && target->offset == 0u) {
        debug_type_patch_add(patches, offset, target);
    }
}

static void debug_type_patches_apply(ObjSection* info,
                                     DebugTypePatchContext* context) {
    if (!info || !context) return;
    for (size_t index = 0u; index < context->count; ++index) {
        DebugTypePatch* patch = &context->patches[index];
        if (!patch->target || patch->target->offset == 0u) {
            rcc_fatal("DWARF type forward reference was not resolved");
            return;
        }
        debug_line_patch_u32(info, patch->offset, patch->target->offset);
    }
}

static void debug_type_patches_free(DebugTypePatchContext* context) {
    if (!context) return;
    rcc_free(context->patches);
    context->patches = NULL;
    context->count = 0u;
    context->capacity = 0u;
}

static void debug_type_context_free(DebugTypeContext* context) {
    if (!context) return;
    for (size_t index = 0u; index < context->count; ++index) {
        rcc_free(context->entries[index].qualifier_base);
        context->entries[index].qualifier_base = NULL;
    }
    rcc_free(context->entries);
    context->entries = NULL;
    context->count = 0u;
    context->capacity = 0u;
}

static DebugTypeEntry* debug_type_find(DebugTypeContext* context,
                                       const Type* type) {
    if (!context || !type) return NULL;
    for (size_t index = 0u; index < context->count; ++index) {
        if (context->entries[index].type == type) {
            return &context->entries[index];
        }
    }
    return NULL;
}

static DebugTypeEntry* debug_type_add(DebugTypeContext* context,
                                      const Type* type) {
    DebugTypeEntry* entry;
    size_t capacity;
    if (!context || !type) return NULL;
    entry = debug_type_find(context, type);
    if (entry) return entry;
    if (context->count == context->capacity) {
        capacity = context->capacity == 0u ? 16u : context->capacity * 2u;
        if (capacity < context->count ||
            capacity > SIZE_MAX / sizeof(*context->entries)) {
            rcc_fatal("DWARF type table is too large");
        }
        context->entries = rcc_realloc(
            context->entries, capacity * sizeof(*context->entries));
        context->capacity = capacity;
    }
    entry = &context->entries[context->count++];
    entry->type = type;
    entry->qualifier_base = NULL;
    entry->offset = 0u;
    entry->collecting = false;
    entry->recursive = false;
    return entry;
}

static void debug_type_collect(DebugTypeContext* context, const Type* type) {
    DebugTypeEntry* entry;
    size_t entry_index;
    if (!context || !type) return;
    entry = debug_type_find(context, type);
    if (entry) {
        if (entry->collecting) entry->recursive = true;
        return;
    }
    if (type->is_const || type->is_volatile || type->is_restrict ||
        type->is_atomic) {
        Type* base_type = rcc_alloc(sizeof(*base_type));
        entry = debug_type_add(context, type);
        *base_type = *type;
        base_type->is_const = false;
        base_type->is_volatile = false;
        base_type->is_restrict = false;
        base_type->is_atomic = false;
        entry->qualifier_base = base_type;
        debug_type_collect(context, base_type);
        return;
    }
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
        /* Add before visiting fields so self-referential aggregates
         * terminate.  Non-recursive aggregates are moved after their
         * member types, making their DW_AT_type references ready when the
         * DIEs are emitted. */
        entry = debug_type_add(context, type);
        entry->collecting = true;
        for (TypeField* field = type->fields; field; field = field->next) {
            if (field->type) debug_type_collect(context, field->type);
            entry = debug_type_find(context, type);
        }
        entry = debug_type_find(context, type);
        entry->collecting = false;
        if (entry->recursive) return;
        entry_index = (size_t)(entry - context->entries);
        if (entry_index + 1u < context->count) {
            DebugTypeEntry saved = *entry;
            memmove(&context->entries[entry_index],
                    &context->entries[entry_index + 1u],
                    (context->count - entry_index - 1u) *
                        sizeof(*context->entries));
            context->entries[context->count - 1u] = saved;
        }
        return;
    }
    if (type->kind == TYPE_FUNC) {
        /* Function types can recursively mention themselves through a
         * function pointer parameter.  Use the same collecting marker as
         * aggregates so that such types remain a valid opaque DIE instead
         * of recursing forever. */
        entry = debug_type_add(context, type);
        entry->collecting = true;
        debug_type_collect(context, type->ret_type);
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            debug_type_collect(context, parameter->type);
        }
        entry = debug_type_find(context, type);
        entry->collecting = false;
        if (entry->recursive) return;
        entry_index = (size_t)(entry - context->entries);
        if (entry_index + 1u < context->count) {
            DebugTypeEntry saved = *entry;
            memmove(&context->entries[entry_index],
                    &context->entries[entry_index + 1u],
                    (context->count - entry_index - 1u) *
                        sizeof(*context->entries));
            context->entries[context->count - 1u] = saved;
        }
        return;
    }
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY ||
        type->kind == TYPE_VECTOR) {
        debug_type_collect(context, type->base);
    }
    debug_type_add(context, type);
}

static void debug_collect_stmt_types(DebugTypeContext* context,
                                     const Stmt* statement);

static void debug_collect_decl_type(DebugTypeContext* context,
                                    const Decl* declaration) {
    if (!context || !declaration) return;
    debug_type_collect(context, declaration->type);
}

static void debug_collect_catch_types(DebugTypeContext* context,
                                      const CxxCatch* handler) {
    for (; handler; handler = handler->next) {
        debug_collect_decl_type(context, handler->parameter);
        debug_collect_stmt_types(context, handler->body);
    }
}

static void debug_collect_stmt_types(DebugTypeContext* context,
                                     const Stmt* statement) {
    const StmtList* item;
    if (!context || !statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                debug_collect_stmt_types(context, item->stmt);
            }
            break;
        case STMT_IF:
            debug_collect_stmt_types(context, statement->if_then);
            debug_collect_stmt_types(context, statement->if_else);
            break;
        case STMT_WHILE:
        case STMT_DO:
            debug_collect_stmt_types(context, statement->while_body);
            break;
        case STMT_FOR:
            debug_collect_stmt_types(context, statement->for_init);
            debug_collect_stmt_types(context, statement->for_body);
            break;
        case STMT_SWITCH:
            debug_collect_stmt_types(context, statement->switch_body);
            break;
        case STMT_CASE:
            debug_collect_stmt_types(context, statement->case_stmt);
            break;
        case STMT_DEFAULT:
            debug_collect_stmt_types(context, statement->default_stmt);
            break;
        case STMT_LABEL:
            debug_collect_stmt_types(context, statement->label_stmt);
            break;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR &&
                (!statement->decl->var_is_global ||
                 statement->decl->var_is_static_local)) {
                debug_collect_decl_type(context, statement->decl);
            }
            break;
        case STMT_TRY:
            debug_collect_stmt_types(context, statement->try_body);
            debug_collect_catch_types(context, statement->try_catches);
            break;
        default:
            break;
    }
}

static void debug_collect_function_types(DebugTypeContext* context,
                                         const Decl* function) {
    if (!context || !function) return;
    if (function->type && function->type->kind == TYPE_FUNC) {
        debug_type_collect(context, function->type->ret_type);
    }
    debug_type_collect(context, function->type);
    debug_type_collect(context, function->func_method_owner);
    debug_collect_decl_type(context, function->func_this_param);
    for (const DeclList* parameter = function->func_params; parameter;
         parameter = parameter->next) {
        debug_collect_decl_type(context, parameter->decl);
    }
    debug_collect_stmt_types(context, function->func_body);
}

static int debug_line_file_index(const char* const* files, int file_count,
                                 const char* file);

static void debug_file_add(const char*** files, int* file_count,
                           size_t* file_capacity, const char* file) {
    size_t capacity;
    if (!files || !*files || !file_count || !file_capacity || !file ||
        file[0] == '\0' || debug_line_file_index(*files, *file_count, file) != 0) {
        return;
    }
    if ((size_t)*file_count == *file_capacity) {
        capacity = *file_capacity == 0u ? 16u : *file_capacity * 2u;
        if (capacity < (size_t)*file_count ||
            capacity > SIZE_MAX / sizeof(**files)) {
            rcc_fatal("DWARF file table is too large");
        }
        *files = rcc_realloc((void*)*files, capacity * sizeof(**files));
        *file_capacity = capacity;
    }
    (*files)[(*file_count)++] = file;
}

static void debug_collect_stmt_files(const Stmt* statement,
                                     const char*** files, int* file_count,
                                     size_t* file_capacity);

static void debug_collect_decl_file(const Decl* declaration,
                                    const char*** files, int* file_count,
                                    size_t* file_capacity) {
    if (!declaration) return;
    debug_file_add(files, file_count, file_capacity,
                   declaration->loc.filename);
}

static void debug_collect_catch_files(const CxxCatch* handler,
                                      const char*** files, int* file_count,
                                      size_t* file_capacity) {
    for (; handler; handler = handler->next) {
        debug_collect_decl_file(handler->parameter, files, file_count,
                                file_capacity);
        debug_collect_stmt_files(handler->body, files, file_count,
                                 file_capacity);
    }
}

static void debug_collect_stmt_files(const Stmt* statement,
                                     const char*** files, int* file_count,
                                     size_t* file_capacity) {
    const StmtList* item;
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                debug_collect_stmt_files(item->stmt, files, file_count,
                                         file_capacity);
            }
            break;
        case STMT_IF:
            debug_collect_stmt_files(statement->if_then, files, file_count,
                                     file_capacity);
            debug_collect_stmt_files(statement->if_else, files, file_count,
                                     file_capacity);
            break;
        case STMT_WHILE:
        case STMT_DO:
            debug_collect_stmt_files(statement->while_body, files, file_count,
                                     file_capacity);
            break;
        case STMT_FOR:
            debug_collect_stmt_files(statement->for_init, files, file_count,
                                     file_capacity);
            debug_collect_stmt_files(statement->for_body, files, file_count,
                                     file_capacity);
            break;
        case STMT_SWITCH:
            debug_collect_stmt_files(statement->switch_body, files, file_count,
                                     file_capacity);
            break;
        case STMT_CASE:
            debug_collect_stmt_files(statement->case_stmt, files, file_count,
                                     file_capacity);
            break;
        case STMT_DEFAULT:
            debug_collect_stmt_files(statement->default_stmt, files, file_count,
                                     file_capacity);
            break;
        case STMT_LABEL:
            debug_collect_stmt_files(statement->label_stmt, files, file_count,
                                     file_capacity);
            break;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR &&
                (!statement->decl->var_is_global ||
                 statement->decl->var_is_static_local)) {
                debug_collect_decl_file(statement->decl, files, file_count,
                                        file_capacity);
            }
            break;
        case STMT_TRY:
            debug_collect_stmt_files(statement->try_body, files, file_count,
                                     file_capacity);
            debug_collect_catch_files(statement->try_catches, files,
                                      file_count, file_capacity);
            break;
        default:
            break;
    }
}

static void debug_collect_function_files(const Decl* function,
                                         const char*** files, int* file_count,
                                         size_t* file_capacity) {
    if (!function) return;
    debug_collect_decl_file(function->func_this_param, files, file_count,
                            file_capacity);
    for (const DeclList* parameter = function->func_params; parameter;
         parameter = parameter->next) {
        debug_collect_decl_file(parameter->decl, files, file_count,
                                file_capacity);
    }
    debug_collect_stmt_files(function->func_body, files, file_count,
                             file_capacity);
}

static const char* debug_type_name(const Type* type) {
    if (!type) return "<missing type>";
    switch (type->kind) {
        case TYPE_VOID: return "void";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return type->is_unsigned ? "unsigned char" : "char";
        case TYPE_SHORT: return type->is_unsigned ? "unsigned short" : "short";
        case TYPE_INT: return type->is_unsigned ? "unsigned int" : "int";
        case TYPE_LONG: return type->is_unsigned ? "unsigned long" : "long";
        case TYPE_LLONG:
            return type->is_unsigned ? "unsigned long long" : "long long";
        case TYPE_FLOAT: return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_PTR: return "pointer";
        case TYPE_ARRAY: return "array";
        case TYPE_VECTOR: return "vector";
        case TYPE_FUNC: return "function";
        case TYPE_STRUCT:
        case TYPE_UNION:
            return type->tag ? type->tag : "anonymous aggregate";
        case TYPE_ENUM:
            return type->enum_tag ? type->enum_tag : "anonymous enum";
        case TYPE_NULLPTR: return "std::nullptr_t";
        default: return "<unknown type>";
    }
}

static uint8_t debug_type_encoding(const Type* type) {
    if (!type) return 0xffu;
    switch (type->kind) {
        case TYPE_VOID: return 0x00u;       /* DW_ATE_void */
        case TYPE_BOOL: return 0x02u;       /* DW_ATE_boolean */
        case TYPE_FLOAT:
        case TYPE_DOUBLE: return 0x04u;     /* DW_ATE_float */
        case TYPE_CHAR:
            return type->is_unsigned ? 0x08u : 0x06u; /* unsigned/signed char */
        case TYPE_SHORT:
        case TYPE_INT:
        case TYPE_LONG:
        case TYPE_LLONG:
        case TYPE_ENUM:
        case TYPE_NULLPTR:
            return type->is_unsigned ? 0x07u : 0x05u; /* unsigned/signed */
        default: return 0xffu;
    }
}

static void debug_emit_type_dies(ObjSection* info, ObjSection* strings,
                                 DebugTypeContext* context,
                                 DebugTypePatchContext* patches) {
    if (!info || !strings || !context) return;
    for (size_t index = 0u; index < context->count; ++index) {
        DebugTypeEntry* entry = &context->entries[index];
        const Type* type = entry->type;
        if (info->size > UINT32_MAX) {
            rcc_fatal("DWARF type DIE offset exceeds 32-bit range");
        }
        entry->offset = (uint32_t)info->size;
        if (entry->qualifier_base) {
            DebugTypeEntry* base =
                debug_type_find(context, entry->qualifier_base);
            if (!base) {
                rcc_fatal("DWARF qualified base type was not collected");
                return;
            }
            if (type->is_const) {
                section_add_byte(info, 20u);  /* DW_TAG_const_type */
            } else if (type->is_volatile) {
                section_add_byte(info, 21u);  /* DW_TAG_volatile_type */
            } else if (type->is_restrict) {
                section_add_byte(info, 22u);  /* DW_TAG_restrict_type */
            } else {
                section_add_byte(info, 23u);  /* DW_TAG_atomic_type */
            }
            debug_type_ref(info, patches, base);
        } else if (type->kind == TYPE_PTR) {
            section_add_byte(info, 6u);        /* DW_TAG_pointer_type */
            section_add_byte(info, (uint8_t)(type->size > 255 ? 255 :
                                             (type->size < 0 ? 0 : type->size)));
            DebugTypeEntry* base = debug_type_find(context, type->base);
            debug_type_ref(info, patches, base);
        } else if (type->kind == TYPE_ARRAY || type->kind == TYPE_VECTOR) {
            DebugTypeEntry* base = debug_type_find(context, type->base);
            section_add_byte(info, 10u);       /* DW_TAG_array_type */
            debug_type_ref(info, patches, base);
            if (type->array_len >= 0) {
                section_add_byte(info, 11u);   /* DW_TAG_subrange_type */
                debug_line_u32(info, type->array_len > 0
                                     ? (uint32_t)type->array_len - 1u : 0u);
                section_add_byte(info, 0u);
            }
            section_add_byte(info, 0u);
        } else if (type->kind == TYPE_FUNC) {
            DebugTypeEntry* return_type =
                debug_type_find(context, type->ret_type);
            if (!return_type) {
                rcc_fatal("DWARF function return type was not collected");
                return;
            }
            section_add_byte(info, 18u);       /* DW_TAG_subroutine_type */
            debug_type_ref(info, patches, return_type);
            for (TypeParam* parameter = type->params; parameter;
                 parameter = parameter->next) {
                DebugTypeEntry* parameter_type =
                    debug_type_find(context, parameter->type);
                if (!parameter_type) {
                    rcc_fatal(
                        "DWARF function parameter type was not collected");
                    return;
                }
                section_add_byte(info, 19u);   /* DW_TAG_formal_parameter */
                debug_type_ref(info, patches, parameter_type);
            }
            section_add_byte(info, 0u);
        } else if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
            section_add_byte(info, type->kind == TYPE_UNION ? 13u : 12u);
            debug_line_u32(info, debug_str_add(strings, debug_type_name(type)));
            debug_line_u32(info, (uint32_t)(type->size < 0 ? 0 : type->size));
            for (TypeField* field = type->fields; field; field = field->next) {
                DebugTypeEntry* field_type;
                uint64_t data_bit_offset;
                if (!field->name || !field->type) continue;
                field_type = debug_type_find(context, field->type);
                if (!field_type) {
                    rcc_fatal("DWARF aggregate field type was not collected");
                    return;
                }
                section_add_byte(info, field->is_bitfield ? 17u : 14u);
                debug_line_u32(info, debug_str_add(strings, field->name));
                debug_type_ref(info, patches, field_type);
                debug_expr_member_location(info, field->offset);
                if (field->is_bitfield) {
                    if (field->offset < 0 ||
                        (uint64_t)field->offset > UINT32_MAX / 8u ||
                        (uint64_t)field->offset * 8u + field->bit_offset >
                            UINT32_MAX) {
                        rcc_fatal("DWARF bit-field offset exceeds 32-bit range");
                        return;
                    }
                    data_bit_offset = (uint64_t)field->offset * 8u +
                                      field->bit_offset;
                    debug_line_u32(info, field->bit_width);
                    debug_line_u32(info, (uint32_t)data_bit_offset);
                }
                if (field->cxx_access > 2u) {
                    rcc_fatal("DWARF member accessibility is invalid");
                    return;
                }
                /* DW_ACCESS_public/protected/private are 1/2/3, while the
                 * AST stores these access levels as 0/1/2. */
                section_add_byte(info, (uint8_t)(field->cxx_access + 1u));
            }
            section_add_byte(info, 0u);
        } else if (type->kind == TYPE_ENUM) {
            section_add_byte(info, 15u);      /* DW_TAG_enumeration_type */
            debug_line_u32(info, debug_str_add(strings, debug_type_name(type)));
            section_add_byte(info, (uint8_t)(type->size > 255 ? 255 :
                                             (type->size < 0 ? 0 : type->size)));
            for (int constant = 0; constant < type->enum_constant_count;
                 ++constant) {
                EnumConstantInfo* item = &type->enum_constants[constant];
                if (!item->name) continue;
                section_add_byte(info, 16u); /* DW_TAG_enumerator */
                debug_line_u32(info, debug_str_add(strings, item->name));
                debug_line_sleb(info, item->value);
            }
            section_add_byte(info, 0u);
        } else if (debug_type_encoding(type) != 0xffu) {
            section_add_byte(info, 5u);        /* DW_TAG_base_type */
            debug_line_u32(info, debug_str_add(strings, debug_type_name(type)));
            section_add_byte(info, (uint8_t)(type->size > 255 ? 255 :
                                             (type->size < 0 ? 0 : type->size)));
            section_add_byte(info, debug_type_encoding(type));
        } else {
            /* Preserve the real byte size and source spelling without
             * pretending that aggregates/functions have scalar encoding. */
            section_add_byte(info, 7u);        /* DW_TAG_unspecified_type */
            debug_line_u32(info, debug_str_add(strings, debug_type_name(type)));
            section_add_byte(info, (uint8_t)(type->size > 255 ? 255 :
                                             (type->size < 0 ? 0 : type->size)));
        }
    }
    debug_type_patches_apply(info, patches);
}

static void debug_expr_breg(ObjSection* section, int architecture,
                            int64_t offset, bool dereference) {
    uint8_t expression[16];
    size_t size = 1u;
    int64_t value = offset;
    bool more;

    expression[0] = architecture == ARCH_X64 ? 0x76u : 0x75u;
    do {
        uint8_t byte = (uint8_t)((uint64_t)value & 0x7fu);
        value >>= 7;
        more = !((value == 0 && (byte & 0x40u) == 0u) ||
                 (value == -1 && (byte & 0x40u) != 0u));
        if (more) byte |= 0x80u;
        if (size + 1u >= sizeof(expression)) {
            rcc_fatal("DWARF variable location expression is too large");
            return;
        }
        expression[size++] = byte;
    } while (more);
    if (dereference) {
        if (size >= sizeof(expression)) {
            rcc_fatal("DWARF VLA location expression is too large");
            return;
        }
        expression[size++] = 0x06u; /* DW_OP_deref: load saved VLA address */
    }
    debug_line_uleb(section, (uint64_t)size);
    section_add_bytes(section, expression, size);
}

static void debug_expr_addr(ObjectFile* obj, ObjSection* info,
                            int info_section, const char* symbol,
                            int architecture) {
    uint64_t address_offset;
    uint32_t address_size = architecture == ARCH_X64 ? 8u : 4u;

    if (!obj || !info || info_section < 0 || !symbol || symbol[0] == '\0') {
        rcc_fatal("DWARF global variable location is missing a symbol");
        return;
    }
    debug_line_uleb(info, (uint64_t)address_size + 1u);
    section_add_byte(info, 0x03u); /* DW_OP_addr */
    address_offset = info->size;
    for (uint32_t byte = 0u; byte < address_size; ++byte) {
        section_add_byte(info, 0u);
    }
    objfile_add_reloc(obj, info_section, address_offset, symbol,
                      address_size == 8u ? RELOC_ABS64 : RELOC_ABS32U, 0);
}

static void debug_expr_member_location(ObjSection* info, int offset) {
    uint64_t value = offset < 0 ? 0u : (uint64_t)offset;
    debug_line_uleb(info, 2u);
    section_add_byte(info, 0x23u); /* DW_OP_plus_uconst */
    debug_line_uleb(info, value);
}

static void debug_emit_variable_die(ObjSection* info, ObjSection* strings,
                                    DebugTypeContext* types,
                                    const char* const* files, int file_count,
                                    const Module* mod,
                                    const Decl* declaration,
                                    uint8_t abbreviation, int architecture) {
    DebugTypeEntry* type_entry;
    int file_index;
    int64_t frame_offset;
    bool has_frame_location = true;
    if (!info || !strings || !declaration || !declaration->name ||
        declaration->name[0] == '\0') return;
    type_entry = debug_type_find(types, declaration->type);
    if (!type_entry) {
        rcc_fatal("DWARF variable type was not collected");
        return;
    }
    file_index = debug_line_file_index(files, file_count,
                                       declaration->loc.filename);
    frame_offset = declaration->var_offset;
    if (mod && mod->debug_verified_backend) {
        has_frame_location = false;
        for (size_t index = 0u;
             index < mod->debug_variable_location_count; ++index) {
            const DebugVariableLocation* location =
                &mod->debug_variable_locations[index];
            if (location->declaration == declaration) {
                frame_offset = location->frame_offset;
                has_frame_location = true;
                break;
            }
        }
        if (!has_frame_location) {
            if (abbreviation == 3u) abbreviation = 31u;
            else if (abbreviation == 4u) abbreviation = 32u;
            else if (abbreviation == 27u) abbreviation = 33u;
            else return;
        }
    }
    section_add_byte(info, abbreviation);
    debug_line_u32(info, debug_str_add(strings, declaration->name));
    debug_line_u32(info, type_entry->offset);
    debug_line_u32(info, (uint32_t)file_index);
    debug_line_u32(info, declaration->loc.line);
    debug_line_u32(info, declaration->loc.column);
    if (has_frame_location) {
        debug_expr_breg(info, architecture, frame_offset,
                        declaration->var_is_vla);
    }
}

static void debug_emit_global_variable_die(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const ModuleSymbol* symbol, const Decl* declaration,
    const char* filename, int info_section, int architecture) {
    DebugTypeEntry* type_entry;
    const char* symbol_name;
    char* scoped_name = NULL;
    int file_index;

    if (!obj || !info || !strings || !types || !mod || !symbol ||
        !declaration || declaration->kind != DECL_VAR ||
        !declaration->name || declaration->name[0] == '\0') {
        return;
    }
    type_entry = debug_type_find(types, declaration->type);
    if (!type_entry) {
        rcc_fatal("DWARF global variable type was not collected");
        return;
    }
    file_index = debug_line_file_index(files, file_count,
                                       declaration->loc.filename);
    section_add_byte(info, 9u);
    debug_line_u32(info, debug_str_add(strings, declaration->name));
    debug_line_u32(info, type_entry->offset);
    debug_line_u32(info, (uint32_t)file_index);
    debug_line_u32(info, declaration->loc.line);
    debug_line_u32(info, declaration->loc.column);
    section_add_byte(info, symbol->is_global ? 1u : 0u);
    debug_line_u32(info, debug_str_add(strings, symbol->name));
    symbol_name = symbol->name;
    if (!symbol->is_global) {
        scoped_name = module_scoped_symbol(filename, symbol->name);
        symbol_name = scoped_name;
    }
    debug_expr_addr(obj, info, info_section, symbol_name, architecture);
    rcc_free(scoped_name);
}

static void debug_emit_stmt_locals(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, int info_section,
    const ModuleSymbol* function, const Stmt* statement, int architecture);

static int debug_line_file_index(const char* const* files, int file_count,
                                 const char* file);

static int objfile_section_index(const ObjectFile* object,
                                 const ObjSection* target);

static size_t debug_statement_range_count(const Stmt* statement) {
    size_t count = 0u;
    for (const StmtDebugRange* range = statement
             ? statement->debug_code_ranges : NULL;
         range; range = range->next) {
        ++count;
    }
    return count;
}

static uint32_t debug_emit_statement_ranges(
    ObjectFile* obj, const char* filename, const ModuleSymbol* function,
    const Stmt* statement, ObjSection* ranges, int architecture) {
    const char* symbol_name;
    char* scoped_name = NULL;
    int ranges_section;
    uint32_t range_offset;
    int address_size = architecture == ARCH_X64 ? 8 : 4;
    if (!obj || !filename || !function || !statement || !ranges) {
        rcc_fatal("DWARF range-list inputs are incomplete");
        return 0u;
    }
    if (ranges->size > UINT32_MAX) {
        rcc_fatal("DWARF range-list section exceeds 32-bit offsets");
        return 0u;
    }
    ranges_section = objfile_section_index(obj, ranges);
    if (ranges_section < 0) {
        rcc_fatal("DWARF range-list section is detached");
        return 0u;
    }
    range_offset = (uint32_t)ranges->size;
    symbol_name = function->name;
    if (!function->is_global) {
        scoped_name = module_scoped_symbol(filename, function->name);
        symbol_name = scoped_name;
    }
    for (const StmtDebugRange* range = statement->debug_code_ranges;
         range; range = range->next) {
        uint64_t start_offset;
        uint64_t end_offset;
        if (range->end <= range->start ||
            range->start < function->offset ||
            range->end < function->offset) {
            rcc_fatal("DWARF statement range is outside its function");
            rcc_free(scoped_name);
            return 0u;
        }
        start_offset = ranges->size;
        debug_line_address(ranges, 0u, address_size);
        objfile_add_reloc(
            obj, ranges_section, start_offset, symbol_name,
            address_size == 8 ? RELOC_ABS64 : RELOC_ABS32U,
            (int64_t)(range->start - function->offset));
        end_offset = ranges->size;
        debug_line_address(ranges, 0u, address_size);
        objfile_add_reloc(
            obj, ranges_section, end_offset, symbol_name,
            address_size == 8 ? RELOC_ABS64 : RELOC_ABS32U,
            (int64_t)(range->end - function->offset));
    }
    debug_line_address(ranges, 0u, address_size);
    debug_line_address(ranges, 0u, address_size);
    rcc_free(scoped_name);
    return range_offset;
}

static void debug_emit_lexical_block_die(
    ObjectFile* obj, ObjSection* info, const char* const* files, int file_count,
    const Module* mod, const char* filename, int info_section,
    const ModuleSymbol* function, const Stmt* statement, int architecture) {
    const char* symbol_name;
    char* scoped_name = NULL;
    int file_index;
    ObjSection* ranges = NULL;
    uint64_t address_offset;
    uint32_t range_offset = 0u;
    uint32_t range_size;
    size_t range_count;
    if (!obj || !info || !mod || !function || !statement) return;
    file_index = debug_line_file_index(files, file_count,
                                       statement->loc.filename);
    if (statement->debug_code_end < statement->debug_code_start ||
        statement->debug_code_start < function->offset) {
        rcc_fatal("DWARF lexical block code range is invalid");
        return;
    }
    range_size = statement->debug_code_end - statement->debug_code_start;
    range_count = debug_statement_range_count(statement);
    if (range_count > 0u) {
        ranges = objfile_get_section(obj, ".debug_ranges");
        if (!ranges) {
            ranges = objfile_add_section(
                obj, ".debug_ranges", SECT_DEBUG_RANGES, 0u);
            ranges->align = 1u;
        }
        range_offset = debug_emit_statement_ranges(
            obj, filename, function, statement, ranges, architecture);
    }
    symbol_name = function->name;
    if (!function->is_global) {
        scoped_name = module_scoped_symbol(filename, function->name);
        symbol_name = scoped_name;
    }
    if (range_count > 0u) {
        section_add_byte(info, 25u);
        debug_line_u32(info, range_offset);
    } else {
        section_add_byte(info, 24u);
        address_offset = info->size;
        for (int byte = 0; byte < (architecture == ARCH_X64 ? 8 : 4);
             ++byte) {
            section_add_byte(info, 0u);
        }
        objfile_add_reloc(
            obj, info_section, address_offset, symbol_name,
            architecture == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U,
            (int64_t)(statement->debug_code_start - function->offset));
        debug_line_u32(info, range_size);
    }
    section_add_byte(info, (uint8_t)file_index);
    debug_line_u32(info, statement->loc.line);
    debug_line_u32(info, statement->loc.column);
    rcc_free(scoped_name);
}

static void debug_emit_catch_locals(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, int info_section,
    const ModuleSymbol* function, const CxxCatch* handler, int architecture) {
    for (; handler; handler = handler->next) {
        if (handler->parameter && handler->parameter->kind == DECL_PARAM) {
            debug_emit_variable_die(info, strings, types,
                                    files, file_count,
                                    mod,
                                    handler->parameter, 3u,
                                    architecture);
        }
        debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                               mod, filename, info_section, function,
                               handler->body, architecture);
    }
}

static void debug_emit_stmt_locals(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, int info_section,
    const ModuleSymbol* function, const Stmt* statement, int architecture) {
    const StmtList* item;
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            {
                bool has_range = !statement->block_no_scope &&
                    statement->debug_code_end > statement->debug_code_start;
                if (has_range) {
                debug_emit_lexical_block_die(
                    obj, info, files, file_count, mod, filename, info_section,
                    function, statement, architecture);
                }
                for (item = statement->block_stmts; item; item = item->next) {
                    debug_emit_stmt_locals(
                        obj, info, strings, types, files, file_count, mod,
                        filename, info_section, function, item->stmt,
                        architecture);
                }
                if (has_range) section_add_byte(info, 0u);
            }
            break;
        case STMT_IF:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->if_then, architecture);
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->if_else, architecture);
            break;
        case STMT_WHILE:
        case STMT_DO:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->while_body, architecture);
            break;
        case STMT_FOR:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->for_init, architecture);
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->for_body, architecture);
            break;
        case STMT_SWITCH:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->switch_body, architecture);
            break;
        case STMT_CASE:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->case_stmt, architecture);
            break;
        case STMT_DEFAULT:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->default_stmt, architecture);
            break;
        case STMT_LABEL:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->label_stmt, architecture);
            break;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR &&
                !statement->decl->var_is_global &&
                !statement->decl->var_is_static_local) {
                debug_emit_variable_die(info, strings, types,
                                        files, file_count,
                                        mod,
                                        statement->decl, 4u,
                                        architecture);
            }
            break;
        case STMT_TRY:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->try_body, architecture);
            debug_emit_catch_locals(obj, info, strings, types, files, file_count,
                                    mod, filename, info_section, function,
                                    statement->try_catches, architecture);
            break;
        default:
            break;
    }
}

static Decl* debug_find_function_decl(const Module* mod,
                                      const ModuleSymbol* symbol) {
    if (!mod || !mod->debug_ast || !symbol || !symbol->name) return NULL;
    for (DeclList* item = mod->debug_ast->decls; item; item = item->next) {
        Decl* declaration = item->decl;
        const char* link_name;
        if (!declaration || declaration->kind != DECL_FUNC ||
            !declaration->func_body) {
            continue;
        }
        link_name = declaration->link_name
                        ? declaration->link_name : declaration->name;
        if (!link_name || strcmp(link_name, symbol->name) != 0) continue;
        return declaration;
    }
    return NULL;
}

static const TypeMethod* debug_find_function_method(
    const Decl* declaration) {
    if (!declaration || !declaration->func_is_cxx_method ||
        !declaration->func_method_owner ||
        (declaration->func_method_owner->kind != TYPE_STRUCT &&
         declaration->func_method_owner->kind != TYPE_UNION)) {
        return NULL;
    }
    for (TypeMethod* method = declaration->func_method_owner->methods;
         method; method = method->next) {
        if (method->function_decl == declaration) return method;
    }
    return NULL;
}

static const char* debug_function_source_name(const Decl* declaration,
                                              const char* fallback) {
    const TypeMethod* method = debug_find_function_method(declaration);
    if (method && method->name) return method->name;
    return declaration && declaration->name ? declaration->name : fallback;
}

static uint8_t debug_function_accessibility(const TypeMethod* method) {
    if (method->cxx_access > 2u) {
        rcc_fatal("DWARF member-function accessibility is invalid");
    }
    /* DWARF uses 1=public, 2=protected, 3=private. */
    return (uint8_t)(method->cxx_access + 1u);
}

static const ModuleSymbol* debug_find_global_symbol(const Module* mod,
                                                     const Decl* declaration) {
    const char* link_name;
    if (!mod || !declaration || declaration->kind != DECL_VAR ||
        !declaration->var_is_global) return NULL;
    link_name = declaration->link_name
                    ? declaration->link_name : declaration->name;
    if (!link_name || link_name[0] == '\0') return NULL;
    for (int index = 0; index < mod->symbol_count; ++index) {
        const ModuleSymbol* symbol = &mod->symbols[index];
        if (!symbol->is_defined || strcmp(symbol->name, link_name) != 0 ||
            (symbol->section != MODULE_SYMBOL_DATA &&
             symbol->section != MODULE_SYMBOL_BSS)) {
            continue;
        }
        return symbol;
    }
    return NULL;
}

static const ModuleSymbol* debug_find_static_local_symbol(
    const Module* mod, const Decl* declaration) {
    const char* link_name;
    if (!mod || !declaration || !declaration->var_is_static_local) return NULL;
    link_name = declaration->link_name
                    ? declaration->link_name : declaration->name;
    if (!link_name || link_name[0] == '\0') return NULL;
    for (int index = 0; index < mod->symbol_count; ++index) {
        const ModuleSymbol* symbol = &mod->symbols[index];
        if (!symbol->is_defined || strcmp(symbol->name, link_name) != 0 ||
            (symbol->section != MODULE_SYMBOL_DATA &&
             symbol->section != MODULE_SYMBOL_BSS)) {
            continue;
        }
        return symbol;
    }
    return NULL;
}

static const Decl* debug_find_global_decl(const Module* mod,
                                          const ModuleSymbol* symbol) {
    const Decl* tentative = NULL;
    const Decl* external = NULL;
    if (!mod || !mod->debug_ast || !symbol || !symbol->name) return NULL;
    for (DeclList* item = mod->debug_ast->decls; item; item = item->next) {
        const Decl* declaration = item->decl;
        const char* link_name;
        if (!declaration || declaration->kind != DECL_VAR ||
            !declaration->var_is_global) continue;
        link_name = declaration->link_name
                        ? declaration->link_name : declaration->name;
        if (!link_name || strcmp(link_name, symbol->name) != 0) continue;
        if (declaration->var_init) return declaration;
        if (declaration->storage != STORAGE_EXTERN) {
            if (!tentative) tentative = declaration;
        } else if (!external) {
            external = declaration;
        }
    }
    return tentative ? tentative : external;
}

static void debug_emit_static_local_stmt(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, int info_section,
    const Stmt* statement, int architecture) {
    const StmtList* item;
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                debug_emit_static_local_stmt(
                    obj, info, strings, types, files, file_count, mod,
                    filename, info_section, item->stmt, architecture);
            }
            break;
        case STMT_IF:
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->if_then, architecture);
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->if_else, architecture);
            break;
        case STMT_WHILE:
        case STMT_DO:
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->while_body, architecture);
            break;
        case STMT_FOR:
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->for_init, architecture);
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->for_body, architecture);
            break;
        case STMT_SWITCH:
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->switch_body, architecture);
            break;
        case STMT_CASE:
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->case_stmt, architecture);
            break;
        case STMT_DEFAULT:
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->default_stmt, architecture);
            break;
        case STMT_LABEL:
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->label_stmt, architecture);
            break;
        case STMT_TRY:
            debug_emit_static_local_stmt(
                obj, info, strings, types, files, file_count, mod, filename,
                info_section, statement->try_body, architecture);
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                debug_emit_static_local_stmt(
                    obj, info, strings, types, files, file_count, mod,
                    filename, info_section, handler->body, architecture);
            }
            break;
        case STMT_DECL: {
            const Decl* declaration = statement->decl;
            const ModuleSymbol* symbol = debug_find_static_local_symbol(
                mod, declaration);
            if (symbol && declaration->loc.filename &&
                declaration->loc.filename[0] != '\0' &&
                declaration->loc.line != 0) {
                debug_emit_global_variable_die(
                    obj, info, strings, types, files, file_count, mod, symbol,
                    declaration, filename, info_section, architecture);
            }
            break;
        }
        default:
            break;
    }
}

static void debug_emit_function_locals(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, int info_section,
    const ModuleSymbol* symbol, int architecture) {
    if (!mod || !mod->debug_statement_ranges) return;
    Decl* function = debug_find_function_decl(mod, symbol);
    if (!function) return;
    if (function->func_this_param) {
        debug_emit_variable_die(info, strings, types, files, file_count, mod,
                                function->func_this_param, 27u,
                                architecture);
        section_add_byte(info, 1u); /* DW_AT_artificial */
    }
    for (DeclList* parameter = function->func_params; parameter;
         parameter = parameter->next) {
        debug_emit_variable_die(info, strings, types, files, file_count, mod,
                                parameter->decl, 3u,
                                architecture);
    }
    debug_emit_static_local_stmt(
        obj, info, strings, types, files, file_count, mod, filename,
        info_section, function->func_body, architecture);
    debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                           mod, filename, info_section, symbol,
                           function->func_body, architecture);
}

static int debug_line_file_index(const char* const* files, int file_count,
                                 const char* file) {
    for (int index = 0; index < file_count; ++index) {
        if (strcmp(files[index], file) == 0) return index + 1;
    }
    return 0;
}

static int objfile_section_index(const ObjectFile* object,
                                 const ObjSection* target) {
    int index = 0;
    for (const ObjSection* section = object->sections; section;
         section = section->next, ++index) {
        if (section == target) return index;
    }
    return -1;
}

typedef struct DebugLinePoint {
    const ModuleSymbol* function;
    const char* source_file;
    uint32_t offset;
    uint32_t line;
    uint32_t column;
    bool is_function;
} DebugLinePoint;

static void debug_line_point_add(
    DebugLinePoint** points, int* point_count, size_t* point_capacity,
    const ModuleSymbol* function, const char* source_file, uint32_t offset,
    uint32_t line, uint32_t column, bool is_function) {
    if (!points || !*points || !point_count || !point_capacity ||
        !function || !source_file || source_file[0] == '\0' || line == 0u) {
        return;
    }
    if ((size_t)*point_count >= *point_capacity) {
        size_t next_capacity = *point_capacity < 16u
            ? 16u : *point_capacity * 2u;
        *points = rcc_realloc(*points,
                              next_capacity * sizeof(**points));
        *point_capacity = next_capacity;
    }
    (*points)[*point_count].function = function;
    (*points)[*point_count].source_file = source_file;
    (*points)[*point_count].offset = offset;
    (*points)[*point_count].line = line;
    (*points)[*point_count].column = column;
    (*points)[*point_count].is_function = is_function;
    ++*point_count;
}

static void debug_collect_line_stmt_points(
    const Module* mod, const Stmt* statement,
    const ModuleSymbol* function,
    DebugLinePoint** points, int* point_count, size_t* point_capacity) {
    const StmtDebugRange* debug_range;
    if (!statement) return;
    if (statement->debug_code_ranges) {
        for (debug_range = statement->debug_code_ranges; debug_range;
             debug_range = debug_range->next) {
            if (debug_range->end > debug_range->start) {
                debug_line_point_add(
                    points, point_count, point_capacity, function,
                    statement->loc.filename, debug_range->start,
                    statement->loc.line, statement->loc.column, false);
            }
        }
    } else if (mod && mod->debug_legacy_statement_lines &&
               statement->kind != STMT_BLOCK &&
               statement->debug_line_valid) {
        debug_line_point_add(
            points, point_count, point_capacity, function,
            statement->loc.filename, statement->debug_line_offset,
            statement->loc.line, statement->loc.column, false);
    } else if (statement->kind == STMT_BLOCK &&
               !statement->block_no_scope &&
               statement->debug_code_end > statement->debug_code_start) {
        /* Keep the classic coarse block range as a compatibility fallback
         * for callers that have not exposed exact encoder envelopes. */
        debug_line_point_add(
            points, point_count, point_capacity, function,
            statement->loc.filename, statement->debug_code_start,
            statement->loc.line, statement->loc.column, false);
    }
    switch (statement->kind) {
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                debug_collect_line_stmt_points(
                    mod, item->stmt, function, points, point_count,
                    point_capacity);
            }
            break;
        case STMT_IF:
            debug_collect_line_stmt_points(
                mod, statement->if_then, function, points, point_count,
                point_capacity);
            debug_collect_line_stmt_points(
                mod, statement->if_else, function, points, point_count,
                point_capacity);
            break;
        case STMT_WHILE:
        case STMT_DO:
            debug_collect_line_stmt_points(
                mod, statement->while_body, function, points, point_count,
                point_capacity);
            break;
        case STMT_FOR:
            debug_collect_line_stmt_points(
                mod, statement->for_init, function, points, point_count,
                point_capacity);
            debug_collect_line_stmt_points(
                mod, statement->for_body, function, points, point_count,
                point_capacity);
            break;
        case STMT_SWITCH:
            debug_collect_line_stmt_points(
                mod, statement->switch_body, function, points, point_count,
                point_capacity);
            break;
        case STMT_CASE:
            debug_collect_line_stmt_points(
                mod, statement->case_stmt, function, points, point_count,
                point_capacity);
            break;
        case STMT_DEFAULT:
            debug_collect_line_stmt_points(
                mod, statement->default_stmt, function, points, point_count,
                point_capacity);
            break;
        case STMT_LABEL:
            debug_collect_line_stmt_points(
                mod, statement->label_stmt, function, points, point_count,
                point_capacity);
            break;
        case STMT_TRY:
            debug_collect_line_stmt_points(
                mod, statement->try_body, function, points, point_count,
                point_capacity);
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                debug_collect_line_stmt_points(
                    mod, handler->body, function, points, point_count,
                    point_capacity);
            }
            break;
        default:
            break;
    }
}

static int debug_line_point_compare(const void* left, const void* right) {
    const DebugLinePoint* a = (const DebugLinePoint*)left;
    const DebugLinePoint* b = (const DebugLinePoint*)right;
    int source_compare;
    if (a->offset < b->offset) return -1;
    if (a->offset > b->offset) return 1;
    if (a->function->offset < b->function->offset) return -1;
    if (a->function->offset > b->function->offset) return 1;
    if (a->is_function != b->is_function) return a->is_function ? -1 : 1;
    source_compare = strcmp(a->source_file, b->source_file);
    if (source_compare != 0) return source_compare;
    if (a->line < b->line) return -1;
    if (a->line > b->line) return 1;
    if (a->column < b->column) return -1;
    if (a->column > b->column) return 1;
    return 0;
}

static void module_emit_debug_line(ObjectFile* obj, Module* mod,
                                   const char* filename) {
    const ModuleSymbol** functions;
    DebugLinePoint* points;
    const char** files;
    ObjSection* line;
    int function_count = 0;
    int point_count = 0;
    int file_count = 0;
    size_t file_capacity;
    size_t point_capacity;
    int line_section;
    int address_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
    uint64_t unit_length_offset;
    uint64_t header_length_offset;
    uint64_t header_start;
    uint32_t current_line = 1u;

    if (!g_opts.debug_info || !obj || !mod || mod->symbol_count <= 0) return;
    functions = rcc_alloc((size_t)mod->symbol_count * sizeof(*functions));
    point_capacity = (size_t)mod->symbol_count;
    if (point_capacity == 0u) point_capacity = 1u;
    points = rcc_alloc(point_capacity * sizeof(*points));
    file_capacity = (size_t)mod->symbol_count;
    if (file_capacity == 0u) file_capacity = 1u;
    files = rcc_alloc(file_capacity * sizeof(*files));
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* symbol = &mod->symbols[index];
        if (!symbol->is_defined || symbol->section != MODULE_SYMBOL_CODE ||
            !symbol->source_file || symbol->source_file[0] == '\0' ||
            symbol->source_line == 0u) continue;
        functions[function_count++] = symbol;
        debug_line_point_add(
            &points, &point_count, &point_capacity, symbol,
            symbol->source_file, symbol->offset, symbol->source_line,
            symbol->source_column, true);
        debug_file_add(&files, &file_count, &file_capacity,
                       symbol->source_file);
    }
    for (int index = 0; index < function_count; ++index) {
        Decl* declaration = debug_find_function_decl(mod, functions[index]);
        debug_collect_function_files(
            declaration, &files, &file_count, &file_capacity);
        if (mod->debug_statement_ranges) {
            debug_collect_line_stmt_points(
                mod, declaration ? declaration->func_body : NULL,
                functions[index],
                &points, &point_count, &point_capacity);
        }
    }
    for (int index = 0; index < point_count; ++index) {
        debug_file_add(&files, &file_count, &file_capacity,
                       points[index].source_file);
    }
    if (function_count == 0) {
        rcc_free(functions);
        rcc_free(points);
        rcc_free(files);
        return;
    }
    qsort(points, (size_t)point_count, sizeof(*points),
          debug_line_point_compare);

    line = objfile_add_section(obj, ".debug_line", SECT_DEBUG_LINE, 0u);
    line->align = 1u;
    line_section = objfile_section_index(obj, line);
    if (line_section < 0) rcc_fatal("DWARF line section is detached");

    unit_length_offset = line->size;
    debug_line_u32(line, 0u);
    debug_line_u16(line, 4u);
    header_length_offset = line->size;
    debug_line_u32(line, 0u);
    header_start = line->size;
    section_add_byte(line, 1u);       /* minimum_instruction_length */
    section_add_byte(line, 1u);       /* default_is_stmt */
    section_add_byte(line, (uint8_t)-5); /* line_base */
    section_add_byte(line, 14u);      /* line_range */
    section_add_byte(line, 13u);      /* opcode_base */
    {
        static const uint8_t standard_lengths[] = {
            0u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u
        };
        section_add_bytes(line, standard_lengths, sizeof(standard_lengths));
    }
    section_add_byte(line, 0u);       /* include_directories terminator */
    for (int index = 0; index < file_count; ++index) {
        section_add_bytes(line, (const uint8_t*)files[index],
                          strlen(files[index]) + 1u);
        debug_line_uleb(line, 0u);    /* directory index */
        debug_line_uleb(line, 0u);    /* modification time */
        debug_line_uleb(line, 0u);    /* file size */
    }
    section_add_byte(line, 0u);       /* file_names terminator */
    if (line->size - header_start > UINT32_MAX) {
        rcc_fatal("DWARF line table header is too large");
    }
    debug_line_patch_u32(line, header_length_offset,
                         (uint32_t)(line->size - header_start));

    for (int index = 0; index < point_count; ++index) {
        const DebugLinePoint* point = &points[index];
        const ModuleSymbol* function = point->function;
        const char* symbol_name = function->name;
        char* scoped_name = NULL;
        uint64_t address_offset;
        int file_index = debug_line_file_index(
            files, file_count, point->source_file);
        int64_t line_delta = (int64_t)point->line -
                             (int64_t)current_line;
        if (point->offset < function->offset) {
            rcc_fatal("DWARF line point precedes its function");
        }
        section_add_byte(line, 0u);
        debug_line_uleb(line, (uint64_t)address_size + 1u);
        section_add_byte(line, 2u);    /* DW_LNE_set_address */
        address_offset = line->size;
        for (int byte = 0; byte < address_size; ++byte) {
            section_add_byte(line, 0u);
        }
        if (!function->is_global) {
            scoped_name = module_scoped_symbol(filename, function->name);
            symbol_name = scoped_name;
        }
        objfile_add_reloc(
            obj, line_section, address_offset, symbol_name,
            address_size == 8 ? RELOC_ABS64 : RELOC_ABS32U,
            (int64_t)(point->offset - function->offset));
        rcc_free(scoped_name);
        section_add_byte(line, 4u);    /* DW_LNS_set_file */
        debug_line_uleb(line, (uint64_t)file_index);
        if (point->column > 0u) {
            section_add_byte(line, 5u); /* DW_LNS_set_column */
            debug_line_uleb(line, point->column);
        }
        section_add_byte(line, 3u);    /* DW_LNS_advance_line */
        debug_line_sleb(line, line_delta);
        current_line = point->line;
        section_add_byte(line, 1u);    /* DW_LNS_copy */
    }
    section_add_byte(line, 0u);
    debug_line_uleb(line, 1u);
    section_add_byte(line, 1u);        /* DW_LNE_end_sequence */
    if (line->size - unit_length_offset - 4u > UINT32_MAX) {
        rcc_fatal("DWARF line table is too large");
    }
    debug_line_patch_u32(line, unit_length_offset,
                         (uint32_t)(line->size - unit_length_offset - 4u));
    rcc_free(functions);
    rcc_free(points);
    rcc_free(files);
}

static uint32_t debug_str_add(ObjSection* strings, const char* value) {
    uint64_t offset = 0u;
    size_t length;
    if (!strings || !value) return 0u;
    while (offset < strings->size) {
        const char* current = (const char*)strings->data + offset;
        size_t current_length = strlen(current);
        if (strcmp(current, value) == 0) {
            if (offset > UINT32_MAX) {
                rcc_fatal("DWARF string table exceeds 32-bit offsets");
            }
            return (uint32_t)offset;
        }
        offset += current_length + 1u;
    }
    length = strlen(value) + 1u;
    if (strings->size > UINT32_MAX - length) {
        rcc_fatal("DWARF string table exceeds 32-bit offsets");
    }
    section_add_bytes(strings, (const uint8_t*)value, length);
    return (uint32_t)offset;
}

static uint32_t debug_function_size(const Module* mod,
                                    const ModuleSymbol* function) {
    uint32_t size;
    if (function && function->size != 0u) return function->size;
    if (!mod || !function || function->offset > mod->code.size ||
        mod->code.size - function->offset > UINT32_MAX) return 0u;
    size = (uint32_t)(mod->code.size - function->offset);
    for (int index = 0; index < mod->symbol_count; ++index) {
        const ModuleSymbol* candidate = &mod->symbols[index];
        if (!candidate->is_defined ||
            candidate->section != MODULE_SYMBOL_CODE ||
            !candidate->source_file || candidate->source_line == 0u ||
            candidate->offset <= function->offset) continue;
        if (candidate->offset - function->offset < size) {
            size = candidate->offset - function->offset;
        }
    }
    return size;
}

static uint16_t debug_cxx_language(void) {
    switch (g_opts.cxx_standard) {
        case 11: return 0x001au; /* DW_LANG_C_plus_plus_11 */
        case 14: return 0x0021u; /* DW_LANG_C_plus_plus_14 */
        case 17: return 0x002au; /* DW_LANG_C_plus_plus_17 */
        case 20: return 0x002bu; /* DW_LANG_C_plus_plus_20 */
        default: return 0x0004u; /* DW_LANG_C_plus_plus */
    }
}

static void module_emit_debug_info(ObjectFile* obj, Module* mod,
                                   const char* filename) {
    const ModuleSymbol** functions;
    const char** files;
    DebugTypeContext types = {0};
    DebugTypePatchContext type_patches = {0};
    ObjSection* strings;
    ObjSection* abbrev;
    ObjSection* info;
    int function_count = 0;
    int global_count = 0;
    int file_count = 0;
    size_t file_capacity;
    int info_section;
    uint64_t unit_length_offset;
    uint32_t producer_offset;
    uint32_t unit_name_offset;

    if (!g_opts.debug_info || !obj || !mod || mod->symbol_count <= 0) return;
    functions = rcc_alloc((size_t)mod->symbol_count * sizeof(*functions));
    file_capacity = (size_t)mod->symbol_count;
    if (file_capacity == 0u) file_capacity = 1u;
    files = rcc_alloc(file_capacity * sizeof(*files));
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* symbol = &mod->symbols[index];
        if (!symbol->is_defined || symbol->section != MODULE_SYMBOL_CODE ||
            !symbol->source_file || symbol->source_file[0] == '\0' ||
            symbol->source_line == 0u) continue;
        functions[function_count++] = symbol;
        debug_file_add(&files, &file_count, &file_capacity,
                       symbol->source_file);
    }
    for (DeclList* item = mod->debug_ast ? mod->debug_ast->decls : NULL;
         item; item = item->next) {
        Decl* declaration = item->decl;
        const ModuleSymbol* symbol = debug_find_global_symbol(mod, declaration);
        if (!symbol || debug_find_global_decl(mod, symbol) != declaration ||
            !declaration->loc.filename ||
            declaration->loc.filename[0] == '\0' || declaration->loc.line == 0) {
            continue;
        }
        ++global_count;
        debug_collect_decl_type(&types, declaration);
        debug_file_add(&files, &file_count, &file_capacity,
                       declaration->loc.filename);
    }
    if (function_count == 0 && global_count == 0) {
        rcc_free(functions);
        rcc_free(files);
        return;
    }

    for (int index = 0; index < function_count; ++index) {
        Decl* function = debug_find_function_decl(mod, functions[index]);
        debug_collect_function_types(&types, function);
        debug_collect_function_files(function, &files, &file_count,
                                     &file_capacity);
    }

    strings = objfile_add_section(obj, ".debug_str", SECT_DEBUG_STR, 0u);
    abbrev = objfile_add_section(obj, ".debug_abbrev", SECT_DEBUG_ABBREV, 0u);
    info = objfile_add_section(obj, ".debug_info", SECT_DEBUG_INFO, 0u);
    strings->align = abbrev->align = info->align = 1u;
    section_add_byte(strings, 0u);
    producer_offset = debug_str_add(strings, "RCC");
    unit_name_offset = debug_str_add(strings, filename);
    info_section = objfile_section_index(obj, info);
    if (info_section < 0) rcc_fatal("DWARF info section is detached");

    /* Abbreviation 1: compile unit with producer, language, line table, and
     * source name.  Abbreviation 2: a source-level function DIE.  The
     * DW_AT_inline value records the source declaration property; it does
     * not claim that a call site was actually inlined. */
    debug_line_uleb(abbrev, 1u);
    debug_line_uleb(abbrev, 0x11u);    /* DW_TAG_compile_unit */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x25u);    /* DW_AT_producer */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x13u);    /* DW_AT_language */
    debug_line_uleb(abbrev, 0x05u);    /* DW_FORM_data2 */
    debug_line_uleb(abbrev, 0x10u);    /* DW_AT_stmt_list */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 2u);
    debug_line_uleb(abbrev, 0x2eu);    /* DW_TAG_subprogram */
    section_add_byte(abbrev, 1u);       /* subprogram has child DIEs */
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x11u);    /* DW_AT_low_pc */
    debug_line_uleb(abbrev, 0x01u);    /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0x12u);    /* DW_AT_high_pc */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3fu);    /* DW_AT_external */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0x6eu);    /* DW_AT_linkage_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x40u);    /* DW_AT_frame_base */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x20u);    /* DW_AT_inline */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Abbreviation 8: generated/source-less functions without a recoverable
     * AST return type.  Keep the DIE valid without inventing a void/scalar
     * type reference. */
    debug_line_uleb(abbrev, 8u);
    debug_line_uleb(abbrev, 0x2eu);    /* DW_TAG_subprogram */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x11u);    /* DW_AT_low_pc */
    debug_line_uleb(abbrev, 0x01u);    /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0x12u);    /* DW_AT_high_pc */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3fu);    /* DW_AT_external */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0x6eu);    /* DW_AT_linkage_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x40u);    /* DW_AT_frame_base */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x20u);    /* DW_AT_inline */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 3u);
    debug_line_uleb(abbrev, 0x05u);     /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x3au);     /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3bu);     /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);     /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x02u);     /* DW_AT_location */
    debug_line_uleb(abbrev, 0x18u);     /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 4u);
    debug_line_uleb(abbrev, 0x34u);     /* DW_TAG_variable */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x3au);     /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3bu);     /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);     /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x02u);     /* DW_AT_location */
    debug_line_uleb(abbrev, 0x18u);     /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 9u);
    debug_line_uleb(abbrev, 0x34u);     /* DW_TAG_variable */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x3au);     /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3bu);     /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);     /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3fu);     /* DW_AT_external */
    debug_line_uleb(abbrev, 0x0cu);     /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0x6eu);     /* DW_AT_linkage_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x02u);     /* DW_AT_location */
    debug_line_uleb(abbrev, 0x18u);     /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 5u);
    debug_line_uleb(abbrev, 0x24u);     /* DW_TAG_base_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3eu);     /* DW_AT_encoding */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 6u);
    debug_line_uleb(abbrev, 0x0fu);     /* DW_TAG_pointer_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x0bu);     /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 7u);
    debug_line_uleb(abbrev, 0x3bu);     /* DW_TAG_unspecified_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Abbreviations 10-14 describe the aggregate type DIEs emitted below:
     * arrays with a bounded subrange, structures/unions with children, and
     * ordinary data members with a DW_OP_plus_uconst location. */
    debug_line_uleb(abbrev, 10u);
    debug_line_uleb(abbrev, 0x01u);     /* DW_TAG_array_type */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 11u);
    debug_line_uleb(abbrev, 0x21u);     /* DW_TAG_subrange_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x2fu);     /* DW_AT_upper_bound */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 12u);
    debug_line_uleb(abbrev, 0x13u);     /* DW_TAG_structure_type */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 13u);
    debug_line_uleb(abbrev, 0x17u);     /* DW_TAG_union_type */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 14u);
    debug_line_uleb(abbrev, 0x0du);     /* DW_TAG_member */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x02u);     /* DW_AT_data_member_location */
    debug_line_uleb(abbrev, 0x18u);     /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x32u);     /* DW_AT_accessibility */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 15u);
    debug_line_uleb(abbrev, 0x04u);     /* DW_TAG_enumeration_type */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 16u);
    debug_line_uleb(abbrev, 0x28u);     /* DW_TAG_enumerator */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x1cu);     /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0du);     /* DW_FORM_sdata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 17u);
    debug_line_uleb(abbrev, 0x0du);     /* DW_TAG_member */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x02u);     /* DW_AT_data_member_location */
    debug_line_uleb(abbrev, 0x18u);     /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x0du);     /* DW_AT_bit_size */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x6bu);     /* DW_AT_data_bit_offset */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x32u);     /* DW_AT_accessibility */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 18u);
    debug_line_uleb(abbrev, 0x15u);    /* DW_TAG_subroutine_type */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 19u);
    debug_line_uleb(abbrev, 0x05u);    /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 24u);
    debug_line_uleb(abbrev, 0x0bu);     /* DW_TAG_lexical_block */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x11u);     /* DW_AT_low_pc */
    debug_line_uleb(abbrev, 0x01u);     /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0x12u);     /* DW_AT_high_pc */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3au);     /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3bu);     /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);     /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 25u);
    debug_line_uleb(abbrev, 0x0bu);     /* DW_TAG_lexical_block */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x23u);     /* DW_AT_ranges */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3au);     /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3bu);     /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);     /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 20u);
    debug_line_uleb(abbrev, 0x26u);     /* DW_TAG_const_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 21u);
    debug_line_uleb(abbrev, 0x35u);     /* DW_TAG_volatile_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 22u);
    debug_line_uleb(abbrev, 0x37u);     /* DW_TAG_restrict_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 23u);
    debug_line_uleb(abbrev, 0x47u);     /* DW_TAG_atomic_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Abbreviation 26 is a source-level member function.  Its explicit
     * object-pointer reference names the artificial `this` parameter DIE. */
    debug_line_uleb(abbrev, 26u);
    debug_line_uleb(abbrev, 0x2eu);    /* DW_TAG_subprogram */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x11u);    /* DW_AT_low_pc */
    debug_line_uleb(abbrev, 0x01u);    /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0x12u);    /* DW_AT_high_pc */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3fu);    /* DW_AT_external */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0x6eu);    /* DW_AT_linkage_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x40u);    /* DW_AT_frame_base */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x20u);    /* DW_AT_inline */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x64u);    /* DW_AT_object_pointer */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1du);    /* DW_AT_containing_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Abbreviation 27 marks the synthetic C++ `this` parameter artificial. */
    debug_line_uleb(abbrev, 27u);
    debug_line_uleb(abbrev, 0x05u);    /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x02u);    /* DW_AT_location */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x34u);    /* DW_AT_artificial */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Static C++ member functions have an owning class but no object pointer. */
    debug_line_uleb(abbrev, 28u);
    debug_line_uleb(abbrev, 0x2eu);    /* DW_TAG_subprogram */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x11u);    /* DW_AT_low_pc */
    debug_line_uleb(abbrev, 0x01u);    /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0x12u);    /* DW_AT_high_pc */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3fu);    /* DW_AT_external */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0x6eu);    /* DW_AT_linkage_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x40u);    /* DW_AT_frame_base */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x20u);    /* DW_AT_inline */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x1du);    /* DW_AT_containing_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Member-function variants carry the source access specifier. */
    debug_line_uleb(abbrev, 29u);
    debug_line_uleb(abbrev, 0x2eu);    /* DW_TAG_subprogram */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x11u);    /* DW_AT_low_pc */
    debug_line_uleb(abbrev, 0x01u);    /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0x12u);    /* DW_AT_high_pc */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3fu);    /* DW_AT_external */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0x6eu);    /* DW_AT_linkage_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x40u);    /* DW_AT_frame_base */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x20u);    /* DW_AT_inline */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x64u);    /* DW_AT_object_pointer */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1du);    /* DW_AT_containing_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x32u);    /* DW_AT_accessibility */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 30u);
    debug_line_uleb(abbrev, 0x2eu);    /* DW_TAG_subprogram */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x11u);    /* DW_AT_low_pc */
    debug_line_uleb(abbrev, 0x01u);    /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0x12u);    /* DW_AT_high_pc */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3fu);    /* DW_AT_external */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0x6eu);    /* DW_AT_linkage_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x40u);    /* DW_AT_frame_base */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x20u);    /* DW_AT_inline */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x1du);    /* DW_AT_containing_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x32u);    /* DW_AT_accessibility */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Optimized verified locals retain their source DIE but do not claim a
     * stack location after mem2reg removes the backing alloca. */
    debug_line_uleb(abbrev, 31u);
    debug_line_uleb(abbrev, 0x05u);    /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 32u);
    debug_line_uleb(abbrev, 0x34u);    /* DW_TAG_variable */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 33u);
    debug_line_uleb(abbrev, 0x05u);    /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x34u);    /* DW_AT_artificial */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    section_add_byte(abbrev, 0u);

    unit_length_offset = info->size;
    debug_line_u32(info, 0u);
    debug_line_u16(info, 4u);
    debug_line_u32(info, 0u);          /* .debug_abbrev offset */
    section_add_byte(info, (uint8_t)(g_opts.target_arch == ARCH_X64 ? 8 : 4));
    section_add_byte(info, 1u);        /* compile-unit abbreviation */
    debug_line_u32(info, producer_offset);
    debug_line_u16(info, g_opts.cxx_mode ? debug_cxx_language() : 0x000cu);
                                      /* C++ standard / C99 */
    debug_line_u32(info, 0u);          /* .debug_line offset */
    debug_line_u32(info, unit_name_offset);

    debug_emit_type_dies(info, strings, &types, &type_patches);
    debug_type_patches_free(&type_patches);

    for (DeclList* item = mod->debug_ast ? mod->debug_ast->decls : NULL;
         item; item = item->next) {
        Decl* declaration = item->decl;
        const ModuleSymbol* symbol = debug_find_global_symbol(mod, declaration);
        if (!symbol || debug_find_global_decl(mod, symbol) != declaration ||
            !declaration->loc.filename ||
            declaration->loc.filename[0] == '\0' || declaration->loc.line == 0) {
            continue;
        }
        debug_emit_global_variable_die(
            obj, info, strings, &types, files, file_count, mod, symbol,
            declaration, filename, info_section,
            g_opts.target_arch == ARCH_X64 ? ARCH_X64 : ARCH_X86);
    }

    for (int index = 0; index < function_count; ++index) {
        const ModuleSymbol* function = functions[index];
        const char* symbol_name = function->name;
        Decl* function_decl = debug_find_function_decl(mod, function);
        const TypeMethod* function_method =
            debug_find_function_method(function_decl);
        DebugTypeEntry* return_type = NULL;
        DebugTypeEntry* containing_type = NULL;
        char* scoped_name = NULL;
        int file_index = debug_line_file_index(
            files, file_count, function->source_file);
        uint64_t address_offset;
        uint32_t name_offset = debug_str_add(
            strings, debug_function_source_name(function_decl,
                                                function->name));
        uint8_t function_abbreviation;
        if (function_decl && function_decl->type &&
            function_decl->type->kind == TYPE_FUNC) {
            return_type = debug_type_find(&types,
                                          function_decl->type->ret_type);
            if (!return_type) {
                rcc_fatal("DWARF function return type was not collected");
            }
        }
        if (function_decl && function_decl->func_method_owner) {
            containing_type = debug_type_find(
                &types, function_decl->func_method_owner);
            if (!containing_type) {
                rcc_fatal("DWARF member-function owner type was not collected");
            }
        }
        if (return_type && function_method) {
            function_abbreviation = function_decl->func_this_param ? 29u : 30u;
        } else if (return_type && function_decl &&
                   function_decl->func_this_param) {
            function_abbreviation = 26u;
        } else if (return_type && function_decl &&
                   function_decl->func_method_owner) {
            function_abbreviation = 28u;
        } else {
            function_abbreviation = return_type ? 2u : 8u;
        }
        section_add_byte(info, function_abbreviation);
        debug_line_u32(info, name_offset);
        address_offset = info->size;
        for (int byte = 0; byte < (g_opts.target_arch == ARCH_X64 ? 8 : 4);
             ++byte) {
            section_add_byte(info, 0u);
        }
        if (!function->is_global) {
            scoped_name = module_scoped_symbol(filename, function->name);
            symbol_name = scoped_name;
        }
        objfile_add_reloc(
            obj, info_section, address_offset, symbol_name,
            g_opts.target_arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U, 0);
        rcc_free(scoped_name);
        debug_line_u32(info, debug_function_size(mod, function));
        section_add_byte(info, (uint8_t)file_index);
        debug_line_u32(info, function->source_line);
        debug_line_u32(info, function->source_column);
        section_add_byte(info, function->is_global ? 1u : 0u);
        debug_line_u32(info, debug_str_add(strings, function->name));
        if (return_type) debug_line_u32(info, return_type->offset);
        /* The current C/C++ x86 backends retain a frame pointer, so expose
         * the same EBP/RBP base used by stack-local locations.  Full CFI and
         * unwind ranges remain a separate debug/unwind feature. */
        debug_expr_breg(info, g_opts.target_arch == ARCH_X64
                                  ? ARCH_X64 : ARCH_X86, 0, false);
        /* DW_INL_declared_inlined is 3.  This is deliberately based on the
         * parsed declaration, not on a guessed call-site optimization state. */
        section_add_byte(info, function_decl && function_decl->func_is_inline
                                ? 3u : 0u);
        if (return_type && function_decl &&
            function_decl->func_this_param) {
            if (!containing_type ||
                info->size > UINT32_MAX - 2u * sizeof(uint32_t)) {
                rcc_fatal("DWARF object-pointer reference exceeds 32-bit range");
            }
            /* The artificial `this` parameter is emitted as the first child;
             * the containing-type reference and access byte follow the
             * object-pointer ref before that child begins. */
            debug_line_u32(info,
                           (uint32_t)(info->size + 2u * sizeof(uint32_t) +
                                      (function_method ? 1u : 0u)));
            debug_line_u32(info, containing_type->offset);
        } else if (return_type && function_decl &&
                   function_decl->func_method_owner) {
            if (!containing_type) {
                rcc_fatal("DWARF static member has no containing type");
            }
            debug_line_u32(info, containing_type->offset);
        }
        if (return_type && function_method) {
            section_add_byte(info,
                             debug_function_accessibility(function_method));
        }
        debug_emit_function_locals(
            obj, info, strings, &types, files, file_count, mod, filename,
            info_section, function,
            g_opts.target_arch == ARCH_X64 ? ARCH_X64 : ARCH_X86);
        section_add_byte(info, 0u);    /* end of subprogram children */
    }
    section_add_byte(info, 0u);        /* end of compile-unit children */
    section_add_byte(info, 0u);        /* end of compile-unit DIE */
    if (info->size - unit_length_offset - 4u > UINT32_MAX) {
        rcc_fatal("DWARF info section is too large");
    }
    debug_line_patch_u32(info, unit_length_offset,
                         (uint32_t)(info->size - unit_length_offset - 4u));
    rcc_free(functions);
    rcc_free(files);
    debug_type_context_free(&types);
}

static void debug_frame_sleb(ObjSection* section, int64_t value) {
    uint64_t encoded = (uint64_t)value;
    bool more;
    do {
        uint8_t byte = (uint8_t)(encoded & 0x7fu);
        encoded >>= 7;
        more = !(((encoded == 0u) && ((byte & 0x40u) == 0u)) ||
                 ((encoded == UINT64_MAX) && ((byte & 0x40u) != 0u)));
        if (more) byte |= 0x80u;
        section_add_byte(section, byte);
    } while (more);
}

static void debug_frame_advance(ObjSection* section, uint64_t delta) {
    if (delta <= 0x3fu) {
        section_add_byte(section, (uint8_t)(0x40u | delta));
    } else if (delta <= UINT8_MAX) {
        section_add_byte(section, 0x02u); /* DW_CFA_advance_loc1 */
        section_add_byte(section, (uint8_t)delta);
    } else if (delta <= UINT16_MAX) {
        section_add_byte(section, 0x03u); /* DW_CFA_advance_loc2 */
        section_add_byte(section, (uint8_t)delta);
        section_add_byte(section, (uint8_t)(delta >> 8));
    } else {
        section_add_byte(section, 0x04u); /* DW_CFA_advance_loc4 */
        debug_line_u32(section, (uint32_t)delta);
    }
}

typedef enum DebugFramePrologueKind {
    DEBUG_FRAME_PROLOGUE_NONE = 0,
    DEBUG_FRAME_PROLOGUE_STANDARD,
    DEBUG_FRAME_PROLOGUE_I686_ALIGNED
} DebugFramePrologueKind;

static DebugFramePrologueKind debug_frame_prologue_kind(
    const Module* mod, const ModuleSymbol* function) {
    static const uint8_t x86_prologue[] = {0x55u, 0x89u, 0xe5u};
    static const uint8_t x64_prologue[] = {0x55u, 0x48u, 0x89u, 0xe5u};
    static const uint8_t i686_aligned_prefix[] = {
        0x55u,                         /* push ebp */
        0x8bu, 0x04u, 0x24u,          /* mov eax,[esp] */
        0x8bu, 0x54u, 0x24u, 0x04u,   /* mov edx,[esp+4] */
        0x81u, 0xe4u,                 /* and esp, immediate */
        0x00u, 0x00u, 0x00u, 0x00u,   /* alignment mask */
        0x89u, 0x04u, 0x24u,          /* mov [esp],eax */
        0x89u, 0x54u, 0x24u, 0x04u,   /* mov [esp+4],edx */
        0x89u, 0xe5u                  /* mov ebp,esp */
    };
    const uint8_t* prologue = g_opts.target_arch == ARCH_X64
        ? x64_prologue : x86_prologue;
    size_t size = g_opts.target_arch == ARCH_X64
        ? sizeof(x64_prologue) : sizeof(x86_prologue);
    if (!mod || !function || function->offset > mod->code.size) {
        return DEBUG_FRAME_PROLOGUE_NONE;
    }
    if (g_opts.target_arch == ARCH_X86 &&
        sizeof(i686_aligned_prefix) <= mod->code.size - function->offset) {
        const uint8_t* code = mod->code.data + function->offset;
        if (memcmp(code, i686_aligned_prefix, 10u) == 0 &&
            code[14] == i686_aligned_prefix[14] &&
            code[15] == i686_aligned_prefix[15] &&
            code[16] == i686_aligned_prefix[16] &&
            code[17] == i686_aligned_prefix[17] &&
            code[18] == i686_aligned_prefix[18] &&
            code[19] == i686_aligned_prefix[19] &&
            code[20] == i686_aligned_prefix[20] &&
            code[21] == i686_aligned_prefix[21] &&
            code[22] == i686_aligned_prefix[22]) {
            return DEBUG_FRAME_PROLOGUE_I686_ALIGNED;
        }
    }
    if (size <= mod->code.size - function->offset &&
        memcmp(mod->code.data + function->offset, prologue, size) == 0) {
        return DEBUG_FRAME_PROLOGUE_STANDARD;
    }
    return DEBUG_FRAME_PROLOGUE_NONE;
}

static void module_emit_debug_frame(ObjectFile* obj, Module* mod,
                                    const char* filename) {
    const ModuleSymbol** functions;
    ObjSection* frame;
    int function_count = 0;
    int frame_section;
    const uint32_t pointer_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    const uint32_t stack_register = g_opts.target_arch == ARCH_X64 ? 7u : 4u;
    const uint32_t frame_register = g_opts.target_arch == ARCH_X64 ? 6u : 5u;
    const uint32_t return_register = g_opts.target_arch == ARCH_X64 ? 16u : 8u;
    uint64_t cie_offset;

    if (!g_opts.debug_info || !obj || !mod || mod->symbol_count <= 0) return;
    functions = rcc_alloc((size_t)mod->symbol_count * sizeof(*functions));
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* symbol = &mod->symbols[index];
        if (!symbol->is_defined || symbol->section != MODULE_SYMBOL_CODE ||
            !symbol->source_file || symbol->source_file[0] == '\0' ||
            symbol->source_line == 0u ||
            debug_frame_prologue_kind(mod, symbol) ==
                DEBUG_FRAME_PROLOGUE_NONE) continue;
        functions[function_count++] = symbol;
    }
    if (function_count == 0) {
        rcc_free(functions);
        return;
    }

    frame = objfile_add_section(obj, ".debug_frame", SECT_DEBUG_FRAME, 0u);
    frame->align = 1u;
    frame_section = objfile_section_index(obj, frame);
    if (frame_section < 0) rcc_fatal("DWARF frame section is detached");

    /* DWARF32 CIE: the initial rule describes the call-site CFA and return
     * address.  The FDEs below then describe the fixed frame-pointer
     * prologue/epilogue emitted by both x86 backends. */
    cie_offset = frame->size;
    debug_line_u32(frame, 0u);
    debug_line_u32(frame, UINT32_MAX);
    section_add_byte(frame, 1u);       /* DWARF CIE version */
    section_add_byte(frame, 0u);       /* empty augmentation */
    debug_line_uleb(frame, 1u);        /* code alignment factor */
    debug_frame_sleb(frame, -(int64_t)pointer_size);
    debug_line_uleb(frame, return_register);
    section_add_byte(frame, 0x0cu);    /* DW_CFA_def_cfa */
    debug_line_uleb(frame, stack_register);
    debug_line_uleb(frame, pointer_size);
    section_add_byte(frame, (uint8_t)(0x80u + return_register));
    debug_line_uleb(frame, 1u);
    if (frame->size - cie_offset - 4u > UINT32_MAX) {
        rcc_fatal("DWARF CIE is too large");
    }
    debug_line_patch_u32(frame, cie_offset,
                         (uint32_t)(frame->size - cie_offset - 4u));

    for (int index = 0; index < function_count; ++index) {
        const ModuleSymbol* function = functions[index];
        const char* symbol_name = function->name;
        char* scoped_name = NULL;
        uint64_t fde_offset = frame->size;
        uint64_t address_offset;
        uint32_t function_size = debug_function_size(mod, function);
        DebugFramePrologueKind prologue_kind = debug_frame_prologue_kind(
            mod, function);
        /* The location after the complete `mov fp, sp` instruction is the
         * first PC at which the frame register is the CFA base.  The x86
         * instruction is two bytes (89 e5), while the x64 form is three
         * bytes (48 89 e5). */
        uint64_t prologue_after_fp = prologue_kind ==
            DEBUG_FRAME_PROLOGUE_I686_ALIGNED ? 23u
            : g_opts.target_arch == ARCH_X64 ? 4u : 3u;
        uint64_t after_leave = function_size >= 1u
            ? (uint64_t)function_size - 1u : 0u;
        uint64_t current_pc;
        uint64_t function_end = (uint64_t)function->offset + function_size;
        size_t emitted_epilogues = 0u;

        debug_line_u32(frame, 0u);
        debug_line_u32(frame, (uint32_t)cie_offset);
        address_offset = frame->size;
        for (uint32_t byte = 0u; byte < pointer_size; ++byte) {
            section_add_byte(frame, 0u);
        }
        if (!function->is_global) {
            scoped_name = module_scoped_symbol(filename, function->name);
            symbol_name = scoped_name;
        }
        objfile_add_reloc(obj, frame_section, address_offset, symbol_name,
                          g_opts.target_arch == ARCH_X64
                              ? RELOC_ABS64 : RELOC_ABS32U, 0);
        rcc_free(scoped_name);
        for (uint32_t byte = 0u; byte < pointer_size; ++byte) {
            section_add_byte(frame, (uint8_t)(function_size >> (byte * 8u)));
        }

        /* push fp: the saved frame pointer is two words below the new CFA. */
        debug_frame_advance(frame, 1u);
        section_add_byte(frame, 0x0eu); /* DW_CFA_def_cfa_offset */
        debug_line_uleb(frame, pointer_size * 2u);
        section_add_byte(frame, (uint8_t)(0x80u + frame_register));
        debug_line_uleb(frame, 1u);
        if (prologue_kind == DEBUG_FRAME_PROLOGUE_I686_ALIGNED) {
            /* The aligned i686 prologue copies the saved EBP and return
             * address to the newly aligned stack before establishing EBP.
             * Once those copies complete, the new ESP is a valid CFA base. */
            debug_frame_advance(frame, 20u);
            section_add_byte(frame, 0x0cu); /* DW_CFA_def_cfa */
            debug_line_uleb(frame, stack_register);
            debug_line_uleb(frame, pointer_size * 2u);
            debug_frame_advance(frame, 2u);
        } else {
            /* mov fp, sp: subsequent locals use the stable frame register. */
            debug_frame_advance(frame, prologue_after_fp - 1u);
        }
        section_add_byte(frame, 0x0du); /* DW_CFA_def_cfa_register */
        debug_line_uleb(frame, frame_register);
        current_pc = prologue_after_fp;
        if (!mod->debug_verified_backend) {
            for (size_t epilogue_index = 0u;
                 epilogue_index < mod->debug_frame_epilogue_count;
                 ++epilogue_index) {
                uint32_t return_pc =
                    mod->debug_frame_epilogue_pcs[epilogue_index];
                uint64_t relative_pc;
                if (return_pc <= function->offset ||
                    (uint64_t)return_pc >= function_end ||
                    (size_t)return_pc >= mod->code.size ||
                    mod->code.data[return_pc] != 0xc3u) {
                    continue;
                }
                relative_pc = (uint64_t)return_pc - function->offset;
                if (relative_pc <= current_pc ||
                    relative_pc >= function_size) {
                    continue;
                }
                debug_frame_advance(frame, relative_pc - current_pc);
                section_add_byte(frame, 0x0du); /* DW_CFA_def_cfa_register */
                debug_line_uleb(frame, stack_register);
                /* `leave` restores ESP to the return-address slot. */
                section_add_byte(frame, 0x0eu); /* DW_CFA_def_cfa_offset */
                debug_line_uleb(frame, pointer_size);
                section_add_byte(frame, (uint8_t)(0xc0u + frame_register));
                current_pc = relative_pc;
                ++emitted_epilogues;

                /* A later branch target may resume the function after this
                 * non-terminal return.  Restore the normal frame-body rules
                 * at the next instruction boundary for that code range. */
                if (relative_pc + 1u < function_size) {
                    debug_frame_advance(frame, 1u);
                    section_add_byte(frame, 0x0du);
                    debug_line_uleb(frame, frame_register);
                    section_add_byte(frame, 0x0eu);
                    debug_line_uleb(frame, pointer_size * 2u);
                    section_add_byte(frame,
                                     (uint8_t)(0x80u + frame_register));
                    debug_line_uleb(frame, 1u);
                    current_pc = relative_pc + 1u;
                }
            }
        }
        if (emitted_epilogues == 0u &&
            after_leave > prologue_after_fp) {
            debug_frame_advance(frame, after_leave - current_pc);
            section_add_byte(frame, 0x0du); /* DW_CFA_def_cfa_register */
            debug_line_uleb(frame, stack_register);
            section_add_byte(frame, 0x0eu); /* DW_CFA_def_cfa_offset */
            debug_line_uleb(frame, pointer_size);
            section_add_byte(frame, (uint8_t)(0xc0u + frame_register));
        }

        if (frame->size - fde_offset - 4u > UINT32_MAX) {
            rcc_fatal("DWARF FDE is too large");
        }
        debug_line_patch_u32(frame, fde_offset,
                             (uint32_t)(frame->size - fde_offset - 4u));
    }
    rcc_free(functions);
}

ObjectFile* module_to_objfile(Module* mod, const char* filename) {
    ObjectFile* obj = objfile_new(filename, g_opts.target_arch);
    int next_section = 1;
    int rodata_section = -1;
    int init_array_section = -1;
    int fini_array_section = -1;
    int data_section = -1;
    int bss_section = -1;
    int tls_section = -1;

    /* Create .text section */
    ObjSection* text = objfile_add_section(obj, ".text", SECT_CODE,
                                           SECT_FLAG_EXEC | SECT_FLAG_ALLOC);
    section_add_data(text, mod->code.data, mod->code.size);

    if (mod->init_array.size > 0u) {
        ObjSection* init_array = objfile_add_section(
            obj, ".init_array", SECT_INIT_ARRAY, SECT_FLAG_ALLOC);
        section_add_data(init_array, mod->init_array.data,
                         mod->init_array.size);
        init_array->align = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
        init_array_section = next_section++;
    }

    if (mod->fini_array.size > 0u) {
        ObjSection* fini_array = objfile_add_section(
            obj, ".fini_array", SECT_FINI_ARRAY, SECT_FLAG_ALLOC);
        section_add_data(fini_array, mod->fini_array.data,
                         mod->fini_array.size);
        fini_array->align = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
        fini_array_section = next_section++;
    }

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

    if (mod->tls.size > 0u) {
        ObjSection* tls = objfile_add_section(
            obj, ".tls", SECT_TLS, SECT_FLAG_WRITE | SECT_FLAG_ALLOC);
        section_add_data(tls, mod->tls.data, mod->tls.size);
        tls->align = mod->tls_align;
        tls_section = next_section++;
    }

    /* Add symbols from module */
    for (int i = 0; i < mod->symbol_count; i++) {
        ModuleSymbol* ms = &mod->symbols[i];
        const char* object_name = ms->name;
        char* scoped_name = NULL;
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
        /* A source-level weak import is a real weak undefined symbol, not a
         * strong unresolved reference.  Keep the weak binding in the object
         * so the linker may resolve it to zero or replace it with a strong
         * provider, matching the already-supported weak definition path. */
        SymbolType type = ms->is_weak
            ? SYM_WEAK
            : (ms->is_defined
                ? (ms->is_global ? SYM_GLOBAL : SYM_LOCAL)
                : SYM_UNDEF);
        SymbolBinding binding = ms->section == MODULE_SYMBOL_CODE
            ? BIND_CODE : ms->section == MODULE_SYMBOL_BSS
                ? BIND_BSS : ms->section == MODULE_SYMBOL_TLS
                    ? BIND_TLS : BIND_DATA;
        int section = -1;
        if (ms->is_defined) {
            section = ms->section == MODULE_SYMBOL_CODE ? 0
                : ms->section == MODULE_SYMBOL_RODATA ? rodata_section
                : ms->section == MODULE_SYMBOL_BSS ? bss_section
                : ms->section == MODULE_SYMBOL_TLS ? tls_section
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
            scoped_name = module_scoped_symbol(filename, ms->name);
            object_name = scoped_name;
        }

        objfile_add_symbol(obj, object_name, type, binding, section,
                           ms->offset, 0);
        rcc_free(scoped_name);
    }

    module_emit_debug_sections(obj, mod, filename);

    /* Add relocations */
    for (int i = 0; i < mod->reloc_count; i++) {
        ModuleReloc* mr = &mod->relocs_arr[i];
        int source_section = mr->source_section == MODULE_SYMBOL_CODE ? 0
            : mr->source_section == MODULE_SYMBOL_RODATA ? rodata_section
            : mr->source_section == MODULE_SYMBOL_INIT_ARRAY
                ? init_array_section
            : mr->source_section == MODULE_SYMBOL_FINI_ARRAY
                ? fini_array_section
            : mr->source_section == MODULE_SYMBOL_DATA ? data_section
            : mr->source_section == MODULE_SYMBOL_TLS ? tls_section
            : -1;
        uint64_t source_size = mr->source_section == MODULE_SYMBOL_CODE
            ? mod->code.size
            : mr->source_section == MODULE_SYMBOL_RODATA
                ? mod->rodata.size
                : mr->source_section == MODULE_SYMBOL_INIT_ARRAY
                    ? mod->init_array.size
                : mr->source_section == MODULE_SYMBOL_FINI_ARRAY
                    ? mod->fini_array.size
                : mr->source_section == MODULE_SYMBOL_DATA
                    ? mod->data.size
                    : mr->source_section == MODULE_SYMBOL_TLS
                        ? mod->tls.size : 0u;
        const ModuleSymbol* referenced_symbol = mr->symbol_name
            ? module_find_symbol(mod, mr->symbol_name) : NULL;
        uint64_t relocation_width = mr->is_relative || !mr->is_64bit
            ? 4u : 8u;
        RelocType type = mr->is_tls ? RELOC_TLSOFF32S : mr->is_got
            ? RELOC_GOT32 :
            mr->is_relative && (g_opts.pic || g_opts.pie) &&
                    (!referenced_symbol || !referenced_symbol->is_defined)
                ? RELOC_PLT32 : mr->is_relative ? RELOC_REL32 :
            (mr->is_64bit ? RELOC_ABS64 : RELOC_ABS32U);

        if (source_section < 0 || mr->offset > source_size ||
            relocation_width > source_size - mr->offset) {
            rcc_error((SourceLoc){filename, 0, 0},
                      "invalid relocation source section or offset");
            objfile_free(obj);
            return NULL;
        }

        /* Use symbol name directly if available */
        const char* sym_name = mr->symbol_name;
        char* scoped_name = NULL;
        const ModuleSymbol* module_symbol = sym_name
            ? module_find_symbol(mod, sym_name) : NULL;

        if (module_symbol && module_symbol->is_defined &&
            !module_symbol->is_global) {
            scoped_name = module_scoped_symbol(filename, sym_name);
            sym_name = scoped_name;
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
            objfile_add_reloc(obj, source_section, mr->offset, sym_name, type,
                              (int64_t)mr->target);
        }
        rcc_free(scoped_name);
    }

    return obj;
}

void module_emit_debug_sections(ObjectFile* obj, Module* mod,
                                const char* filename) {
    module_emit_debug_line(obj, mod, filename);
    module_emit_debug_info(obj, mod, filename);
    module_emit_debug_frame(obj, mod, filename);
}

/* Emit object file from Module */
bool rcc_emit_obj(Module* mod, const char* filename) {
    /* Local symbols need a translation-unit scope so independently compiled
     * objects cannot collide at link time.  The output path is not that
     * identity: embedding it makes otherwise identical objects depend on the
     * selected build directory.  Prefer the source path used by the driver;
     * retain the filename fallback for direct emitter users. */
    const char* translation_unit = g_opts.input_file[0] != '\0'
        ? g_opts.input_file : filename;
    ObjectFile* obj = module_to_objfile(mod, translation_unit);
    bool ok;
    if (!obj) return false;
    ok = objfile_write(obj, filename);
    objfile_free(obj);
    return ok;
}
