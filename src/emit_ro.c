/*
 * RCC - RinOS C Compiler
 * Object File (.ro) Emitter
 */

#include "rcc.h"
#include "objfile.h"
#include "codegen.h"
#include "ast_cxx.h"
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
           type == RELOC_ABS32S || type == RELOC_TLSOFF32S ||
           type == RELOC_TLSINDEX_MODULE64 ||
           type == RELOC_TLSINDEX_OFFSET64;
}

static bool ro_section_policy(uint16_t arch, const RoSection* section) {
    uint32_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    uint32_t allowed_flags = SECT_FLAG_WRITE | SECT_FLAG_EXEC |
                             SECT_FLAG_ALLOC | SECT_FLAG_COMDAT;
    if (section->type < SECT_CODE || section->type > SECT_DEBUG_LOC ||
        (section->flags & ~allowed_flags) != 0u ||
        (section->flags & (SECT_FLAG_WRITE | SECT_FLAG_EXEC)) ==
            (SECT_FLAG_WRITE | SECT_FLAG_EXEC) ||
        (!(section->type >= SECT_DEBUG_LINE &&
           section->type <= SECT_DEBUG_LOC) &&
         (section->flags & SECT_FLAG_ALLOC) == 0u)) return false;

    if (section->type >= SECT_DEBUG_LINE &&
        section->type <= SECT_DEBUG_LOC) {
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
            width = (rr.type == RELOC_ABS64 ||
                     rr.type == RELOC_TLSINDEX_MODULE64 ||
                     rr.type == RELOC_TLSINDEX_OFFSET64) ? 8u :
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
static bool debug_expression_uleb(uint8_t* expression, size_t capacity,
                                 size_t* size, uint64_t value);

static int debug_class_virtual_base_index(const CxxClass* class_info,
                                          const CxxClass* base) {
    if (!class_info || !base) return -1;
    for (int index = 0; index < class_info->virtual_base_count; ++index) {
        if (class_info->virtual_bases[index].base == base) return index;
    }
    return -1;
}

static void debug_expr_virtual_base_location(
    ObjSection* info, const CxxClass* class_info, int virtual_base_index,
    int architecture) {
    uint8_t expression[32];
    size_t size = 0u;
    uint32_t pointer_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t table_entry_offset;
    if (!info || !class_info || virtual_base_index < 0 ||
        virtual_base_index >= class_info->virtual_base_count ||
        class_info->virtual_base_pointer_offset < 0 ||
        !class_info->virtual_bases[virtual_base_index].base ||
        class_info->virtual_bases[virtual_base_index].offset < 0) {
        rcc_fatal("DWARF virtual-base location has incomplete layout");
        return;
    }
    table_entry_offset = (uint64_t)(unsigned)virtual_base_index * pointer_size;

    /* The ABI stores a pointer to signed, pointer-width offsets at the
     * per-subobject vbase-pointer slot. DW_AT_data_member_location supplies
     * the containing object's address on the expression stack. */
    expression[size++] = 0x12u; /* DW_OP_dup: retain the subobject address */
    expression[size++] = 0x23u; /* DW_OP_plus_uconst: vbase-pointer slot */
    if (!debug_expression_uleb(
            expression, sizeof(expression), &size,
            (uint64_t)(unsigned)class_info->virtual_base_pointer_offset)) {
        rcc_fatal("DWARF virtual-base expression exceeds its buffer");
        return;
    }
    expression[size++] = 0x94u; /* DW_OP_deref_size: load table pointer */
    expression[size++] = (uint8_t)pointer_size;
    expression[size++] = 0x10u; /* DW_OP_constu: table entry byte offset */
    if (!debug_expression_uleb(expression, sizeof(expression), &size,
                               table_entry_offset)) {
        rcc_fatal("DWARF virtual-base expression exceeds its buffer");
        return;
    }
    expression[size++] = 0x22u; /* DW_OP_plus: address the selected entry */
    expression[size++] = 0x94u; /* DW_OP_deref_size: load relative offset */
    expression[size++] = (uint8_t)pointer_size;
    expression[size++] = 0x22u; /* DW_OP_plus: add to subobject address */

    debug_line_uleb(info, size);
    for (size_t index = 0u; index < size; ++index) {
        section_add_byte(info, expression[index]);
    }
}

typedef struct DebugTypeEntry {
    const Type* type;
    Type* qualifier_base;
    Type* enum_underlying_fallback;
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
        rcc_free(context->entries[index].enum_underlying_fallback);
        context->entries[index].enum_underlying_fallback = NULL;
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
    entry->enum_underlying_fallback = NULL;
    entry->offset = 0u;
    entry->collecting = false;
    entry->recursive = false;
    return entry;
}

static bool debug_type_has_supported_inheritance(const Type* type) {
    CxxClass* class_info;
    if (!type || type->kind != TYPE_STRUCT || !type->cxx_class) return false;
    class_info = type->cxx_class;
    if (class_info->base_count <= 0 || !type->is_complete || type->size < 0 ||
        !class_info->bases ||
        !class_info->base_offsets) {
        return false;
    }
    if (class_info->virtual_base_count > 0 &&
        (!class_info->virtual_bases ||
         class_info->virtual_base_pointer_offset < 0)) {
        return false;
    }
    for (int index = 0; index < class_info->base_count; ++index) {
        CxxClass* base = class_info->bases[index].base;
        if (!base || !base->type || !base->type->is_complete ||
            base->type->size < 0 || class_info->base_offsets[index] < 0) {
            return false;
        }
        if (class_info->bases[index].is_virtual) {
            int virtual_index = debug_class_virtual_base_index(class_info,
                                                               base);
            if (virtual_index < 0 ||
                class_info->virtual_bases[virtual_index].offset < 0) {
                return false;
            }
        }
    }
    return true;
}

static bool debug_type_is_dependent(const Type* type, unsigned depth) {
    if (!type) return false;
    if (depth >= 32u || type->cxx_dependent ||
        type->cxx_dependent_member_name) {
        return true;
    }
    if (type->kind == TYPE_PTR) {
        return debug_type_is_dependent(type->base, depth + 1u) ||
            (type->cxx_is_member_pointer &&
             debug_type_is_dependent(type->cxx_member_pointer_owner,
                                     depth + 1u));
    }
    if (type->kind == TYPE_ARRAY || type->kind == TYPE_VECTOR) {
        return debug_type_is_dependent(type->base, depth + 1u);
    }
    if (type->kind == TYPE_FUNC) {
        if (debug_type_is_dependent(type->ret_type, depth + 1u)) return true;
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            if (debug_type_is_dependent(parameter->type, depth + 1u)) {
                return true;
            }
        }
    }
    return false;
}

static void debug_type_collect(DebugTypeContext* context, const Type* type);
static bool debug_type_has_integral_constant_encoding(const Type* type);

static Type* debug_template_find_nested_alias_type(
    CxxClass* cls, const char* name, unsigned depth) {
    if (!cls || !name || depth >= 64u) return NULL;
    for (CxxTypeAlias* alias = cls->type_aliases; alias;
         alias = alias->next) {
        if (alias->name && strcmp(alias->name, name) == 0) {
            return alias->type;
        }
    }
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        if (!base && cls->bases[index].type_pattern) {
            base = cls->bases[index].type_pattern->cxx_class;
        }
        Type* found = debug_template_find_nested_alias_type(
            base, name, depth + 1u);
        if (found) return found;
    }
    return NULL;
}

static Type* debug_template_resolve_nested_type_path(
    CxxClass* cls, const char* path, unsigned depth) {
    const char* separator;
    char component[512];
    size_t component_length;
    Type* nested_type;
    if (!cls || !path || !*path || depth >= 64u) return NULL;
    separator = strstr(path, "::");
    component_length = separator ? (size_t)(separator - path) : strlen(path);
    if (component_length == 0u || component_length >= sizeof(component)) {
        return NULL;
    }
    memcpy(component, path, component_length);
    component[component_length] = '\0';
    nested_type = debug_template_find_nested_alias_type(
        cls, component, depth + 1u);
    if (!nested_type || !separator) return nested_type;
    if (!nested_type->cxx_class || separator[2] == '\0') return NULL;
    return debug_template_resolve_nested_type_path(
        nested_type->cxx_class, separator + 2, depth + 1u);
}

/* A non-type template parameter may use a preceding type parameter as its
 * declared type (`template<class T, T... Values>`).  The declaration retains
 * that dependent carrier, while a concrete specialization stores the actual
 * type argument in the corresponding template slot. */
static Type* debug_template_value_parameter_type(
    const CxxTemplate* templ, const TemplateParam* parameter,
    Type* const* arguments, int argument_count) {
    Type* parameter_type;
    int type_parameter_index;
    if (!parameter || !(parameter_type = parameter->type)) return NULL;
    if (!debug_type_is_dependent(parameter_type, 0u)) return parameter_type;
    if (parameter_type->cxx_dependent_member_name &&
        parameter_type->cxx_template_param_index >= 0 && arguments &&
        parameter_type->cxx_template_param_index < argument_count) {
        Type* owner = arguments[
            parameter_type->cxx_template_param_index];
        Type* nested_type = owner && owner->cxx_class
            ? debug_template_resolve_nested_type_path(
                  owner->cxx_class,
                  parameter_type->cxx_dependent_member_name, 0u)
            : NULL;
        if (nested_type) {
            return nested_type;
        }
    }
    if (!parameter_type->cxx_dependent ||
        parameter_type->cxx_template_param_index < 0) {
        return NULL;
    }
    type_parameter_index = parameter_type->cxx_template_param_index;
    if (!templ || !arguments || type_parameter_index >= argument_count ||
        type_parameter_index >= templ->param_count ||
        templ->params[type_parameter_index].kind != TPARAM_TYPE ||
        !arguments[type_parameter_index] ||
        debug_type_is_dependent(arguments[type_parameter_index], 0u)) {
        return NULL;
    }
    return arguments[type_parameter_index];
}

static void debug_type_collect_class_template_arguments(
    DebugTypeContext* context, const CxxClass* class_info) {
    CxxTemplate* templ;
    Type** arguments;
    int argument_count;
    if (!context || !class_info) return;
    templ = class_info->template_identity_tmpl
        ? class_info->template_identity_tmpl : class_info->templ;
    arguments = class_info->template_identity_tmpl
        ? class_info->template_identity_args : class_info->template_args;
    argument_count = class_info->template_identity_tmpl
        ? class_info->template_identity_arg_count
        : class_info->template_arg_count;
    if (!templ || !templ->params) return;
    for (int index = 0; index < templ->param_count; ++index) {
        TemplateParam* parameter = &templ->params[index];
        Type* argument_type = NULL;
        Type* value_type = NULL;
        if (parameter->is_pack) {
            /* Class-template type arguments are stored flattened: the pack
             * begins at its declared parameter index and extends to the
             * specialization's argument count.  Only the supported trailing
             * class-type pack has an unambiguous boundary here. */
            if (parameter->kind == TPARAM_TYPE &&
                index == templ->param_count - 1 && arguments) {
                for (int pack_index = index; pack_index < argument_count;
                     ++pack_index) {
                    argument_type = arguments[pack_index];
                    if (argument_type && !debug_type_is_dependent(
                            argument_type, 0u)) {
                        debug_type_collect(context, argument_type);
                    }
                }
            } else if (parameter->kind == TPARAM_NONTYPE &&
                       index == templ->param_count - 1) {
                value_type = debug_template_value_parameter_type(
                    templ, parameter, arguments, argument_count);
                if (debug_type_has_integral_constant_encoding(value_type)) {
                    debug_type_collect(context, value_type);
                }
            }
            continue;
        }
        if (index >= argument_count) continue;
        if (parameter->kind == TPARAM_TYPE) {
            argument_type = arguments ? arguments[index] : NULL;
        } else if (parameter->kind == TPARAM_NONTYPE) {
            value_type = debug_template_value_parameter_type(
                templ, parameter, arguments, argument_count);
            argument_type = value_type;
        }
        if (argument_type &&
            !debug_type_is_dependent(argument_type, 0u)) {
            debug_type_collect(context, argument_type);
        }
    }
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
        if (debug_type_has_supported_inheritance(type)) {
            CxxClass* class_info = type->cxx_class;
            for (int index = 0; index < class_info->base_count; ++index) {
                debug_type_collect(context,
                                   class_info->bases[index].base->type);
                entry = debug_type_find(context, type);
            }
        }
        for (TypeField* field = type->fields; field; field = field->next) {
            if (field->type) debug_type_collect(context, field->type);
            entry = debug_type_find(context, type);
        }
        if (type->cxx_class) {
            for (CxxTypeAlias* alias = type->cxx_class->type_aliases; alias;
                 alias = alias->next) {
                if (!alias->type || debug_type_is_dependent(alias->type, 0u)) {
                    continue;
                }
                debug_type_collect(context, alias->type);
                entry = debug_type_find(context, type);
            }
            debug_type_collect_class_template_arguments(
                context, type->cxx_class);
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
    if (type->kind == TYPE_ENUM) {
        Type* underlying_type = type->enum_underlying_type;
        Type* fallback = NULL;
        if (!underlying_type) {
            /* C enums have an int-compatible representation but do not
             * retain a Type pointer for it. Build a local scalar descriptor
             * so this shared object/RLD emitter has no parser-only builtin
             * dependency. */
            fallback = rcc_alloc(sizeof(*fallback));
            memset(fallback, 0, sizeof(*fallback));
            fallback->kind = TYPE_INT;
            fallback->size = type->size;
            fallback->align = type->align;
            fallback->is_unsigned = type->is_unsigned;
            underlying_type = fallback;
        }
        debug_type_collect(context, underlying_type);
        entry = debug_type_add(context, type);
        entry->enum_underlying_fallback = fallback;
        return;
    }
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY ||
        type->kind == TYPE_VECTOR) {
        debug_type_collect(context, type->base);
        if (type->kind == TYPE_PTR && type->cxx_is_member_pointer) {
            debug_type_collect(context, type->cxx_member_pointer_owner);
        }
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
            } else if (statement->decl &&
                       statement->decl->kind == DECL_TYPEDEF) {
                Type* alias_type = statement->decl->typedef_type
                    ? statement->decl->typedef_type : statement->decl->type;
                if (alias_type &&
                    !debug_type_is_dependent(alias_type, 0u)) {
                    debug_type_collect(context, alias_type);
                }
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
    if (function->func_template_owner) {
        CxxTemplate* templ = function->func_template_owner;
        for (int index = 0; index < templ->param_count; ++index) {
            TemplateParam* parameter = &templ->params[index];
            Type* argument_type = NULL;
            Type* value_type = NULL;
            if (parameter->is_pack) {
                int pack_length = function->func_template_pack_counts
                    ? function->func_template_pack_counts[index]
                    : function->func_template_pack_count;
                int pack_offset = function->func_template_pack_offsets
                    ? function->func_template_pack_offsets[index] : 0;
                if (parameter->kind == TPARAM_TYPE &&
                    function->func_template_pack_count >= 0 &&
                    function->func_template_pack_args) {
                    for (int pack_index = 0;
                         pack_index < pack_length;
                         ++pack_index) {
                        argument_type = function->func_template_pack_args[
                            pack_offset + pack_index];
                        if (argument_type && !debug_type_is_dependent(
                                argument_type, 0u)) {
                            debug_type_collect(context, argument_type);
                        }
                    }
                } else if (parameter->kind == TPARAM_NONTYPE &&
                           function->func_template_pack_count >= 0) {
                    value_type = debug_template_value_parameter_type(
                        templ, parameter, function->func_template_args,
                        function->func_template_arg_count);
                    if (debug_type_has_integral_constant_encoding(
                            value_type)) {
                        debug_type_collect(context, value_type);
                    }
                }
                continue;
            }
            if (index >= function->func_template_arg_count) continue;
            if (parameter->kind == TPARAM_TYPE) {
                argument_type = function->func_template_args
                    ? function->func_template_args[index] : NULL;
            } else if (parameter->kind == TPARAM_NONTYPE) {
                value_type = debug_template_value_parameter_type(
                    templ, parameter, function->func_template_args,
                    function->func_template_arg_count);
                argument_type = value_type;
            }
            if (argument_type &&
                !debug_type_is_dependent(argument_type, 0u)) {
                debug_type_collect(context, argument_type);
            }
        }
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

static void debug_collect_inline_files(const Module* mod,
                                       const char*** files, int* file_count,
                                       size_t* file_capacity) {
    if (!mod) return;
    for (size_t index = 0u; index < mod->debug_inline_call_count; ++index) {
        const ModuleDebugInlineCall* call = &mod->debug_inline_calls[index];
        if (call->call_location.line > 0) {
            debug_file_add(files, file_count, file_capacity,
                           call->call_location.filename);
        }
        if (call->callee && call->callee->loc.line > 0) {
            debug_file_add(files, file_count, file_capacity,
                           call->callee->loc.filename);
        }
        for (const DeclList* parameter = call->callee
                 ? call->callee->func_params : NULL;
             parameter; parameter = parameter->next) {
            if (parameter->decl && parameter->decl->loc.line > 0) {
                debug_file_add(files, file_count, file_capacity,
                               parameter->decl->loc.filename);
            }
        }
        if (call->callee && call->callee->func_body &&
            call->callee->func_body->kind == STMT_BLOCK) {
            for (const StmtList* local = call->callee->func_body->block_stmts;
                 local; local = local->next) {
                const Decl* declaration = local->stmt &&
                    local->stmt->kind == STMT_DECL
                    ? local->stmt->decl : NULL;
                if (declaration && declaration->loc.line > 0) {
                    debug_file_add(files, file_count, file_capacity,
                                   declaration->loc.filename);
                }
            }
        }
    }
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
            } else if (statement->decl &&
                       statement->decl->kind == DECL_TYPEDEF) {
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
        case TYPE_SHORT:
            if (type->is_char16) return "char16_t";
            return type->is_unsigned ? "unsigned short" : "short";
        case TYPE_INT:
            if (type->is_wchar) return "wchar_t";
            if (type->is_char32) return "char32_t";
            return type->is_unsigned ? "unsigned int" : "int";
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

static const char* debug_template_argument_name(const Type* argument) {
    CxxTemplate* actual_template = argument
        ? argument->cxx_template : NULL;
    const char* spelling = argument ? argument->tag : NULL;
    CxxNamespace* namespace_stack[32];
    int namespace_count = 0;
    char qualified_name[512];
    size_t length = 0u;
    if (!actual_template || !actual_template->name ||
        actual_template->name[0] == '\0') {
        return NULL;
    }
    if (spelling && strstr(spelling, "::")) {
        return spelling;
    }
    for (CxxNamespace* current = actual_template->ns;
         current && current->name; current = current->parent) {
        if (namespace_count >= (int)(sizeof(namespace_stack) /
                                     sizeof(namespace_stack[0]))) {
            rcc_fatal("DWARF template-template namespace is too deep");
            return NULL;
        }
        namespace_stack[namespace_count++] = current;
    }
    if (namespace_count == 0) {
        return spelling && spelling[0] != '\0'
            ? spelling : actual_template->name;
    }
    for (int index = namespace_count - 1; index >= 0; --index) {
        size_t component_length = strlen(namespace_stack[index]->name);
        size_t separator_length = length == 0u ? 0u : 2u;
        if (separator_length + component_length >=
            sizeof(qualified_name) - length) {
            rcc_fatal("DWARF template-template argument name is too long");
            return NULL;
        }
        if (separator_length) {
            memcpy(qualified_name + length, "::", separator_length);
            length += separator_length;
        }
        memcpy(qualified_name + length, namespace_stack[index]->name,
               component_length);
        length += component_length;
    }
    if (length + 2u + strlen(actual_template->name) >=
        sizeof(qualified_name)) {
        rcc_fatal("DWARF template-template argument name is too long");
        return NULL;
    }
    memcpy(qualified_name + length, "::", 2u);
    length += 2u;
    strcpy(qualified_name + length, actual_template->name);
    return rcc_intern(qualified_name);
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

static bool debug_type_has_integral_constant_encoding(const Type* type) {
    if (!type) return false;
    switch (type->kind) {
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_SHORT:
        case TYPE_INT:
        case TYPE_LONG:
        case TYPE_LLONG:
        case TYPE_ENUM:
            return true;
        default:
            return false;
    }
}

static void debug_emit_type_dies(ObjSection* info, ObjSection* strings,
                                 const char* const* files, int file_count,
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
        } else if (type->kind == TYPE_PTR && type->is_reference) {
            DebugTypeEntry* base = debug_type_find(context, type->base);
            if (!base) {
                rcc_fatal("DWARF reference base type was not collected");
                return;
            }
            section_add_byte(info, type->is_rvalue_reference ? 38u : 37u);
            debug_type_ref(info, patches, base);
        } else if (type->kind == TYPE_PTR && type->cxx_is_member_pointer) {
            DebugTypeEntry* member_type = debug_type_find(context, type->base);
            DebugTypeEntry* owner_type = debug_type_find(
                context, type->cxx_member_pointer_owner);
            if (!member_type || !owner_type) {
                rcc_fatal("DWARF member-pointer component type was not collected");
                return;
            }
            section_add_byte(info, 57u); /* DW_TAG_ptr_to_member_type */
            section_add_byte(info, (uint8_t)(type->size > 255 ? 255 :
                                             (type->size < 0 ? 0 : type->size)));
            debug_type_ref(info, patches, member_type);
            debug_type_ref(info, patches, owner_type);
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
            bool emit_inheritance =
                debug_type_has_supported_inheritance(type);
            CxxClass* class_info = emit_inheritance ? type->cxx_class : NULL;
            section_add_byte(info, type->kind == TYPE_UNION ? 13u : 12u);
            debug_line_u32(info, debug_str_add(strings, debug_type_name(type)));
            debug_line_u32(info, (uint32_t)(type->size < 0 ? 0 : type->size));
            if (type->cxx_class) {
                CxxClass* template_class = type->cxx_class;
                CxxTemplate* templ = template_class->template_identity_tmpl
                    ? template_class->template_identity_tmpl
                    : template_class->templ;
                Type** arguments = template_class->template_identity_tmpl
                    ? template_class->template_identity_args
                    : template_class->template_args;
                int argument_count = template_class->template_identity_tmpl
                    ? template_class->template_identity_arg_count
                    : template_class->template_arg_count;
                int64_t* value_arguments =
                    template_class->template_identity_tmpl
                        ? template_class->template_identity_value_args
                        : template_class->template_value_args;
                bool* value_present =
                    template_class->template_identity_tmpl
                        ? template_class->template_identity_value_present
                        : template_class->template_value_present;
                if (templ && templ->params) {
                    for (int parameter_index = 0;
                         parameter_index < templ->param_count;
                         ++parameter_index) {
                        TemplateParam* parameter =
                            &templ->params[parameter_index];
                        DebugTypeEntry* parameter_type = NULL;
                        if (!parameter->name) continue;
                        if (parameter->is_pack) {
                            if (parameter_index == templ->param_count - 1 &&
                                parameter->kind == TPARAM_TYPE) {
                                section_add_byte(info, 50u);
                                debug_line_u32(
                                    info, debug_str_add(strings,
                                                        parameter->name));
                                for (int pack_index = parameter_index;
                                     pack_index < argument_count;
                                     ++pack_index) {
                                    Type* argument_type = arguments
                                        ? arguments[pack_index] : NULL;
                                    if (!argument_type ||
                                        debug_type_is_dependent(
                                            argument_type, 0u)) {
                                        continue;
                                    }
                                    parameter_type = debug_type_find(
                                        context, argument_type);
                                    if (!parameter_type) {
                                        rcc_fatal(
                                            "DWARF class-template pack type was not collected");
                                        return;
                                    }
                                    section_add_byte(info, 51u);
                                    debug_type_ref(info, patches,
                                                   parameter_type);
                                }
                                section_add_byte(info, 0u);
                            } else if (
                                parameter_index == templ->param_count - 1 &&
                                parameter->kind == TPARAM_TEMPLATE) {
                                section_add_byte(info, 50u);
                                debug_line_u32(
                                    info, debug_str_add(strings,
                                                        parameter->name));
                                for (int pack_index = parameter_index;
                                     pack_index < argument_count;
                                     ++pack_index) {
                                    Type* template_argument = arguments
                                        ? arguments[pack_index] : NULL;
                                    const char* template_name =
                                        template_argument
                                            ? template_argument->tag : NULL;
                                    if (!template_name && template_argument &&
                                        template_argument->cxx_template) {
                                        template_name = template_argument
                                            ->cxx_template->name;
                                    }
                                    if (!template_argument ||
                                        !template_argument->cxx_template ||
                                        !template_name ||
                                        template_name[0] == '\0') {
                                        rcc_fatal(
                                            "DWARF class-template template-pack argument is missing");
                                        return;
                                    }
                                    section_add_byte(info, 55u);
                                    debug_line_u32(
                                        info, debug_str_add(strings,
                                                            template_name));
                                }
                                section_add_byte(info, 0u);
                            } else if (
                                parameter_index == templ->param_count - 1 &&
                                parameter->kind == TPARAM_NONTYPE) {
                                Type* value_type =
                                    debug_template_value_parameter_type(
                                        templ, parameter, arguments,
                                        argument_count);
                                if (!debug_type_has_integral_constant_encoding(
                                        value_type)) {
                                    continue;
                                }
                                parameter_type = debug_type_find(
                                    context, value_type);
                                if (!parameter_type) {
                                    rcc_fatal(
                                        "DWARF class-template value-pack type was not collected");
                                    return;
                                }
                                section_add_byte(info, 50u);
                                debug_line_u32(
                                    info, debug_str_add(strings,
                                                        parameter->name));
                                for (int pack_index = parameter_index;
                                     pack_index < argument_count;
                                     ++pack_index) {
                                    if (!value_arguments || !value_present ||
                                        !value_present[pack_index]) {
                                        continue;
                                    }
                                    section_add_byte(
                                        info, value_type->is_unsigned
                                            ? 53u : 52u);
                                    debug_type_ref(info, patches,
                                                   parameter_type);
                                    if (value_type->is_unsigned) {
                                        debug_line_uleb(
                                            info, (uint64_t)value_arguments[
                                                pack_index]);
                                    } else {
                                        debug_line_sleb(
                                            info, value_arguments[pack_index]);
                                    }
                                }
                                section_add_byte(info, 0u);
                            }
                            continue;
                        }
                        if (parameter->kind == TPARAM_TEMPLATE) {
                            Type* template_argument = arguments
                                ? arguments[parameter_index] : NULL;
                            const char* template_name = template_argument
                                ? template_argument->tag : NULL;
                            if (!template_name && template_argument &&
                                template_argument->cxx_template) {
                                template_name =
                                    template_argument->cxx_template->name;
                            }
                            if (template_argument &&
                                template_argument->cxx_template &&
                                template_name && template_name[0] != '\0') {
                                section_add_byte(info, 54u);
                                debug_line_u32(
                                    info, debug_str_add(strings,
                                                        parameter->name));
                                debug_line_u32(
                                    info, debug_str_add(strings,
                                                        template_name));
                            }
                            continue;
                        }
                        if (parameter_index >= argument_count) continue;
                        if (parameter->kind == TPARAM_TYPE) {
                            Type* argument_type = arguments
                                ? arguments[parameter_index] : NULL;
                            if (!argument_type || debug_type_is_dependent(
                                    argument_type, 0u)) {
                                continue;
                            }
                            parameter_type = debug_type_find(
                                context, argument_type);
                            if (!parameter_type) {
                                rcc_fatal(
                                    "DWARF class-template argument type was not collected");
                                return;
                            }
                            section_add_byte(info, 47u);
                            debug_line_u32(
                                info, debug_str_add(strings, parameter->name));
                            debug_type_ref(info, patches, parameter_type);
                        } else if (parameter->kind == TPARAM_NONTYPE &&
                                   value_arguments &&
                                   value_present &&
                                   value_present[parameter_index]) {
                            Type* value_type =
                                debug_template_value_parameter_type(
                                    templ, parameter, arguments,
                                    argument_count);
                            if (!debug_type_has_integral_constant_encoding(
                                    value_type)) {
                                continue;
                            }
                            parameter_type = debug_type_find(
                                context, value_type);
                            if (!parameter_type) {
                                rcc_fatal(
                                    "DWARF class-template value type was not collected");
                                return;
                            }
                            section_add_byte(
                                info, value_type->is_unsigned
                                    ? 49u : 48u);
                            debug_line_u32(
                                info, debug_str_add(strings, parameter->name));
                            debug_type_ref(info, patches, parameter_type);
                            if (value_type->is_unsigned) {
                                debug_line_uleb(
                                    info, (uint64_t)value_arguments[
                                        parameter_index]);
                            } else {
                                debug_line_sleb(
                                    info, value_arguments[parameter_index]);
                            }
                        }
                    }
                }
            }
            if (emit_inheritance) {
                for (int base_index = 0;
                     base_index < class_info->base_count; ++base_index) {
                    CxxClass* base = class_info->bases[base_index].base;
                    bool virtual_base =
                        class_info->bases[base_index].is_virtual;
                    DebugTypeEntry* base_type =
                        debug_type_find(context, base->type);
                    if (!base_type) {
                        rcc_fatal(
                            "DWARF inheritance base type was not collected");
                        return;
                    }
                    section_add_byte(info, virtual_base ? 45u : 43u);
                    debug_type_ref(info, patches, base_type);
                    if (virtual_base) {
                        int virtual_index = debug_class_virtual_base_index(
                            class_info, base);
                        debug_expr_virtual_base_location(
                            info, class_info, virtual_index,
                            g_opts.target_arch == ARCH_X64
                                ? ARCH_X64 : ARCH_X86);
                        section_add_byte(info, 1u); /* DW_VIRTUALITY_virtual */
                    } else {
                        debug_expr_member_location(
                            info, class_info->base_offsets[base_index]);
                    }
                    if (class_info->bases[base_index].access >
                        ACCESS_PRIVATE) {
                        rcc_fatal(
                            "DWARF inheritance accessibility is invalid");
                        return;
                    }
                    section_add_byte(
                        info, (uint8_t)(class_info->bases[base_index].access +
                                        1));
                }
            }
            for (TypeField* field = type->fields; field; field = field->next) {
                DebugTypeEntry* field_type;
                uint64_t data_bit_offset;
                if (!field->name || !field->type) continue;
                if (emit_inheritance && field->cxx_declaring_class &&
                    field->cxx_declaring_class != class_info) {
                    continue;
                }
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
            if (type->cxx_class) {
                for (CxxTypeAlias* alias = type->cxx_class->type_aliases;
                     alias; alias = alias->next) {
                    DebugTypeEntry* alias_type;
                    int file_index;
                    if (!alias->name || !alias->type ||
                        debug_type_is_dependent(alias->type, 0u) ||
                        !alias->loc.filename || alias->loc.line <= 0 ||
                        alias->loc.column < 0) {
                        continue;
                    }
                    alias_type = debug_type_find(context, alias->type);
                    file_index = debug_line_file_index(
                        files, file_count, alias->loc.filename);
                    if (!alias_type || file_index <= 0) {
                        rcc_fatal(
                            "DWARF class alias is missing its type or source file");
                        return;
                    }
                    if (alias->access > ACCESS_PRIVATE) {
                        rcc_fatal("DWARF class alias accessibility is invalid");
                        return;
                    }
                    section_add_byte(info, 46u); /* DW_TAG_typedef */
                    debug_line_u32(info, debug_str_add(strings, alias->name));
                    debug_type_ref(info, patches, alias_type);
                    debug_line_u32(info, (uint32_t)file_index);
                    debug_line_u32(info, (uint32_t)alias->loc.line);
                    debug_line_u32(info, (uint32_t)alias->loc.column);
                    section_add_byte(info, (uint8_t)(alias->access + 1u));
                }
            }
            section_add_byte(info, 0u);
        } else if (type->kind == TYPE_ENUM) {
            const Type* underlying_type_value = type->enum_underlying_type
                ? type->enum_underlying_type : entry->enum_underlying_fallback;
            DebugTypeEntry* underlying_type = debug_type_find(
                context, underlying_type_value);
            if (!underlying_type) {
                rcc_fatal("DWARF enum underlying type was not collected");
                return;
            }
            /* Abbreviation 36 carries DW_AT_enum_class for scoped enums. */
            section_add_byte(info, type->enum_is_scoped ? 36u : 15u);
            debug_line_u32(info, debug_str_add(strings, debug_type_name(type)));
            debug_type_ref(info, patches, underlying_type);
            section_add_byte(info, (uint8_t)(type->size > 255 ? 255 :
                                             (type->size < 0 ? 0 : type->size)));
            if (type->enum_is_scoped) {
                section_add_byte(info, 1u);  /* DW_AT_enum_class */
            }
            for (int constant = 0; constant < type->enum_constant_count;
                 ++constant) {
                EnumConstantInfo* item = &type->enum_constants[constant];
                if (!item->name) continue;
                /* Abbreviation 35 stores full-width unsigned values. */
                section_add_byte(info, type->is_unsigned ? 35u : 16u);
                debug_line_u32(info, debug_str_add(strings, item->name));
                if (type->is_unsigned) {
                    debug_line_uleb(info, (uint64_t)item->value);
                } else {
                    debug_line_sleb(info, item->value);
                }
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

static bool debug_expression_uleb(uint8_t* expression, size_t capacity,
                                 size_t* size, uint64_t value) {
    do {
        uint8_t byte;
        if (!expression || !size || *size >= capacity) return false;
        byte = (uint8_t)(value & 0x7fu);
        value >>= 7u;
        if (value != 0u) byte |= 0x80u;
        expression[(*size)++] = byte;
    } while (value != 0u);
    return true;
}

static bool debug_expression_sleb_bounded(uint8_t* expression,
                                          size_t capacity, size_t* size,
                                          int64_t value) {
    bool more;
    do {
        uint8_t byte = (uint8_t)((uint64_t)value & 0x7fu);
        value >>= 7;
        more = !((value == 0 && (byte & 0x40u) == 0u) ||
                 (value == -1 && (byte & 0x40u) != 0u));
        if (more) byte |= 0x80u;
        if (!expression || !size || *size >= capacity) return false;
        expression[(*size)++] = byte;
    } while (more);
    return true;
}

static size_t debug_build_expr_breg(uint8_t expression[32], int architecture,
                                    int64_t offset, uint32_t alignment,
                                    bool dereference) {
    size_t size = 1u;
    int64_t value = offset;
    bool more;
    if (!expression) return 0u;
    expression[0] = architecture == ARCH_X64 ? 0x76u : 0x75u;
    do {
        uint8_t byte = (uint8_t)((uint64_t)value & 0x7fu);
        value >>= 7;
        more = !((value == 0 && (byte & 0x40u) == 0u) ||
                 (value == -1 && (byte & 0x40u) != 0u));
        if (more) byte |= 0x80u;
        if (size + 1u >= 32u) {
            rcc_fatal("DWARF variable location expression is too large");
            return 0u;
        }
        expression[size++] = byte;
    } while (more);
    if (alignment != 0u) {
        uint64_t pointer_mask;
        if (alignment < 2u ||
            alignment > RCC_MAX_SUPPORTED_ALIGNMENT ||
            (alignment & (alignment - 1u)) != 0u || size >= 32u) {
            rcc_fatal("DWARF dynamic local alignment is invalid");
            return 0u;
        }
        expression[size++] = 0x23u; /* DW_OP_plus_uconst: round-up bias */
        if (!debug_expression_uleb(
                expression, 32u, &size, (uint64_t)alignment - 1u) ||
            size >= 32u) return 0u;
        expression[size++] = 0x10u; /* DW_OP_constu: address-width mask */
        pointer_mask = architecture == ARCH_X64
            ? UINT64_MAX : UINT32_MAX;
        pointer_mask &= ~((uint64_t)alignment - 1u);
        if (!debug_expression_uleb(
                expression, 32u, &size, pointer_mask) ||
            size >= 32u) return 0u;
        expression[size++] = 0x1au; /* DW_OP_and */
    }
    if (dereference) {
        if (size >= 32u) {
            rcc_fatal("DWARF VLA location expression is too large");
            return 0u;
        }
        expression[size++] = 0x06u; /* DW_OP_deref: load saved VLA address */
    }
    return size;
}

static void debug_expr_breg(ObjSection* section, int architecture,
                            int64_t offset, uint32_t alignment,
                            bool dereference) {
    uint8_t expression[32];
    size_t size = debug_build_expr_breg(expression, architecture, offset,
                                        alignment, dereference);
    if (!section || size == 0u) return;
    debug_line_uleb(section, (uint64_t)size);
    section_add_bytes(section, expression, size);
}

static bool debug_type_has_vla_bound(const Type* type) {
    if (!type) return false;
    if (type->kind == TYPE_PTR) {
        return debug_type_has_vla_bound(type->base);
    }
    if (type->kind != TYPE_ARRAY) return false;
    return (type->array_len < 0 && type->array_bound != NULL) ||
           debug_type_has_vla_bound(type->base);
}

static int debug_vla_extent_index(const Type* owner, const Type* target) {
    const Type* cursor = owner;
    int index;
    if (cursor && cursor->kind == TYPE_PTR) cursor = cursor->base;
    for (; cursor && cursor->kind == TYPE_ARRAY; cursor = cursor->base) {
        if (cursor != target) continue;
        index = 0;
        for (cursor = target->base;
             cursor && cursor->kind == TYPE_ARRAY; cursor = cursor->base) {
            ++index;
        }
        return index;
    }
    return -1;
}

static bool debug_vla_extent_frame_offset(const Decl* declaration,
                                          const Type* extent_type,
                                          int architecture,
                                          int64_t* offset) {
    const Type* owner;
    int index;
    int word_size;
    int64_t frame_offset;
    if (!declaration || !extent_type || !offset) return false;
    owner = declaration->param_array_type
                ? declaration->param_array_type : declaration->type;
    index = debug_vla_extent_index(owner, extent_type);
    word_size = architecture == ARCH_X64 ? 8 : 4;
    if (index < 0 || index >= declaration->var_vla_extent_count) {
        return false;
    }
    frame_offset = (int64_t)declaration->var_vla_extent_offset +
                   (int64_t)index * word_size;
    if (frame_offset < INT_MIN || frame_offset > INT_MAX) return false;
    *offset = frame_offset;
    return true;
}

static void debug_expr_append_sleb(uint8_t* expression, size_t* size,
                                   int64_t value) {
    bool more;
    do {
        uint8_t byte = (uint8_t)((uint64_t)value & 0x7fu);
        value >>= 7;
        more = !((value == 0 && (byte & 0x40u) == 0u) ||
                 (value == -1 && (byte & 0x40u) != 0u));
        if (more) byte |= 0x80u;
        expression[(*size)++] = byte;
    } while (more);
}

static bool debug_vla_count_expression(const Decl* declaration,
                                       const Type* array_type,
                                       int architecture,
                                       uint8_t* expression,
                                       size_t* expression_size) {
    const Type* element_type;
    int64_t extent_offset;
    size_t size = 0u;
    if (!declaration || !array_type || array_type->kind != TYPE_ARRAY ||
        array_type->array_len >= 0 || !array_type->array_bound ||
        !expression || !expression_size ||
        !debug_vla_extent_frame_offset(declaration, array_type, architecture,
                                       &extent_offset)) {
        return false;
    }
    expression[size++] = architecture == ARCH_X64 ? 0x76u : 0x75u;
    debug_expr_append_sleb(expression, &size, extent_offset);
    expression[size++] = 0x06u; /* DW_OP_deref: load saved byte extent */

    element_type = array_type->base;
    if (element_type && element_type->kind == TYPE_ARRAY &&
        debug_type_has_vla_bound(element_type)) {
        int64_t element_extent_offset;
        if (!debug_vla_extent_frame_offset(declaration, element_type,
                                           architecture,
                                           &element_extent_offset)) {
            return false;
        }
        expression[size++] = architecture == ARCH_X64 ? 0x76u : 0x75u;
        debug_expr_append_sleb(expression, &size, element_extent_offset);
        expression[size++] = 0x06u; /* DW_OP_deref */
    } else {
        uint64_t element_size = element_type && element_type->size > 0
                                    ? (uint64_t)element_type->size : 0u;
        if (element_size == 0u) return false;
        expression[size++] = 0x10u; /* DW_OP_constu */
        do {
            uint8_t byte = (uint8_t)(element_size & 0x7fu);
            element_size >>= 7u;
            if (element_size != 0u) byte |= 0x80u;
            expression[size++] = byte;
        } while (element_size != 0u);
    }
    expression[size++] = 0x1bu; /* DW_OP_div: byte extent / element size */
    *expression_size = size;
    return true;
}

static uint32_t debug_emit_contextual_type_die(
    ObjSection* info, DebugTypeContext* types, const Type* type,
    const Decl* declaration, int architecture) {
    DebugTypeEntry* entry;
    uint32_t base_offset;
    uint32_t die_offset;
    if (!info || !types || !type) return UINT32_MAX;
    if (!debug_type_has_vla_bound(type)) {
        entry = debug_type_find(types, type);
        return entry ? entry->offset : UINT32_MAX;
    }
    if (info->size > UINT32_MAX) {
        rcc_fatal("DWARF contextual type DIE offset exceeds 32-bit range");
        return UINT32_MAX;
    }
    entry = debug_type_find(types, type);
    if (entry && entry->qualifier_base) {
        base_offset = debug_emit_contextual_type_die(
            info, types, entry->qualifier_base, declaration, architecture);
        if (base_offset == UINT32_MAX) return UINT32_MAX;
        if (info->size > UINT32_MAX) {
            rcc_fatal("DWARF contextual type DIE offset exceeds 32-bit range");
            return UINT32_MAX;
        }
        die_offset = (uint32_t)info->size;
        if (type->is_const) section_add_byte(info, 20u);
        else if (type->is_volatile) section_add_byte(info, 21u);
        else if (type->is_restrict) section_add_byte(info, 22u);
        else section_add_byte(info, 23u);
        debug_line_u32(info, base_offset);
        return die_offset;
    }
    if (type->kind == TYPE_PTR) {
        base_offset = debug_emit_contextual_type_die(
            info, types, type->base, declaration, architecture);
        if (base_offset == UINT32_MAX) return UINT32_MAX;
        if (info->size > UINT32_MAX) {
            rcc_fatal("DWARF contextual type DIE offset exceeds 32-bit range");
            return UINT32_MAX;
        }
        die_offset = (uint32_t)info->size;
        section_add_byte(info, 6u); /* DW_TAG_pointer_type */
        section_add_byte(info, (uint8_t)(type->size > 255 ? 255 :
                                         (type->size < 0 ? 0 : type->size)));
        debug_line_u32(info, base_offset);
        return die_offset;
    }
    if (type->kind == TYPE_ARRAY) {
        uint8_t expression[64];
        size_t expression_size = 0u;
        bool has_dynamic_count = type->array_len < 0 && type->array_bound &&
            debug_vla_count_expression(declaration, type, architecture,
                                       expression, &expression_size);
        base_offset = debug_emit_contextual_type_die(
            info, types, type->base, declaration, architecture);
        if (base_offset == UINT32_MAX) return UINT32_MAX;
        if (info->size > UINT32_MAX) {
            rcc_fatal("DWARF contextual type DIE offset exceeds 32-bit range");
            return UINT32_MAX;
        }
        die_offset = (uint32_t)info->size;
        section_add_byte(info, 10u); /* DW_TAG_array_type */
        debug_line_u32(info, base_offset);
        if (type->array_len >= 0) {
            section_add_byte(info, 11u); /* fixed DW_TAG_subrange_type */
            debug_line_u32(info, type->array_len > 0
                                     ? (uint32_t)type->array_len - 1u : 0u);
            section_add_byte(info, 0u);
        } else if (has_dynamic_count) {
            section_add_byte(info, 34u); /* DW_AT_count expression */
            debug_line_uleb(info, expression_size);
            section_add_bytes(info, expression, expression_size);
            section_add_byte(info, 0u);
        }
        section_add_byte(info, 0u);
        return die_offset;
    }
    return entry ? entry->offset : UINT32_MAX;
}

static void debug_expr_addr(ObjectFile* obj, ObjSection* info,
                            int info_section, const char* symbol,
                            int architecture, bool is_tls) {
    uint64_t address_offset;
    uint32_t address_size = architecture == ARCH_X64 ? 8u : 4u;

    if (!obj || !info || info_section < 0 || !symbol || symbol[0] == '\0') {
        rcc_fatal("DWARF global variable location is missing a symbol");
        return;
    }
    if (is_tls) {
        /* DW_OP_form_tls_address converts a signed TLS template offset to
         * the current thread's address; DW_OP_addr would describe a normal
         * process address and is incorrect for a TLS symbol. */
        debug_line_uleb(info, 6u);
        section_add_byte(info, 0x0du); /* DW_OP_const4s */
        address_offset = info->size;
        for (uint32_t byte = 0u; byte < sizeof(uint32_t); ++byte) {
            section_add_byte(info, 0u);
        }
        section_add_byte(info, 0x9bu); /* DW_OP_form_tls_address */
        objfile_add_reloc(obj, info_section, address_offset, symbol,
                          RELOC_TLSOFF32S, 0);
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

static void debug_emit_variable_die(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, const ModuleSymbol* function,
    const Decl* declaration, const Stmt* declaration_statement,
    const Stmt* location_scope,
    uint8_t abbreviation, int architecture);

static uint32_t debug_emit_location_list(
    ObjectFile* obj, const Module* mod, const char* filename,
    const ModuleSymbol* function,
    const Stmt* scope, const Stmt* declaration_statement,
    int architecture, int64_t frame_offset, uint32_t alignment,
    bool dereference, bool* empty);

static void debug_emit_variable_die(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, const ModuleSymbol* function,
    const Decl* declaration, const Stmt* declaration_statement,
    const Stmt* location_scope,
    uint8_t abbreviation, int architecture) {
    DebugTypeEntry* type_entry;
    uint32_t type_offset;
    int file_index;
    int64_t frame_offset;
    uint32_t frame_alignment = 0u;
    bool has_frame_location = true;
    bool use_location_list = false;
    bool empty_location_list = false;
    uint32_t location_list_offset = 0u;
    int64_t constant_value = 0;
    uint64_t unsigned_constant_value = 0u;
    uint64_t floating_constant_bits = 0u;
    uint8_t floating_constant_size = 0u;
    bool has_constant_value = false;
    if (!obj || !info || !strings || !declaration || !declaration->name ||
        declaration->name[0] == '\0') return;
    type_entry = debug_type_find(types, declaration->type);
    if (!type_entry) {
        rcc_fatal("DWARF variable type was not collected");
        return;
    }
    type_offset = type_entry->offset;
    if (debug_type_has_vla_bound(declaration->type)) {
        uint32_t contextual_type_offset = debug_emit_contextual_type_die(
            info, types, declaration->type, declaration,
            architecture == ARCH_X64 ? ARCH_X64 : ARCH_X86);
        if (contextual_type_offset != UINT32_MAX) {
            type_offset = contextual_type_offset;
        }
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
                frame_alignment = location->alignment;
                has_frame_location = true;
                break;
            }
        }
        if (has_frame_location && declaration_statement &&
            !declaration_statement->debug_code_ranges &&
            declaration_statement->debug_code_end <=
                declaration_statement->debug_code_start &&
            !declaration_statement->debug_location_start_valid) {
            /* No instruction in this lexical lifetime can describe the
             * declaration's frame slot.  Do not make it appear available
             * from the enclosing block's first PC. */
            has_frame_location = false;
        }
    }
    if (has_frame_location && abbreviation == 4u && location_scope &&
        location_scope->debug_code_end > location_scope->debug_code_start) {
        location_list_offset = debug_emit_location_list(
            obj, mod, filename, function, location_scope,
            declaration_statement, architecture, frame_offset,
            frame_alignment, declaration->var_is_vla,
            &empty_location_list);
        if (empty_location_list) {
            has_frame_location = false;
        } else {
            use_location_list = location_list_offset != UINT32_MAX;
            if (use_location_list) abbreviation = 39u;
        }
    }
    if (!has_frame_location) {
        if (abbreviation == 3u) abbreviation = 31u;
        else if (abbreviation == 4u) abbreviation = 32u;
        else if (abbreviation == 27u) abbreviation = 33u;
        else return;
        if (abbreviation == 32u && declaration->kind == DECL_VAR &&
            declaration->type && declaration->type->is_const &&
            !declaration->type->is_volatile &&
            !declaration->type->is_reference &&
            ((declaration->type->kind >= TYPE_BOOL &&
              declaration->type->kind <= TYPE_LLONG) ||
             declaration->type->kind == TYPE_ENUM ||
             declaration->type->kind == TYPE_FLOAT ||
             declaration->type->kind == TYPE_DOUBLE) &&
            declaration->var_debug_const_value_valid) {
            /* Keep the known source value available to debuggers after
             * optimization removes the object's stack storage. */
            if (declaration->var_debug_const_value_float) {
                floating_constant_bits =
                    declaration->var_debug_const_float_bits;
                floating_constant_size =
                    declaration->var_debug_const_float_size;
                abbreviation = 56u;
            } else if (declaration->var_debug_const_value_unsigned) {
                unsigned_constant_value =
                    declaration->var_debug_const_unsigned_value;
                abbreviation = 55u;
            } else {
                constant_value = declaration->var_debug_const_value;
                abbreviation = 54u;
            }
            has_constant_value = true;
        }
    }
    section_add_byte(info, abbreviation);
    debug_line_u32(info, debug_str_add(strings, declaration->name));
    debug_line_u32(info, type_offset);
    debug_line_u32(info, (uint32_t)file_index);
    debug_line_u32(info, declaration->loc.line);
    debug_line_u32(info, declaration->loc.column);
    if (has_frame_location && use_location_list) {
        debug_line_u32(info, location_list_offset);
    } else if (has_frame_location) {
        debug_expr_breg(info, architecture, frame_offset, frame_alignment,
                        declaration->var_is_vla);
    } else if (has_constant_value) {
        if (declaration->var_debug_const_value_float) {
            section_add_byte(info, floating_constant_size);
            for (uint8_t byte_index = 0u;
                 byte_index < floating_constant_size; ++byte_index) {
                section_add_byte(
                    info, (uint8_t)(floating_constant_bits >>
                                    (byte_index * 8u)));
            }
        } else if (declaration->var_debug_const_value_unsigned) {
            debug_line_uleb(info, unsigned_constant_value);
        } else {
            debug_line_sleb(info, constant_value);
        }
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
    debug_expr_addr(obj, info, info_section, symbol_name, architecture,
                    declaration->var_is_thread_local);
    rcc_free(scoped_name);
}

static void debug_emit_stmt_locals(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, int info_section,
    const ModuleSymbol* function, const Stmt* statement,
    const Stmt* location_scope, int architecture);

static int debug_line_file_index(const char* const* files, int file_count,
                                 const char* file);

static int objfile_section_index(const ObjectFile* object,
                                 const ObjSection* target);

static uint32_t debug_function_size(const Module* mod,
                                    const ModuleSymbol* function);

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

static bool debug_clip_location_range(
    uint32_t raw_start, uint32_t raw_end, uint32_t function_start,
    uint32_t function_end, bool has_lifetime_start, uint32_t lifetime_start,
    uint32_t* clipped_start, uint32_t* clipped_end, bool* included) {
    if (!clipped_start || !clipped_end || !included ||
        raw_end <= raw_start || raw_start < function_start ||
        raw_end > function_end) {
        return false;
    }
    *clipped_start = raw_start;
    *clipped_end = raw_end;
    if (has_lifetime_start && *clipped_start < lifetime_start) {
        *clipped_start = lifetime_start;
    }
    *included = *clipped_end > *clipped_start;
    return true;
}

static void debug_location_list_append_entry(
    ObjectFile* obj, ObjSection* locations, int locations_section,
    const char* symbol_name, int architecture, uint32_t function_offset,
    uint32_t start, uint32_t end, const uint8_t* expression,
    size_t expression_size) {
    int address_size = architecture == ARCH_X64 ? 8 : 4;
    uint64_t start_reloc = locations->size;
    debug_line_address(locations, 0u, address_size);
    objfile_add_reloc(
        obj, locations_section, start_reloc, symbol_name,
        address_size == 8 ? RELOC_ABS64 : RELOC_ABS32U,
        (int64_t)(start - function_offset));
    uint64_t end_reloc = locations->size;
    debug_line_address(locations, 0u, address_size);
    objfile_add_reloc(
        obj, locations_section, end_reloc, symbol_name,
        address_size == 8 ? RELOC_ABS64 : RELOC_ABS32U,
        (int64_t)(end - function_offset));
    debug_line_u16(locations, (uint16_t)expression_size);
    section_add_bytes(locations, expression, expression_size);
}

static uint32_t debug_emit_location_list(
    ObjectFile* obj, const Module* mod, const char* filename,
    const ModuleSymbol* function, const Stmt* scope,
    const Stmt* declaration_statement, int architecture,
    int64_t frame_offset, uint32_t alignment, bool dereference,
    bool* empty) {
    ObjSection* locations;
    const char* symbol_name;
    char* scoped_name = NULL;
    uint8_t expression[32];
    size_t expression_size;
    uint32_t list_offset;
    uint32_t function_size;
    uint32_t function_end;
    uint32_t lifetime_start = 0u;
    bool has_lifetime_start = false;
    uint64_t entry_size;
    uint64_t list_size;
    uint64_t entry_count;
    int locations_section;
    int address_size = architecture == ARCH_X64 ? 8 : 4;
    if (empty) *empty = false;
    if (!obj || !mod || !filename || !function || !scope ||
        scope->debug_code_end <= scope->debug_code_start) {
        return UINT32_MAX;
    }
    function_size = debug_function_size(mod, function);
    if (function_size == 0u || function->offset > UINT32_MAX - function_size) {
        rcc_fatal("DWARF location-list function range is invalid");
        return UINT32_MAX;
    }
    if (declaration_statement) {
        if (declaration_statement->debug_code_ranges) {
            for (const StmtDebugRange* range =
                     declaration_statement->debug_code_ranges;
                 range; range = range->next) {
                if (!has_lifetime_start || range->end > lifetime_start) {
                    lifetime_start = range->end;
                    has_lifetime_start = true;
                }
            }
        } else if (!mod->debug_verified_backend ||
                   declaration_statement->debug_code_end >
                       declaration_statement->debug_code_start) {
            lifetime_start = declaration_statement->debug_code_end;
            has_lifetime_start = true;
        } else if (declaration_statement->debug_location_start_valid) {
            lifetime_start = declaration_statement->debug_location_start;
            has_lifetime_start = true;
        }
    }
    function_end = function->offset + function_size;
    expression_size = debug_build_expr_breg(
        expression, architecture, frame_offset, alignment, dereference);
    if (expression_size == 0u || expression_size > UINT16_MAX) {
        rcc_fatal("DWARF location-list expression exceeds 16-bit length");
        return UINT32_MAX;
    }
    entry_count = 0u;
    if (scope->debug_code_ranges) {
        for (const StmtDebugRange* range = scope->debug_code_ranges;
             range; range = range->next) {
            uint32_t start;
            uint32_t end;
            bool included;
            if (!debug_clip_location_range(
                    range->start, range->end, function->offset,
                    function_end, has_lifetime_start, lifetime_start,
                    &start, &end, &included)) {
                rcc_fatal("DWARF variable location range is outside function '%s'",
                          function->name ? function->name : "<unnamed>");
                return UINT32_MAX;
            }
            if (included) ++entry_count;
        }
    } else {
        uint32_t start;
        uint32_t end;
        bool included;
        if (!debug_clip_location_range(
                scope->debug_code_start, scope->debug_code_end,
                function->offset, function_end, has_lifetime_start,
                lifetime_start, &start, &end, &included)) {
            rcc_fatal("DWARF variable location range is outside function '%s'",
                      function->name ? function->name : "<unnamed>");
            return UINT32_MAX;
        }
        if (included) entry_count = 1u;
    }
    if (entry_count == 0u) {
        if (empty) *empty = true;
        return UINT32_MAX;
    }
    locations = objfile_get_section(obj, ".debug_loc");
    if (!locations) {
        locations = objfile_add_section(
            obj, ".debug_loc", SECT_DEBUG_LOC, 0u);
        locations->align = 1u;
    }
    if (locations->size > UINT32_MAX) {
        rcc_fatal("DWARF location-list section exceeds 32-bit offsets");
        return UINT32_MAX;
    }
    locations_section = objfile_section_index(obj, locations);
    if (locations_section < 0) {
        rcc_fatal("DWARF location-list section is detached");
        return UINT32_MAX;
    }
    entry_size = (uint64_t)address_size * 2u + 2u + expression_size;
    if (entry_size == 0u ||
        entry_count > (UINT32_MAX - (uint64_t)address_size * 2u) /
                          entry_size) {
        rcc_fatal("DWARF location list exceeds 32-bit section offsets");
        return UINT32_MAX;
    }
    list_size = entry_count * entry_size + (uint64_t)address_size * 2u;
    if (list_size > UINT32_MAX - locations->size) {
        rcc_fatal("DWARF location-list section exceeds 32-bit offsets");
        return UINT32_MAX;
    }
    symbol_name = function->name;
    if (!function->is_global) {
        scoped_name = module_scoped_symbol(filename, function->name);
        symbol_name = scoped_name;
    }
    list_offset = (uint32_t)locations->size;
    uint64_t emitted_entries = 0u;
    if (scope->debug_code_ranges) {
        for (const StmtDebugRange* range = scope->debug_code_ranges;
             range; range = range->next) {
            uint32_t start;
            uint32_t end;
            bool included;
            if (!debug_clip_location_range(
                    range->start, range->end, function->offset,
                    function_end, has_lifetime_start, lifetime_start,
                    &start, &end, &included)) {
                rcc_free(scoped_name);
                rcc_fatal("DWARF variable location range changed during emission");
                return UINT32_MAX;
            }
            if (!included) continue;
            debug_location_list_append_entry(
                obj, locations, locations_section, symbol_name, architecture,
                function->offset, start, end, expression, expression_size);
            ++emitted_entries;
        }
    } else {
        uint32_t start;
        uint32_t end;
        bool included;
        if (!debug_clip_location_range(
                scope->debug_code_start, scope->debug_code_end,
                function->offset, function_end, has_lifetime_start,
                lifetime_start, &start, &end, &included)) {
            rcc_free(scoped_name);
            rcc_fatal("DWARF variable location range changed during emission");
            return UINT32_MAX;
        }
        if (included) {
            debug_location_list_append_entry(
                obj, locations, locations_section, symbol_name, architecture,
                function->offset, start, end, expression, expression_size);
            ++emitted_entries;
        }
    }
    if (emitted_entries != entry_count) {
        rcc_free(scoped_name);
        rcc_fatal("DWARF location-list entry count changed during emission");
        return UINT32_MAX;
    }
    debug_line_address(locations, 0u, address_size);
    debug_line_address(locations, 0u, address_size);
    rcc_free(scoped_name);
    return list_offset;
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
        rcc_fatal("DWARF lexical block code range is invalid in '%s' "
                  "(function=%u, block=%u..%u)",
                  function->name ? function->name : "<unnamed>",
                  function->offset, statement->debug_code_start,
                  statement->debug_code_end);
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
    debug_line_u32(info, (uint32_t)file_index);
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
            debug_emit_variable_die(
                obj, info, strings, types, files, file_count, mod, filename,
                function, handler->parameter, NULL, NULL, 3u, architecture);
        }
        debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                               mod, filename, info_section, function,
                               handler->body, NULL, architecture);
    }
}

static void debug_emit_local_type_alias_die(
    ObjSection* info, ObjSection* strings, DebugTypeContext* types,
    const char* const* files, int file_count, const Decl* declaration) {
    Type* alias_type;
    DebugTypeEntry* type_entry;
    int file_index;
    if (!info || !strings || !types || !declaration ||
        declaration->kind != DECL_TYPEDEF || !declaration->name ||
        !declaration->loc.filename || declaration->loc.line <= 0 ||
        declaration->loc.column < 0) {
        return;
    }
    alias_type = declaration->typedef_type
        ? declaration->typedef_type : declaration->type;
    if (!alias_type || debug_type_is_dependent(alias_type, 0u)) return;
    type_entry = debug_type_find(types, alias_type);
    file_index = debug_line_file_index(
        files, file_count, declaration->loc.filename);
    if (!type_entry || file_index <= 0) {
        rcc_fatal("DWARF local typedef is missing its type or source file");
        return;
    }
    section_add_byte(info, 44u); /* DW_TAG_typedef */
    debug_line_u32(info, debug_str_add(strings, declaration->name));
    debug_type_ref(info, NULL, type_entry);
    debug_line_u32(info, (uint32_t)file_index);
    debug_line_u32(info, (uint32_t)declaration->loc.line);
    debug_line_u32(info, (uint32_t)declaration->loc.column);
}

static void debug_emit_stmt_locals(
    ObjectFile* obj, ObjSection* info, ObjSection* strings,
    DebugTypeContext* types, const char* const* files, int file_count,
    const Module* mod, const char* filename, int info_section,
    const ModuleSymbol* function, const Stmt* statement,
    const Stmt* location_scope, int architecture) {
    const StmtList* item;
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            {
                bool has_range = !statement->block_no_scope &&
                    statement->debug_code_end > statement->debug_code_start;
                const Stmt* child_scope = has_range ? statement
                                                     : location_scope;
                if (has_range) {
                debug_emit_lexical_block_die(
                    obj, info, files, file_count, mod, filename, info_section,
                    function, statement, architecture);
                }
                for (item = statement->block_stmts; item; item = item->next) {
                    debug_emit_stmt_locals(
                        obj, info, strings, types, files, file_count, mod,
                        filename, info_section, function, item->stmt,
                        child_scope, architecture);
                }
                if (has_range) section_add_byte(info, 0u);
            }
            break;
        case STMT_FOR:
            {
                bool has_range = statement->debug_code_end >
                                 statement->debug_code_start;
                const Stmt* child_scope = has_range ? statement
                                                    : location_scope;
                if (has_range) {
                    debug_emit_lexical_block_die(
                        obj, info, files, file_count, mod, filename,
                        info_section, function, statement, architecture);
                }
                debug_emit_stmt_locals(
                    obj, info, strings, types, files, file_count, mod,
                    filename, info_section, function, statement->for_init,
                    child_scope, architecture);
                debug_emit_stmt_locals(
                    obj, info, strings, types, files, file_count, mod,
                    filename, info_section, function, statement->for_body,
                    child_scope, architecture);
                if (has_range) section_add_byte(info, 0u);
            }
            break;
        case STMT_IF:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->if_then, location_scope,
                                   architecture);
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->if_else, location_scope,
                                   architecture);
            break;
        case STMT_WHILE:
        case STMT_DO:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->while_body, location_scope,
                                   architecture);
            break;
        case STMT_SWITCH:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->switch_body, location_scope,
                                   architecture);
            break;
        case STMT_CASE:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->case_stmt, location_scope,
                                   architecture);
            break;
        case STMT_DEFAULT:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->default_stmt, location_scope,
                                   architecture);
            break;
        case STMT_LABEL:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->label_stmt, location_scope,
                                   architecture);
            break;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR &&
                !statement->decl->var_is_global &&
                !statement->decl->var_is_static_local) {
                debug_emit_variable_die(
                    obj, info, strings, types, files, file_count, mod,
                    filename, function, statement->decl, statement,
                    location_scope, 4u, architecture);
            } else if (statement->decl &&
                       statement->decl->kind == DECL_TYPEDEF) {
                debug_emit_local_type_alias_die(
                    info, strings, types, files, file_count,
                    statement->decl);
            }
            break;
        case STMT_TRY:
            debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                                   mod, filename, info_section, function,
                                   statement->try_body, location_scope,
                                   architecture);
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
            (symbol->section != MODULE_SYMBOL_RODATA &&
             symbol->section != MODULE_SYMBOL_DATA &&
             symbol->section != MODULE_SYMBOL_BSS &&
             symbol->section != MODULE_SYMBOL_TLS)) {
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
            (symbol->section != MODULE_SYMBOL_RODATA &&
             symbol->section != MODULE_SYMBOL_DATA &&
             symbol->section != MODULE_SYMBOL_BSS &&
             symbol->section != MODULE_SYMBOL_TLS)) {
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
        debug_emit_variable_die(
            obj, info, strings, types, files, file_count, mod, filename,
            symbol, function->func_this_param, NULL, NULL, 27u,
            architecture);
        section_add_byte(info, 1u); /* DW_AT_artificial */
    }
    for (DeclList* parameter = function->func_params; parameter;
         parameter = parameter->next) {
        debug_emit_variable_die(
            obj, info, strings, types, files, file_count, mod, filename,
            symbol, parameter->decl, NULL, NULL, 3u, architecture);
    }
    debug_emit_static_local_stmt(
        obj, info, strings, types, files, file_count, mod, filename,
        info_section, function->func_body, architecture);
    debug_emit_stmt_locals(obj, info, strings, types, files, file_count,
                           mod, filename, info_section, symbol,
                           function->func_body, NULL, architecture);
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
    for (DeclList* item = mod->debug_ast ? mod->debug_ast->decls : NULL;
         item; item = item->next) {
        Decl* declaration = item->decl;
        const ModuleSymbol* symbol = debug_find_global_symbol(mod, declaration);
        if (!symbol || debug_find_global_decl(mod, symbol) != declaration ||
            !declaration->loc.filename ||
            declaration->loc.filename[0] == '\0' || declaration->loc.line <= 0) {
            continue;
        }
        debug_file_add(&files, &file_count, &file_capacity,
                       declaration->loc.filename);
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
    debug_collect_inline_files(mod, &files, &file_count, &file_capacity);
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

typedef struct DebugInlineOrigin {
    const Decl* declaration;
    uint32_t die_offset;
} DebugInlineOrigin;

typedef struct DebugInlineLocalOrigin {
    const Decl* function;
    const Decl* declaration;
    uint32_t die_offset;
    struct DebugInlineLocalOrigin* next;
} DebugInlineLocalOrigin;

typedef struct DebugInlineParameterOrigin {
    const Decl* function;
    const Decl* declaration;
    uint32_t die_offset;
    struct DebugInlineParameterOrigin* next;
} DebugInlineParameterOrigin;

static DebugInlineOrigin* debug_inline_origin_find(
    DebugInlineOrigin* origins, size_t origin_count,
    const Decl* declaration) {
    if (!origins || !declaration) return NULL;
    for (size_t index = 0u; index < origin_count; ++index) {
        if (origins[index].declaration == declaration) return &origins[index];
    }
    return NULL;
}

static bool debug_inline_local_constant_supported(const Decl* declaration) {
    const Type* type = declaration ? declaration->type : NULL;
    if (!declaration || declaration->kind != DECL_VAR ||
        !declaration->name || !declaration->name[0] ||
        !declaration->loc.filename || declaration->loc.line <= 0 ||
        declaration->loc.column < 0 || declaration->var_is_global ||
        declaration->var_is_static_local ||
        declaration->var_is_thread_local || declaration->var_is_vla ||
        declaration->var_cleanup || declaration->var_cleanups || !type ||
        !type->is_const || type->is_volatile || type->is_reference ||
        !declaration->var_debug_const_value_valid) {
        return false;
    }
    if (declaration->var_debug_const_value_float) {
        return (type->kind == TYPE_FLOAT &&
                declaration->var_debug_const_float_size == 4u) ||
            (type->kind == TYPE_DOUBLE &&
             declaration->var_debug_const_float_size == 8u);
    }
    return ((type->kind >= TYPE_BOOL && type->kind <= TYPE_LLONG) ||
            type->kind == TYPE_ENUM) &&
        type->size > 0 && type->size <= 8;
}

static DebugInlineLocalOrigin* debug_inline_local_origin_find(
    DebugInlineLocalOrigin* origins, const Decl* function,
    const Decl* declaration) {
    for (; origins; origins = origins->next) {
        if (origins->function == function &&
            origins->declaration == declaration) return origins;
    }
    return NULL;
}

static DebugInlineParameterOrigin* debug_inline_parameter_origin_find(
    DebugInlineParameterOrigin* origins, const Decl* function,
    const Decl* declaration) {
    for (; origins; origins = origins->next) {
        if (origins->function == function &&
            origins->declaration == declaration) return origins;
    }
    return NULL;
}

static bool debug_inline_parameter_storage_type(const Type* type) {
    return type &&
        ((type->kind >= TYPE_BOOL && type->kind <= TYPE_LLONG) ||
         type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE ||
         type->kind == TYPE_PTR || type->kind == TYPE_NULLPTR ||
         type->kind == TYPE_ENUM);
}

static bool debug_inline_parameter_storage_type_equal(
    const Type* left, const Type* right, unsigned int depth) {
    if (!left || !right || depth >= 32u || left->kind != right->kind ||
        left->size != right->size || left->is_unsigned != right->is_unsigned ||
        left->is_plain_char != right->is_plain_char ||
        left->is_char8 != right->is_char8 ||
        left->is_wchar != right->is_wchar ||
        left->is_char16 != right->is_char16 ||
        left->is_char32 != right->is_char32 ||
        left->is_const != right->is_const ||
        left->is_volatile != right->is_volatile ||
        left->is_atomic != right->is_atomic ||
        left->is_restrict != right->is_restrict ||
        left->is_reference != right->is_reference ||
        left->is_rvalue_reference != right->is_rvalue_reference) {
        return false;
    }
    if (left == right) return true;
    switch (left->kind) {
        case TYPE_PTR:
            return left->cxx_is_member_pointer ==
                       right->cxx_is_member_pointer &&
                   (!left->cxx_is_member_pointer ||
                    debug_inline_parameter_storage_type_equal(
                        left->cxx_member_pointer_owner,
                        right->cxx_member_pointer_owner, depth + 1u)) &&
                   debug_inline_parameter_storage_type_equal(
                       left->base, right->base, depth + 1u);
        case TYPE_ENUM:
            return left->enum_is_scoped == right->enum_is_scoped &&
                   left->enum_tag && right->enum_tag &&
                   strcmp(left->enum_tag, right->enum_tag) == 0 &&
                   ((!left->cxx_namespace && !right->cxx_namespace) ||
                    (left->cxx_namespace && right->cxx_namespace &&
                     strcmp(left->cxx_namespace,
                            right->cxx_namespace) == 0)) &&
                   ((!left->cxx_scope_identity &&
                     !right->cxx_scope_identity) ||
                    (left->cxx_scope_identity &&
                     right->cxx_scope_identity &&
                     strcmp(left->cxx_scope_identity,
                            right->cxx_scope_identity) == 0));
        case TYPE_STRUCT:
        case TYPE_UNION:
            return left->cxx_class &&
                   left->cxx_class == right->cxx_class;
        default:
            return left->kind != TYPE_FUNC;
    }
}

static bool debug_inline_parameter_storage_type_equal_ignoring_top_cv(
    const Type* left, const Type* right, unsigned int depth) {
    Type left_unqualified;
    Type right_unqualified;
    if (!left || !right) return false;
    left_unqualified = *left;
    right_unqualified = *right;
    left_unqualified.is_const = right_unqualified.is_const;
    left_unqualified.is_volatile = right_unqualified.is_volatile;
    return debug_inline_parameter_storage_type_equal(
        &left_unqualified, &right_unqualified, depth);
}

static bool debug_inline_parameter_expression_is_read_only(
    const Expr* expression, const Decl* callee) {
    if (!expression || !callee) return false;
    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
            return true;
        case EXPR_IDENT:
            for (const DeclList* item = callee->func_params; item;
                 item = item->next) {
                if (item->decl == expression->ident_decl) return true;
            }
            return false;
        case EXPR_POS:
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_CAST:
            return debug_inline_parameter_expression_is_read_only(
                expression->kind == EXPR_CAST
                    ? expression->cast_expr : expression->unary_operand,
                callee);
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_COMMA:
            return debug_inline_parameter_expression_is_read_only(
                       expression->binary_lhs, callee) &&
                   debug_inline_parameter_expression_is_read_only(
                       expression->binary_rhs, callee);
        case EXPR_COND:
            return debug_inline_parameter_expression_is_read_only(
                       expression->cond_test, callee) &&
                   debug_inline_parameter_expression_is_read_only(
                       expression->cond_then, callee) &&
                   debug_inline_parameter_expression_is_read_only(
                       expression->cond_else, callee);
        default:
            return false;
    }
}

static bool debug_inline_parameter_return_tree_is_read_only(
    const Stmt* statement, const Decl* callee, unsigned int depth) {
    if (!statement || !callee || depth > 16u) return false;
    if (statement->kind == STMT_BLOCK) {
        const StmtList* item = statement->block_stmts;
        return item && !item->next &&
            debug_inline_parameter_return_tree_is_read_only(
                item->stmt, callee, depth + 1u);
    }
    if (statement->kind == STMT_RETURN) {
        return statement->return_val &&
            debug_inline_parameter_expression_is_read_only(
                statement->return_val, callee);
    }
    if (statement->kind == STMT_IF) {
        return statement->if_cond && statement->if_then &&
            statement->if_else &&
            debug_inline_parameter_expression_is_read_only(
                statement->if_cond, callee) &&
            debug_inline_parameter_return_tree_is_read_only(
                statement->if_then, callee, depth + 1u) &&
            debug_inline_parameter_return_tree_is_read_only(
                statement->if_else, callee, depth + 1u);
    }
    return false;
}

static bool debug_inline_parameter_body_is_read_only(const Decl* callee) {
    return callee && debug_inline_parameter_return_tree_is_read_only(
        callee->func_body, callee, 0u);
}

static bool debug_inline_integer_constant_value(
    const Expr* expression, int64_t* value, unsigned int depth);

static bool debug_inline_address_path(
    const Expr* expression, const Decl** source, int64_t* address_addend,
    bool allow_one_past, bool allow_frame_source, unsigned int depth);

static const Expr* debug_inline_static_array_designator(
    const Expr* expression, const Type** array_type) {
    if (array_type) *array_type = NULL;
    while (expression && expression->kind == EXPR_CAST &&
           expression->cast_expr) {
        const Type* cast_type = expression->cast_type;
        const Type* source_type = expression->cast_expr->type;
        int address_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
        if (!cast_type || cast_type->kind != TYPE_PTR || !source_type ||
            cast_type->cxx_is_member_pointer ||
            cast_type->size != address_size ||
            expression->cxx_pointer_adjustment_valid ||
            expression->cxx_member_pointer_adjustment_valid ||
            expression->cxx_virtual_base_adjustment ||
            (source_type->kind != TYPE_ARRAY &&
             source_type->kind != TYPE_PTR) ||
            (source_type->kind == TYPE_PTR &&
             (source_type->size != address_size ||
              source_type->cxx_is_member_pointer))) {
            return NULL;
        }
        expression = expression->cast_expr;
    }
    if (!expression) return NULL;
    if (expression->type && expression->type->kind == TYPE_ARRAY) {
        if (array_type) *array_type = expression->type;
        return expression;
    }
    if (expression->kind == EXPR_INDEX && expression->index_base &&
        expression->index_base->type) {
        const Type* base_type = expression->index_base->type;
        const Type* subarray_type =
            (base_type->kind == TYPE_ARRAY || base_type->kind == TYPE_PTR)
                ? base_type->base : NULL;
        if (subarray_type && subarray_type->kind == TYPE_ARRAY &&
            subarray_type->size > 0) {
            if (array_type) *array_type = subarray_type;
            return expression;
        }
    }
    if (expression->kind == EXPR_MEMBER && expression->member_field &&
        expression->member_field->type &&
        expression->member_field->type->kind == TYPE_ARRAY &&
        expression->member_field->type->size > 0 &&
        expression->member_field->type->base && expression->type &&
        expression->type->kind == TYPE_PTR && expression->type->base &&
        expression->type->base->size ==
            expression->member_field->type->base->size) {
        if (array_type) *array_type = expression->member_field->type;
        return expression;
    }
    if (expression->kind == EXPR_IDENT && expression->ident_decl &&
        expression->ident_decl->kind == DECL_VAR &&
        expression->ident_decl->type &&
        expression->ident_decl->type->kind == TYPE_ARRAY) {
        if (array_type) *array_type = expression->ident_decl->type;
        return expression;
    }
    return NULL;
}

static bool debug_inline_integer_constant_type(const Type* type) {
    return type &&
        ((type->kind >= TYPE_BOOL && type->kind <= TYPE_LLONG) ||
         type->kind == TYPE_ENUM);
}

static bool debug_inline_integer_constant_mask(const Type* type,
                                               uint64_t* mask) {
    unsigned int width;
    if (!mask || !debug_inline_integer_constant_type(type) ||
        type->size <= 0 || type->size > 8) {
        return false;
    }
    width = (unsigned int)type->size * 8u;
    *mask = width == 64u ? UINT64_MAX : (UINT64_C(1) << width) - 1u;
    return true;
}

static int64_t debug_inline_integer_constant_signed_minimum(
    const Type* type) {
    unsigned int width = (unsigned int)type->size * 8u;
    return width == 64u ? INT64_MIN
        : -(INT64_C(1) << (width - 1u));
}

static bool debug_inline_integer_constant_signed_value(
    uint64_t bits, const Type* type, int64_t* value) {
    uint64_t mask;
    uint64_t sign_bit;
    unsigned int width;
    if (!value || !debug_inline_integer_constant_mask(type, &mask) ||
        type->is_unsigned || type->kind == TYPE_BOOL) {
        return false;
    }
    width = (unsigned int)type->size * 8u;
    bits &= mask;
    sign_bit = UINT64_C(1) << (width - 1u);
    if ((bits & sign_bit) == 0u) {
        *value = (int64_t)bits;
    } else if (width == 64u) {
        uint64_t magnitude = (~bits) + 1u;
        *value = magnitude == sign_bit
            ? INT64_MIN : -(int64_t)magnitude;
    } else {
        *value = -(int64_t)((UINT64_C(1) << width) - bits);
    }
    return true;
}

static bool debug_inline_integer_constant_signed_fits(
    int64_t value, const Type* type) {
    unsigned int width;
    int64_t minimum;
    int64_t maximum;
    if (!debug_inline_integer_constant_type(type) || type->size <= 0 ||
        type->size > 8 ||
        type->is_unsigned || type->kind == TYPE_BOOL) {
        return false;
    }
    width = (unsigned int)type->size * 8u;
    if (width == 64u) return true;
    minimum = debug_inline_integer_constant_signed_minimum(type);
    maximum = (INT64_C(1) << (width - 1u)) - 1;
    return value >= minimum && value <= maximum;
}

static bool debug_inline_integer_constant_bits(
    const Expr* expression, uint64_t* value, unsigned int depth);

static bool debug_inline_integer_constant_convert_bits(
    uint64_t input, const Type* source_type, const Type* target_type,
    uint64_t* output) {
    uint64_t source_mask;
    uint64_t target_mask;
    unsigned int source_width;
    if (!output || !debug_inline_integer_constant_mask(source_type,
                                                        &source_mask) ||
        !debug_inline_integer_constant_mask(target_type, &target_mask)) {
        return false;
    }
    input &= source_mask;
    if (target_type->kind == TYPE_BOOL) {
        *output = input != 0u;
        return true;
    }
    source_width = (unsigned int)source_type->size * 8u;
    if (source_type->kind != TYPE_BOOL && !source_type->is_unsigned &&
        source_width < 64u &&
        (input & (UINT64_C(1) << (source_width - 1u))) != 0u) {
        input |= ~source_mask;
    }
    *output = input & target_mask;
    return true;
}

static bool debug_inline_integer_constant_bits(
    const Expr* expression, uint64_t* value, unsigned int depth) {
    uint64_t left_bits;
    uint64_t right_bits;
    uint64_t result_bits;
    uint64_t mask;
    int64_t left;
    int64_t right;
    int64_t result;
    const Type* result_type;
    if (!expression || !value || depth >= 32u ||
        !debug_inline_integer_constant_mask(expression->type, &mask)) {
        return false;
    }
    result_type = expression->type;
    if (expression->kind == EXPR_INT_LIT ||
        expression->kind == EXPR_CHAR_LIT) {
        int64_t literal = expression->kind == EXPR_INT_LIT
            ? expression->int_val : expression->char_val;
        result_bits = (uint64_t)literal & mask;
        if (result_type->kind == TYPE_BOOL) result_bits = literal != 0;
        *value = result_bits;
        return true;
    }
    if (expression->kind == EXPR_CAST) {
        if (!expression->cast_expr ||
            !debug_inline_integer_constant_bits(
                expression->cast_expr, &left_bits, depth + 1u) ||
            !debug_inline_integer_constant_convert_bits(
                left_bits, expression->cast_expr->type,
                expression->cast_type, &result_bits)) {
            return false;
        }
        return debug_inline_integer_constant_convert_bits(
            result_bits, expression->cast_type, result_type, value);
    }
    if (expression->kind == EXPR_POS) {
        return expression->unary_operand &&
            debug_inline_integer_constant_bits(
                expression->unary_operand, &left_bits, depth + 1u) &&
            debug_inline_integer_constant_convert_bits(
                left_bits, expression->unary_operand->type, result_type,
                value);
    }
    if (expression->kind == EXPR_NEG) {
        if (!expression->unary_operand ||
            !debug_inline_integer_constant_bits(
                expression->unary_operand, &left_bits, depth + 1u) ||
            !debug_inline_integer_constant_convert_bits(
                left_bits, expression->unary_operand->type, result_type,
                &left_bits)) {
            return false;
        }
        if (result_type->is_unsigned) {
            *value = (UINT64_C(0) - left_bits) & mask;
            return true;
        }
        if (!debug_inline_integer_constant_signed_value(
                left_bits, result_type, &left) || left == INT64_MIN) {
            return false;
        }
        result = -left;
        if (!debug_inline_integer_constant_signed_fits(
                result, result_type)) {
            return false;
        }
        *value = (uint64_t)result & mask;
        return true;
    }
    if (expression->kind != EXPR_ADD && expression->kind != EXPR_SUB &&
        expression->kind != EXPR_MUL && expression->kind != EXPR_DIV &&
        expression->kind != EXPR_MOD) {
        return false;
    }
    if (!expression->binary_lhs || !expression->binary_rhs ||
        !debug_inline_integer_constant_bits(
            expression->binary_lhs, &left_bits, depth + 1u) ||
        !debug_inline_integer_constant_bits(
            expression->binary_rhs, &right_bits, depth + 1u) ||
        !debug_inline_integer_constant_convert_bits(
            left_bits, expression->binary_lhs->type, result_type,
            &left_bits) ||
        !debug_inline_integer_constant_convert_bits(
            right_bits, expression->binary_rhs->type, result_type,
            &right_bits)) {
        return false;
    }
    if (result_type->is_unsigned) {
        switch (expression->kind) {
            case EXPR_ADD: result_bits = left_bits + right_bits; break;
            case EXPR_SUB: result_bits = left_bits - right_bits; break;
            case EXPR_MUL: result_bits = left_bits * right_bits; break;
            case EXPR_DIV:
                if (right_bits == 0u) return false;
                result_bits = left_bits / right_bits;
                break;
            case EXPR_MOD:
                if (right_bits == 0u) return false;
                result_bits = left_bits % right_bits;
                break;
            default: return false;
        }
        *value = result_bits & mask;
        return true;
    }
    if (!debug_inline_integer_constant_signed_value(
            left_bits, result_type, &left) ||
        !debug_inline_integer_constant_signed_value(
            right_bits, result_type, &right)) {
        return false;
    }
    switch (expression->kind) {
        case EXPR_ADD:
            if ((right > 0 && left > INT64_MAX - right) ||
                (right < 0 && left < INT64_MIN - right)) return false;
            result = left + right;
            break;
        case EXPR_SUB:
            if ((right < 0 && left > INT64_MAX + right) ||
                (right > 0 && left < INT64_MIN + right)) return false;
            result = left - right;
            break;
        case EXPR_MUL:
            if (left > 0) {
                if ((right > 0 && left > INT64_MAX / right) ||
                    (right < 0 && right < INT64_MIN / left)) return false;
            } else if (left < 0) {
                if ((right > 0 && left < INT64_MIN / right) ||
                    (right < 0 && left < INT64_MAX / right)) return false;
            }
            result = left * right;
            break;
        case EXPR_DIV:
            if (right == 0 ||
                (right == -1 && left ==
                    debug_inline_integer_constant_signed_minimum(
                        result_type))) {
                return false;
            }
            result = left / right;
            break;
        case EXPR_MOD:
            if (right == 0 ||
                (right == -1 && left ==
                    debug_inline_integer_constant_signed_minimum(
                        result_type))) {
                return false;
            }
            result = left % right;
            break;
        default: return false;
    }
    if (!debug_inline_integer_constant_signed_fits(result, result_type)) {
        return false;
    }
    *value = (uint64_t)result & mask;
    return true;
}

static bool debug_inline_integer_constant_value(
    const Expr* expression, int64_t* value, unsigned int depth) {
    uint64_t bits;
    uint64_t mask;
    if (!value || !debug_inline_integer_constant_bits(
            expression, &bits, depth) ||
        !debug_inline_integer_constant_mask(expression->type, &mask)) {
        return false;
    }
    bits &= mask;
    if (expression->type->kind == TYPE_BOOL) {
        *value = bits != 0u;
        return true;
    }
    if (expression->type->is_unsigned) {
        if (bits > (uint64_t)INT64_MAX) return false;
        *value = (int64_t)bits;
        return true;
    }
    return debug_inline_integer_constant_signed_value(
        bits, expression->type, value);
}

static bool debug_inline_static_pointer_addition(
    const Expr* expression, const Decl** source, int64_t* address_addend) {
    const Expr* pointer_expression;
    const Expr* offset_expression;
    const Expr* array_designator;
    const Type* pointer_type;
    const Type* array_type = NULL;
    const Type* element_type;
    int64_t element_count;
    int64_t integer_offset;
    int64_t base_addend;
    int64_t object_start;
    int64_t object_extent;
    int64_t object_end;
    int64_t byte_offset;
    int64_t result_addend;
    bool subtract;
    if (!expression || !source || !address_addend ||
        (expression->kind != EXPR_ADD && expression->kind != EXPR_SUB) ||
        !expression->type || expression->type->kind != TYPE_PTR ||
        expression->type->cxx_is_member_pointer ||
        !expression->type->base || expression->type->base->kind == TYPE_FUNC ||
        expression->type->base->size <= 0) {
        return false;
    }
    subtract = expression->kind == EXPR_SUB;
    pointer_expression = expression->binary_lhs;
    offset_expression = expression->binary_rhs;
    if (!subtract && pointer_expression &&
        pointer_expression->type &&
        pointer_expression->type->kind != TYPE_PTR &&
        pointer_expression->type->kind != TYPE_ARRAY) {
        pointer_expression = expression->binary_rhs;
        offset_expression = expression->binary_lhs;
    }
    if (!pointer_expression || !pointer_expression->type ||
        (pointer_expression->type->kind != TYPE_PTR &&
         pointer_expression->type->kind != TYPE_ARRAY) ||
        pointer_expression->type->cxx_is_member_pointer ||
        !debug_inline_integer_constant_value(
            offset_expression, &integer_offset, 0u)) {
        return false;
    }
    if (subtract) {
        if (integer_offset == INT64_MIN) return false;
        element_count = -integer_offset;
    } else {
        element_count = integer_offset;
    }
    pointer_type = expression->type;
    element_type = pointer_type->base;
    if (element_count > 0 &&
        element_count > INT64_MAX / element_type->size) return false;
    if (element_count < 0 &&
        element_count < INT64_MIN / element_type->size) return false;
    byte_offset = element_count * element_type->size;
    array_designator = debug_inline_static_array_designator(
        pointer_expression, &array_type);
    if (array_designator) {
        if (!array_type || array_type->size <= 0 ||
            array_type->array_len < 0 ||
            !debug_inline_address_path(
                array_designator, source, &base_addend, false, false, 0u)) {
            return false;
        }
        object_start = base_addend;
        object_extent = array_type->size;
    } else if (pointer_expression->kind == EXPR_ADDR &&
               pointer_expression->unary_operand) {
        const Type* pointed_object_type =
            pointer_expression->unary_operand->type;
        const Expr* addressed_array = NULL;
        const Type* addressed_array_type = NULL;
        if (pointer_expression->unary_operand->kind == EXPR_INDEX &&
            pointer_expression->unary_operand->index_base &&
            pointer_expression->unary_operand->index_expr) {
            int64_t array_index;
            int64_t indexed_byte_offset;
            int64_t array_end;
            addressed_array = debug_inline_static_array_designator(
                pointer_expression->unary_operand->index_base,
                &addressed_array_type);
            if (!addressed_array || !addressed_array_type ||
                addressed_array_type->array_len < 0 ||
                !addressed_array_type->base ||
                addressed_array_type->base->size <= 0 ||
                !debug_inline_integer_constant_value(
                    pointer_expression->unary_operand->index_expr,
                    &array_index, 0u) ||
                array_index < 0 ||
                array_index >= addressed_array_type->array_len ||
                !debug_inline_address_path(
                    addressed_array, source, &object_start, false, false,
                    0u) ||
                array_index > INT64_MAX /
                    addressed_array_type->base->size ||
                object_start < 0 ||
                object_start > INT64_MAX - addressed_array_type->size) {
                return false;
            }
            indexed_byte_offset = array_index *
                addressed_array_type->base->size;
            array_end = object_start + addressed_array_type->size;
            if (object_start > INT64_MAX - indexed_byte_offset) {
                return false;
            }
            base_addend = object_start + indexed_byte_offset;
            if (base_addend > array_end) return false;
            object_extent = addressed_array_type->size;
        } else {
            if (!pointed_object_type || pointed_object_type->size <= 0 ||
                element_type->size > pointed_object_type->size ||
                !debug_inline_address_path(
                    pointer_expression->unary_operand, source,
                    &base_addend, false, false, 0u)) {
                return false;
            }
            object_start = base_addend;
            object_extent = element_type->size;
        }
    } else {
        return false;
    }
    if (object_start < 0 || base_addend < object_start ||
        object_extent < 0 || object_start > INT64_MAX - object_extent ||
        (byte_offset > 0 && base_addend > INT64_MAX - byte_offset) ||
        (byte_offset < 0 && base_addend < INT64_MIN - byte_offset) ||
        !*source || !(*source)->type || (*source)->type->size <= 0 ||
        object_start > (*source)->type->size - object_extent) {
        return false;
    }
    object_end = object_start + object_extent;
    result_addend = base_addend + byte_offset;
    if (result_addend < object_start || result_addend > object_end) {
        return false;
    }
    *address_addend = result_addend;
    return true;
}

static bool debug_inline_address_path(
    const Expr* expression, const Decl** source, int64_t* address_addend,
    bool allow_one_past, bool allow_frame_source, unsigned int depth) {
    if (!expression || !source || !address_addend || depth >= 32u) {
        return false;
    }
    if (expression->kind == EXPR_IDENT && expression->ident_decl) {
        const Decl* declaration = expression->ident_decl;
        if ((declaration->kind == DECL_VAR && declaration->type &&
             declaration->type->size > 0 &&
             (declaration->var_is_global ||
              declaration->var_is_static_local ||
              (allow_frame_source && !declaration->var_is_vla))) ||
            (allow_frame_source && declaration->kind == DECL_PARAM &&
             declaration->type && declaration->type->size > 0) ||
            (declaration->kind == DECL_FUNC &&
             declaration->func_is_defined)) {
            *source = declaration;
            *address_addend = 0;
            return true;
        }
    }
    if (expression->kind == EXPR_MEMBER && expression->member_base &&
        expression->member_field) {
        const Type* base_type = expression->member_base->type;
        const TypeField* field = expression->member_field;
        int64_t base_addend;
        if (!base_type ||
            (base_type->kind != TYPE_STRUCT &&
             base_type->kind != TYPE_UNION) ||
            field->is_bitfield || field->from_virtual_base ||
            field->offset < 0 || !field->type || field->type->size <= 0 ||
            base_type->size <= 0 ||
            field->offset > base_type->size - field->type->size ||
            expression->cxx_virtual_base_adjustment ||
            expression->cxx_virtual_base_member_access ||
            !debug_inline_address_path(
                expression->member_base, source, &base_addend, false,
                allow_frame_source, depth + 1u) ||
            base_addend > INT64_MAX - field->offset) {
            return false;
        }
        *address_addend = base_addend + field->offset;
        return true;
    }
    if (expression->kind == EXPR_INDEX && expression->index_base &&
        expression->index_expr) {
        const Type* array_type = expression->index_base->type;
        const Type* element_type;
        int64_t element_index;
        int64_t base_addend;
        int64_t element_addend;
        if (array_type && array_type->kind != TYPE_ARRAY) {
            const Type* recovered_array_type = NULL;
            if (debug_inline_static_array_designator(
                    expression->index_base, &recovered_array_type)) {
                array_type = recovered_array_type;
            }
        }
        if (!array_type || array_type->kind != TYPE_ARRAY ||
            array_type->array_len < 0 || !array_type->base ||
            array_type->base->size <= 0 || !expression->type ||
            expression->type->size <= 0 ||
            !debug_inline_integer_constant_value(
                expression->index_expr, &element_index, 0u) ||
            element_index < 0 ||
            (allow_one_past
                ? element_index > array_type->array_len
                : element_index >= array_type->array_len) ||
            !debug_inline_address_path(
                expression->index_base, source, &base_addend, false,
                allow_frame_source, depth + 1u)) {
            return false;
        }
        element_type = array_type->base;
        if (element_index > INT64_MAX / element_type->size) return false;
        element_addend = element_index * element_type->size;
        if (base_addend > INT64_MAX - element_addend) return false;
        *address_addend = base_addend + element_addend;
        return true;
    }
    return false;
}

static const ModuleSymbol* debug_inline_parameter_address_constant(
    const Module* mod, const char* filename, const Decl* callee,
    const ModuleDebugInlineParameterValue* value, const ExprList* argument,
    char** scoped_symbol, int64_t* address_addend) {
    const Expr* address;
    const Expr* argument_expression;
    const Decl* source;
    const Type* parameter_type;
    const Type* argument_type;
    const ModuleSymbol* symbol;
    uint8_t address_size;
    int64_t pointer_adjustment = 0;
    if (scoped_symbol) *scoped_symbol = NULL;
    if (address_addend) *address_addend = 0;
    if (!mod || !filename || !value || !value->declaration || !argument ||
        !argument->expr || argument->cxx_temporary_owner || !scoped_symbol ||
        !address_addend) {
        return NULL;
    }
    argument_expression = argument->expr;
    address = argument_expression;
    parameter_type = value->declaration->type;
    argument_type = argument_expression->type;
    address_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    while (address->kind == EXPR_CAST && address->cast_expr) {
        const Type* cast_type = address->cast_type;
        const Type* source_type = address->cast_expr->type;
        bool source_is_function = source_type &&
            (source_type->kind == TYPE_FUNC ||
             (source_type->kind == TYPE_PTR && source_type->base &&
              source_type->base->kind == TYPE_FUNC));
        bool target_is_function = cast_type &&
            cast_type->kind == TYPE_PTR && cast_type->base &&
            cast_type->base->kind == TYPE_FUNC;
        if (!cast_type || cast_type->kind != TYPE_PTR ||
            cast_type->cxx_is_member_pointer ||
            cast_type->size != address_size || !source_type ||
            address->cxx_member_pointer_adjustment_valid ||
            address->cxx_virtual_base_adjustment ||
            (source_is_function != target_is_function) ||
            (!source_is_function &&
             (source_type->kind != TYPE_PTR ||
              source_type->cxx_is_member_pointer ||
              source_type->size != address_size))) {
            return NULL;
        }
        if (address->cxx_pointer_adjustment_valid) {
            int64_t adjustment = address->cxx_pointer_adjustment;
            if ((adjustment > 0 &&
                 pointer_adjustment > INT64_MAX - adjustment) ||
                (adjustment < 0 &&
                 pointer_adjustment < INT64_MIN - adjustment)) {
                return NULL;
            }
            pointer_adjustment += adjustment;
        }
        address = address->cast_expr;
    }
    if ((address->kind == EXPR_ADD || address->kind == EXPR_SUB) &&
        debug_inline_static_pointer_addition(
            address, &source, address_addend)) {
        /* The pointer arithmetic helper has already proved that the
         * displacement stays inside its statically named object or reaches
         * its one-past address. */
    } else if (address->kind == EXPR_ADDR && address->unary_operand &&
        debug_inline_address_path(
            address->unary_operand, &source, address_addend, true, false,
            0u)) {
        if (!source->type || *address_addend > source->type->size) {
            return NULL;
        }
    } else if (address->kind == EXPR_IDENT && address->ident_decl &&
               address->ident_decl->kind == DECL_FUNC) {
        source = address->ident_decl;
    } else {
        return NULL;
    }
    if (pointer_adjustment != 0) {
        const Type* complete_type;
        const Type* adjusted_type;
        int64_t adjusted_addend;
        if (source->kind != DECL_VAR || !source->type ||
            (source->type->kind != TYPE_STRUCT &&
             source->type->kind != TYPE_UNION) ||
            !argument_type || argument_type->kind != TYPE_PTR ||
            !argument_type->base ||
            (argument_type->base->kind != TYPE_STRUCT &&
             argument_type->base->kind != TYPE_UNION)) {
            return NULL;
        }
        complete_type = source->type;
        adjusted_type = argument_type->base;
        if (!parameter_type || parameter_type->kind != TYPE_PTR ||
            !parameter_type->base ||
            parameter_type->base->kind != TYPE_STRUCT ||
            adjusted_type->kind != TYPE_STRUCT ||
            !parameter_type->base->cxx_class ||
            parameter_type->base->cxx_class != adjusted_type->cxx_class ||
            complete_type->size <= 0 || adjusted_type->size <= 0 ||
            *address_addend < 0 ||
            (pointer_adjustment > 0 &&
             *address_addend > INT64_MAX - pointer_adjustment) ||
            (pointer_adjustment < 0 &&
             *address_addend < INT64_MIN - pointer_adjustment)) {
            return NULL;
        }
        adjusted_addend = *address_addend + pointer_adjustment;
        if (adjusted_addend < 0 ||
            adjusted_addend > complete_type->size - adjusted_type->size) {
            return NULL;
        }
        *address_addend = adjusted_addend;
    }
    if (value->declaration->kind != DECL_PARAM ||
        (source->kind != DECL_VAR && source->kind != DECL_FUNC) ||
        !parameter_type || parameter_type->kind != TYPE_PTR ||
        parameter_type->cxx_is_member_pointer ||
        parameter_type->size != address_size || parameter_type->is_volatile ||
        parameter_type->is_reference || parameter_type->is_atomic ||
        !debug_inline_parameter_body_is_read_only(callee)) {
        return NULL;
    }
    if (source->kind == DECL_FUNC) {
        if (!parameter_type->base ||
            parameter_type->base->kind != TYPE_FUNC || !argument_type ||
            (argument_type->kind != TYPE_FUNC &&
             (argument_type->kind != TYPE_PTR || !argument_type->base ||
              argument_type->base->kind != TYPE_FUNC))) {
            return NULL;
        }
    } else if (!argument_type || argument_type->kind != TYPE_PTR ||
               argument_type->cxx_is_member_pointer ||
               argument_type->size != address_size ||
               !argument_type->base ||
               argument_type->base->kind == TYPE_FUNC ||
               !parameter_type->base ||
               parameter_type->base->kind == TYPE_FUNC) {
        return NULL;
    }
    if (source->kind == DECL_VAR && source->var_is_global) {
        symbol = debug_find_global_symbol(mod, source);
    } else if (source->kind == DECL_VAR && source->var_is_static_local) {
        symbol = debug_find_static_local_symbol(mod, source);
    } else if (source->kind == DECL_FUNC && source->func_is_defined) {
        const char* link_name = source->link_name
            ? source->link_name : source->name;
        symbol = NULL;
        if (!link_name || link_name[0] == '\0') return NULL;
        for (int index = 0; index < mod->symbol_count; ++index) {
            const ModuleSymbol* candidate = &mod->symbols[index];
            if (candidate->is_defined &&
                candidate->section == MODULE_SYMBOL_CODE &&
                strcmp(candidate->name, link_name) == 0) {
                symbol = candidate;
                break;
            }
        }
    } else {
        return NULL;
    }
    if (!symbol || symbol->section == MODULE_SYMBOL_TLS) return NULL;
    if (!symbol->is_global) {
        *scoped_symbol = module_scoped_symbol(filename, symbol->name);
        if (!*scoped_symbol) return NULL;
    }
    return symbol;
}

static bool debug_inline_parameter_frame_offset(
    const Module* mod, const Decl* source, int64_t* frame_offset) {
    int64_t stable_frame_offset;
    if (!mod || !source || !frame_offset ||
        (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_vla) {
        return false;
    }
    stable_frame_offset = source->var_offset;
    if (mod->debug_verified_backend) {
        bool found_frame_location = false;
        for (size_t index = 0u;
             index < mod->debug_variable_location_count; ++index) {
            const DebugVariableLocation* location =
                &mod->debug_variable_locations[index];
            if (location->declaration != source) continue;
            stable_frame_offset = location->frame_offset;
            found_frame_location = true;
            break;
        }
        if (!found_frame_location) return false;
    }
    if (stable_frame_offset == 0) return false;
    *frame_offset = stable_frame_offset;
    return true;
}

enum {
    DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY = 512,
    DEBUG_INLINE_I686_U64_DIVMOD_EXPRESSION_CAPACITY = 8192
};

static bool debug_inline_integer_expression_type(const Type* type) {
    return type && !type->is_volatile && !type->is_reference &&
        !type->is_atomic && type->size > 0 && type->size <= 8 &&
        ((type->kind >= TYPE_BOOL && type->kind <= TYPE_LLONG) ||
         type->kind == TYPE_ENUM);
}

static uint8_t debug_inline_integer_comparison_opcode(ExprKind kind) {
    switch (kind) {
        case EXPR_EQ: return 0x29u;
        case EXPR_GE: return 0x2au;
        case EXPR_GT: return 0x2bu;
        case EXPR_LE: return 0x2cu;
        case EXPR_LT: return 0x2du;
        case EXPR_NE: return 0x2eu;
        default: return 0u;
    }
}

static const Type* debug_inline_integer_comparison_common_type(
    const Type* left, const Type* right, Type promoted[2]) {
    const Type* operands[2] = {left, right};
    const Type* unsigned_type;
    const Type* signed_type;
    if (!left || !right || !promoted) return NULL;
    for (size_t index = 0u; index < 2u; ++index) {
        if (operands[index]->kind == TYPE_ENUM &&
            !operands[index]->enum_is_scoped) {
            const Type* underlying = operands[index]->enum_underlying_type;
            promoted[index] = underlying
                ? *underlying : *operands[index];
            if (!underlying) {
                promoted[index].kind = TYPE_INT;
                promoted[index].size = 4;
                promoted[index].align = 4;
                promoted[index].is_unsigned = false;
            } else if (underlying->kind < TYPE_INT) {
                promoted[index].kind = TYPE_INT;
                promoted[index].size = 4;
                promoted[index].align = 4;
                promoted[index].is_unsigned =
                    underlying->is_unsigned && underlying->size >= 4;
            }
            promoted[index].is_const = false;
            promoted[index].is_volatile = false;
            operands[index] = &promoted[index];
        } else if (operands[index]->kind < TYPE_INT) {
            promoted[index] = *operands[index];
            promoted[index].kind = TYPE_INT;
            promoted[index].size = 4;
            promoted[index].is_unsigned = false;
            promoted[index].is_const = false;
            promoted[index].is_volatile = false;
            operands[index] = &promoted[index];
        }
    }
    if (operands[0]->is_unsigned == operands[1]->is_unsigned) {
        if (operands[0]->size != operands[1]->size) {
            return operands[0]->size > operands[1]->size
                ? operands[0] : operands[1];
        }
        return operands[0]->kind >= operands[1]->kind
            ? operands[0] : operands[1];
    }
    unsigned_type = operands[0]->is_unsigned ? operands[0] : operands[1];
    signed_type = operands[0]->is_unsigned ? operands[1] : operands[0];
    if (unsigned_type->kind >= signed_type->kind ||
        signed_type->size <= unsigned_type->size) {
        return unsigned_type;
    }
    return signed_type;
}

static bool debug_inline_integer_representation_matches(
    const Type* left, const Type* right) {
    bool left_unsigned;
    bool right_unsigned;
    if (!left || !right) return false;
    left_unsigned = left->is_unsigned || left->kind == TYPE_BOOL;
    right_unsigned = right->is_unsigned || right->kind == TYPE_BOOL;
    return left->size == right->size && left_unsigned == right_unsigned;
}

static bool debug_inline_integer_expression_append_byte(
    uint8_t* expression, size_t capacity, size_t* size, uint8_t value) {
    if (!expression || !size || *size >= capacity) return false;
    expression[(*size)++] = value;
    return true;
}

static bool debug_inline_integer_expression_normalize(
    uint8_t* expression, size_t capacity, size_t* size,
    const Type* type, int architecture) {
    unsigned int address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned int type_size;
    unsigned int shift;
    uint64_t mask;
    bool is_unsigned;
    if (!debug_inline_integer_expression_type(type) ||
        (unsigned int)type->size > address_size) {
        return false;
    }
    type_size = (unsigned int)type->size;
    if (type_size == address_size) return true;
    is_unsigned = type->is_unsigned || type->kind == TYPE_BOOL;
    if (is_unsigned) {
        mask = (UINT64_C(1) << (type_size * 8u)) - 1u;
        return debug_inline_integer_expression_append_byte(
                   expression, capacity, size, 0x10u) && /* constu */
            debug_expression_uleb(expression, capacity, size, mask) &&
            debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x1au); /* and */
    }
    shift = (address_size - type_size) * 8u;
    return debug_inline_integer_expression_append_byte(
               expression, capacity, size, 0x10u) && /* constu */
        debug_expression_uleb(expression, capacity, size, shift) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x24u) && /* shl */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x10u) && /* constu */
        debug_expression_uleb(expression, capacity, size, shift) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x26u); /* shra */
}

static bool debug_inline_integer_division_uses_signed_stack_safely(
    const Type* type, int architecture) {
    unsigned int address_size = architecture == ARCH_X64 ? 8u : 4u;
    if (!debug_inline_integer_expression_type(type) ||
        type->kind == TYPE_BOOL) {
        return false;
    }
    /* DWARF's generic DW_OP_div is signed. For a narrower unsigned type,
       zero-extension makes every value nonnegative in the address-sized
       signed stack representation. Full-width unsigned values need another
       exact lowering and must not be interpreted as signed here. */
    return !type->is_unsigned || (unsigned int)type->size < address_size;
}

static bool debug_inline_integer_division_uses_full_width_unsigned(
    const Type* type, int architecture) {
    unsigned int address_size = architecture == ARCH_X64 ? 8u : 4u;
    return type && type->is_unsigned &&
        (unsigned int)type->size == address_size &&
        (address_size == 4u || address_size == 8u);
}

static bool debug_inline_integer_expression_append_constu(
    uint8_t* expression, size_t capacity, size_t* size, uint64_t value) {
    return debug_inline_integer_expression_append_byte(
               expression, capacity, size, 0x10u) &&
        debug_expression_uleb(expression, capacity, size, value);
}

static bool debug_inline_integer_expression_append_pick(
    uint8_t* expression, size_t capacity, size_t* size, uint8_t index) {
    return debug_inline_integer_expression_append_byte(
               expression, capacity, size, 0x15u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, index);
}

static bool debug_inline_integer_expression_append_branch(
    uint8_t* expression, size_t capacity, size_t* size, uint8_t opcode,
    size_t* operand_offset) {
    if (!expression || !size || !operand_offset || *size > capacity ||
        capacity - *size < 3u ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, opcode)) {
        return false;
    }
    *operand_offset = *size;
    expression[(*size)++] = 0u;
    expression[(*size)++] = 0u;
    return true;
}

static bool debug_inline_integer_expression_patch_branch(
    uint8_t* expression, size_t size, size_t operand_offset,
    size_t target_offset) {
    int64_t displacement;
    uint16_t encoded_displacement;
    if (!expression || operand_offset > size || size - operand_offset < 2u ||
        target_offset > (size_t)INT64_MAX ||
        operand_offset + 2u > (size_t)INT64_MAX) {
        return false;
    }
    displacement = (int64_t)target_offset -
        (int64_t)(operand_offset + 2u);
    if (displacement < INT16_MIN || displacement > INT16_MAX) return false;
    encoded_displacement = (uint16_t)(int16_t)displacement;
    expression[operand_offset] = (uint8_t)encoded_displacement;
    expression[operand_offset + 1u] =
        (uint8_t)(encoded_displacement >> 8u);
    return true;
}

static bool debug_inline_integer_expression_clean_to_top(
    uint8_t* expression, size_t capacity, size_t* size,
    size_t stack_depth) {
    size_t entries_to_remove;
    if (!expression || !size || stack_depth == 0u) return false;
    entries_to_remove = stack_depth - 1u;
    while (entries_to_remove >= 2u) {
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x17u) || /* DW_OP_rot */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x13u) || /* DW_OP_drop */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x13u)) {
            return false;
        }
        entries_to_remove -= 2u;
    }
    if (entries_to_remove != 0u) {
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x16u) || /* DW_OP_swap */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x13u)) { /* DW_OP_drop */
            return false;
        }
    }
    return true;
}

static bool debug_inline_integer_expression_append_unsigned_ge(
    uint8_t* expression, size_t capacity, size_t* size,
    uint64_t sign_bit) {
    /* Preserve the two operands and append (lhs >= rhs) using a signed
       generic comparison after flipping each operand's sign bit. */
    return debug_inline_integer_expression_append_pick(
               expression, capacity, size, 1u) && /* lhs */
        debug_inline_integer_expression_append_pick(
            expression, capacity, size, 1u) && /* rhs */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, sign_bit) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x27u) && /* DW_OP_xor */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) && /* DW_OP_swap */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, sign_bit) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x27u) && /* DW_OP_xor */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) && /* DW_OP_swap */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x2au); /* DW_OP_ge */
}

static bool debug_inline_integer_expression_append_full_width_unsigned_division(
    uint8_t* expression, size_t capacity, size_t* size,
    int architecture, bool remainder) {
    unsigned int address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned int address_bits = address_size * 8u;
    uint64_t sign_bit = UINT64_C(1) << (address_bits - 1u);
    size_t high_branch_operand;
    size_t low_path_end_operand;
    size_t high_path_offset;
    size_t expression_end;
    if (!expression || !size || address_size > 8u ||
        !debug_inline_integer_expression_append_pick(
            expression, capacity, size, 0u) || /* divisor */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, address_bits - 1u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) || /* DW_OP_shr */
        !debug_inline_integer_expression_append_branch(
            expression, capacity, size, 0x28u, &high_branch_operand)) {
        return false;
    }

    /* Low-divisor path. Halving the dividend makes the signed DW_OP_div
       safe; the final quotient bit is recovered from the remainder. */
    if (!debug_inline_integer_expression_append_pick(
            expression, capacity, size, 1u) || /* dividend */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) || /* DW_OP_shr */
        !debug_inline_integer_expression_append_pick(
            expression, capacity, size, 1u) || /* divisor */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1bu) || /* DW_OP_div */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x12u) || /* DW_OP_dup */
        !debug_inline_integer_expression_append_pick(
            expression, capacity, size, 2u) || /* divisor */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) || /* DW_OP_mul */
        !debug_inline_integer_expression_append_pick(
            expression, capacity, size, 3u) || /* dividend */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) || /* DW_OP_shr */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) || /* DW_OP_swap */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1cu)) { /* DW_OP_minus */
        return false;
    }
    /* Stack: dividend, divisor, q0, r0. Append input parity and
       ceil((divisor - parity) / 2), then compare the signed-safe values. */
    if (!debug_inline_integer_expression_append_pick(
            expression, capacity, size, 3u) || /* dividend */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) || /* DW_OP_and */
        !debug_inline_integer_expression_append_pick(
            expression, capacity, size, 3u) || /* divisor */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) || /* DW_OP_shr */
        !debug_inline_integer_expression_append_pick(
            expression, capacity, size, 4u) || /* divisor */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) || /* DW_OP_and */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_pick(
            expression, capacity, size, 3u) || /* parity */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1cu) || /* 1 - parity */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) || /* DW_OP_and */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) || /* DW_OP_plus */
        !debug_inline_integer_expression_append_pick(
            expression, capacity, size, 2u) || /* r0 */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) || /* DW_OP_swap */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x2au)) { /* DW_OP_ge */
        return false;
    }

    if (!remainder) {
        /* q = 2*q0 + carry. */
        if (!debug_inline_integer_expression_append_pick(
                expression, capacity, size, 3u) || /* q0 */
            !debug_inline_integer_expression_append_constu(
                expression, capacity, size, 1u) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x24u) || /* DW_OP_shl */
            !debug_inline_integer_expression_append_pick(
                expression, capacity, size, 1u) || /* carry */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x22u) || /* DW_OP_plus */
            !debug_inline_integer_expression_clean_to_top(
                expression, capacity, size, 7u)) {
            return false;
        }
    } else {
        /* r = (2*r0 + parity) - carry*divisor modulo the address width. */
        if (!debug_inline_integer_expression_append_pick(
                expression, capacity, size, 2u) || /* r0 */
            !debug_inline_integer_expression_append_constu(
                expression, capacity, size, 1u) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x24u) || /* DW_OP_shl */
            !debug_inline_integer_expression_append_pick(
                expression, capacity, size, 2u) || /* parity */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x22u) || /* DW_OP_plus */
            !debug_inline_integer_expression_append_pick(
                expression, capacity, size, 1u) || /* carry */
            !debug_inline_integer_expression_append_pick(
                expression, capacity, size, 6u) || /* divisor */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x1eu) || /* DW_OP_mul */
            !debug_inline_integer_expression_append_pick(
                expression, capacity, size, 1u) || /* raw remainder */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x16u) || /* DW_OP_swap */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x1cu) || /* raw - correction */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x16u) || /* DW_OP_swap */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x13u) || /* DW_OP_drop */
            !debug_inline_integer_expression_clean_to_top(
                expression, capacity, size, 7u)) {
            return false;
        }
    }
    if (!debug_inline_integer_expression_append_branch(
            expression, capacity, size, 0x2fu, &low_path_end_operand)) {
        return false;
    }
    high_path_offset = *size;
    if (!debug_inline_integer_expression_patch_branch(
            expression, *size, high_branch_operand, high_path_offset) ||
        !debug_inline_integer_expression_append_unsigned_ge(
            expression, capacity, size, sign_bit)) {
        return false;
    }
    if (remainder) {
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x12u) || /* DW_OP_dup */
            !debug_inline_integer_expression_append_pick(
                expression, capacity, size, 2u) || /* divisor */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x1eu) || /* carry*divisor */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x16u) || /* DW_OP_swap */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x13u) || /* DW_OP_drop */
            !debug_inline_integer_expression_append_pick(
                expression, capacity, size, 2u) || /* dividend */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x16u) || /* DW_OP_swap */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x1cu)) { /* dividend - product */
            return false;
        }
    }
    if (!debug_inline_integer_expression_clean_to_top(
            expression, capacity, size, 3u)) {
        return false;
    }
    expression_end = *size;
    return debug_inline_integer_expression_patch_branch(
        expression, expression_end, low_path_end_operand, expression_end);
}

static bool debug_inline_integer_literal_bits(
    const Expr* expression, uint64_t* bits, unsigned int depth) {
    if (!expression || !bits || depth >= 32u) return false;
    if (expression->kind == EXPR_INT_LIT) {
        *bits = (uint64_t)expression->int_val;
        return true;
    }
    if (expression->kind == EXPR_POS && expression->unary_operand) {
        return debug_inline_integer_literal_bits(
            expression->unary_operand, bits, depth + 1u);
    }
    if ((expression->kind == EXPR_NEG ||
         expression->kind == EXPR_BITNOT) && expression->unary_operand &&
        debug_inline_integer_literal_bits(
            expression->unary_operand, bits, depth + 1u)) {
        *bits = expression->kind == EXPR_NEG
            ? UINT64_C(0) - *bits : ~*bits;
        return true;
    }
    return false;
}

static int64_t debug_inline_integer_bits_to_signed(uint64_t bits,
                                                   unsigned int width) {
    uint64_t modulus;
    if (width >= 64u) {
        if (bits <= (uint64_t)INT64_MAX) return (int64_t)bits;
        return -((int64_t)(~bits)) - 1;
    }
    modulus = UINT64_C(1) << width;
    bits &= modulus - 1u;
    if ((bits & (modulus >> 1u)) != 0u) {
        return -(int64_t)(modulus - bits);
    }
    return (int64_t)bits;
}

static bool debug_inline_parameter_i686_u64_comparison_expression(
    const Module* mod, ExprKind kind, const Expr* left, const Expr* right,
    const Type* comparison_type, uint8_t* expression, size_t capacity,
    size_t* expression_size, unsigned int depth);

static bool debug_build_inline_integer_value_expression(
    const Module* mod, int architecture, const Expr* expression,
    uint8_t* output, size_t capacity, size_t* output_size,
    unsigned int depth) {
    int64_t signed_constant;
    uint64_t unsigned_constant;
    if (!mod || !expression || !output || !output_size || depth >= 32u ||
        !debug_inline_integer_expression_type(expression->type)) {
        return false;
    }
    if (debug_inline_integer_literal_bits(
            expression, &unsigned_constant, 0u)) {
        if (expression->type->is_unsigned ||
            expression->type->kind == TYPE_BOOL) {
            return debug_inline_integer_expression_append_byte(
                       output, capacity, output_size, 0x10u) &&
                debug_expression_uleb(
                    output, capacity, output_size, unsigned_constant) &&
                debug_inline_integer_expression_normalize(
                    output, capacity, output_size, expression->type,
                    architecture);
        }
        signed_constant = debug_inline_integer_bits_to_signed(
            unsigned_constant, (unsigned int)expression->type->size * 8u);
        return debug_inline_integer_expression_append_byte(
                   output, capacity, output_size, 0x11u) &&
            debug_expression_sleb_bounded(
                output, capacity, output_size, signed_constant) &&
            debug_inline_integer_expression_normalize(
                output, capacity, output_size, expression->type,
                architecture);
    }

    if (expression->kind == EXPR_IDENT && expression->ident_decl) {
        const Decl* source = expression->ident_decl;
        int64_t frame_offset;
        uint8_t address_expression[32];
        size_t address_expression_size;
        if ((source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
            source->var_is_global || source->var_is_static_local ||
            source->var_is_thread_local || source->var_is_vla ||
            !source->type ||
            !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
                source->type, expression->type, 0u) ||
            !debug_inline_parameter_frame_offset(
                mod, source, &frame_offset)) {
            return false;
        }
        address_expression_size = debug_build_expr_breg(
            address_expression, architecture, frame_offset, 0u, false);
        if (address_expression_size == 0u || *output_size > capacity ||
            address_expression_size > capacity - *output_size) {
            return false;
        }
        memcpy(output + *output_size, address_expression,
               address_expression_size);
        *output_size += address_expression_size;
        return debug_inline_integer_expression_append_byte(
                   output, capacity, output_size, 0x94u) && /* deref_size */
            debug_inline_integer_expression_append_byte(
                output, capacity, output_size,
                (uint8_t)source->type->size) &&
            debug_inline_integer_expression_normalize(
                output, capacity, output_size, expression->type,
                architecture);
    }

    if (expression->kind == EXPR_CAST && expression->cast_expr &&
        (unsigned int)expression->type->size <=
            (architecture == ARCH_X64 ? 8u : 4u) &&
        debug_inline_integer_expression_type(expression->cast_expr->type) &&
        (unsigned int)expression->cast_expr->type->size <=
            (architecture == ARCH_X64 ? 8u : 4u) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->cast_expr, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture)) {
        return true;
    }

    if (expression->kind == EXPR_POS && expression->unary_operand &&
        debug_inline_integer_expression_type(
            expression->unary_operand->type) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->unary_operand, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture)) {
        return true;
    }

    if ((expression->kind == EXPR_NEG ||
         expression->kind == EXPR_BITNOT) && expression->unary_operand &&
        debug_inline_integer_expression_type(
            expression->unary_operand->type) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->unary_operand->type, expression->type, 0u) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->unary_operand, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, output_size,
            expression->kind == EXPR_NEG ? 0x1fu : 0x20u) &&
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture)) {
        return true;
    }

    if (expression->kind == EXPR_NOT && expression->unary_operand &&
        architecture == ARCH_X86 &&
        expression->unary_operand->type &&
        debug_inline_integer_expression_type(
            expression->unary_operand->type) &&
        expression->unary_operand->type->size == 8 &&
        expression->unary_operand->kind == EXPR_IDENT &&
        expression->unary_operand->ident_decl) {
        const Expr* operand = expression->unary_operand;
        const Decl* source = operand->ident_decl;
        int64_t frame_offset;
        uint8_t address_expression[32];
        size_t address_expression_size;
        if ((source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
            source->var_is_global || source->var_is_static_local ||
            source->var_is_thread_local || source->var_is_vla ||
            !source->type || source->type->size != 8 ||
            !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
                source->type, operand->type, 0u) ||
            !debug_inline_parameter_frame_offset(
                mod, source, &frame_offset) || frame_offset > INT64_MAX - 4) {
            return false;
        }
        address_expression_size = debug_build_expr_breg(
            address_expression, architecture, frame_offset, 0u, false);
        if (address_expression_size == 0u || *output_size > capacity ||
            address_expression_size > capacity - *output_size) {
            return false;
        }
        memcpy(output + *output_size, address_expression,
               address_expression_size);
        *output_size += address_expression_size;
        if (!debug_inline_integer_expression_append_byte(
                output, capacity, output_size, 0x94u) || /* deref_size */
            !debug_inline_integer_expression_append_byte(
                output, capacity, output_size, 4u)) {
            return false;
        }
        address_expression_size = debug_build_expr_breg(
            address_expression, architecture, frame_offset + 4, 0u, false);
        if (address_expression_size == 0u || *output_size > capacity ||
            address_expression_size > capacity - *output_size) {
            return false;
        }
        memcpy(output + *output_size, address_expression,
               address_expression_size);
        *output_size += address_expression_size;
        return debug_inline_integer_expression_append_byte(
                   output, capacity, output_size, 0x94u) && /* deref_size */
            debug_inline_integer_expression_append_byte(
                output, capacity, output_size, 4u) &&
            debug_inline_integer_expression_append_byte(
                output, capacity, output_size, 0x21u) && /* DW_OP_or */
            debug_inline_integer_expression_append_byte(
                output, capacity, output_size, 0x10u) && /* constu */
            debug_expression_uleb(output, capacity, output_size, 0u) &&
            debug_inline_integer_expression_append_byte(
                output, capacity, output_size, 0x29u) && /* DW_OP_eq */
            debug_inline_integer_expression_normalize(
                output, capacity, output_size, expression->type,
                architecture);
    }

    if (debug_inline_integer_comparison_opcode(expression->kind) != 0u &&
        expression->binary_lhs &&
        expression->binary_rhs &&
        debug_inline_integer_expression_type(
            expression->binary_lhs->type) &&
        debug_inline_integer_expression_type(
            expression->binary_rhs->type)) {
        Type promoted_types[2];
        const Type* comparison_type =
            debug_inline_integer_comparison_common_type(
                expression->binary_lhs->type,
                expression->binary_rhs->type, promoted_types);
        const Type* left_type = expression->binary_lhs->type;
        const Type* right_type = expression->binary_rhs->type;
        uint8_t comparison_opcode =
            debug_inline_integer_comparison_opcode(expression->kind);
        bool ordered = expression->kind != EXPR_EQ &&
            expression->kind != EXPR_NE;
        unsigned int address_size = architecture == ARCH_X64 ? 8u : 4u;
        bool full_width_unsigned_ordering;
        uint64_t sign_bit = UINT64_C(1) << (address_size * 8u - 1u);
        if (!debug_inline_integer_expression_type(comparison_type)) {
            return false;
        }
        if (architecture == ARCH_X86 && comparison_type->size == 8 &&
            left_type->size == 8 && right_type->size == 8) {
            if (!debug_inline_parameter_i686_u64_comparison_expression(
                    mod, expression->kind, expression->binary_lhs,
                    expression->binary_rhs, comparison_type, output,
                    capacity, output_size, depth + 1u) ||
                !debug_inline_integer_expression_normalize(
                    output, capacity, output_size, expression->type,
                    architecture)) {
                return false;
            }
            return true;
        }
        if ((unsigned int)left_type->size > address_size ||
            (unsigned int)right_type->size > address_size ||
            (unsigned int)comparison_type->size > address_size) {
            return false;
        }
        full_width_unsigned_ordering = ordered &&
            comparison_type->is_unsigned &&
            (unsigned int)comparison_type->size == address_size;
        if (!debug_build_inline_integer_value_expression(
                mod, architecture, expression->binary_lhs, output, capacity,
                output_size, depth + 1u) ||
            (!debug_inline_integer_representation_matches(
                 left_type, comparison_type) &&
             !debug_inline_integer_expression_normalize(
                 output, capacity, output_size, comparison_type,
                 architecture)) ||
            (full_width_unsigned_ordering &&
             (!debug_inline_integer_expression_append_constu(
                  output, capacity, output_size, sign_bit) ||
              !debug_inline_integer_expression_append_byte(
                  output, capacity, output_size, 0x27u))) ||
            !debug_build_inline_integer_value_expression(
                mod, architecture, expression->binary_rhs, output, capacity,
                output_size, depth + 1u) ||
            (!debug_inline_integer_representation_matches(
                 right_type, comparison_type) &&
             !debug_inline_integer_expression_normalize(
                 output, capacity, output_size, comparison_type,
                 architecture)) ||
            (full_width_unsigned_ordering &&
             (!debug_inline_integer_expression_append_constu(
                  output, capacity, output_size, sign_bit) ||
              !debug_inline_integer_expression_append_byte(
                  output, capacity, output_size, 0x27u))) ||
            !debug_inline_integer_expression_append_byte(
                output, capacity, output_size, comparison_opcode) ||
            !debug_inline_integer_expression_normalize(
                output, capacity, output_size, expression->type,
                architecture)) {
            return false;
        }
        return true;
    }

    if (expression->kind == EXPR_NOT && expression->unary_operand &&
        debug_inline_integer_expression_type(
            expression->unary_operand->type) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->unary_operand, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, output_size, 0x10u) && /* DW_OP_constu */
        debug_expression_uleb(output, capacity, output_size, 0u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, output_size, 0x29u) && /* DW_OP_eq */
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture)) {
        return true;
    }

    if ((expression->kind == EXPR_DIV || expression->kind == EXPR_MOD) &&
        expression->binary_lhs && expression->binary_rhs &&
        debug_inline_integer_division_uses_full_width_unsigned(
            expression->type, architecture) &&
        debug_inline_integer_expression_type(expression->binary_lhs->type) &&
        debug_inline_integer_expression_type(expression->binary_rhs->type) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_lhs->type, expression->type, 0u) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_rhs->type, expression->type, 0u) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->binary_lhs, output, capacity,
            output_size, depth + 1u) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->binary_rhs, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_append_full_width_unsigned_division(
            output, capacity, output_size, architecture,
            expression->kind == EXPR_MOD) &&
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture)) {
        return true;
    }

    /* DW_OP_mod's signed remainder rule is not sufficiently portable. Build
       the remainder as lhs - (lhs / rhs) * rhs. Unsigned operands are safe
       only when they fit the positive range of the generic DWARF stack. */
    if (expression->kind == EXPR_MOD && expression->binary_lhs &&
        expression->binary_rhs &&
        debug_inline_integer_division_uses_signed_stack_safely(
            expression->type, architecture) &&
        debug_inline_integer_expression_type(expression->binary_lhs->type) &&
        debug_inline_integer_expression_type(expression->binary_rhs->type) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_lhs->type, expression->type, 0u) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_rhs->type, expression->type, 0u) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->binary_lhs, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, output_size, 0x12u) && /* DW_OP_dup */
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->binary_rhs, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, output_size, 0x1bu) && /* DW_OP_div */
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->binary_rhs, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, output_size, 0x1eu) && /* DW_OP_mul */
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, output_size, 0x1cu) && /* DW_OP_minus */
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture)) {
        return true;
    }

    if ((expression->kind == EXPR_ADD || expression->kind == EXPR_SUB ||
         expression->kind == EXPR_MUL || expression->kind == EXPR_DIV ||
         expression->kind == EXPR_BITAND ||
         expression->kind == EXPR_BITOR || expression->kind == EXPR_BITXOR ||
         expression->kind == EXPR_LSHIFT ||
         expression->kind == EXPR_RSHIFT) &&
        expression->binary_lhs && expression->binary_rhs &&
        (expression->kind != EXPR_DIV ||
         debug_inline_integer_division_uses_signed_stack_safely(
             expression->type, architecture)) &&
        debug_inline_integer_expression_type(expression->binary_lhs->type) &&
        debug_inline_integer_expression_type(expression->binary_rhs->type) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_lhs->type, expression->type, 0u) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_rhs->type, expression->type, 0u) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->binary_lhs, output, capacity,
            output_size, depth + 1u) &&
        debug_build_inline_integer_value_expression(
            mod, architecture, expression->binary_rhs, output, capacity,
            output_size, depth + 1u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, output_size,
            expression->kind == EXPR_ADD ? 0x22u :
                expression->kind == EXPR_SUB ? 0x1cu :
                expression->kind == EXPR_MUL ? 0x1eu :
                expression->kind == EXPR_DIV ? 0x1bu :
                expression->kind == EXPR_BITAND ? 0x1au :
                expression->kind == EXPR_BITOR ? 0x21u :
                expression->kind == EXPR_BITXOR ? 0x27u :
                expression->kind == EXPR_LSHIFT ? 0x24u :
                expression->type->is_unsigned ? 0x25u : 0x26u) &&
        debug_inline_integer_expression_normalize(
            output, capacity, output_size, expression->type, architecture)) {
        return true;
    }
    return false;
}

static bool debug_inline_integer_expression_append_frame_u32(
    uint8_t* expression, size_t capacity, size_t* size,
    int architecture, int64_t frame_offset) {
    uint8_t address_expression[32];
    if (!expression || !size || *size > capacity) {
        return false;
    }
    size_t address_expression_size = debug_build_expr_breg(
        address_expression, architecture, frame_offset, 0u, false);
    if (address_expression_size == 0u ||
        address_expression_size > capacity - *size) {
        return false;
    }
    memcpy(expression + *size, address_expression, address_expression_size);
    *size += address_expression_size;
    return debug_inline_integer_expression_append_byte(
               expression, capacity, size, 0x94u) && /* DW_OP_deref_size */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 4u);
}

static bool debug_inline_parameter_i686_u64_append_word_expression(
    const Module* mod, const Expr* value, bool high_word, uint8_t* expression,
    size_t capacity, size_t* expression_size, unsigned int depth) {
    uint64_t literal_bits;
    if (!mod || !value || !expression || !expression_size || depth >= 32u ||
        !debug_inline_integer_expression_type(value->type) ||
        value->type->size != 8) {
        return false;
    }
    if (value->kind == EXPR_CAST && value->cast_expr &&
        value->cast_expr->type && value->cast_expr->type->size == 8) {
        return debug_inline_parameter_i686_u64_append_word_expression(
            mod, value->cast_expr, high_word, expression, capacity,
            expression_size, depth + 1u);
    }
    if (value->kind == EXPR_POS && value->unary_operand &&
        value->unary_operand->type &&
        value->unary_operand->type->size == 8) {
        return debug_inline_parameter_i686_u64_append_word_expression(
            mod, value->unary_operand, high_word, expression, capacity,
            expression_size, depth + 1u);
    }
    if (debug_inline_integer_literal_bits(value, &literal_bits, 0u)) {
        uint32_t word = high_word
            ? (uint32_t)(literal_bits >> 32u) : (uint32_t)literal_bits;
        return debug_inline_integer_expression_append_constu(
            expression, capacity, expression_size, word);
    }
    if (value->kind == EXPR_IDENT && value->ident_decl) {
        const Decl* source = value->ident_decl;
        int64_t frame_offset;
        if ((source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
            source->var_is_global || source->var_is_static_local ||
            source->var_is_thread_local || source->var_is_vla ||
            !source->type || source->type->size != 8 ||
            !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
                source->type, value->type, 0u) ||
            !debug_inline_parameter_frame_offset(
                mod, source, &frame_offset) ||
            (high_word && frame_offset > INT64_MAX - 4)) {
            return false;
        }
        if (high_word) frame_offset += 4;
        return debug_inline_integer_expression_append_frame_u32(
            expression, capacity, expression_size, ARCH_X86, frame_offset);
    }
    return false;
}

static bool debug_inline_parameter_i686_u64_append_word_less(
    const Module* mod, const Expr* left, const Expr* right, bool high_word,
    bool unsigned_high_word, bool reverse, uint8_t* expression,
    size_t capacity, size_t* expression_size) {
    const Expr* first = reverse ? right : left;
    const Expr* second = reverse ? left : right;
    bool unsigned_compare = !high_word || unsigned_high_word;
    if (!debug_inline_parameter_i686_u64_append_word_expression(
            mod, first, high_word, expression, capacity, expression_size,
            0u) ||
        (unsigned_compare &&
         (!debug_inline_integer_expression_append_constu(
              expression, capacity, expression_size,
              UINT32_C(0x80000000)) ||
          !debug_inline_integer_expression_append_byte(
              expression, capacity, expression_size, 0x27u))) ||
        !debug_inline_parameter_i686_u64_append_word_expression(
            mod, second, high_word, expression, capacity, expression_size,
            0u) ||
        (unsigned_compare &&
         (!debug_inline_integer_expression_append_constu(
              expression, capacity, expression_size,
              UINT32_C(0x80000000)) ||
          !debug_inline_integer_expression_append_byte(
              expression, capacity, expression_size, 0x27u))) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, expression_size, 0x2du)) {
        return false;
    }
    return true;
}

static bool debug_inline_parameter_i686_u64_append_word_equal(
    const Module* mod, const Expr* left, const Expr* right, bool high_word,
    uint8_t* expression, size_t capacity, size_t* expression_size) {
    return debug_inline_parameter_i686_u64_append_word_expression(
               mod, left, high_word, expression, capacity, expression_size,
               0u) &&
        debug_inline_parameter_i686_u64_append_word_expression(
            mod, right, high_word, expression, capacity, expression_size,
            0u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, expression_size, 0x29u);
}

static bool debug_inline_parameter_i686_u64_comparison_expression(
    const Module* mod, ExprKind kind, const Expr* left, const Expr* right,
    const Type* comparison_type, uint8_t* expression, size_t capacity,
    size_t* expression_size, unsigned int depth) {
    bool unsigned_type;
    bool reverse;
    bool negate;
    if (!mod || !left || !right || !comparison_type || !expression ||
        !expression_size || depth >= 32u || comparison_type->size != 8 ||
        !debug_inline_integer_expression_type(comparison_type)) {
        return false;
    }
    unsigned_type = comparison_type->is_unsigned ||
        comparison_type->kind == TYPE_BOOL;
    if (kind == EXPR_EQ || kind == EXPR_NE) {
        if (!debug_inline_parameter_i686_u64_append_word_equal(
                mod, left, right, true, expression, capacity,
                expression_size) ||
            !debug_inline_parameter_i686_u64_append_word_equal(
                mod, left, right, false, expression, capacity,
                expression_size) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, expression_size, 0x1au)) {
            return false;
        }
        return kind != EXPR_NE ||
            (debug_inline_integer_expression_append_constu(
                 expression, capacity, expression_size, 0u) &&
             debug_inline_integer_expression_append_byte(
                 expression, capacity, expression_size, 0x29u));
    }
    if (kind != EXPR_LT && kind != EXPR_LE &&
        kind != EXPR_GT && kind != EXPR_GE) {
        return false;
    }
    reverse = kind == EXPR_GT || kind == EXPR_LE;
    negate = kind == EXPR_LE || kind == EXPR_GE;
    if (!debug_inline_parameter_i686_u64_append_word_less(
            mod, left, right, true, unsigned_type, reverse, expression,
            capacity, expression_size) ||
        !debug_inline_parameter_i686_u64_append_word_equal(
            mod, left, right, true, expression, capacity, expression_size) ||
        !debug_inline_parameter_i686_u64_append_word_less(
            mod, left, right, false, true, reverse, expression, capacity,
            expression_size) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, expression_size, 0x1au) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, expression_size, 0x21u)) {
        return false;
    }
    return !negate ||
        (debug_inline_integer_expression_append_constu(
             expression, capacity, expression_size, 0u) &&
         debug_inline_integer_expression_append_byte(
             expression, capacity, expression_size, 0x29u));
}

static bool debug_inline_parameter_i686_u64_add_constant_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Expr* source_expression_value;
    const Expr* constant_expression;
    const Decl* source;
    int64_t frame_offset;
    uint64_t constant;
    uint32_t low_constant;
    uint32_t high_constant;
    size_t size = 0u;
    const uint64_t sign_bit = UINT64_C(0x80000000);
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || source_expression->kind != EXPR_ADD ||
        parameter_type->size != 8u ||
        !source_expression->binary_lhs || !source_expression->binary_rhs) {
        return false;
    }
    if (source_expression->binary_lhs->kind == EXPR_IDENT &&
        source_expression->binary_lhs->ident_decl) {
        source_expression_value = source_expression->binary_lhs;
        constant_expression = source_expression->binary_rhs;
    } else if (source_expression->binary_rhs->kind == EXPR_IDENT &&
               source_expression->binary_rhs->ident_decl) {
        source_expression_value = source_expression->binary_rhs;
        constant_expression = source_expression->binary_lhs;
    } else {
        return false;
    }
    source = source_expression_value->ident_decl;
    if (!source || (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_thread_local || source->var_is_vla || !source->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression_value->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            constant_expression->type, parameter_type, 0u) ||
        !debug_inline_integer_literal_bits(constant_expression, &constant, 0u) ||
        !debug_inline_parameter_frame_offset(mod, source, &frame_offset) ||
        frame_offset > INT64_MAX - 4) {
        return false;
    }
    low_constant = (uint32_t)constant;
    high_constant = (uint32_t)(constant >> 32u);

    /* First piece: (source.low + constant.low) modulo 2^32. */
    if (!debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, low_constant) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x22u) || /* DW_OP_plus */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* DW_OP_stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }

    /* Second piece: source.high + constant.high + carry. Compare the wrapped
       low sum with the original low word after flipping the sign bit, so the
       generic signed DW_OP_lt implements an exact unsigned comparison. */
    if (!debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset + 4) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, high_constant) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x22u) || /* high base */
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, low_constant) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x22u) || /* wrapped low sum */
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, sign_bit) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x27u) || /* xor sign bit */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x16u) || /* swap */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, sign_bit) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x27u) || /* xor sign bit */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x16u) || /* swap */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x2du) || /* DW_OP_lt */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x22u) || /* add carry */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* DW_OP_stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_variables_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Expr* left_expression;
    const Expr* right_expression;
    const Decl* left_source;
    const Decl* right_source;
    int64_t left_frame_offset;
    int64_t right_frame_offset;
    size_t size = 0u;
    const uint64_t sign_bit = UINT64_C(0x80000000);
    uint8_t bitwise_operation = 0u;
    bool is_subtract;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size ||
        (source_expression->kind != EXPR_ADD &&
         source_expression->kind != EXPR_SUB &&
         source_expression->kind != EXPR_BITAND &&
         source_expression->kind != EXPR_BITOR &&
         source_expression->kind != EXPR_BITXOR) ||
        parameter_type->size != 8u ||
        !source_expression->binary_lhs || !source_expression->binary_rhs) {
        return false;
    }
    is_subtract = source_expression->kind == EXPR_SUB;
    switch (source_expression->kind) {
    case EXPR_BITAND: bitwise_operation = 0x1au; break; /* DW_OP_and */
    case EXPR_BITOR: bitwise_operation = 0x21u; break; /* DW_OP_or */
    case EXPR_BITXOR: bitwise_operation = 0x27u; break; /* DW_OP_xor */
    default: break;
    }
    left_expression = source_expression->binary_lhs;
    right_expression = source_expression->binary_rhs;
    if (left_expression->kind != EXPR_IDENT ||
        !left_expression->ident_decl || right_expression->kind != EXPR_IDENT ||
        !right_expression->ident_decl ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            left_expression->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            right_expression->type, parameter_type, 0u)) {
        return false;
    }
    left_source = left_expression->ident_decl;
    right_source = right_expression->ident_decl;
    if (!left_source || !right_source ||
        (left_source->kind != DECL_VAR && left_source->kind != DECL_PARAM) ||
        (right_source->kind != DECL_VAR && right_source->kind != DECL_PARAM) ||
        left_source->var_is_global || left_source->var_is_static_local ||
        left_source->var_is_thread_local || left_source->var_is_vla ||
        right_source->var_is_global || right_source->var_is_static_local ||
        right_source->var_is_thread_local || right_source->var_is_vla ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            left_source->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            right_source->type, parameter_type, 0u) ||
        !debug_inline_parameter_frame_offset(
            mod, left_source, &left_frame_offset) ||
        !debug_inline_parameter_frame_offset(
            mod, right_source, &right_frame_offset) ||
        left_frame_offset > INT64_MAX - 4 ||
        right_frame_offset > INT64_MAX - 4) {
        return false;
    }

    if (bitwise_operation != 0u) {
        for (unsigned int word = 0u; word < 2u; ++word) {
            int64_t word_offset = (int64_t)(word * 4u);
            if (!debug_inline_integer_expression_append_frame_u32(
                    expression, capacity, &size, ARCH_X86,
                    left_frame_offset + word_offset) ||
                !debug_inline_integer_expression_append_frame_u32(
                    expression, capacity, &size, ARCH_X86,
                    right_frame_offset + word_offset) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, bitwise_operation) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x9fu) || /* stack_value */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x93u) || /* DW_OP_piece */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 4u)) {
                return false;
            }
        }
        *expression_size = size;
        return true;
    }

    /* Emit the low word and the high-word base before deriving carry/borrow. */
    if (!debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, left_frame_offset) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, right_frame_offset) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, is_subtract ? 0x1cu : 0x22u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, left_frame_offset + 4) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, right_frame_offset + 4) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, is_subtract ? 0x1cu : 0x22u)) {
        return false;
    }
    if (is_subtract) {
        /* lhs.low < rhs.low, comparing unsigned words via sign-bit flipping. */
        if (!debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, left_frame_offset) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, sign_bit) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x27u) || /* lhs low ^ sign */
            !debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, right_frame_offset) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, sign_bit) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x27u) || /* rhs low ^ sign */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x2du) || /* DW_OP_lt */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x1cu)) { /* subtract borrow */
            return false;
        }
    } else {
        /* Compare the wrapped low sum with lhs.low to derive unsigned carry. */
        if (!debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, left_frame_offset) ||
            !debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, right_frame_offset) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x22u) || /* wrapped low sum */
            !debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, left_frame_offset) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, sign_bit) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x27u) || /* left low ^ sign */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x16u) || /* swap */
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, sign_bit) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x27u) || /* sum ^ sign */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x16u) || /* swap */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x2du) || /* DW_OP_lt */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x22u)) { /* add carry */
            return false;
        }
    }
    if (!debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_bitwise_constant_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Expr* source_expression_value;
    const Expr* constant_expression;
    const Decl* source;
    int64_t frame_offset;
    uint64_t constant;
    uint8_t operation;
    size_t size = 0u;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || parameter_type->size != 8u ||
        !source_expression->binary_lhs ||
        !source_expression->binary_rhs) {
        return false;
    }
    switch (source_expression->kind) {
    case EXPR_BITAND: operation = 0x1au; break; /* DW_OP_and */
    case EXPR_BITOR: operation = 0x21u; break; /* DW_OP_or */
    case EXPR_BITXOR: operation = 0x27u; break; /* DW_OP_xor */
    default: return false;
    }
    if (source_expression->binary_lhs->kind == EXPR_IDENT &&
        source_expression->binary_lhs->ident_decl) {
        source_expression_value = source_expression->binary_lhs;
        constant_expression = source_expression->binary_rhs;
    } else if (source_expression->binary_rhs->kind == EXPR_IDENT &&
               source_expression->binary_rhs->ident_decl) {
        source_expression_value = source_expression->binary_rhs;
        constant_expression = source_expression->binary_lhs;
    } else {
        return false;
    }
    source = source_expression_value->ident_decl;
    if (!source || (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_thread_local || source->var_is_vla || !source->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression_value->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            constant_expression->type, parameter_type, 0u) ||
        !debug_inline_integer_literal_bits(constant_expression, &constant, 0u) ||
        !debug_inline_parameter_frame_offset(mod, source, &frame_offset) ||
        frame_offset > INT64_MAX - 4) {
        return false;
    }

    /* A 32-bit DWARF stack entry cannot represent the value as a whole.
       Emit each storage-ordered word independently, retaining all 64 bits. */
    for (unsigned int word = 0u; word < 2u; ++word) {
        uint32_t word_constant = word == 0u
            ? (uint32_t)constant : (uint32_t)(constant >> 32u);
        if (!debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86,
                frame_offset + (int64_t)(word * 4u)) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, word_constant) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, operation) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x9fu) || /* stack_value */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x93u) || /* DW_OP_piece */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 4u)) {
            return false;
        }
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_i686_append_u32_product_high(
    const Module* mod, int64_t frame_offset, uint32_t multiplier,
    uint8_t* expression, size_t capacity, size_t* size) {
    const uint32_t mask16 = UINT32_C(0xffff);
    const uint32_t low_multiplier = multiplier & mask16;
    const uint32_t high_multiplier = multiplier >> 16u;
    if (!mod || !expression || !size || frame_offset > INT64_MAX - 4) {
        return false;
    }

    /* For a = a0 + 2^16*a1 and m = m0 + 2^16*m1,
       high32(a*m) = a1*m1 + high16(a0*m0) + low16(a0*m1) +
       low16(a1*m0), with the middle-word carry included. Each term fits
       the i686 DWARF stack and arithmetic wraps exactly at 32 bits. */
    return debug_inline_integer_expression_append_frame_u32(
               expression, capacity, size, ARCH_X86, frame_offset) &&
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, mask16) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) && /* a0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, low_multiplier) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a0*m0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* high16(a0*m0) */

        debug_inline_integer_expression_append_frame_u32(
            expression, capacity, size, ARCH_X86, frame_offset) &&
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, mask16) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) && /* a0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, high_multiplier) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a0*m1 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, mask16) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) && /* low16(a0*m1) */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) && /* middle sum */

        debug_inline_integer_expression_append_frame_u32(
            expression, capacity, size, ARCH_X86, frame_offset) &&
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* a1 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, low_multiplier) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a1*m0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, mask16) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) && /* low16(a1*m0) */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) && /* middle sum */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* middle carry */

        debug_inline_integer_expression_append_frame_u32(
            expression, capacity, size, ARCH_X86, frame_offset) &&
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, mask16) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) && /* a0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, high_multiplier) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a0*m1 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* high16(a0*m1) */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) &&

        debug_inline_integer_expression_append_frame_u32(
            expression, capacity, size, ARCH_X86, frame_offset) &&
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* a1 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, low_multiplier) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a1*m0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* high16(a1*m0) */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) &&

        debug_inline_integer_expression_append_frame_u32(
            expression, capacity, size, ARCH_X86, frame_offset) &&
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* a1 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, high_multiplier) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a1*m1 */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u); /* high product */
}

static bool debug_inline_i686_append_frame_u16(
    const Module* mod, int64_t frame_offset, bool high_half,
    uint8_t* expression, size_t capacity, size_t* size) {
    if (!mod || !expression || !size || frame_offset > INT64_MAX - 4 ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, size, ARCH_X86, frame_offset)) {
        return false;
    }
    if (high_half) {
        return debug_inline_integer_expression_append_constu(
                   expression, capacity, size, 16u) &&
            debug_inline_integer_expression_append_byte(
                expression, capacity, size, 0x25u); /* logical >> 16 */
    }
    return debug_inline_integer_expression_append_constu(
               expression, capacity, size, UINT32_C(0xffff)) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au); /* low 16 bits */
}

static bool debug_inline_i686_append_u32_product_high_variables(
    const Module* mod, int64_t left_frame_offset, int64_t right_frame_offset,
    uint8_t* expression, size_t capacity, size_t* size) {
    /* For a 32x32 product, split each input into 16-bit limbs. The high
       word is a1*b1 + high16(a0*b0) + high16(a0*b1) + high16(a1*b0),
       including the carry from the three middle-word low halves. */
    return debug_inline_i686_append_frame_u16(
               mod, left_frame_offset, false, expression, capacity, size) &&
        debug_inline_i686_append_frame_u16(
            mod, right_frame_offset, false, expression, capacity, size) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a0*b0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* high16(a0*b0) */

        debug_inline_i686_append_frame_u16(
            mod, left_frame_offset, false, expression, capacity, size) &&
        debug_inline_i686_append_frame_u16(
            mod, right_frame_offset, true, expression, capacity, size) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a0*b1 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, UINT32_C(0xffff)) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) && /* low16(a0*b1) */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) && /* middle sum */

        debug_inline_i686_append_frame_u16(
            mod, left_frame_offset, true, expression, capacity, size) &&
        debug_inline_i686_append_frame_u16(
            mod, right_frame_offset, false, expression, capacity, size) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a1*b0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, UINT32_C(0xffff)) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) && /* low16(a1*b0) */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) && /* middle sum */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* middle carry */

        debug_inline_i686_append_frame_u16(
            mod, left_frame_offset, false, expression, capacity, size) &&
        debug_inline_i686_append_frame_u16(
            mod, right_frame_offset, true, expression, capacity, size) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a0*b1 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* high16(a0*b1) */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) &&

        debug_inline_i686_append_frame_u16(
            mod, left_frame_offset, true, expression, capacity, size) &&
        debug_inline_i686_append_frame_u16(
            mod, right_frame_offset, false, expression, capacity, size) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a1*b0 */
        debug_inline_integer_expression_append_constu(
            expression, capacity, size, 16u) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x25u) && /* high16(a1*b0) */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u) &&

        debug_inline_i686_append_frame_u16(
            mod, left_frame_offset, true, expression, capacity, size) &&
        debug_inline_i686_append_frame_u16(
            mod, right_frame_offset, true, expression, capacity, size) &&
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1eu) && /* a1*b1 */
        debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x22u);
}

static bool debug_inline_parameter_i686_u64_mul_variables_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Expr* left_expression;
    const Expr* right_expression;
    const Decl* left_source;
    const Decl* right_source;
    int64_t left_frame_offset;
    int64_t right_frame_offset;
    size_t size = 0u;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || source_expression->kind != EXPR_MUL ||
        parameter_type->size != 8u ||
        !source_expression->binary_lhs || !source_expression->binary_rhs) {
        return false;
    }
    left_expression = source_expression->binary_lhs;
    right_expression = source_expression->binary_rhs;
    if (left_expression->kind != EXPR_IDENT ||
        !left_expression->ident_decl || right_expression->kind != EXPR_IDENT ||
        !right_expression->ident_decl ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            left_expression->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            right_expression->type, parameter_type, 0u)) {
        return false;
    }
    left_source = left_expression->ident_decl;
    right_source = right_expression->ident_decl;
    if (!left_source || !right_source ||
        (left_source->kind != DECL_VAR && left_source->kind != DECL_PARAM) ||
        (right_source->kind != DECL_VAR && right_source->kind != DECL_PARAM) ||
        left_source->var_is_global || left_source->var_is_static_local ||
        left_source->var_is_thread_local || left_source->var_is_vla ||
        right_source->var_is_global || right_source->var_is_static_local ||
        right_source->var_is_thread_local || right_source->var_is_vla ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            left_source->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            right_source->type, parameter_type, 0u) ||
        !debug_inline_parameter_frame_offset(
            mod, left_source, &left_frame_offset) ||
        !debug_inline_parameter_frame_offset(
            mod, right_source, &right_frame_offset) ||
        left_frame_offset > INT64_MAX - 4 ||
        right_frame_offset > INT64_MAX - 4) {
        return false;
    }

    /* Product modulo 2^64: low32(lhs.low*rhs.low), plus its exact high32
       and the low words of the two cross-products. */
    if (!debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, left_frame_offset) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, right_frame_offset) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1eu) || /* low product */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u) ||
        !debug_inline_i686_append_u32_product_high_variables(
            mod, left_frame_offset, right_frame_offset,
            expression, capacity, &size) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, left_frame_offset) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, right_frame_offset + 4) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1eu) || /* lhs.low*rhs.high */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x22u) || /* add cross-product */
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, left_frame_offset + 4) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, right_frame_offset) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1eu) || /* lhs.high*rhs.low */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x22u) || /* add cross-product */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_mul_constant_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Expr* source_expression_value;
    const Expr* constant_expression;
    const Decl* source;
    int64_t frame_offset;
    uint64_t constant;
    uint32_t low_constant;
    uint32_t high_constant;
    size_t size = 0u;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || source_expression->kind != EXPR_MUL ||
        parameter_type->size != 8u ||
        !source_expression->binary_lhs || !source_expression->binary_rhs) {
        return false;
    }
    if (source_expression->binary_lhs->kind == EXPR_IDENT &&
        source_expression->binary_lhs->ident_decl) {
        source_expression_value = source_expression->binary_lhs;
        constant_expression = source_expression->binary_rhs;
    } else if (source_expression->binary_rhs->kind == EXPR_IDENT &&
               source_expression->binary_rhs->ident_decl) {
        source_expression_value = source_expression->binary_rhs;
        constant_expression = source_expression->binary_lhs;
    } else {
        return false;
    }
    source = source_expression_value->ident_decl;
    if (!source || (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_thread_local || source->var_is_vla || !source->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression_value->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            constant_expression->type, parameter_type, 0u) ||
        !debug_inline_integer_literal_bits(constant_expression, &constant, 0u) ||
        !debug_inline_parameter_frame_offset(mod, source, &frame_offset) ||
        frame_offset > INT64_MAX - 4) {
        return false;
    }
    low_constant = (uint32_t)constant;
    high_constant = (uint32_t)(constant >> 32u);

    /* The low piece is the wrapped low-word product. The high piece adds
       both cross-products and the exact high word of low*low. */
    if (!debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, low_constant) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1eu) || /* low product */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset + 4) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, low_constant) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1eu) || /* high*constant.low */
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, high_constant) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1eu) || /* low*constant.high */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x22u) || /* cross-products */
        !debug_inline_i686_append_u32_product_high(
            mod, frame_offset, low_constant, expression, capacity, &size) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x22u) || /* add low carry */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_sub_constant_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Expr* source_expression_value;
    const Expr* constant_expression;
    const Decl* source;
    int64_t frame_offset;
    uint64_t constant;
    uint32_t low_constant;
    uint32_t high_constant;
    size_t size = 0u;
    const uint64_t sign_bit = UINT64_C(0x80000000);
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || source_expression->kind != EXPR_SUB ||
        parameter_type->size != 8u ||
        !source_expression->binary_lhs || !source_expression->binary_rhs ||
        source_expression->binary_lhs->kind != EXPR_IDENT ||
        !source_expression->binary_lhs->ident_decl) {
        return false;
    }
    source_expression_value = source_expression->binary_lhs;
    constant_expression = source_expression->binary_rhs;
    source = source_expression_value->ident_decl;
    if (!source || (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_thread_local || source->var_is_vla || !source->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression_value->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            constant_expression->type, parameter_type, 0u) ||
        !debug_inline_integer_literal_bits(constant_expression, &constant, 0u) ||
        !debug_inline_parameter_frame_offset(mod, source, &frame_offset) ||
        frame_offset > INT64_MAX - 4) {
        return false;
    }
    low_constant = (uint32_t)constant;
    high_constant = (uint32_t)(constant >> 32u);

    /* The low word wraps modulo 2^32; the high word subtracts an unsigned
       borrow computed by comparing the original low words. */
    if (!debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, low_constant) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1cu) || /* DW_OP_minus */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset + 4) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, high_constant) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1cu) || /* high difference */
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, sign_bit) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x27u) || /* xor sign bit */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, low_constant) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, sign_bit) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x27u) || /* xor sign bit */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x2du) || /* unsigned low < const */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1cu) || /* subtract borrow */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_shift_constant_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Decl* source;
    int64_t frame_offset;
    uint64_t shift_count;
    size_t size = 0u;
    bool is_left_shift;
    bool is_unsigned;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size ||
        parameter_type->size != 8u || !source_expression->binary_lhs ||
        !source_expression->binary_rhs ||
        source_expression->binary_lhs->kind != EXPR_IDENT ||
        !source_expression->binary_lhs->ident_decl ||
        (source_expression->kind != EXPR_LSHIFT &&
         source_expression->kind != EXPR_RSHIFT) ||
        !debug_inline_integer_literal_bits(
            source_expression->binary_rhs, &shift_count, 0u) ||
        shift_count >= 64u ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->binary_lhs->type, parameter_type, 0u)) {
        return false;
    }
    source = source_expression->binary_lhs->ident_decl;
    if (!source || (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_thread_local || source->var_is_vla || !source->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source->type, parameter_type, 0u) ||
        !debug_inline_parameter_frame_offset(mod, source, &frame_offset) ||
        frame_offset > INT64_MAX - 4) {
        return false;
    }
    is_left_shift = source_expression->kind == EXPR_LSHIFT;
    is_unsigned = parameter_type->is_unsigned;

    /* A 32-bit DWARF stack entry cannot hold the full value. Split the shift
       across both storage words, including cross-word and sign-fill bits. */
    for (unsigned int word = 0u; word < 2u; ++word) {
        int64_t word_offset = frame_offset + (int64_t)(word * 4u);
        if (shift_count == 0u) {
            if (!debug_inline_integer_expression_append_frame_u32(
                    expression, capacity, &size, ARCH_X86, word_offset)) {
                return false;
            }
        } else if (is_left_shift) {
            if (shift_count < 32u) {
                if (!debug_inline_integer_expression_append_frame_u32(
                        expression, capacity, &size, ARCH_X86,
                        word == 0u ? frame_offset : frame_offset + 4) ||
                    !debug_inline_integer_expression_append_constu(
                        expression, capacity, &size, shift_count) ||
                    !debug_inline_integer_expression_append_byte(
                        expression, capacity, &size, 0x24u)) { /* shl */
                    return false;
                }
                if (word == 1u &&
                    (!debug_inline_integer_expression_append_frame_u32(
                         expression, capacity, &size, ARCH_X86, frame_offset) ||
                     !debug_inline_integer_expression_append_constu(
                         expression, capacity, &size, 32u - shift_count) ||
                     !debug_inline_integer_expression_append_byte(
                         expression, capacity, &size, 0x25u) || /* shr */
                     !debug_inline_integer_expression_append_byte(
                         expression, capacity, &size, 0x21u))) { /* or */
                    return false;
                }
            } else if (word == 0u) {
                if (!debug_inline_integer_expression_append_constu(
                        expression, capacity, &size, 0u)) {
                    return false;
                }
            } else if (!debug_inline_integer_expression_append_frame_u32(
                           expression, capacity, &size, ARCH_X86,
                           frame_offset) ||
                       !debug_inline_integer_expression_append_constu(
                           expression, capacity, &size, shift_count - 32u) ||
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size, 0x24u)) { /* shl */
                return false;
            }
        } else if (shift_count < 32u) {
            if (word == 0u) {
                if (!debug_inline_integer_expression_append_frame_u32(
                        expression, capacity, &size, ARCH_X86, frame_offset) ||
                    !debug_inline_integer_expression_append_constu(
                        expression, capacity, &size, shift_count) ||
                    !debug_inline_integer_expression_append_byte(
                        expression, capacity, &size, 0x25u) || /* shr */
                    !debug_inline_integer_expression_append_frame_u32(
                        expression, capacity, &size, ARCH_X86,
                        frame_offset + 4) ||
                    !debug_inline_integer_expression_append_constu(
                        expression, capacity, &size, 32u - shift_count) ||
                    !debug_inline_integer_expression_append_byte(
                        expression, capacity, &size, 0x24u) || /* shl */
                    !debug_inline_integer_expression_append_byte(
                        expression, capacity, &size, 0x21u)) { /* or */
                    return false;
                }
            } else if (!debug_inline_integer_expression_append_frame_u32(
                           expression, capacity, &size, ARCH_X86,
                           frame_offset + 4) ||
                       !debug_inline_integer_expression_append_constu(
                           expression, capacity, &size, shift_count) ||
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size,
                           is_unsigned ? 0x25u : 0x26u)) { /* shr/shra */
                return false;
            }
        } else if (word == 0u && shift_count == 32u) {
            if (!debug_inline_integer_expression_append_frame_u32(
                    expression, capacity, &size, ARCH_X86,
                    frame_offset + 4)) {
                return false;
            }
        } else if (word == 0u &&
                   (!debug_inline_integer_expression_append_frame_u32(
                        expression, capacity, &size, ARCH_X86,
                        frame_offset + 4) ||
                    !debug_inline_integer_expression_append_constu(
                        expression, capacity, &size, shift_count - 32u) ||
                    !debug_inline_integer_expression_append_byte(
                        expression, capacity, &size,
                        is_unsigned ? 0x25u : 0x26u))) { /* shr/shra */
            return false;
        } else if (word == 1u) {
            if (is_unsigned) {
                if (!debug_inline_integer_expression_append_constu(
                        expression, capacity, &size, 0u)) {
                    return false;
                }
            } else if (!debug_inline_integer_expression_append_frame_u32(
                           expression, capacity, &size, ARCH_X86,
                           frame_offset + 4) ||
                       !debug_inline_integer_expression_append_constu(
                           expression, capacity, &size, 31u) ||
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size, 0x26u)) { /* shra */
                return false;
            }
        }

        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x9fu) || /* stack_value */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x93u) || /* DW_OP_piece */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 4u)) {
            return false;
        }
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_signed_u64_node(
    const Expr* expression) {
    return expression &&
        debug_inline_integer_expression_type(expression->type) &&
        expression->type->size == 8u && !expression->type->is_unsigned;
}

static bool debug_inline_parameter_i686_expression_equal(
    const Expr* lhs, const Expr* rhs, unsigned int depth) {
    uint64_t lhs_literal;
    uint64_t rhs_literal;
    bool lhs_is_literal;
    bool rhs_is_literal;

    if (lhs == rhs) return true;
    if (!lhs || !rhs || depth >= 32u || lhs->kind != rhs->kind ||
        lhs->type != rhs->type) {
        return false;
    }
    lhs_is_literal = debug_inline_integer_literal_bits(
        lhs, &lhs_literal, 0u);
    rhs_is_literal = debug_inline_integer_literal_bits(
        rhs, &rhs_literal, 0u);
    if (lhs_is_literal || rhs_is_literal) {
        return lhs_is_literal && rhs_is_literal &&
            lhs_literal == rhs_literal;
    }
    switch (lhs->kind) {
        case EXPR_IDENT:
            return lhs->ident_decl && lhs->ident_decl == rhs->ident_decl;
        case EXPR_POS:
        case EXPR_NEG:
        case EXPR_BITNOT:
            return debug_inline_parameter_i686_expression_equal(
                lhs->unary_operand, rhs->unary_operand, depth + 1u);
        case EXPR_CAST:
            return lhs->cast_type == rhs->cast_type &&
                debug_inline_parameter_i686_expression_equal(
                    lhs->cast_expr, rhs->cast_expr, depth + 1u);
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
            return debug_inline_parameter_i686_expression_equal(
                       lhs->binary_lhs, rhs->binary_lhs, depth + 1u) &&
                debug_inline_parameter_i686_expression_equal(
                    lhs->binary_rhs, rhs->binary_rhs, depth + 1u);
        default:
            return false;
    }
}

static bool debug_inline_parameter_i686_u64_signed_modulo_rewrite(
    const Expr* expression, const Expr** operand, uint64_t* modulus) {
    const Expr* product;
    const Expr* quotient;
    const Expr* adjusted;
    const Expr* bias;
    const Expr* sign_mask;
    const Expr* source_expression;
    uint64_t divisor_bits;
    uint64_t quotient_shift;
    uint64_t bias_bits;
    uint64_t sign_shift;

    if (!expression || !operand || !modulus ||
        expression->kind != EXPR_SUB || !expression->binary_lhs ||
        !expression->binary_rhs ||
        expression->binary_rhs->kind != EXPR_MUL ||
        !expression->binary_rhs->binary_lhs ||
        !expression->binary_rhs->binary_rhs ||
        !debug_inline_parameter_i686_signed_u64_node(expression) ||
        !debug_inline_parameter_i686_signed_u64_node(
            expression->binary_lhs)) {
        return false;
    }

    source_expression = expression->binary_lhs;
    product = expression->binary_rhs;
    quotient = product->binary_lhs;
    if (quotient->kind != EXPR_RSHIFT || !quotient->binary_lhs ||
        !quotient->binary_rhs ||
        !debug_inline_parameter_i686_signed_u64_node(product) ||
        !debug_inline_parameter_i686_signed_u64_node(quotient) ||
        !debug_inline_integer_literal_bits(
            product->binary_rhs, &divisor_bits, 0u) ||
        divisor_bits < 2u || divisor_bits > UINT64_C(0x100000000) ||
        (divisor_bits & (divisor_bits - 1u)) != 0u ||
        !debug_inline_integer_literal_bits(
            quotient->binary_rhs, &quotient_shift, 0u) ||
        quotient_shift == 0u || quotient_shift >= 63u ||
        divisor_bits != (UINT64_C(1) << quotient_shift)) {
        return false;
    }

    adjusted = quotient->binary_lhs;
    if (adjusted->kind != EXPR_ADD || !adjusted->binary_lhs ||
        !debug_inline_parameter_i686_expression_equal(
            adjusted->binary_lhs, source_expression, 0u) ||
        !adjusted->binary_rhs || adjusted->binary_rhs->kind != EXPR_BITAND ||
        !adjusted->binary_rhs->binary_lhs ||
        !adjusted->binary_rhs->binary_rhs ||
        !debug_inline_parameter_i686_signed_u64_node(adjusted) ||
        !debug_inline_parameter_i686_signed_u64_node(
            adjusted->binary_lhs) ||
        !debug_inline_parameter_i686_signed_u64_node(adjusted->binary_rhs) ||
        !debug_inline_integer_literal_bits(
            adjusted->binary_rhs->binary_rhs, &bias_bits, 0u) ||
        bias_bits != divisor_bits - 1u) {
        return false;
    }

    bias = adjusted->binary_rhs;
    sign_mask = bias->binary_lhs;
    if (sign_mask->kind != EXPR_RSHIFT || !sign_mask->binary_lhs ||
        !debug_inline_parameter_i686_expression_equal(
            sign_mask->binary_lhs, source_expression, 0u) ||
        !sign_mask->binary_rhs ||
        !debug_inline_parameter_i686_signed_u64_node(sign_mask) ||
        !debug_inline_parameter_i686_signed_u64_node(
            sign_mask->binary_lhs) ||
        !debug_inline_integer_literal_bits(
            sign_mask->binary_rhs, &sign_shift, 0u) ||
        sign_shift != 63u) {
        return false;
    }

    *operand = source_expression;
    *modulus = divisor_bits;
    return true;
}

static bool debug_inline_parameter_i686_u64_append_low_word_expression(
    const Module* mod, const Expr* expression, uint8_t* output,
    size_t capacity, size_t* size, unsigned int depth) {
    uint64_t literal_bits;
    const Expr* modulo_operand = NULL;
    uint64_t modulo_divisor = 0u;
    if (!mod || !expression || !output || !size || depth >= 32u ||
        !debug_inline_integer_expression_type(expression->type) ||
        expression->type->size != 8) {
        return false;
    }
    if (debug_inline_integer_literal_bits(
            expression, &literal_bits, 0u)) {
        return debug_inline_integer_expression_append_constu(
            output, capacity, size, (uint32_t)literal_bits);
    }
    if (debug_inline_parameter_i686_u64_signed_modulo_rewrite(
            expression, &modulo_operand, &modulo_divisor) &&
        debug_inline_parameter_i686_u64_append_low_word_expression(
            mod, modulo_operand, output, capacity, size, depth + 1u) &&
        debug_inline_integer_expression_append_constu(
            output, capacity, size, modulo_divisor - 1u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, size, 0x1au)) { /* DW_OP_and */
        /* The optimizer expands signed x % 2^n into a truncating quotient
           correction. On a defined shift count (0..63), the remainder's low
           bits are exactly x & (2^n - 1). */
        return true;
    }
    if (expression->kind == EXPR_IDENT && expression->ident_decl) {
        const Decl* source = expression->ident_decl;
        int64_t frame_offset;
        if ((source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
            source->var_is_global || source->var_is_static_local ||
            source->var_is_thread_local || source->var_is_vla ||
            !source->type ||
            !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
                source->type, expression->type, 0u) ||
            !debug_inline_parameter_frame_offset(
                mod, source, &frame_offset)) {
            return false;
        }
        return debug_inline_integer_expression_append_frame_u32(
            output, capacity, size, ARCH_X86, frame_offset);
    }
    if (expression->kind == EXPR_POS && expression->unary_operand &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->unary_operand->type, expression->type, 0u) &&
        debug_inline_parameter_i686_u64_append_low_word_expression(
            mod, expression->unary_operand, output, capacity, size,
            depth + 1u)) {
        return true;
    }
    if ((expression->kind == EXPR_NEG ||
         expression->kind == EXPR_BITNOT) && expression->unary_operand &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->unary_operand->type, expression->type, 0u) &&
        debug_inline_parameter_i686_u64_append_low_word_expression(
            mod, expression->unary_operand, output, capacity, size,
            depth + 1u)) {
        return debug_inline_integer_expression_append_byte(
            output, capacity, size,
            expression->kind == EXPR_NEG ? 0x1fu : 0x20u);
    }
    if (expression->kind == EXPR_MOD && expression->binary_lhs &&
        expression->binary_rhs &&
        debug_inline_integer_literal_bits(
            expression->binary_rhs, &literal_bits, 0u) &&
        literal_bits != 0u &&
        (literal_bits & (literal_bits - 1u)) == 0u &&
        literal_bits <= UINT64_C(0x100000000) &&
        (expression->type->is_unsigned ||
         literal_bits <= (uint64_t)INT64_MAX) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_lhs->type, expression->type, 0u) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_rhs->type, expression->type, 0u) &&
        debug_inline_parameter_i686_u64_append_low_word_expression(
            mod, expression->binary_lhs, output, capacity, size,
            depth + 1u) &&
        debug_inline_integer_expression_append_constu(
            output, capacity, size, literal_bits - 1u) &&
        debug_inline_integer_expression_append_byte(
            output, capacity, size, 0x1au)) { /* low word & (modulus-1) */
        /* In two's-complement arithmetic, the low n bits of x % 2^n are
           exactly x's low n bits. For a defined 64-bit shift the resulting
           count is 0..63, so the 32-bit DWARF stack value is exact. A
           negative count would make the source shift undefined. */
        return true;
    }
    if ((expression->kind == EXPR_ADD || expression->kind == EXPR_SUB ||
         expression->kind == EXPR_MUL || expression->kind == EXPR_BITAND ||
         expression->kind == EXPR_BITOR || expression->kind == EXPR_BITXOR) &&
        expression->binary_lhs && expression->binary_rhs &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_lhs->type, expression->type, 0u) &&
        debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            expression->binary_rhs->type, expression->type, 0u) &&
        debug_inline_parameter_i686_u64_append_low_word_expression(
            mod, expression->binary_lhs, output, capacity, size, depth + 1u) &&
        debug_inline_parameter_i686_u64_append_low_word_expression(
            mod, expression->binary_rhs, output, capacity, size, depth + 1u)) {
        uint8_t opcode = expression->kind == EXPR_ADD ? 0x22u :
            expression->kind == EXPR_SUB ? 0x1cu :
            expression->kind == EXPR_MUL ? 0x1eu :
            expression->kind == EXPR_BITAND ? 0x1au :
            expression->kind == EXPR_BITOR ? 0x21u : 0x27u;
        return debug_inline_integer_expression_append_byte(
            output, capacity, size, opcode);
    }
    return false;
}

static bool debug_inline_parameter_i686_u64_append_shift_count(
    const Module* mod, const Expr* count_expression,
    const Decl* count_source, int64_t count_frame_offset,
    uint8_t* expression, size_t capacity, size_t* size) {
    if (!mod || !count_expression || !expression || !size) return false;
    /* A direct 64-bit count is safe to read from its low word for defined
       64-bit shifts (0..63). Computed 64-bit arithmetic has the same low
       32-bit result as its full-width value; defined shift counts therefore
       retain their exact value on the 32-bit DWARF stack. */
    if (count_source && count_source->type &&
        count_source->type->size == 8) {
        return debug_inline_integer_expression_append_frame_u32(
            expression, capacity, size, ARCH_X86, count_frame_offset);
    }
    if (count_expression->type && count_expression->type->size == 8) {
        return debug_inline_parameter_i686_u64_append_low_word_expression(
            mod, count_expression, expression, capacity, size, 0u);
    }
    return count_expression->type &&
        (unsigned int)count_expression->type->size <= 4u &&
        debug_build_inline_integer_value_expression(
            mod, ARCH_X86, count_expression, expression, capacity, size, 0u);
}

static bool debug_inline_parameter_i686_u64_shift_variable_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Expr* source_value_expression;
    const Expr* shift_count_expression;
    const Decl* source;
    const Decl* shift_count_source;
    int64_t source_frame_offset;
    int64_t shift_count_frame_offset = 0;
    size_t size = 0u;
    bool is_left_shift;
    bool is_unsigned;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || parameter_type->size != 8u ||
        !source_expression->binary_lhs || !source_expression->binary_rhs ||
        source_expression->binary_lhs->kind != EXPR_IDENT ||
        !source_expression->binary_lhs->ident_decl ||
        (source_expression->kind != EXPR_LSHIFT &&
         source_expression->kind != EXPR_RSHIFT) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->binary_lhs->type, parameter_type, 0u)) {
        return false;
    }
    source_value_expression = source_expression->binary_lhs;
    shift_count_expression = source_expression->binary_rhs;
    source = source_value_expression->ident_decl;
    shift_count_source = shift_count_expression->kind == EXPR_IDENT
        ? shift_count_expression->ident_decl : NULL;
    if (!source ||
        (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_thread_local || source->var_is_vla ||
        !source->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source->type, parameter_type, 0u) ||
        !debug_inline_integer_expression_type(shift_count_expression->type) ||
        (shift_count_source &&
         ((shift_count_source->kind != DECL_VAR &&
           shift_count_source->kind != DECL_PARAM) ||
          shift_count_source->var_is_global ||
          shift_count_source->var_is_static_local ||
          shift_count_source->var_is_thread_local ||
          shift_count_source->var_is_vla || !shift_count_source->type ||
          (shift_count_source->type->size != 4u &&
           shift_count_source->type->size != 8u) ||
          !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
              shift_count_expression->type,
              shift_count_source->type, 0u))) ||
        (!shift_count_source &&
         (unsigned int)shift_count_expression->type->size > 4u &&
         shift_count_expression->type->size != 8) ||
        !debug_inline_parameter_frame_offset(
            mod, source, &source_frame_offset) ||
        (shift_count_source && !debug_inline_parameter_frame_offset(
            mod, shift_count_source, &shift_count_frame_offset)) ||
        source_frame_offset > INT64_MAX - 4) {
        return false;
    }
    is_left_shift = source_expression->kind == EXPR_LSHIFT;
    is_unsigned = parameter_type->is_unsigned;

    /* For a defined 64-bit shift, an 8-byte count is necessarily in 0..63,
       so its low word contains the full count. Split each result word at the
       32-bit boundary; count zero has its own path so no DWARF shift is asked
       to use the invalid width 32. */
    for (unsigned int word = 0u; word < 2u; ++word) {
        size_t zero_count_branch_operand;
        size_t high_count_branch_operand;
        size_t low_count_path_end_operand;
        size_t high_count_path_end_operand;
        size_t high_count_path_offset;
        size_t zero_count_path_offset;
        int64_t source_word_offset =
            source_frame_offset + (int64_t)(word * 4u);
        if (!debug_inline_parameter_i686_u64_append_shift_count(
                mod, shift_count_expression, shift_count_source,
                shift_count_frame_offset, expression, capacity, &size) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, 0u) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x29u) || /* DW_OP_eq */
            !debug_inline_integer_expression_append_branch(
                expression, capacity, &size, 0x28u,
                &zero_count_branch_operand) || /* DW_OP_bra */
            !debug_inline_parameter_i686_u64_append_shift_count(
                mod, shift_count_expression, shift_count_source,
                shift_count_frame_offset, expression, capacity, &size) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, 32u) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x2au) || /* DW_OP_ge */
            !debug_inline_integer_expression_append_branch(
                expression, capacity, &size, 0x28u,
                &high_count_branch_operand)) { /* DW_OP_bra */
            return false;
        }

        if (is_left_shift) {
            if (word == 0u) {
                if (!debug_inline_integer_expression_append_frame_u32(
                        expression, capacity, &size, ARCH_X86,
                        source_frame_offset) ||
                    !debug_inline_parameter_i686_u64_append_shift_count(
                        mod, shift_count_expression, shift_count_source,
                        shift_count_frame_offset, expression, capacity,
                        &size) ||
                    !debug_inline_integer_expression_append_byte(
                        expression, capacity, &size, 0x24u)) { /* DW_OP_shl */
                    return false;
                }
            } else if (!debug_inline_integer_expression_append_frame_u32(
                           expression, capacity, &size, ARCH_X86,
                           source_frame_offset + 4) ||
                       !debug_inline_parameter_i686_u64_append_shift_count(
                           mod, shift_count_expression, shift_count_source,
                           shift_count_frame_offset, expression, capacity,
                           &size) ||
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size, 0x24u) || /* DW_OP_shl */
                       !debug_inline_integer_expression_append_frame_u32(
                           expression, capacity, &size, ARCH_X86,
                           source_frame_offset) ||
                       !debug_inline_integer_expression_append_constu(
                           expression, capacity, &size, 32u) ||
                       !debug_inline_parameter_i686_u64_append_shift_count(
                           mod, shift_count_expression, shift_count_source,
                           shift_count_frame_offset, expression, capacity,
                           &size) ||
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size, 0x1cu) || /* 32-count */
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size, 0x25u) || /* DW_OP_shr */
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size, 0x21u)) { /* DW_OP_or */
                return false;
            }
        } else if (word == 0u) {
            if (!debug_inline_integer_expression_append_frame_u32(
                    expression, capacity, &size, ARCH_X86,
                    source_frame_offset) ||
                !debug_inline_parameter_i686_u64_append_shift_count(
                    mod, shift_count_expression, shift_count_source,
                    shift_count_frame_offset, expression, capacity, &size) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x25u) || /* DW_OP_shr */
                !debug_inline_integer_expression_append_frame_u32(
                    expression, capacity, &size, ARCH_X86,
                    source_frame_offset + 4) ||
                !debug_inline_integer_expression_append_constu(
                    expression, capacity, &size, 32u) ||
                !debug_inline_parameter_i686_u64_append_shift_count(
                    mod, shift_count_expression, shift_count_source,
                    shift_count_frame_offset, expression, capacity, &size) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x1cu) || /* 32-count */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x24u) || /* DW_OP_shl */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x21u)) { /* DW_OP_or */
                return false;
            }
        } else if (!debug_inline_integer_expression_append_frame_u32(
                       expression, capacity, &size, ARCH_X86,
                       source_frame_offset + 4) ||
                   !debug_inline_parameter_i686_u64_append_shift_count(
                       mod, shift_count_expression, shift_count_source,
                       shift_count_frame_offset, expression, capacity,
                       &size) ||
                   !debug_inline_integer_expression_append_byte(
                       expression, capacity, &size,
                       is_unsigned ? 0x25u : 0x26u)) { /* shr/shra */
            return false;
        }

        if (!debug_inline_integer_expression_append_branch(
                expression, capacity, &size, 0x2fu,
                &low_count_path_end_operand)) { /* DW_OP_skip */
            return false;
        }
        high_count_path_offset = size;
        if (!debug_inline_integer_expression_patch_branch(
                expression, size, high_count_branch_operand,
                high_count_path_offset)) {
            return false;
        }
        if (is_left_shift) {
            if (word == 0u) {
                if (!debug_inline_integer_expression_append_constu(
                        expression, capacity, &size, 0u)) {
                    return false;
                }
            } else if (!debug_inline_integer_expression_append_frame_u32(
                           expression, capacity, &size, ARCH_X86,
                           source_frame_offset) ||
                       !debug_inline_parameter_i686_u64_append_shift_count(
                           mod, shift_count_expression, shift_count_source,
                           shift_count_frame_offset, expression, capacity,
                           &size) ||
                       !debug_inline_integer_expression_append_constu(
                           expression, capacity, &size, 32u) ||
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size, 0x1cu) || /* count-32 */
                       !debug_inline_integer_expression_append_byte(
                           expression, capacity, &size, 0x24u)) { /* DW_OP_shl */
                return false;
            }
        } else if (word == 0u) {
            if (!debug_inline_integer_expression_append_frame_u32(
                    expression, capacity, &size, ARCH_X86,
                    source_frame_offset + 4) ||
                !debug_inline_parameter_i686_u64_append_shift_count(
                    mod, shift_count_expression, shift_count_source,
                    shift_count_frame_offset, expression, capacity, &size) ||
                !debug_inline_integer_expression_append_constu(
                    expression, capacity, &size, 32u) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x1cu) || /* count-32 */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size,
                    is_unsigned ? 0x25u : 0x26u)) { /* shr/shra */
                return false;
            }
        } else if (is_unsigned) {
            if (!debug_inline_integer_expression_append_constu(
                    expression, capacity, &size, 0u)) {
                return false;
            }
        } else if (!debug_inline_integer_expression_append_frame_u32(
                       expression, capacity, &size, ARCH_X86,
                       source_frame_offset + 4) ||
                   !debug_inline_integer_expression_append_constu(
                       expression, capacity, &size, 31u) ||
                   !debug_inline_integer_expression_append_byte(
                       expression, capacity, &size, 0x26u)) { /* DW_OP_shra */
            return false;
        }
        if (!debug_inline_integer_expression_append_branch(
                expression, capacity, &size, 0x2fu,
                &high_count_path_end_operand)) { /* DW_OP_skip */
            return false;
        }
        zero_count_path_offset = size;
        if (!debug_inline_integer_expression_patch_branch(
                expression, size, zero_count_branch_operand,
                zero_count_path_offset) ||
            !debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, source_word_offset) ||
            !debug_inline_integer_expression_patch_branch(
                expression, size, low_count_path_end_operand, size) ||
            !debug_inline_integer_expression_patch_branch(
                expression, size, high_count_path_end_operand, size) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x9fu) || /* stack_value */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x93u) || /* DW_OP_piece */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 4u)) {
            return false;
        }
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_integer_expression_append_raw(
    uint8_t* expression, size_t capacity, size_t* size,
    const uint8_t* source, size_t source_size) {
    if (!expression || !size || (!source && source_size != 0u) ||
        *size > capacity || source_size > capacity - *size) {
        return false;
    }
    if (source_size != 0u) {
        memcpy(expression + *size, source, source_size);
        *size += source_size;
    }
    return true;
}

static bool debug_inline_parameter_i686_u64_division_input_chunk(
    int64_t frame_offset, unsigned int bit_offset, unsigned int bit_width,
    uint8_t* expression, size_t capacity, size_t* expression_size) {
    unsigned int word_index;
    unsigned int word_bit_offset;
    unsigned int low_width;
    uint64_t low_mask;
    if (!expression || !expression_size || bit_offset >= 64u ||
        bit_width == 0u || bit_width > 16u ||
        bit_width > 64u - bit_offset) {
        return false;
    }
    word_index = bit_offset / 32u;
    word_bit_offset = bit_offset % 32u;
    low_width = bit_width;
    if (low_width > 32u - word_bit_offset) {
        low_width = 32u - word_bit_offset;
    }
    low_mask = (UINT64_C(1) << low_width) - 1u;
    if (!debug_inline_integer_expression_append_frame_u32(
            expression, capacity, expression_size, ARCH_X86,
            frame_offset + (int64_t)(word_index * 4u)) ||
        (word_bit_offset != 0u &&
         (!debug_inline_integer_expression_append_constu(
              expression, capacity, expression_size, word_bit_offset) ||
          !debug_inline_integer_expression_append_byte(
              expression, capacity, expression_size, 0x25u))) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, expression_size, low_mask) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, expression_size, 0x1au)) {
        return false;
    }
    if (low_width < bit_width) {
        unsigned int high_width = bit_width - low_width;
        uint64_t high_mask = (UINT64_C(1) << high_width) - 1u;
        if (word_index != 0u ||
            !debug_inline_integer_expression_append_frame_u32(
                expression, capacity, expression_size, ARCH_X86,
                frame_offset + 4) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, expression_size, high_mask) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, expression_size, 0x1au) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, expression_size, low_width) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, expression_size, 0x24u) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, expression_size, 0x21u)) {
            return false;
        }
    }
    return true;
}

static bool debug_inline_parameter_i686_u64_quotient_piece(
    unsigned int chunk_width, unsigned int chunk_count, unsigned int word,
    uint8_t* expression, size_t capacity, size_t* expression_size) {
    unsigned int word_start = word * 32u;
    unsigned int word_end = word_start + 32u;
    size_t size;
    bool has_value = false;
    if (chunk_width == 0u || chunk_width > 16u || chunk_count == 0u ||
        chunk_count > 64u || word > 1u || !expression ||
        !expression_size) {
        return false;
    }
    size = *expression_size;
    for (unsigned int remaining = chunk_count; remaining > 0u; --remaining) {
        unsigned int quotient_index = remaining - 1u;
        unsigned int digit_index = chunk_count - 1u - quotient_index;
        unsigned int digit_start = digit_index * chunk_width;
        unsigned int digit_end = digit_start + chunk_width;
        unsigned int fragment_start;
        unsigned int fragment_end;
        unsigned int fragment_width;
        unsigned int source_shift;
        unsigned int destination_shift;
        uint64_t mask;
        if (digit_end > 64u) digit_end = 64u;
        fragment_start = digit_start > word_start ? digit_start : word_start;
        fragment_end = digit_end < word_end ? digit_end : word_end;
        if (fragment_start >= fragment_end) continue;
        fragment_width = fragment_end - fragment_start;
        source_shift = fragment_start - digit_start;
        destination_shift = fragment_start - word_start;
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x15u) || /* DW_OP_pick */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size,
                (uint8_t)(digit_index + (has_value ? 1u : 0u)))) {
            return false;
        }
        if (source_shift != 0u &&
            (!debug_inline_integer_expression_append_constu(
                 expression, capacity, &size, source_shift) ||
             !debug_inline_integer_expression_append_byte(
                 expression, capacity, &size, 0x25u))) { /* shr */
            return false;
        }
        mask = fragment_width == 32u
            ? UINT32_MAX : (UINT64_C(1) << fragment_width) - 1u;
        if (fragment_width != 32u &&
            (!debug_inline_integer_expression_append_constu(
                 expression, capacity, &size, mask) ||
             !debug_inline_integer_expression_append_byte(
                 expression, capacity, &size, 0x1au))) { /* and */
            return false;
        }
        if (destination_shift != 0u &&
            (!debug_inline_integer_expression_append_constu(
                 expression, capacity, &size, destination_shift) ||
             !debug_inline_integer_expression_append_byte(
                 expression, capacity, &size, 0x24u))) { /* shl */
            return false;
        }
        if (has_value &&
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x21u)) { /* or */
            return false;
        }
        has_value = true;
    }
    if (!has_value ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_division_digit_large_divisor(
    uint32_t divisor, uint8_t* expression, size_t capacity, size_t* size) {
    size_t high_bit_branch_operand;
    size_t at_least_divisor_branch_operand;
    size_t zero_digit_end_operand;
    size_t subtract_path_offset;
    size_t done_offset;
    if (divisor <= UINT32_C(1073741824) || divisor > INT32_MAX ||
        !expression || !size) {
        return false;
    }

    /* A one-bit radix makes each quotient digit zero or one. A set sign bit
       proves the partial dividend is greater than this positive divisor;
       otherwise signed DW_OP_ge is an exact unsigned comparison. */
    if (!debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x12u) || /* dup partial */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, UINT32_C(0x80000000)) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1au) || /* and */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 0u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x2eu) || /* ne */
        !debug_inline_integer_expression_append_branch(
            expression, capacity, size, 0x28u,
            &high_bit_branch_operand) || /* bra */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x12u) || /* dup partial */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, divisor) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x2au) || /* signed ge */
        !debug_inline_integer_expression_append_branch(
            expression, capacity, size, 0x28u,
            &at_least_divisor_branch_operand) || /* bra */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 0u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) || /* [0, partial] */
        !debug_inline_integer_expression_append_branch(
            expression, capacity, size, 0x2fu,
            &zero_digit_end_operand)) { /* skip */
        return false;
    }

    subtract_path_offset = *size;
    if (!debug_inline_integer_expression_patch_branch(
            expression, *size, high_bit_branch_operand,
            subtract_path_offset) ||
        !debug_inline_integer_expression_patch_branch(
            expression, *size, at_least_divisor_branch_operand,
            subtract_path_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) || /* [1, partial] */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, divisor) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1cu)) { /* minus */
        return false;
    }

    done_offset = *size;
    return debug_inline_integer_expression_patch_branch(
        expression, *size, zero_digit_end_operand, done_offset);
}

static bool debug_inline_parameter_i686_u64_division_digit_u32_overflow(
    uint32_t divisor, uint8_t* expression, size_t capacity, size_t* size) {
    size_t carry_branch_operand;
    size_t compare_branch_operand;
    size_t zero_digit_end_operand;
    size_t subtract_path_offset;
    size_t done_offset;
    if (divisor <= INT32_MAX || !expression || !size) return false;

    /* Input stack: [quotient digits..., carry from rem*2, partial rem]. The
       carry means the 33-bit partial dividend is necessarily >= divisor.
       Otherwise xor-bias both 32-bit values so signed DW_OP_ge performs an
       exact unsigned comparison. */
    if (!debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x15u) || /* pick carry */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_branch(
            expression, capacity, size, 0x28u,
            &carry_branch_operand) || /* bra */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x12u) || /* dup partial */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, UINT32_C(0x80000000)) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x27u) || /* xor sign bias */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, divisor) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, UINT32_C(0x80000000)) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x27u) || /* xor sign bias */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x2au) || /* signed ge */
        !debug_inline_integer_expression_append_branch(
            expression, capacity, size, 0x28u,
            &compare_branch_operand) || /* bra */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) || /* swap */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x13u) || /* drop carry */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 0u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) || /* append zero digit */
        !debug_inline_integer_expression_append_branch(
            expression, capacity, size, 0x2fu,
            &zero_digit_end_operand)) { /* skip */
        return false;
    }

    subtract_path_offset = *size;
    if (!debug_inline_integer_expression_patch_branch(
            expression, *size, carry_branch_operand, subtract_path_offset) ||
        !debug_inline_integer_expression_patch_branch(
            expression, *size, compare_branch_operand, subtract_path_offset) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) || /* swap */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x13u) || /* drop carry */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, 1u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x16u) || /* [quotient, 1, partial] */
        !debug_inline_integer_expression_append_constu(
            expression, capacity, size, divisor) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, size, 0x1cu)) { /* minus */
        return false;
    }

    done_offset = *size;
    return debug_inline_integer_expression_patch_branch(
        expression, *size, zero_digit_end_operand, done_offset);
}

static bool debug_inline_parameter_i686_u64_power_of_two_div_expression(
    int64_t frame_offset, uint64_t divisor, bool is_remainder,
    uint8_t* expression, size_t capacity, size_t* expression_size) {
    unsigned int shift = 0u;
    size_t size = 0u;
    uint32_t high_mask;
    if (!expression || !expression_size || divisor <= UINT32_MAX ||
        (divisor & (divisor - 1u)) != 0u || frame_offset > INT64_MAX - 4) {
        return false;
    }
    while ((divisor >> shift) != 1u) {
        ++shift;
        if (shift >= 64u) return false;
    }
    if (shift < 32u) return false;

    if (is_remainder) {
        if (!debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, frame_offset)) {
            return false;
        }
        high_mask = shift == 32u
            ? 0u : (UINT32_C(1) << (shift - 32u)) - 1u;
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x9fu) || /* stack_value */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x93u) || /* piece */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 4u) ||
            !debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, frame_offset + 4)) {
            return false;
        }
        if (!debug_inline_integer_expression_append_constu(
                expression, capacity, &size, high_mask) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x1au)) { /* and */
            return false;
        }
    } else {
        if (!debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86, frame_offset + 4)) {
            return false;
        }
        if (shift != 32u &&
            (!debug_inline_integer_expression_append_constu(
                 expression, capacity, &size, shift - 32u) ||
             !debug_inline_integer_expression_append_byte(
                 expression, capacity, &size, 0x25u))) { /* shr */
            return false;
        }
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x9fu) || /* stack_value */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x93u) || /* piece */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 4u) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, 0u)) {
            return false;
        }
    }
    if (!debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_div_constant_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Expr* dividend_expression;
    const Decl* dividend;
    int64_t frame_offset;
    uint64_t divisor;
    size_t size = 0u;
    uint64_t radix;
    unsigned int chunk_width;
    unsigned int chunk_count;
    bool is_remainder;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || parameter_type->size != 8 ||
        !parameter_type->is_unsigned ||
        (source_expression->kind != EXPR_DIV &&
         source_expression->kind != EXPR_MOD) ||
        !source_expression->binary_lhs || !source_expression->binary_rhs ||
        source_expression->binary_lhs->kind != EXPR_IDENT ||
        !source_expression->binary_lhs->ident_decl ||
        !debug_inline_integer_literal_bits(
            source_expression->binary_rhs, &divisor, 0u) ||
        divisor == 0u ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->binary_lhs->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->binary_rhs->type, parameter_type, 0u)) {
        return false;
    }
    dividend_expression = source_expression->binary_lhs;
    dividend = dividend_expression->ident_decl;
    if ((dividend->kind != DECL_VAR && dividend->kind != DECL_PARAM) ||
        dividend->var_is_global || dividend->var_is_static_local ||
        dividend->var_is_thread_local || dividend->var_is_vla ||
        !dividend->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            dividend->type, parameter_type, 0u) ||
        !debug_inline_parameter_frame_offset(mod, dividend, &frame_offset) ||
        frame_offset > INT64_MAX - 6) {
        return false;
    }
    is_remainder = source_expression->kind == EXPR_MOD;
    if (divisor > (uint64_t)UINT32_MAX) {
        return debug_inline_parameter_i686_u64_power_of_two_div_expression(
            frame_offset, divisor, is_remainder, expression, capacity,
            expression_size);
    }
    bool large_divisor = divisor > UINT64_C(1073741824);
    chunk_width = 16u;
    while (chunk_width > 1u && divisor >
           ((uint64_t)INT32_MAX + 1u) / (UINT64_C(1) << chunk_width)) {
        --chunk_width;
    }
    if (large_divisor) {
        chunk_width = 1u;
    } else if (divisor > ((uint64_t)INT32_MAX + 1u) / 2u) {
        return false;
    }
    radix = UINT64_C(1) << chunk_width;
    chunk_count = (64u + chunk_width - 1u) / chunk_width;

    /* Keep each quotient digit and the running remainder on the DWARF stack.
       The largest radix satisfying divisor*radix-1 <= INT32_MAX makes every
       signed DW_OP_div/mod exact. For divisors above INT32_MAX, one-bit digits
       carry the 33rd partial-dividend bit separately and use biased unsigned
       comparison. This avoids exponential expression growth. */
    if (!debug_inline_integer_expression_append_constu(
            expression, capacity, &size, 0u)) {
        return false;
    }
    for (unsigned int chunk_index = 0u;
         chunk_index < chunk_count; ++chunk_index) {
        uint8_t chunk_expression[64];
        size_t chunk_size = 0u;
        unsigned int bit_offset =
            (chunk_count - 1u - chunk_index) * chunk_width;
        unsigned int bit_width = 64u - bit_offset;
        if (bit_width > chunk_width) bit_width = chunk_width;
        if (!debug_inline_parameter_i686_u64_division_input_chunk(
                frame_offset, bit_offset, bit_width,
                chunk_expression, sizeof(chunk_expression), &chunk_size)) {
            return false;
        }
        if (divisor > (uint64_t)INT32_MAX) {
            if (!debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x12u) || /* dup rem */
                !debug_inline_integer_expression_append_constu(
                    expression, capacity, &size, UINT32_C(0x80000000)) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x1au) || /* and */
                !debug_inline_integer_expression_append_constu(
                    expression, capacity, &size, 0u) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x2eu) || /* ne */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x16u) || /* [q,carry,rem] */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x12u) || /* dup rem */
                !debug_inline_integer_expression_append_constu(
                    expression, capacity, &size, radix) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x1eu) || /* mul */
                !debug_inline_integer_expression_append_raw(
                    expression, capacity, &size,
                    chunk_expression, chunk_size) ||
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x22u) || /* plus */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x16u) || /* swap */
                !debug_inline_integer_expression_append_byte(
                    expression, capacity, &size, 0x13u) || /* drop rem */
                !debug_inline_parameter_i686_u64_division_digit_u32_overflow(
                    (uint32_t)divisor, expression, capacity, &size)) {
                return false;
            }
        } else if (!debug_inline_integer_expression_append_byte(
                       expression, capacity, &size, 0x12u) || /* dup rem */
                   !debug_inline_integer_expression_append_constu(
                       expression, capacity, &size, radix) ||
                   !debug_inline_integer_expression_append_byte(
                       expression, capacity, &size, 0x1eu) || /* mul */
                   !debug_inline_integer_expression_append_raw(
                       expression, capacity, &size,
                       chunk_expression, chunk_size) ||
                   !debug_inline_integer_expression_append_byte(
                       expression, capacity, &size, 0x22u) || /* plus */
                   !debug_inline_integer_expression_append_byte(
                       expression, capacity, &size, 0x16u) || /* swap */
                   !debug_inline_integer_expression_append_byte(
                       expression, capacity, &size, 0x13u) || /* drop old rem */
                   (large_divisor
                       ? !debug_inline_parameter_i686_u64_division_digit_large_divisor(
                             (uint32_t)divisor, expression, capacity, &size)
                       : (!debug_inline_integer_expression_append_byte(
                              expression, capacity, &size, 0x12u) ||
                          !debug_inline_integer_expression_append_constu(
                              expression, capacity, &size, divisor) ||
                          !debug_inline_integer_expression_append_byte(
                              expression, capacity, &size, 0x1bu) || /* div */
                          !debug_inline_integer_expression_append_byte(
                              expression, capacity, &size, 0x16u) || /* swap */
                          !debug_inline_integer_expression_append_constu(
                              expression, capacity, &size, divisor) ||
                          !debug_inline_integer_expression_append_byte(
                              expression, capacity, &size, 0x1du)))) { /* mod */
            return false;
        }
    }

    if (is_remainder) {
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x9fu) || /* stack_value */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x93u) || /* piece */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 4u) ||
            !debug_inline_integer_expression_append_constu(
                expression, capacity, &size, 0u) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x9fu) || /* stack_value */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x93u) || /* piece */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 4u)) {
            return false;
        }
    } else {
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x13u)) { /* drop remainder */
            return false;
        }
        for (unsigned int word = 0u; word < 2u; ++word) {
            if (!debug_inline_parameter_i686_u64_quotient_piece(
                    chunk_width, chunk_count, word, expression,
                    capacity, &size)) {
                return false;
            }
        }
    }
    for (unsigned int digit = 0u; digit < chunk_count; ++digit) {
        if (!debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x13u)) { /* drop quotient */
            return false;
        }
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_bitnot_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Decl* source;
    int64_t frame_offset;
    size_t size = 0u;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || source_expression->kind != EXPR_BITNOT ||
        !source_expression->unary_operand ||
        parameter_type->size != 8u ||
        source_expression->unary_operand->kind != EXPR_IDENT ||
        !source_expression->unary_operand->ident_decl) {
        return false;
    }
    source = source_expression->unary_operand->ident_decl;
    if (!source || (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_thread_local || source->var_is_vla || !source->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->unary_operand->type, parameter_type, 0u) ||
        !debug_inline_parameter_frame_offset(mod, source, &frame_offset) ||
        frame_offset > INT64_MAX - 4) {
        return false;
    }
    for (unsigned int word = 0u; word < 2u; ++word) {
        if (!debug_inline_integer_expression_append_frame_u32(
                expression, capacity, &size, ARCH_X86,
                frame_offset + (int64_t)(word * 4u)) ||
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x20u) || /* DW_OP_not */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x9fu) || /* stack_value */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 0x93u) || /* DW_OP_piece */
            !debug_inline_integer_expression_append_byte(
                expression, capacity, &size, 4u)) {
            return false;
        }
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_i686_u64_negate_expression(
    const Module* mod, const Type* parameter_type,
    const Expr* source_expression, uint8_t* expression, size_t capacity,
    size_t* expression_size) {
    const Decl* source;
    int64_t frame_offset;
    size_t size = 0u;
    if (!mod || !parameter_type || !source_expression || !expression ||
        !expression_size || source_expression->kind != EXPR_NEG ||
        !source_expression->unary_operand ||
        parameter_type->size != 8u ||
        source_expression->unary_operand->kind != EXPR_IDENT ||
        !source_expression->unary_operand->ident_decl) {
        return false;
    }
    source = source_expression->unary_operand->ident_decl;
    if (!source || (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        source->var_is_global || source->var_is_static_local ||
        source->var_is_thread_local || source->var_is_vla || !source->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source->type, parameter_type, 0u) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            source_expression->unary_operand->type, parameter_type, 0u) ||
        !debug_inline_parameter_frame_offset(mod, source, &frame_offset) ||
        frame_offset > INT64_MAX - 4) {
        return false;
    }

    /* Negate each storage word modulo 2^32; a nonzero low word borrows one
       from the high word. DW_OP_ne produces the exact 0/1 borrow flag. */
    if (!debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, 0u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x16u) || /* swap */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1cu) || /* 0 - low */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u) ||
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset + 4) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, 0u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x16u) || /* swap */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1cu) || /* 0 - high */
        !debug_inline_integer_expression_append_frame_u32(
            expression, capacity, &size, ARCH_X86, frame_offset) ||
        !debug_inline_integer_expression_append_constu(
            expression, capacity, &size, 0u) ||
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x2eu) || /* low != 0 */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x1cu) || /* subtract borrow */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x9fu) || /* stack_value */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 0x93u) || /* DW_OP_piece */
        !debug_inline_integer_expression_append_byte(
            expression, capacity, &size, 4u)) {
        return false;
    }
    *expression_size = size;
    return true;
}

static bool debug_inline_parameter_integer_expression(
    const Module* mod, int architecture, const Decl* callee,
    const ModuleDebugInlineParameterValue* value, const ExprList* argument,
    uint8_t* expression, size_t expression_capacity,
    size_t* expression_size) {
    const Type* parameter_type;
    const Expr* source_expression;
    size_t size = 0u;
    if (!mod || !callee || !value || !value->declaration || !argument ||
        !argument->expr || argument->cxx_temporary_owner || !expression ||
        !expression_size ||
        (argument->expr->kind != EXPR_IDENT &&
         argument->expr->kind != EXPR_ADD &&
         argument->expr->kind != EXPR_SUB &&
         argument->expr->kind != EXPR_MUL &&
         argument->expr->kind != EXPR_DIV &&
         argument->expr->kind != EXPR_MOD &&
         argument->expr->kind != EXPR_BITAND &&
         argument->expr->kind != EXPR_BITOR &&
         argument->expr->kind != EXPR_BITXOR &&
         argument->expr->kind != EXPR_LSHIFT &&
         argument->expr->kind != EXPR_RSHIFT &&
         argument->expr->kind != EXPR_EQ &&
         argument->expr->kind != EXPR_NE &&
         argument->expr->kind != EXPR_LT &&
         argument->expr->kind != EXPR_GT &&
         argument->expr->kind != EXPR_LE &&
         argument->expr->kind != EXPR_GE &&
         argument->expr->kind != EXPR_POS &&
         argument->expr->kind != EXPR_NEG &&
         argument->expr->kind != EXPR_NOT &&
         argument->expr->kind != EXPR_BITNOT) ||
        !debug_inline_parameter_body_is_read_only(callee)) {
        return false;
    }
    parameter_type = value->declaration->type;
    source_expression = argument->expr;
    if (!debug_inline_integer_expression_type(parameter_type) ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            parameter_type, source_expression->type, 0u)) {
        return false;
    }

    /* Ordinary-width identifier arguments keep the existing frame-location
       path. This value-expression helper handles identifiers directly only
       for the wider-than-address case below (or as subexpressions). */
    if (source_expression->kind == EXPR_IDENT &&
        (architecture == ARCH_X64 || !parameter_type->is_unsigned ||
         (unsigned int)parameter_type->size <= 4u)) {
        return false;
    }

    if (architecture == ARCH_X86 && parameter_type->size == 8u &&
        parameter_type->is_unsigned &&
        (source_expression->kind == EXPR_DIV ||
         source_expression->kind == EXPR_MOD) &&
        debug_inline_parameter_i686_u64_div_constant_expression(
            mod, parameter_type, source_expression, expression,
            expression_capacity,
            expression_size)) {
        return true;
    }

    /* A generic DWARF stack entry is only address-sized. On i686, preserve
       an unchanged 64-bit unsigned inline argument as a memory location
       instead of loading it into that narrower stack entry. The callee body
       is read-only (checked above), so the caller's stable frame slot is the
       exact parameter value for this inline instance. */
    if (architecture == ARCH_X86 && parameter_type->is_unsigned &&
        (unsigned int)parameter_type->size > 4u &&
        source_expression->kind == EXPR_IDENT &&
        source_expression->ident_decl) {
        const Decl* source = source_expression->ident_decl;
        int64_t frame_offset;
        if ((source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
            source->var_is_global || source->var_is_static_local ||
            source->var_is_thread_local || source->var_is_vla ||
            !source->type ||
            !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
                source->type, parameter_type, 0u) ||
            !debug_inline_parameter_frame_offset(
                mod, source, &frame_offset)) {
            return false;
        }
        *expression_size = debug_build_expr_breg(
            expression, architecture, frame_offset, 0u, false);
        return *expression_size != 0u;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        (source_expression->kind == EXPR_ADD ||
         source_expression->kind == EXPR_SUB ||
         source_expression->kind == EXPR_BITAND ||
         source_expression->kind == EXPR_BITOR ||
         source_expression->kind == EXPR_BITXOR) &&
        debug_inline_parameter_i686_u64_variables_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        source_expression->kind == EXPR_ADD &&
        debug_inline_parameter_i686_u64_add_constant_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        source_expression->kind == EXPR_MUL &&
        debug_inline_parameter_i686_u64_mul_variables_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        source_expression->kind == EXPR_MUL &&
        debug_inline_parameter_i686_u64_mul_constant_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        (source_expression->kind == EXPR_BITAND ||
         source_expression->kind == EXPR_BITOR ||
         source_expression->kind == EXPR_BITXOR) &&
        debug_inline_parameter_i686_u64_bitwise_constant_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        source_expression->kind == EXPR_SUB &&
        debug_inline_parameter_i686_u64_sub_constant_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        (source_expression->kind == EXPR_LSHIFT ||
         source_expression->kind == EXPR_RSHIFT) &&
        debug_inline_parameter_i686_u64_shift_constant_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        (source_expression->kind == EXPR_LSHIFT ||
         source_expression->kind == EXPR_RSHIFT)) {
        if (debug_inline_parameter_i686_u64_shift_variable_expression(
                mod, parameter_type, source_expression, expression,
                DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
                expression_size)) {
            return true;
        }
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        source_expression->kind == EXPR_BITNOT &&
        debug_inline_parameter_i686_u64_bitnot_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (architecture == ARCH_X86 &&
        (unsigned int)parameter_type->size == 8u &&
        source_expression->kind == EXPR_NEG &&
        debug_inline_parameter_i686_u64_negate_expression(
            mod, parameter_type, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            expression_size)) {
        return true;
    }

    if (!debug_build_inline_integer_value_expression(
            mod, architecture, source_expression, expression,
            DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY, &size, 0u) ||
        !debug_inline_integer_expression_append_byte(
            expression, DEBUG_INLINE_INTEGER_VALUE_EXPRESSION_CAPACITY,
            &size, 0x9fu)) { /* DW_OP_stack_value */
        return false;
    }
    *expression_size = size;
    return true;
}

typedef struct DebugInlineDynamicPointerIndexTerm {
    int64_t frame_offset;
    int element_size;
    uint8_t value_size;
    uint8_t unsigned_conversion_width; /* Modulo width for signed-to-unsigned
                                          conversions. */
    uint8_t cast_step_count;
    uint8_t cast_steps[16]; /* Width in bytes; bit 7 means sign-extend. */
    bool is_unsigned;
    bool subtract;
    bool is_constant;
    uint64_t constant_value;
    uint64_t index_multiplier;
    int64_t signed_index_multiplier;
    int64_t signed_index_divisor;
} DebugInlineDynamicPointerIndexTerm;

enum { DEBUG_INLINE_INTEGER_INDEX_CAST_LIMIT = 16 };

typedef struct DebugInlineDynamicPointerIndexGroup {
    uint8_t first_term;
    uint8_t term_count;
    uint8_t value_size;
    int element_size;
    bool subtract;
} DebugInlineDynamicPointerIndexGroup;

enum {
    DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT = 32,
    DEBUG_INLINE_POINTER_VALUE_EXPRESSION_CAPACITY = 512
};

typedef struct DebugInlineDynamicPointerIndex {
    DebugInlineDynamicPointerIndexTerm
        terms[DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT];
    DebugInlineDynamicPointerIndexGroup
        groups[DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT];
    uint8_t term_count;
    uint8_t group_count;
    bool is_dynamic;
} DebugInlineDynamicPointerIndex;

static bool debug_inline_frame_integer_index_source(
    const Expr* expression, const Decl** declaration,
    const Type** value_type, uint8_t address_size,
    uint8_t* unsigned_conversion_width) {
    const Expr* index = expression;
    const Type* converted_type;
    const Type* narrowest_type = NULL;
    const Decl* source;
    uint8_t narrowest_width = 0u;
    uint8_t conversion_width = 0u;
    unsigned int outer_cast_count = 0u;
    bool outer_cast_chain_compatible = true;
    if (!index || !declaration || !value_type || !index->type ||
        !unsigned_conversion_width || address_size == 0u ||
        address_size > 8u) {
        return false;
    }
    *unsigned_conversion_width = 0u;
    converted_type = index->type;
    if (!debug_inline_integer_constant_type(converted_type) ||
        converted_type->size <= 0 || converted_type->size > 8 ||
        converted_type->is_volatile || converted_type->is_reference ||
        converted_type->is_atomic) {
        return false;
    }
    while (index->kind == EXPR_CAST && index->cast_expr) {
        const Type* cast_source = index->cast_expr->type;
        /* A signed source widened to unsigned needs a destination-width mask
         * below address width; at address width, sign extension is already
         * the required modulo conversion. */
        if (!cast_source ||
            !debug_inline_integer_constant_type(cast_source) ||
            cast_source->size <= 0 ||
            cast_source->size > 8 ||
            (cast_source->size < converted_type->size &&
             cast_source->is_unsigned != converted_type->is_unsigned &&
             !((cast_source->is_unsigned &&
                !converted_type->is_unsigned) ||
               (!cast_source->is_unsigned && converted_type->is_unsigned &&
                converted_type->size <= (int)address_size))) ||
            cast_source->is_volatile || cast_source->is_reference ||
            cast_source->is_atomic) {
            return false;
        }
        if (!cast_source->is_unsigned && converted_type->is_unsigned &&
            cast_source->size < converted_type->size &&
            converted_type->size < (int)address_size) {
            if (conversion_width != 0u &&
                conversion_width != (uint8_t)converted_type->size) {
                return false;
            }
            conversion_width = (uint8_t)converted_type->size;
        }
        if (cast_source->size > converted_type->size) {
            if (narrowest_width == 0u ||
                converted_type->size < (int)narrowest_width) {
                narrowest_width = (uint8_t)converted_type->size;
                narrowest_type = converted_type;
            }
        } else if (narrowest_width == 0u) {
            ++outer_cast_count;
            if (cast_source->size == converted_type->size) {
                if (cast_source->is_unsigned !=
                    converted_type->is_unsigned) {
                    outer_cast_chain_compatible = false;
                }
            } else if (cast_source->is_unsigned !=
                           converted_type->is_unsigned &&
                       !cast_source->is_unsigned &&
                       (!converted_type->is_unsigned ||
                        converted_type->size > (int)address_size)) {
                outer_cast_chain_compatible = false;
            }
        }
        converted_type = cast_source;
        index = index->cast_expr;
    }
    if (index->kind != EXPR_IDENT || !index->ident_decl) return false;
    source = index->ident_decl;
    if ((source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        !source->type ||
        !debug_inline_integer_constant_type(source->type) ||
        source->type->size != converted_type->size ||
        source->type->is_unsigned != converted_type->is_unsigned ||
        source->type->is_volatile || source->type->is_reference ||
        source->type->is_atomic) {
        return false;
    }
    *declaration = source;
    if (narrowest_width != 0u) {
        /* All supported targets are little-endian x86.  If the final result
         * is no wider than the narrowest cast, its bits are the low bytes of
         * the original slot regardless of earlier integer conversions. For
         * wider results, use that narrowest type when every outer cast is a
         * supported value-preserving extension. */
        if (expression->type->size <= (int)narrowest_width) {
            *value_type = expression->type;
            return true;
        }
        if (!narrowest_type ||
            source->type->size < narrowest_type->size ||
            outer_cast_count == 0u ||
            !outer_cast_chain_compatible) {
            return false;
        }
        if (conversion_width != 0u) {
            bool expression_can_represent_unsigned_conversion =
                expression->type->is_unsigned ||
                expression->type->size > (int)conversion_width;
            if (narrowest_type->is_unsigned ||
                !expression_can_represent_unsigned_conversion ||
                expression->type->size < (int)conversion_width ||
                conversion_width >= address_size) {
                return false;
            }
            *unsigned_conversion_width = conversion_width;
        } else {
            bool compatible_extension =
                (narrowest_type->is_unsigned &&
                 !expression->type->is_unsigned &&
                 expression->type->size > narrowest_type->size) ||
                (!narrowest_type->is_unsigned &&
                 expression->type->is_unsigned &&
                 expression->type->size == (int)address_size);
            if (expression->type->is_unsigned !=
                    narrowest_type->is_unsigned &&
                !compatible_extension) {
                return false;
            }
        }
        *value_type = narrowest_type;
        return true;
    }
    if (conversion_width != 0u) {
        if (!expression->type->is_unsigned ||
            expression->type->size != (int)conversion_width) {
            return false;
        }
        *value_type = source->type;
        *unsigned_conversion_width = conversion_width;
        return true;
    }
    /* Equal-width signed/unsigned conversions preserve the stored bits; use
     * the final expression type to choose extension when moving to the
     * target-address width.  For widening conversions, retain the source
     * type so its value is sign- or zero-extended before pointer scaling. */
    *value_type = source->type->size == expression->type->size
        ? expression->type : source->type;
    return true;
}

static bool debug_inline_frame_integer_index_cast_chain(
    const Expr* expression, const Decl** declaration,
    const Type** source_type, uint8_t address_size,
    uint8_t cast_steps[DEBUG_INLINE_INTEGER_INDEX_CAST_LIMIT],
    uint8_t* cast_step_count) {
    const Expr* index = expression;
    const Type* cast_targets[DEBUG_INLINE_INTEGER_INDEX_CAST_LIMIT];
    const Type* current_type;
    const Decl* source;
    uint8_t cast_count = 0u;
    uint8_t step_count = 0u;
    if (!index || !declaration || !source_type || !cast_steps ||
        !cast_step_count || address_size == 0u || address_size > 8u ||
        !expression->type ||
        !debug_inline_integer_constant_type(expression->type) ||
        expression->type->size <= 0 ||
        expression->type->size > (int)address_size ||
        expression->type->is_volatile || expression->type->is_reference ||
        expression->type->is_atomic) {
        return false;
    }
    *cast_step_count = 0u;
    while (index->kind == EXPR_CAST && index->cast_expr) {
        if (cast_count >= DEBUG_INLINE_INTEGER_INDEX_CAST_LIMIT ||
            !index->type ||
            !debug_inline_integer_constant_type(index->type) ||
            index->type->size <= 0 ||
            index->type->size > (int)address_size ||
            index->type->is_volatile || index->type->is_reference ||
            index->type->is_atomic) {
            return false;
        }
        cast_targets[cast_count++] = index->type;
        index = index->cast_expr;
    }
    if (cast_count == 0u || index->kind != EXPR_IDENT ||
        !index->ident_decl) {
        return false;
    }
    source = index->ident_decl;
    current_type = index->type;
    if ((source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        !current_type ||
        !debug_inline_integer_constant_type(current_type) ||
        current_type->size <= 0 ||
        current_type->size > (int)address_size ||
        current_type->is_volatile || current_type->is_reference ||
        current_type->is_atomic || !source->type ||
        !debug_inline_integer_constant_type(source->type) ||
        source->type->size != current_type->size ||
        source->type->is_unsigned != current_type->is_unsigned ||
        source->type->is_volatile || source->type->is_reference ||
        source->type->is_atomic) {
        return false;
    }
    for (uint8_t target_index = cast_count; target_index > 0u;
         --target_index) {
        const Type* target = cast_targets[target_index - 1u];
        if (target->size < current_type->size ||
            (target->size == current_type->size &&
             target->is_unsigned != current_type->is_unsigned)) {
            if (target->size < (int)address_size) {
                if (step_count >= DEBUG_INLINE_INTEGER_INDEX_CAST_LIMIT) {
                    return false;
                }
                cast_steps[step_count++] = target->is_unsigned
                    ? (uint8_t)target->size
                    : (uint8_t)(0x80u | (uint8_t)target->size);
            }
        } else if (target->size > current_type->size &&
                   !current_type->is_unsigned && target->is_unsigned &&
                   target->size < (int)address_size) {
            if (step_count >= DEBUG_INLINE_INTEGER_INDEX_CAST_LIMIT) {
                return false;
            }
            cast_steps[step_count++] = (uint8_t)target->size;
        }
        current_type = target;
    }
    if (current_type != expression->type) return false;
    *declaration = source;
    *source_type = source->type;
    *cast_step_count = step_count;
    return true;
}

static bool debug_inline_unsigned_index_multiplier(
    const Expr* expression, uint8_t value_size,
    const Expr** indexed_expression, uint64_t* multiplier) {
    const Type* expression_type;
    uint64_t width_mask;
    if (indexed_expression) *indexed_expression = NULL;
    if (!expression || !expression->type || value_size == 0u ||
        value_size > 8u || !indexed_expression || !multiplier) {
        return false;
    }
    expression_type = expression->type;
    if (!debug_inline_integer_constant_type(expression_type) ||
        expression_type->size != value_size || !expression_type->is_unsigned ||
        expression_type->is_volatile || expression_type->is_reference ||
        expression_type->is_atomic) {
        return false;
    }
    width_mask = value_size == 8u ? UINT64_MAX
        : (UINT64_C(1) << (value_size * 8u)) - 1u;
    if (expression->kind == EXPR_MUL && expression->binary_lhs &&
        expression->binary_rhs && expression->binary_lhs->type &&
        expression->binary_rhs->type &&
        debug_inline_integer_constant_type(expression->binary_lhs->type) &&
        debug_inline_integer_constant_type(expression->binary_rhs->type) &&
        expression->binary_lhs->type->size == value_size &&
        expression->binary_rhs->type->size == value_size &&
        expression->binary_lhs->type->is_unsigned &&
        expression->binary_rhs->type->is_unsigned &&
        !expression->binary_lhs->type->is_volatile &&
        !expression->binary_rhs->type->is_volatile &&
        !expression->binary_lhs->type->is_reference &&
        !expression->binary_rhs->type->is_reference &&
        !expression->binary_lhs->type->is_atomic &&
        !expression->binary_rhs->type->is_atomic) {
        uint64_t constant_multiplier;
        if (debug_inline_integer_constant_bits(
                expression->binary_lhs, &constant_multiplier, 0u)) {
            *indexed_expression = expression->binary_rhs;
        } else if (debug_inline_integer_constant_bits(
                       expression->binary_rhs, &constant_multiplier, 0u)) {
            *indexed_expression = expression->binary_lhs;
        } else {
            return false;
        }
        *multiplier = constant_multiplier & width_mask;
        return true;
    }
    if (expression->kind == EXPR_LSHIFT && expression->binary_lhs &&
        expression->binary_rhs && expression->binary_lhs->type &&
        expression->binary_rhs->type &&
        debug_inline_integer_constant_type(expression->binary_lhs->type) &&
        debug_inline_integer_constant_type(expression->binary_rhs->type) &&
        expression->binary_lhs->type->size == value_size &&
        expression->binary_lhs->type->is_unsigned &&
        !expression->binary_lhs->type->is_volatile &&
        !expression->binary_lhs->type->is_reference &&
        !expression->binary_lhs->type->is_atomic &&
        !expression->binary_rhs->type->is_volatile &&
        !expression->binary_rhs->type->is_reference &&
        !expression->binary_rhs->type->is_atomic) {
        int64_t shift_amount;
        if (!debug_inline_integer_constant_value(
                expression->binary_rhs, &shift_amount, 0u) ||
            shift_amount < 0 || shift_amount >= (int64_t)value_size * 8) {
            return false;
        }
        *indexed_expression = expression->binary_lhs;
        *multiplier =
            (UINT64_C(1) << (unsigned int)shift_amount) & width_mask;
        return true;
    }
    return false;
}

static bool debug_inline_signed_index_multiplier(
    const Expr* expression, uint8_t value_size,
    const Expr** indexed_expression, int64_t* multiplier) {
    const Type* expression_type;
    const Type* lhs_type;
    const Type* rhs_type;
    int64_t constant_multiplier;
    if (indexed_expression) *indexed_expression = NULL;
    if (!expression || !expression->type || value_size == 0u ||
        value_size > 8u || !indexed_expression || !multiplier) {
        return false;
    }
    expression_type = expression->type;
    if (!debug_inline_integer_constant_type(expression_type) ||
        expression_type->size != value_size || expression_type->is_unsigned ||
        expression_type->is_volatile || expression_type->is_reference ||
        expression_type->is_atomic || expression->kind != EXPR_MUL ||
        !expression->binary_lhs || !expression->binary_rhs) {
        return false;
    }
    lhs_type = expression->binary_lhs->type;
    rhs_type = expression->binary_rhs->type;
    if (!lhs_type || !rhs_type ||
        !debug_inline_integer_constant_type(lhs_type) ||
        !debug_inline_integer_constant_type(rhs_type) ||
        lhs_type->size != value_size || rhs_type->size != value_size ||
        lhs_type->is_unsigned || rhs_type->is_unsigned ||
        lhs_type->is_volatile || rhs_type->is_volatile ||
        lhs_type->is_reference || rhs_type->is_reference ||
        lhs_type->is_atomic || rhs_type->is_atomic) {
        return false;
    }
    if (debug_inline_integer_constant_value(
            expression->binary_lhs, &constant_multiplier, 0u)) {
        *indexed_expression = expression->binary_rhs;
    } else if (debug_inline_integer_constant_value(
                   expression->binary_rhs, &constant_multiplier, 0u)) {
        *indexed_expression = expression->binary_lhs;
    } else {
        return false;
    }
    *multiplier = constant_multiplier;
    return true;
}

static bool debug_inline_signed_index_divisor(
    const Expr* expression, uint8_t value_size,
    const Expr** indexed_expression, uint8_t* division_size,
    int64_t* divisor) {
    const Expr* division = expression;
    const Type* expression_type;
    const Type* division_type;
    const Type* lhs_type;
    const Type* rhs_type;
    int64_t constant_divisor;
    bool negate_quotient = false;
    if (indexed_expression) *indexed_expression = NULL;
    if (!expression || !expression->type || value_size == 0u ||
        value_size > 8u || !indexed_expression || !division_size ||
        !divisor) {
        return false;
    }
    expression_type = expression->type;
    if (!debug_inline_integer_constant_type(expression_type) ||
        expression_type->size != value_size || expression_type->is_unsigned ||
        expression_type->is_volatile || expression_type->is_reference ||
        expression_type->is_atomic) {
        return false;
    }
    while (division->kind == EXPR_CAST && division->cast_expr) {
        const Type* cast_source_type = division->cast_expr->type;
        const Type* cast_target_type = division->type;
        if (!cast_source_type || !cast_target_type ||
            !debug_inline_integer_constant_type(cast_source_type) ||
            !debug_inline_integer_constant_type(cast_target_type) ||
            cast_source_type->is_unsigned || cast_target_type->is_unsigned ||
            cast_source_type->is_volatile || cast_target_type->is_volatile ||
            cast_source_type->is_reference || cast_target_type->is_reference ||
            cast_source_type->is_atomic || cast_target_type->is_atomic ||
            cast_target_type->size < cast_source_type->size ||
            cast_target_type->size > (int)value_size) {
            return false;
        }
        division = division->cast_expr;
    }
    if (division->kind == EXPR_NEG && division->unary_operand) {
        negate_quotient = true;
        division = division->unary_operand;
    }
    division_type = division->type;
    if (division->kind == EXPR_RSHIFT && division->binary_lhs &&
        division->binary_rhs) {
        const Expr* adjusted = division->binary_lhs;
        const Expr* bias;
        const Expr* sign_mask;
        const Expr* value_source;
        const Expr* sign_source;
        int64_t shift_count;
        int64_t sign_shift;
        int64_t mask_value;
        int width;
        uint64_t magnitude;
        if (!division_type ||
            !debug_inline_integer_constant_type(division_type) ||
            division_type->is_unsigned || division_type->is_volatile ||
            division_type->is_reference || division_type->is_atomic ||
            division_type->size <= 0 || division_type->size > (int)value_size ||
            !debug_inline_integer_constant_value(
                division->binary_rhs, &shift_count, 0u) ||
            shift_count <= 0 || shift_count >= 63 ||
            adjusted->kind != EXPR_ADD || !adjusted->binary_lhs ||
            !adjusted->binary_rhs) {
            return false;
        }
        bias = adjusted->binary_rhs;
        if (bias->kind != EXPR_BITAND || !bias->binary_lhs ||
            !bias->binary_rhs ||
            !debug_inline_integer_constant_value(
                bias->binary_rhs, &mask_value, 0u)) {
            return false;
        }
        sign_mask = bias->binary_lhs;
        if (sign_mask->kind != EXPR_RSHIFT || !sign_mask->binary_lhs ||
            !sign_mask->binary_rhs ||
            !debug_inline_integer_constant_value(
                sign_mask->binary_rhs, &sign_shift, 0u)) {
            return false;
        }
        value_source = adjusted->binary_lhs;
        sign_source = sign_mask->binary_lhs;
        width = division_type->size * 8;
        if (value_source->kind != EXPR_IDENT ||
            sign_source->kind != EXPR_IDENT ||
            !value_source->ident_decl ||
            value_source->ident_decl != sign_source->ident_decl ||
            value_source->type != division_type ||
            sign_source->type != division_type || sign_shift != width - 1 ||
            mask_value != (int64_t)((UINT64_C(1) << shift_count) - 1u)) {
            return false;
        }
        magnitude = UINT64_C(1) << (unsigned int)shift_count;
        if (magnitude > (uint64_t)INT64_MAX) return false;
        *indexed_expression = value_source;
        *division_size = (uint8_t)division_type->size;
        *divisor = (int64_t)magnitude;
        if (negate_quotient) *divisor = -*divisor;
        return true;
    }
    if (division->kind != EXPR_DIV || !division->binary_lhs ||
        !division->binary_rhs) return false;
    lhs_type = division->binary_lhs->type;
    rhs_type = division->binary_rhs->type;
    if (!division_type ||
        !debug_inline_integer_constant_type(division_type) ||
        division_type->is_unsigned || division_type->is_volatile ||
        division_type->is_reference || division_type->is_atomic ||
        division_type->size <= 0 || division_type->size > (int)value_size ||
        !lhs_type || !rhs_type ||
        !debug_inline_integer_constant_type(lhs_type) ||
        !debug_inline_integer_constant_type(rhs_type) ||
        lhs_type->size != division_type->size ||
        rhs_type->size != division_type->size ||
        lhs_type->is_unsigned || rhs_type->is_unsigned ||
        lhs_type->is_volatile || rhs_type->is_volatile ||
        lhs_type->is_reference || rhs_type->is_reference ||
        lhs_type->is_atomic || rhs_type->is_atomic ||
        !debug_inline_integer_constant_value(
            division->binary_rhs, &constant_divisor, 0u) ||
        constant_divisor == 0) {
        return false;
    }
    if (negate_quotient) {
        if (constant_divisor == INT64_MIN) return false;
        constant_divisor = -constant_divisor;
    }
    *indexed_expression = division->binary_lhs;
    *division_size = (uint8_t)division_type->size;
    *divisor = constant_divisor;
    return true;
}

static bool debug_inline_collect_unsigned_computed_index_terms(
    const Module* mod, const Expr* expression, int polarity,
    uint8_t value_size, uint64_t multiplier, int element_size,
    DebugInlineDynamicPointerIndexTerm terms[
        DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT], uint8_t* term_count,
    unsigned int depth) {
    const Type* expression_type;
    const Decl* declaration;
    DebugInlineDynamicPointerIndexTerm* term;
    if (!mod || !expression || !expression->type ||
        (polarity != 1 && polarity != -1) || value_size == 0u ||
        element_size <= 0 || !terms || !term_count || depth >= 32u) {
        return false;
    }
    expression_type = expression->type;
    if (!debug_inline_integer_constant_type(expression_type) ||
        expression_type->size != value_size ||
        !expression_type->is_unsigned || expression_type->is_volatile ||
        expression_type->is_reference || expression_type->is_atomic) {
        return false;
    }
    if (expression->kind == EXPR_ADD || expression->kind == EXPR_SUB) {
        if (!expression->binary_lhs || !expression->binary_rhs) return false;
        return debug_inline_collect_unsigned_computed_index_terms(
                   mod, expression->binary_lhs, polarity, value_size,
                   multiplier,
                   element_size, terms, term_count, depth + 1u) &&
               debug_inline_collect_unsigned_computed_index_terms(
                   mod, expression->binary_rhs,
                   expression->kind == EXPR_SUB ? -polarity : polarity,
                   value_size, multiplier, element_size, terms, term_count,
                   depth + 1u);
    }
    {
        uint64_t constant_value;
        if (debug_inline_integer_constant_bits(
                expression, &constant_value, 0u)) {
            if (*term_count >=
                DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT) {
                return false;
            }
            term = &terms[*term_count];
            term->frame_offset = 0;
            term->element_size = element_size;
            term->value_size = value_size;
            term->unsigned_conversion_width = 0u;
            term->cast_step_count = 0u;
            term->is_unsigned = true;
            term->subtract = polarity < 0;
            term->is_constant = true;
            term->constant_value = constant_value * multiplier;
            if (value_size < 8u) {
                term->constant_value &=
                    (UINT64_C(1) << (value_size * 8u)) - 1u;
            }
            term->index_multiplier = 1u;
            term->signed_index_multiplier = 1;
            term->signed_index_divisor = 0;
            ++*term_count;
            return true;
        }
    }
    {
        const Expr* indexed_expression;
        uint64_t constant_multiplier;
        uint64_t width_mask = value_size == 8u
            ? UINT64_MAX
            : (UINT64_C(1) << (value_size * 8u)) - 1u;
        if (debug_inline_unsigned_index_multiplier(
                expression, value_size, &indexed_expression,
                &constant_multiplier)) {
            return debug_inline_collect_unsigned_computed_index_terms(
                mod, indexed_expression, polarity, value_size,
                (multiplier * constant_multiplier) & width_mask,
                element_size, terms, term_count, depth + 1u);
        }
    }
    if (expression->kind != EXPR_IDENT || !expression->ident_decl ||
        *term_count >= DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT) {
        return false;
    }
    declaration = expression->ident_decl;
    if ((declaration->kind != DECL_VAR &&
         declaration->kind != DECL_PARAM) || !declaration->type ||
        !debug_inline_integer_constant_type(declaration->type) ||
        declaration->type->size != value_size ||
        !declaration->type->is_unsigned || declaration->type->is_volatile ||
        declaration->type->is_reference || declaration->type->is_atomic) {
        return false;
    }
    term = &terms[*term_count];
    if (!debug_inline_parameter_frame_offset(
            mod, declaration, &term->frame_offset)) {
        return false;
    }
    term->element_size = element_size;
    term->value_size = value_size;
    term->unsigned_conversion_width = 0u;
    term->cast_step_count = 0u;
    term->is_unsigned = true;
    term->subtract = polarity < 0;
    term->is_constant = false;
    term->constant_value = 0u;
    term->index_multiplier = multiplier;
    term->signed_index_multiplier = 1;
    term->signed_index_divisor = 0;
    ++*term_count;
    return true;
}

static bool debug_inline_append_dynamic_integer_index(
    const Module* mod, const Expr* expression, int polarity,
    int element_size, uint8_t address_size,
    DebugInlineDynamicPointerIndex* dynamic_index,
    int64_t* byte_displacement, unsigned int depth) {
    const Type* expression_type;
    const Type* value_type;
    const Decl* declaration;
    int64_t element_count;
    int64_t local_displacement;
    if (!mod || !expression || !expression->type ||
        (polarity != 1 && polarity != -1) || element_size <= 0 ||
        !dynamic_index || !byte_displacement || depth >= 32u) {
        return false;
    }
    expression_type = expression->type;
    if (!debug_inline_integer_constant_type(expression_type) ||
        expression_type->size <= 0 ||
        expression_type->size > (int)address_size ||
        expression_type->is_volatile || expression_type->is_reference ||
        expression_type->is_atomic) {
        return false;
    }
    if (debug_inline_integer_constant_value(
            expression, &element_count, 0u)) {
        if (expression_type->is_unsigned && element_count < 0) {
            return false;
        }
        if (polarity < 0) {
            if (element_count == INT64_MIN) return false;
            element_count = -element_count;
        }
        if ((element_count > 0 &&
             element_count > INT64_MAX / element_size) ||
            (element_count < 0 &&
             element_count < INT64_MIN / element_size)) {
            return false;
        }
        local_displacement = element_count * element_size;
        if ((local_displacement > 0 &&
             *byte_displacement > INT64_MAX - local_displacement) ||
            (local_displacement < 0 &&
             *byte_displacement < INT64_MIN - local_displacement)) {
            return false;
        }
        *byte_displacement += local_displacement;
        return true;
    }
    {
        uint8_t unsigned_conversion_width = 0u;
        if (dynamic_index->term_count <
                DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT &&
            debug_inline_frame_integer_index_source(
                expression, &declaration, &value_type, address_size,
                &unsigned_conversion_width)) {
            DebugInlineDynamicPointerIndexTerm* term =
                &dynamic_index->terms[dynamic_index->term_count];
            if (!debug_inline_parameter_frame_offset(
                    mod, declaration, &term->frame_offset)) {
                return false;
            }
            term->element_size = element_size;
            term->value_size = (uint8_t)value_type->size;
            term->unsigned_conversion_width = unsigned_conversion_width;
            term->cast_step_count = 0u;
            term->is_unsigned = value_type->is_unsigned;
            term->subtract = polarity < 0;
            term->is_constant = false;
            term->constant_value = 0u;
            term->index_multiplier = 1u;
            term->signed_index_multiplier = 1;
            term->signed_index_divisor = 0;
            ++dynamic_index->term_count;
            dynamic_index->is_dynamic = true;
            return true;
        }
        {
            uint8_t cast_steps[DEBUG_INLINE_INTEGER_INDEX_CAST_LIMIT];
            uint8_t cast_step_count = 0u;
            if (dynamic_index->term_count <
                    DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT &&
                debug_inline_frame_integer_index_cast_chain(
                    expression, &declaration, &value_type, address_size,
                    cast_steps, &cast_step_count)) {
                DebugInlineDynamicPointerIndexTerm* term =
                    &dynamic_index->terms[dynamic_index->term_count];
                if (!debug_inline_parameter_frame_offset(
                        mod, declaration, &term->frame_offset)) {
                    return false;
                }
                term->element_size = element_size;
                term->value_size = (uint8_t)value_type->size;
                term->unsigned_conversion_width = 0u;
                term->cast_step_count = cast_step_count;
                for (uint8_t step = 0u; step < cast_step_count; ++step) {
                    term->cast_steps[step] = cast_steps[step];
                }
                term->is_unsigned = value_type->is_unsigned;
                term->subtract = polarity < 0;
                term->is_constant = false;
                term->constant_value = 0u;
                term->index_multiplier = 1u;
                term->signed_index_multiplier = 1;
                term->signed_index_divisor = 0;
                ++dynamic_index->term_count;
                dynamic_index->is_dynamic = true;
                return true;
            }
        }
    }
    {
        const Expr* indexed_expression;
        uint8_t unsigned_conversion_width = 0u;
        uint64_t multiplier = 1u;
        int64_t signed_multiplier = 1;
        int64_t signed_divisor = 0;
        uint8_t signed_division_size = 0u;
        bool has_constant_divisor = !expression_type->is_unsigned &&
            debug_inline_signed_index_divisor(
                expression, (uint8_t)expression_type->size,
                &indexed_expression, &signed_division_size,
                &signed_divisor);
        bool has_constant_multiplier = expression_type->is_unsigned
            ? debug_inline_unsigned_index_multiplier(
                  expression, (uint8_t)expression_type->size,
                  &indexed_expression, &multiplier)
            : !has_constant_divisor && debug_inline_signed_index_multiplier(
                  expression, (uint8_t)expression_type->size,
                  &indexed_expression, &signed_multiplier);
        if ((has_constant_multiplier || has_constant_divisor) &&
            debug_inline_frame_integer_index_source(
                indexed_expression, &declaration, &value_type,
                address_size, &unsigned_conversion_width) &&
            unsigned_conversion_width == 0u &&
            value_type->is_unsigned == expression_type->is_unsigned &&
            value_type->size == (has_constant_divisor
                ? signed_division_size : expression_type->size)) {
            DebugInlineDynamicPointerIndexTerm* term;
            DebugInlineDynamicPointerIndexGroup* group = NULL;
            if (dynamic_index->term_count >=
                    DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT ||
                (value_type->is_unsigned &&
                 value_type->size < (int)address_size &&
                 dynamic_index->group_count >=
                    DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT)) {
                return false;
            }
            term = &dynamic_index->terms[dynamic_index->term_count];
            if (!debug_inline_parameter_frame_offset(
                    mod, declaration, &term->frame_offset)) {
                return false;
            }
            term->element_size = element_size;
            term->value_size = (uint8_t)value_type->size;
            term->unsigned_conversion_width = 0u;
            term->cast_step_count = 0u;
            term->is_unsigned = value_type->is_unsigned;
            term->subtract = false;
            term->is_constant = false;
            term->constant_value = 0u;
            term->index_multiplier = has_constant_divisor ? 1u : multiplier;
            term->signed_index_multiplier = has_constant_divisor
                ? 1 : signed_multiplier;
            term->signed_index_divisor = has_constant_divisor
                ? signed_divisor : 0;
            if (value_type->is_unsigned &&
                value_type->size < (int)address_size) {
                group = &dynamic_index->groups[dynamic_index->group_count++];
                group->first_term = dynamic_index->term_count;
                group->term_count = 1u;
                group->value_size = (uint8_t)value_type->size;
                group->element_size = element_size;
                group->subtract = polarity < 0;
            } else {
                term->subtract = polarity < 0;
            }
            ++dynamic_index->term_count;
            dynamic_index->is_dynamic = true;
            return true;
        }
    }
    if (expression->kind == EXPR_CAST && expression->cast_expr &&
        expression->cast_expr->type && expression_type->is_unsigned &&
        expression_type->size > 0 &&
        debug_inline_integer_constant_type(expression->cast_expr->type) &&
        expression->cast_expr->type->is_unsigned &&
        expression->cast_expr->type->size > 0 &&
        expression->cast_expr->type->size <= expression_type->size &&
        !expression->cast_expr->type->is_volatile &&
        !expression->cast_expr->type->is_reference &&
        !expression->cast_expr->type->is_atomic) {
        return debug_inline_append_dynamic_integer_index(
            mod, expression->cast_expr, polarity, element_size,
            address_size, dynamic_index, byte_displacement, depth + 1u);
    }
    if (expression_type->is_unsigned &&
        expression_type->size == (int)address_size) {
        uint64_t constant_bits;
        if (debug_inline_integer_constant_bits(
                expression, &constant_bits, 0u) &&
            constant_bits > (uint64_t)INT64_MAX) {
            DebugInlineDynamicPointerIndexTerm* term;
            DebugInlineDynamicPointerIndexGroup* group;
            if (dynamic_index->term_count >=
                    DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT ||
                dynamic_index->group_count >=
                    DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT) {
                return false;
            }
            term = &dynamic_index->terms[dynamic_index->term_count];
            term->frame_offset = 0;
            term->element_size = element_size;
            term->value_size = (uint8_t)address_size;
            term->unsigned_conversion_width = 0u;
            term->cast_step_count = 0u;
            term->is_unsigned = true;
            term->subtract = false;
            term->is_constant = true;
            term->constant_value = constant_bits;
            term->index_multiplier = 1u;
            term->signed_index_multiplier = 1;
            term->signed_index_divisor = 0;
            group = &dynamic_index->groups[dynamic_index->group_count++];
            group->first_term = dynamic_index->term_count;
            group->term_count = 1u;
            group->value_size = (uint8_t)address_size;
            group->element_size = element_size;
            group->subtract = polarity < 0;
            ++dynamic_index->term_count;
            dynamic_index->is_dynamic = true;
            return true;
        }
    }
    if (expression_type->is_unsigned &&
        expression_type->size < (int)address_size &&
        (expression->kind == EXPR_ADD || expression->kind == EXPR_SUB)) {
        DebugInlineDynamicPointerIndexTerm collected_terms[
            DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT];
        uint8_t collected_count = 0u;
        DebugInlineDynamicPointerIndexGroup* group;
        if (!debug_inline_collect_unsigned_computed_index_terms(
                mod, expression, 1, (uint8_t)expression_type->size, 1u,
                element_size, collected_terms, &collected_count, 0u) ||
            collected_count < 2u ||
            dynamic_index->term_count + collected_count >
                DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT ||
            dynamic_index->group_count >=
                DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT) {
            return false;
        }
        group = &dynamic_index->groups[dynamic_index->group_count++];
        group->first_term = dynamic_index->term_count;
        group->term_count = collected_count;
        group->value_size = (uint8_t)expression_type->size;
        group->element_size = element_size;
        group->subtract = polarity < 0;
        memcpy(&dynamic_index->terms[dynamic_index->term_count],
               collected_terms,
               collected_count * sizeof(collected_terms[0]));
        dynamic_index->term_count += collected_count;
        dynamic_index->is_dynamic = true;
        return true;
    }
    if ((expression->kind != EXPR_ADD &&
         expression->kind != EXPR_SUB) ||
        (expression_type->is_unsigned &&
         expression_type->size != (int)address_size)) {
        return false;
    }
    return debug_inline_append_dynamic_integer_index(
               mod, expression->binary_lhs, polarity, element_size,
               address_size, dynamic_index, byte_displacement, depth + 1u) &&
           debug_inline_append_dynamic_integer_index(
               mod, expression->binary_rhs,
               expression->kind == EXPR_SUB ? -polarity : polarity,
               element_size, address_size, dynamic_index,
               byte_displacement, depth + 1u);
}

static bool debug_inline_pointer_dynamic_index_chain(
    const Module* mod, const Expr* expression,
    const Expr** pointer_expression, int64_t* byte_displacement,
    DebugInlineDynamicPointerIndex* dynamic_index, uint8_t address_size,
    unsigned int depth) {
    const Expr* pointer;
    const Expr* index;
    const Type* pointer_type;
    int64_t nested_displacement = 0;
    int64_t index_displacement = 0;
    bool subtract;
    if (!mod || !expression || !pointer_expression ||
        !byte_displacement || !dynamic_index || depth >= 32u) {
        return false;
    }
    if (expression->kind != EXPR_ADD && expression->kind != EXPR_SUB) {
        *pointer_expression = expression;
        *byte_displacement = 0;
        return true;
    }
    pointer = expression->binary_lhs;
    index = expression->binary_rhs;
    subtract = expression->kind == EXPR_SUB;
    if (!pointer || !pointer->type ||
        (pointer->type->kind != TYPE_PTR &&
         pointer->type->kind != TYPE_ARRAY)) {
        if (subtract || !index || !index->type ||
            (index->type->kind != TYPE_PTR &&
             index->type->kind != TYPE_ARRAY)) {
            return false;
        }
        pointer = expression->binary_rhs;
        index = expression->binary_lhs;
        subtract = false;
    }
    pointer_type = pointer->type;
    if (!pointer_type || pointer_type->cxx_is_member_pointer ||
        !pointer_type->base || pointer_type->base->kind == TYPE_FUNC ||
        pointer_type->base->size <= 0 || !index ||
        !debug_inline_pointer_dynamic_index_chain(
            mod, pointer, pointer_expression, &nested_displacement,
            dynamic_index, address_size, depth + 1u)) {
        return false;
    }
    if (!debug_inline_append_dynamic_integer_index(
            mod, index, subtract ? -1 : 1, pointer_type->base->size,
            address_size, dynamic_index, &index_displacement, 0u) ||
        (index_displacement > 0 &&
         nested_displacement > INT64_MAX - index_displacement) ||
        (index_displacement < 0 &&
         nested_displacement < INT64_MIN - index_displacement)) {
        return false;
    }
    *byte_displacement = nested_displacement + index_displacement;
    return true;
}

static bool debug_inline_dynamic_array_element_address_recursive(
    const Module* mod, const Expr* expression, uint8_t address_size,
    const Decl** source, int64_t* address_addend,
    int64_t* pointer_adjustment, bool* dereference_source,
    DebugInlineDynamicPointerIndex* dynamic_index, unsigned int depth) {
    const Type* array_type;
    const Type* index_base_type;
    const Type* element_type;
    const Type* index_type;
    const Decl* index_declaration;
    const Expr* array_designator;
    DebugInlineDynamicPointerIndexTerm* term;
    int64_t base_addend;
    int64_t constant_index;
    int64_t index_displacement = 0;
    int64_t index_frame_offset;
    uint8_t unsigned_conversion_width = 0u;
    bool index_is_constant;
    bool pointer_subscript = false;
    if (!mod || !expression || expression->kind != EXPR_INDEX ||
        !expression->index_base || !expression->index_expr || !source ||
        !address_addend || !pointer_adjustment || !dereference_source ||
        !dynamic_index || depth >= 32u) {
        return false;
    }
    index_base_type = expression->index_base->type;
    array_type = NULL;
    element_type = NULL;
    if (index_base_type && index_base_type->kind == TYPE_ARRAY) {
        array_type = index_base_type;
        array_designator = expression->index_base;
    } else {
        array_designator = debug_inline_static_array_designator(
            expression->index_base, &array_type);
        if (array_designator) {
            element_type = array_type ? array_type->base : NULL;
        } else if (index_base_type &&
                   index_base_type->kind == TYPE_PTR &&
                   !index_base_type->cxx_is_member_pointer &&
                   index_base_type->size == (int)address_size &&
                   index_base_type->base &&
                   index_base_type->base->kind != TYPE_FUNC &&
                   index_base_type->base->kind != TYPE_PTR &&
                   index_base_type->base->size > 0) {
            pointer_subscript = true;
            array_designator = expression->index_base;
            element_type = index_base_type->base;
        } else {
            return false;
        }
    }
    if (!pointer_subscript) {
        if (!array_type || array_type->kind != TYPE_ARRAY ||
            array_type->array_len < 0 || array_type->size <= 0 ||
            !array_type->base || array_type->base->size <= 0) {
            return false;
        }
        element_type = array_type->base;
    }
    if (!array_designator || !element_type || element_type->size <= 0 ||
        !expression->index_expr->type) {
        return false;
    }
    index_is_constant = debug_inline_integer_constant_value(
        expression->index_expr, &constant_index, 0u);
    if (index_is_constant) {
        if (pointer_subscript) {
            if ((constant_index > 0 &&
                 constant_index > INT64_MAX / element_type->size) ||
                (constant_index < 0 &&
                 constant_index < INT64_MIN / element_type->size)) {
                return false;
            }
        } else {
            if (constant_index < 0 ||
                constant_index >= array_type->array_len ||
                constant_index > INT64_MAX / element_type->size) {
                return false;
            }
        }
        index_displacement = constant_index * element_type->size;
    } else if (!debug_inline_frame_integer_index_source(
                   expression->index_expr, &index_declaration, &index_type,
                   address_size, &unsigned_conversion_width) ||
               index_type->size > (int)address_size ||
               dynamic_index->term_count >=
                   DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT ||
               !debug_inline_parameter_frame_offset(
                   mod, index_declaration, &index_frame_offset)) {
        return false;
    }
    if (pointer_subscript) {
        if (array_designator->kind != EXPR_IDENT ||
            !array_designator->ident_decl ||
            (array_designator->ident_decl->kind != DECL_VAR &&
             array_designator->ident_decl->kind != DECL_PARAM) ||
            !array_designator->type ||
            array_designator->type->kind != TYPE_PTR ||
            array_designator->type->cxx_is_member_pointer ||
            array_designator->type->size != (int)address_size ||
            array_designator->type->is_volatile ||
            array_designator->type->is_reference ||
            array_designator->type->is_atomic ||
            !debug_inline_address_path(
                array_designator, source, &base_addend, false, true, 0u) ||
            *source != array_designator->ident_decl || base_addend != 0) {
            return false;
        }
        *dereference_source = true;
    } else if (array_designator->kind == EXPR_INDEX) {
        if (!debug_inline_dynamic_array_element_address_recursive(
                mod, array_designator, address_size, source, &base_addend,
                pointer_adjustment, dereference_source, dynamic_index,
                depth + 1u)) return false;
    } else if (!debug_inline_address_path(
                   array_designator, source, &base_addend,
                   false, true, 0u)) {
        return false;
    }
    if (index_is_constant) {
        if (*dereference_source) {
            if ((index_displacement > 0 &&
                 *pointer_adjustment > INT64_MAX - index_displacement) ||
                (index_displacement < 0 &&
                 *pointer_adjustment < INT64_MIN - index_displacement)) {
                return false;
            }
            *pointer_adjustment += index_displacement;
        } else {
            if ((index_displacement > 0 &&
                 base_addend > INT64_MAX - index_displacement) ||
                (index_displacement < 0 &&
                 base_addend < INT64_MIN - index_displacement)) {
                return false;
            }
            base_addend += index_displacement;
        }
    } else {
        term = &dynamic_index->terms[dynamic_index->term_count];
        term->frame_offset = index_frame_offset;
        term->element_size = element_type->size;
        term->value_size = (uint8_t)index_type->size;
        term->unsigned_conversion_width = unsigned_conversion_width;
        term->cast_step_count = 0u;
        term->is_unsigned = index_type->is_unsigned;
        term->subtract = false;
        term->is_constant = false;
        term->constant_value = 0u;
        term->index_multiplier = 1u;
        term->signed_index_multiplier = 1;
        term->signed_index_divisor = 0;
        ++dynamic_index->term_count;
        dynamic_index->is_dynamic = true;
    }
    if (!dynamic_index->is_dynamic && depth == 0u &&
        !*dereference_source) return false;
    *address_addend = base_addend;
    return true;
}

static bool debug_inline_dynamic_array_element_address(
    const Module* mod, const Expr* expression, uint8_t address_size,
    const Decl** source, int64_t* address_addend,
    int64_t* pointer_adjustment, bool* dereference_source,
    DebugInlineDynamicPointerIndex* dynamic_index) {
    DebugInlineDynamicPointerIndex candidate_index;
    const Decl* candidate_source = NULL;
    int64_t candidate_addend = 0;
    int64_t candidate_pointer_adjustment;
    bool candidate_dereference_source;
    if (!source || !address_addend || !pointer_adjustment ||
        !dereference_source || !dynamic_index) {
        return false;
    }
    candidate_index = *dynamic_index;
    candidate_pointer_adjustment = *pointer_adjustment;
    candidate_dereference_source = *dereference_source;
    if (!debug_inline_dynamic_array_element_address_recursive(
            mod, expression, address_size, &candidate_source,
            &candidate_addend, &candidate_pointer_adjustment,
            &candidate_dereference_source, &candidate_index, 0u)) {
        return false;
    }
    *source = candidate_source;
    *address_addend = candidate_addend;
    *pointer_adjustment = candidate_pointer_adjustment;
    *dereference_source = candidate_dereference_source;
    *dynamic_index = candidate_index;
    return true;
}

static const ModuleSymbol* debug_inline_parameter_source_symbol(
    const Module* mod, const char* filename,
    const Decl* callee,
    const ModuleDebugInlineParameterValue* value,
    const ExprList* argument, char** scoped_symbol,
    bool* frame_based, int64_t* frame_offset) {
    const Decl* parameter;
    const Decl* source;
    const ModuleSymbol* symbol;
    const Expr* source_expression;
    Type* parameter_type;
    Type* argument_type;
    if (scoped_symbol) *scoped_symbol = NULL;
    if (frame_based) *frame_based = false;
    if (frame_offset) *frame_offset = 0;
    if (!mod || !value || !value->declaration || !argument ||
        !argument->expr || argument->cxx_temporary_owner || !scoped_symbol) {
        return NULL;
    }
    parameter = value->declaration;
    parameter_type = parameter->type;
    argument_type = argument->expr->type;
    source_expression = argument->expr;
    if (parameter_type &&
        (parameter_type->kind == TYPE_FLOAT ||
         parameter_type->kind == TYPE_DOUBLE)) {
        while (source_expression &&
               source_expression->kind == EXPR_CAST) {
            const Expr* operand = source_expression->cast_expr;
            if (!operand || !operand->type || !source_expression->type ||
                !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
                    source_expression->type, operand->type, 0u)) {
                return NULL;
            }
            source_expression = operand;
        }
    }
    if (!source_expression || source_expression->kind != EXPR_IDENT ||
        !source_expression->ident_decl) {
        return NULL;
    }
    source = source_expression->ident_decl;
    if (parameter->kind != DECL_PARAM ||
        (source->kind != DECL_VAR && source->kind != DECL_PARAM) ||
        !parameter_type || !argument_type ||
        !debug_inline_parameter_storage_type(parameter_type) ||
        !debug_inline_parameter_storage_type_equal(
            parameter_type, argument_type, 0u) ||
        !source_expression->type ||
        !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
            parameter_type, source_expression->type, 0u) ||
        parameter_type->size == 0u ||
        parameter_type->size != argument_type->size ||
        parameter_type->is_volatile || parameter_type->is_reference ||
        parameter_type->is_atomic || argument_type->is_volatile ||
        argument_type->is_reference || argument_type->is_atomic ||
        source_expression->type->is_volatile ||
        source_expression->type->is_reference ||
        source_expression->type->is_atomic) {
        return NULL;
    }
    if (!debug_inline_parameter_body_is_read_only(callee)) return NULL;
    if (source->kind == DECL_VAR && source->var_is_global) {
        symbol = debug_find_global_symbol(mod, source);
    } else if (source->kind == DECL_VAR && source->var_is_static_local) {
        symbol = debug_find_static_local_symbol(mod, source);
    } else {
        symbol = NULL;
        if (!frame_based || !frame_offset ||
            !debug_inline_parameter_frame_offset(
                mod, source, frame_offset)) return NULL;
        *frame_based = true;
        return NULL;
    }
    if (!symbol) return NULL;
    if (!symbol->is_global) {
        *scoped_symbol = module_scoped_symbol(filename, symbol->name);
        if (!*scoped_symbol) return NULL;
    }
    return symbol;
}

static bool debug_inline_parameter_frame_address(
    const Module* mod, const Decl* callee,
    const ModuleDebugInlineParameterValue* value,
    const ExprList* argument, int64_t* frame_offset,
    int64_t* address_addend, int64_t* pointer_adjustment,
    bool* dereference_source,
    DebugInlineDynamicPointerIndex* dynamic_index) {
    const Expr* expression;
    const Expr* address_expression;
    const Expr* operand;
    const Expr* pointer_expression = NULL;
    const Decl* source;
    const Type* parameter_type;
    const Type* argument_type;
    int64_t byte_displacement = 0;
    bool address_path_resolved = false;
    uint8_t address_size;
    if (!mod || !callee || !value || !value->declaration || !argument ||
        !argument->expr || argument->cxx_temporary_owner || !frame_offset ||
        !address_addend || !pointer_adjustment || !dereference_source ||
        !dynamic_index) {
        return false;
    }
    dynamic_index->term_count = 0u;
    dynamic_index->is_dynamic = false;
    expression = argument->expr;
    address_size = g_opts.target_arch == ARCH_X64 ? 8u : 4u;
    address_expression = expression;
    *pointer_adjustment = 0;
    while (address_expression) {
        if (address_expression->cxx_virtual_base_adjustment ||
            address_expression->cxx_member_pointer_adjustment_valid) {
            return false;
        }
        if (address_expression->cxx_pointer_adjustment_valid) {
            int64_t adjustment =
                address_expression->cxx_pointer_adjustment;
            if ((adjustment > 0 &&
                 *pointer_adjustment > INT64_MAX - adjustment) ||
                (adjustment < 0 &&
                 *pointer_adjustment < INT64_MIN - adjustment)) {
                return false;
            }
            *pointer_adjustment += adjustment;
        }
        if (address_expression->kind != EXPR_CAST ||
            !address_expression->cast_expr) {
            break;
        }
        const Type* cast_target = address_expression->type;
        const Type* cast_source = address_expression->cast_expr->type;
        if (!cast_target || cast_target->kind != TYPE_PTR ||
            cast_target->cxx_is_member_pointer ||
            cast_target->size != address_size || !cast_source ||
            cast_source->kind != TYPE_PTR ||
            cast_source->cxx_is_member_pointer ||
            cast_source->size != address_size) {
            return false;
        }
        address_expression = address_expression->cast_expr;
    }
    if (address_expression &&
        (address_expression->kind == EXPR_ADD ||
         address_expression->kind == EXPR_SUB)) {
        if (!debug_inline_pointer_dynamic_index_chain(
                mod, address_expression, &pointer_expression,
                &byte_displacement, dynamic_index, address_size, 0u)) {
            return false;
        }
        while (pointer_expression) {
            if (pointer_expression->cxx_virtual_base_adjustment ||
                pointer_expression->cxx_member_pointer_adjustment_valid) {
                return false;
            }
            if (pointer_expression->cxx_pointer_adjustment_valid) {
                int64_t adjustment =
                    pointer_expression->cxx_pointer_adjustment;
                if ((adjustment > 0 &&
                     *pointer_adjustment > INT64_MAX - adjustment) ||
                    (adjustment < 0 &&
                     *pointer_adjustment < INT64_MIN - adjustment)) {
                    return false;
                }
                *pointer_adjustment += adjustment;
            }
            if (pointer_expression->kind != EXPR_CAST ||
                !pointer_expression->cast_expr) {
                break;
            }
            const Type* cast_target = pointer_expression->type;
            const Type* cast_source = pointer_expression->cast_expr->type;
            if (!cast_target || cast_target->kind != TYPE_PTR ||
                cast_target->cxx_is_member_pointer ||
                cast_target->size != address_size || !cast_source ||
                (cast_source->kind != TYPE_PTR &&
                 cast_source->kind != TYPE_ARRAY) ||
                (cast_source->kind == TYPE_PTR &&
                 (cast_source->cxx_is_member_pointer ||
                  cast_source->size != address_size))) {
                return false;
            }
            pointer_expression = pointer_expression->cast_expr;
        }
        if (!pointer_expression || !pointer_expression->type ||
            pointer_expression->type->kind != TYPE_PTR ||
            pointer_expression->type->cxx_is_member_pointer ||
            pointer_expression->type->size != address_size ||
            debug_inline_static_array_designator(
                pointer_expression, NULL)) {
            return false;
        }
        if (pointer_expression->kind == EXPR_ADDR) {
            return false;
        }
        operand = pointer_expression;
        *dereference_source = true;
        if ((byte_displacement > 0 &&
             *pointer_adjustment > INT64_MAX - byte_displacement) ||
            (byte_displacement < 0 &&
             *pointer_adjustment < INT64_MIN - byte_displacement)) {
            return false;
        }
        *pointer_adjustment += byte_displacement;
    } else {
        if (!address_expression || !address_expression->type ||
            address_expression->type->kind != TYPE_PTR ||
            address_expression->type->cxx_is_member_pointer ||
            address_expression->type->size != address_size ||
            address_expression->kind != EXPR_ADDR) {
            return false;
        }
        operand = address_expression->unary_operand;
        *dereference_source = false;
        address_path_resolved = debug_inline_dynamic_array_element_address(
            mod, operand, address_size, &source, address_addend,
            pointer_adjustment, dereference_source, dynamic_index);
    }
    if (!operand || (!address_path_resolved && !debug_inline_address_path(
            operand, &source, address_addend, true, true, 0u))) {
        return false;
    }
    parameter_type = value->declaration->type;
    argument_type = expression->type;
    if (value->declaration->kind != DECL_PARAM ||
        (source && source->kind != DECL_VAR &&
         source->kind != DECL_PARAM) ||
        !source || !source->type || !parameter_type || !argument_type ||
        parameter_type->kind != TYPE_PTR ||
        parameter_type->cxx_is_member_pointer ||
        parameter_type->size != address_size || parameter_type->is_volatile ||
        parameter_type->is_reference || parameter_type->is_atomic ||
        argument_type->kind != TYPE_PTR ||
        argument_type->cxx_is_member_pointer ||
        argument_type->size != address_size || argument_type->is_volatile ||
        argument_type->is_reference || argument_type->is_atomic ||
        !parameter_type->base || parameter_type->base->kind == TYPE_FUNC ||
        !argument_type->base || argument_type->base->kind == TYPE_FUNC ||
        (parameter_type->base->kind != TYPE_VOID &&
         argument_type->base->kind != TYPE_VOID &&
         !debug_inline_parameter_storage_type_equal_ignoring_top_cv(
             parameter_type->base, argument_type->base, 0u)) ||
        !debug_inline_parameter_body_is_read_only(callee)) {
        return false;
    }
    if (!source->type || source->type->size <= 0 || *address_addend < 0 ||
        *address_addend > source->type->size) {
        return false;
    }
    if (*dereference_source) {
        if (source->type->size < (int)address_size ||
            *address_addend >
                source->type->size - (int)address_size) {
            return false;
        }
    } else {
        int64_t result_addend;
        if ((*pointer_adjustment > 0 &&
             *address_addend > INT64_MAX - *pointer_adjustment) ||
            (*pointer_adjustment < 0 &&
             *address_addend < INT64_MIN - *pointer_adjustment)) {
            return false;
        }
        result_addend = *address_addend + *pointer_adjustment;
        if (result_addend < 0 || result_addend > source->type->size ||
            (argument_type->base->kind != TYPE_VOID &&
             (argument_type->base->size <= 0 ||
              (result_addend < source->type->size &&
               argument_type->base->size >
                   source->type->size - result_addend)))) {
            return false;
        }
    }
    return debug_inline_parameter_frame_offset(mod, source, frame_offset);
}

static size_t debug_build_inline_pointer_value_expression(
    uint8_t expression[DEBUG_INLINE_POINTER_VALUE_EXPRESSION_CAPACITY],
    int architecture, int64_t frame_offset,
    int64_t address_addend, bool dereference_source,
    int64_t pointer_adjustment,
    const DebugInlineDynamicPointerIndex* dynamic_index) {
    size_t size;
    const size_t capacity =
        DEBUG_INLINE_POINTER_VALUE_EXPRESSION_CAPACITY;
    if (!expression) goto expression_too_large;
    if (dynamic_index &&
        (dynamic_index->term_count >
             DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT ||
         dynamic_index->group_count >
             DEBUG_INLINE_DYNAMIC_POINTER_INDEX_LIMIT)) {
        goto expression_too_large;
    }
    size = debug_build_expr_breg(
        expression, architecture, frame_offset, 0u, false);
    if (size == 0u || size > capacity) goto expression_too_large;
#define INLINE_EXPR_APPEND_BYTE(value)                                      \
    do {                                                                     \
        if (size >= capacity) goto expression_too_large;                    \
        expression[size++] = (uint8_t)(value);                              \
    } while (0)
#define INLINE_EXPR_APPEND_SLEB(value)                                      \
    do {                                                                     \
        if (!debug_expression_sleb_bounded(                                 \
                expression, capacity, &size, (int64_t)(value))) {            \
            goto expression_too_large;                                      \
        }                                                                    \
    } while (0)
#define INLINE_EXPR_APPEND_ULEB(value)                                      \
    do {                                                                     \
        if (!debug_expression_uleb(                                         \
                expression, capacity, &size, (uint64_t)(value))) {           \
            goto expression_too_large;                                      \
        }                                                                    \
    } while (0)
    if (address_addend > 0) {
        uint64_t value = (uint64_t)address_addend;
        INLINE_EXPR_APPEND_BYTE(0x23u); /* DW_OP_plus_uconst */
        do {
            uint8_t byte = (uint8_t)(value & 0x7fu);
            value >>= 7u;
            if (value != 0u) byte |= 0x80u;
            INLINE_EXPR_APPEND_BYTE(byte);
        } while (value != 0u);
    }
    if (dereference_source) {
        INLINE_EXPR_APPEND_BYTE(0x06u); /* DW_OP_deref */
    }
    if (dynamic_index && dynamic_index->is_dynamic) {
        uint8_t address_size = architecture == ARCH_X64 ? 8u : 4u;
        size_t index = 0u;
        size_t group_index = 0u;
        while (index < dynamic_index->term_count) {
            const DebugInlineDynamicPointerIndexTerm* term =
                &dynamic_index->terms[index];
            const DebugInlineDynamicPointerIndexGroup* group =
                group_index < dynamic_index->group_count &&
                dynamic_index->groups[group_index].first_term == index
                    ? &dynamic_index->groups[group_index] : NULL;
            if (group) {
                if (group->term_count == 0u ||
                    group->term_count > dynamic_index->term_count - index ||
                    group->value_size == 0u ||
                    group->value_size > address_size ||
                    group->element_size <= 0) {
                    goto expression_too_large;
                }
                for (size_t group_term_index = 0u;
                     group_term_index < group->term_count;
                     ++group_term_index) {
                    const DebugInlineDynamicPointerIndexTerm* group_term =
                        &dynamic_index->terms[index + group_term_index];
                    if (group_term->is_constant) {
                        if (group_term->unsigned_conversion_width != 0u) {
                            goto expression_too_large;
                        }
                        INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                        INLINE_EXPR_APPEND_ULEB(group_term->constant_value);
                    } else {
                        if (group_term->value_size == 0u ||
                            group_term->value_size > address_size ||
                            group_term->unsigned_conversion_width != 0u) {
                            goto expression_too_large;
                        }
                        INLINE_EXPR_APPEND_BYTE(architecture == ARCH_X64
                            ? 0x76u : 0x75u);
                        INLINE_EXPR_APPEND_SLEB(group_term->frame_offset);
                        INLINE_EXPR_APPEND_BYTE(0x94u); /* DW_OP_deref_size */
                        INLINE_EXPR_APPEND_BYTE(group_term->value_size);
                        if (group_term->index_multiplier != 1u) {
                            INLINE_EXPR_APPEND_BYTE(0x10u);
                            INLINE_EXPR_APPEND_ULEB(
                                group_term->index_multiplier);
                            INLINE_EXPR_APPEND_BYTE(0x1eu); /* DW_OP_mul */
                        }
                    }
                    if (group_term_index != 0u) {
                        INLINE_EXPR_APPEND_BYTE(group_term->subtract
                            ? 0x1cu /* DW_OP_minus */
                            : 0x22u /* DW_OP_plus */);
                    }
                }
                if (group->value_size < address_size) {
                    uint64_t shift = (uint64_t)group->value_size * 8u;
                    uint64_t mask = (UINT64_C(1) << shift) - 1u;
                    INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                    INLINE_EXPR_APPEND_ULEB(mask);
                    INLINE_EXPR_APPEND_BYTE(0x1au); /* DW_OP_and */
                }
                INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                INLINE_EXPR_APPEND_ULEB((uint64_t)group->element_size);
                INLINE_EXPR_APPEND_BYTE(0x1eu); /* DW_OP_mul */
                INLINE_EXPR_APPEND_BYTE(group->subtract
                    ? 0x1cu /* DW_OP_minus */ : 0x22u /* DW_OP_plus */);
                index += group->term_count;
                ++group_index;
                continue;
            }
            if (term->value_size == 0u ||
                term->value_size > address_size || term->element_size <= 0 ||
                term->unsigned_conversion_width > address_size ||
                term->cast_step_count >
                    DEBUG_INLINE_INTEGER_INDEX_CAST_LIMIT) {
                goto expression_too_large;
            }
            INLINE_EXPR_APPEND_BYTE(architecture == ARCH_X64
                ? 0x76u : 0x75u);
            INLINE_EXPR_APPEND_SLEB(term->frame_offset);
            INLINE_EXPR_APPEND_BYTE(0x94u); /* DW_OP_deref_size */
            INLINE_EXPR_APPEND_BYTE(term->value_size);
            if (!term->is_unsigned && term->value_size < address_size) {
                uint64_t shift = (uint64_t)(address_size -
                    term->value_size) * 8u;
                INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                INLINE_EXPR_APPEND_ULEB(shift);
                INLINE_EXPR_APPEND_BYTE(0x24u); /* DW_OP_shl */
                INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                INLINE_EXPR_APPEND_ULEB(shift);
                INLINE_EXPR_APPEND_BYTE(0x26u); /* DW_OP_shra */
            }
            for (uint8_t step = 0u;
                 step < term->cast_step_count; ++step) {
                uint8_t encoded_width = term->cast_steps[step];
                bool sign_extend = (encoded_width & 0x80u) != 0u;
                uint8_t cast_width = encoded_width & 0x7fu;
                uint64_t shift;
                if (cast_width == 0u || cast_width >= address_size) {
                    goto expression_too_large;
                }
                if (sign_extend) {
                    shift = (uint64_t)(address_size - cast_width) * 8u;
                    INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                    INLINE_EXPR_APPEND_ULEB(shift);
                    INLINE_EXPR_APPEND_BYTE(0x24u); /* DW_OP_shl */
                    INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                    INLINE_EXPR_APPEND_ULEB(shift);
                    INLINE_EXPR_APPEND_BYTE(0x26u); /* DW_OP_shra */
                } else {
                    uint64_t mask =
                        (UINT64_C(1) << (cast_width * 8u)) - 1u;
                    INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                    INLINE_EXPR_APPEND_ULEB(mask);
                    INLINE_EXPR_APPEND_BYTE(0x1au); /* DW_OP_and */
                }
            }
            if (term->unsigned_conversion_width != 0u) {
                uint8_t conversion_width =
                    term->unsigned_conversion_width;
                uint64_t mask;
                if (conversion_width > address_size || conversion_width > 8u) {
                    goto expression_too_large;
                }
                if (conversion_width < address_size) {
                    mask = conversion_width == 8u ? UINT64_MAX
                        : (UINT64_C(1) << (conversion_width * 8u)) - 1u;
                    INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                    INLINE_EXPR_APPEND_ULEB(mask);
                    INLINE_EXPR_APPEND_BYTE(0x1au); /* DW_OP_and */
                }
            }
            if (term->signed_index_divisor != 0) {
                if (term->is_unsigned) goto expression_too_large;
                INLINE_EXPR_APPEND_BYTE(0x11u); /* DW_OP_consts */
                INLINE_EXPR_APPEND_SLEB(term->signed_index_divisor);
                INLINE_EXPR_APPEND_BYTE(0x1bu); /* DW_OP_div */
            }
            if (term->is_unsigned && term->index_multiplier != 1u) {
                INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
                INLINE_EXPR_APPEND_ULEB(term->index_multiplier);
                INLINE_EXPR_APPEND_BYTE(0x1eu); /* DW_OP_mul */
            } else if (!term->is_unsigned &&
                       term->signed_index_multiplier != 1) {
                INLINE_EXPR_APPEND_BYTE(0x11u); /* DW_OP_consts */
                INLINE_EXPR_APPEND_SLEB(term->signed_index_multiplier);
                INLINE_EXPR_APPEND_BYTE(0x1eu); /* DW_OP_mul */
            }
            INLINE_EXPR_APPEND_BYTE(0x10u); /* DW_OP_constu */
            INLINE_EXPR_APPEND_ULEB((uint64_t)term->element_size);
            INLINE_EXPR_APPEND_BYTE(0x1eu); /* DW_OP_mul */
            INLINE_EXPR_APPEND_BYTE(term->subtract
                ? 0x1cu /* DW_OP_minus */ : 0x22u /* DW_OP_plus */);
            ++index;
        }
    }
    if (pointer_adjustment > 0) {
        uint64_t value = (uint64_t)pointer_adjustment;
        INLINE_EXPR_APPEND_BYTE(0x23u); /* DW_OP_plus_uconst */
        do {
            uint8_t byte = (uint8_t)(value & 0x7fu);
            value >>= 7u;
            if (value != 0u) byte |= 0x80u;
            INLINE_EXPR_APPEND_BYTE(byte);
        } while (value != 0u);
    } else if (pointer_adjustment < 0) {
        INLINE_EXPR_APPEND_BYTE(0x11u); /* DW_OP_consts */
        INLINE_EXPR_APPEND_SLEB(pointer_adjustment);
        INLINE_EXPR_APPEND_BYTE(0x22u); /* DW_OP_plus */
    }
    INLINE_EXPR_APPEND_BYTE(0x9fu); /* DW_OP_stack_value */
#undef INLINE_EXPR_APPEND_BYTE
#undef INLINE_EXPR_APPEND_SLEB
#undef INLINE_EXPR_APPEND_ULEB
    return size;

expression_too_large:
#undef INLINE_EXPR_APPEND_BYTE
#undef INLINE_EXPR_APPEND_SLEB
#undef INLINE_EXPR_APPEND_ULEB
    rcc_fatal("inline pointer DWARF expression is too large or malformed");
    return 0u;
}

static void debug_emit_inline_parameter_instances(
    ObjectFile* obj, ObjSection* info, int info_section, const Module* mod,
    const char* filename, int architecture,
    DebugInlineParameterOrigin* origins, const ModuleDebugInlineCall* call) {
    const ExprList* argument;
    if (!obj || !info || !mod || !filename || info_section < 0 || !call ||
        !call->callee || !call->parameter_values) return;
    argument = call->arguments;
    for (size_t index = 0u; index < call->parameter_value_count; ++index) {
        const ModuleDebugInlineParameterValue* value =
            &call->parameter_values[index];
        DebugInlineParameterOrigin* origin;
        const ModuleSymbol* source_symbol;
        char* scoped_source_symbol = NULL;
        bool source_frame_based = false;
        int64_t source_frame_offset = 0;
        int64_t source_frame_address_offset = 0;
        int64_t source_frame_address_addend = 0;
        int64_t source_pointer_adjustment = 0;
        bool source_frame_dereference = false;
        DebugInlineDynamicPointerIndex source_dynamic_index = {0};
        uint8_t integer_value_expression[
            DEBUG_INLINE_I686_U64_DIVMOD_EXPRESSION_CAPACITY];
        size_t integer_value_expression_size = 0u;
        int64_t address_addend = 0;
        if (value->has_signed_integer_constant && value->declaration &&
            (origin = debug_inline_parameter_origin_find(
                origins, call->callee, value->declaration)) != NULL) {
            section_add_byte(info, 63u);
            debug_line_u32(info, origin->die_offset);
            debug_line_sleb(info, value->value);
        } else if (value->has_unsigned_integer_constant &&
                   value->declaration &&
                   (origin = debug_inline_parameter_origin_find(
                       origins, call->callee, value->declaration)) != NULL) {
            section_add_byte(info, 64u);
            debug_line_u32(info, origin->die_offset);
            debug_line_uleb(info, value->unsigned_value);
        } else if (value->has_float_constant && value->declaration &&
                   (value->float_size == 4u || value->float_size == 8u) &&
                   (origin = debug_inline_parameter_origin_find(
                       origins, call->callee, value->declaration)) != NULL) {
            section_add_byte(info, 65u);
            debug_line_u32(info, origin->die_offset);
            section_add_byte(info, value->float_size);
            for (uint8_t byte = 0u; byte < value->float_size; ++byte) {
                section_add_byte(
                    info, (uint8_t)(value->float_bits >> (byte * 8u)));
            }
        } else if (value->has_null_pointer_constant &&
                   value->declaration &&
                   (origin = debug_inline_parameter_origin_find(
                       origins, call->callee, value->declaration)) != NULL) {
            section_add_byte(info, 66u);
            debug_line_u32(info, origin->die_offset);
            debug_line_u32(info, 0u);
            if (g_opts.target_arch == ARCH_X64) {
                debug_line_u32(info, 0u);
            }
        } else if (value->declaration &&
                   (origin = debug_inline_parameter_origin_find(
                       origins, call->callee, value->declaration)) != NULL &&
                   (source_symbol = debug_inline_parameter_address_constant(
                       mod, filename, call->callee, value, argument,
                       &scoped_source_symbol, &address_addend)) != NULL) {
            const char* relocation_symbol = scoped_source_symbol
                ? scoped_source_symbol : source_symbol->name;
            uint64_t address_offset;
            uint32_t address_size = architecture == ARCH_X64 ? 8u : 4u;
            section_add_byte(info, 68u);
            debug_line_u32(info, origin->die_offset);
            address_offset = info->size;
            for (uint32_t byte = 0u; byte < address_size; ++byte) {
                section_add_byte(info, 0u);
            }
            objfile_add_reloc(
                obj, info_section, address_offset, relocation_symbol,
                address_size == 8u ? RELOC_ABS64 : RELOC_ABS32U,
                address_addend);
            rcc_free(scoped_source_symbol);
        } else if (value->declaration &&
                   (origin = debug_inline_parameter_origin_find(
                       origins, call->callee, value->declaration)) != NULL &&
                   debug_inline_parameter_frame_address(
                       mod, call->callee, value, argument,
                       &source_frame_address_offset,
                       &source_frame_address_addend,
                       &source_pointer_adjustment,
                       &source_frame_dereference,
                       &source_dynamic_index)) {
            uint8_t expression[
                DEBUG_INLINE_POINTER_VALUE_EXPRESSION_CAPACITY];
            size_t expression_size =
                debug_build_inline_pointer_value_expression(
                    expression, architecture, source_frame_address_offset,
                    source_frame_address_addend,
                    source_frame_dereference, source_pointer_adjustment,
                    &source_dynamic_index);
            section_add_byte(info, 67u);
            debug_line_u32(info, origin->die_offset);
            debug_line_uleb(info, expression_size);
            section_add_bytes(info, expression, expression_size);
        } else if (value->declaration && argument && argument->expr &&
                   (origin = debug_inline_parameter_origin_find(
                       origins, call->callee, value->declaration)) != NULL &&
                   debug_inline_parameter_integer_expression(
                       mod, architecture, call->callee, value, argument,
                       integer_value_expression,
                       sizeof(integer_value_expression),
                       &integer_value_expression_size)) {
            section_add_byte(info, 67u);
            debug_line_u32(info, origin->die_offset);
            debug_line_uleb(info, integer_value_expression_size);
            section_add_bytes(info, integer_value_expression,
                              integer_value_expression_size);
        } else if (value->declaration &&
                   (origin = debug_inline_parameter_origin_find(
                       origins, call->callee, value->declaration)) != NULL) {
            source_symbol = debug_inline_parameter_source_symbol(
                mod, filename, call->callee, value, argument,
                &scoped_source_symbol, &source_frame_based,
                &source_frame_offset);
            if (source_symbol || source_frame_based) {
                section_add_byte(info, 67u);
                debug_line_u32(info, origin->die_offset);
                if (source_symbol) {
                    const char* relocation_symbol = scoped_source_symbol
                        ? scoped_source_symbol : source_symbol->name;
                    debug_expr_addr(
                        obj, info, info_section, relocation_symbol,
                        architecture,
                        source_symbol->section == MODULE_SYMBOL_TLS);
                } else {
                    debug_expr_breg(info, architecture, source_frame_offset,
                                    0u, false);
                }
                rcc_free(scoped_source_symbol);
            }
        }
        if (argument) argument = argument->next;
    }
}

static void debug_emit_inline_local_origin(
    ObjSection* info, ObjSection* strings, DebugTypeContext* types,
    const char* const* files, int file_count, const Decl* function,
    DebugInlineLocalOrigin** origins) {
    const StmtList* item;
    if (!info || !strings || !types || !function || !origins ||
        !function->func_body ||
        function->func_body->kind != STMT_BLOCK) return;
    for (item = function->func_body->block_stmts; item; item = item->next) {
        const Decl* local = item->stmt && item->stmt->kind == STMT_DECL
            ? item->stmt->decl : NULL;
        DebugTypeEntry* type_entry;
        int file_index;
        DebugInlineLocalOrigin* origin;
        if (!debug_inline_local_constant_supported(local)) continue;
        type_entry = debug_type_find(types, local->type);
        file_index = debug_line_file_index(
            files, file_count, local->loc.filename);
        if (!type_entry || file_index <= 0 || info->size > UINT32_MAX) {
            continue;
        }
        origin = rcc_alloc(sizeof(*origin));
        origin->function = function;
        origin->declaration = local;
        origin->die_offset = (uint32_t)info->size;
        origin->next = *origins;
        *origins = origin;
        /* Abbreviation 32 is the declaration-only form of a local variable:
         * the abstract origin carries identity/type/source, while the
         * concrete inlined instance below carries the actual constant. */
        section_add_byte(info, 32u);
        debug_line_u32(info, debug_str_add(strings, local->name));
        debug_line_u32(info, type_entry->offset);
        debug_line_u32(info, (uint32_t)file_index);
        debug_line_u32(info, (uint32_t)local->loc.line);
        debug_line_u32(info, (uint32_t)local->loc.column);
    }
}

static void debug_emit_inline_local_instance(
    ObjSection* info, DebugInlineLocalOrigin* origins,
    const Decl* function, const Decl* declaration) {
    DebugInlineLocalOrigin* origin = debug_inline_local_origin_find(
        origins, function, declaration);
    if (!info || !origin ||
        !debug_inline_local_constant_supported(declaration)) return;
    if (declaration->var_debug_const_value_float) {
        section_add_byte(info, 62u);
        debug_line_u32(info, origin->die_offset);
        section_add_byte(info, declaration->var_debug_const_float_size);
        for (uint8_t byte = 0u;
             byte < declaration->var_debug_const_float_size; ++byte) {
            section_add_byte(
                info, (uint8_t)(declaration->var_debug_const_float_bits >>
                                (byte * 8u)));
        }
    } else if (declaration->var_debug_const_value_unsigned) {
        section_add_byte(info, 61u);
        debug_line_u32(info, origin->die_offset);
        debug_line_uleb(info,
                        declaration->var_debug_const_unsigned_value);
    } else {
        section_add_byte(info, 60u);
        debug_line_u32(info, origin->die_offset);
        debug_line_sleb(info, declaration->var_debug_const_value);
    }
}

static void debug_emit_inline_local_instances(
    ObjSection* info, DebugInlineLocalOrigin* origins,
    const Decl* function) {
    if (!info || !function || !function->func_body ||
        function->func_body->kind != STMT_BLOCK) return;
    for (const StmtList* item = function->func_body->block_stmts;
         item; item = item->next) {
        const Decl* local = item->stmt && item->stmt->kind == STMT_DECL
            ? item->stmt->decl : NULL;
        debug_emit_inline_local_instance(info, origins, function, local);
    }
}

static void debug_emit_inline_call_tree(
    ObjectFile* obj, ObjSection* info, const char* const* files,
    int file_count, const char* filename, int info_section,
    const Module* mod, const ModuleSymbol* function,
    const Decl* function_decl, DebugInlineOrigin* origins,
    size_t origin_count, DebugInlineLocalOrigin* local_origins,
    DebugInlineParameterOrigin* parameter_origins,
    bool* emitted, size_t call_index) {
    const ModuleDebugInlineCall* call;
    DebugInlineOrigin* origin;
    uint64_t function_end;
    uint64_t address_offset;
    const char* relocation_symbol;
    char* scoped_relocation_symbol = NULL;
    int call_file_index;
    size_t child_index;

    if (!obj || !info || !mod || !function || !function_decl || !emitted ||
        call_index >= mod->debug_inline_call_count || emitted[call_index]) {
        return;
    }
    call = &mod->debug_inline_calls[call_index];
    function_end = (uint64_t)function->offset +
                   debug_function_size(mod, function);
    if (call->caller != function_decl ||
        call->code_start < function->offset ||
        call->code_end <= call->code_start ||
        (uint64_t)call->code_end > function_end ||
        !call->call_location.filename || call->call_location.line <= 0 ||
        call->call_location.column < 0) {
        return;
    }
    origin = debug_inline_origin_find(origins, origin_count, call->callee);
    call_file_index = debug_line_file_index(
        files, file_count, call->call_location.filename);
    if (!origin || call_file_index <= 0 || info->size > UINT32_MAX) return;

    emitted[call_index] = true;
    section_add_byte(info, 41u);
    debug_line_u32(info, origin->die_offset);
    address_offset = info->size;
    for (int byte = 0;
         byte < (g_opts.target_arch == ARCH_X64 ? 8 : 4); ++byte) {
        section_add_byte(info, 0u);
    }
    relocation_symbol = function->name;
    if (!function->is_global) {
        scoped_relocation_symbol = module_scoped_symbol(
            filename, function->name);
        relocation_symbol = scoped_relocation_symbol;
    }
    objfile_add_reloc(
        obj, info_section, address_offset, relocation_symbol,
        g_opts.target_arch == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U,
        (int64_t)(call->code_start - function->offset));
    rcc_free(scoped_relocation_symbol);
    debug_line_u32(info, call->code_end - call->code_start);
    debug_line_u32(info, (uint32_t)call_file_index);
    debug_line_u32(info, (uint32_t)call->call_location.line);
    debug_line_u32(info, (uint32_t)call->call_location.column);

    debug_emit_inline_local_instances(info, local_origins, call->callee);
    debug_emit_inline_parameter_instances(
        obj, info, info_section, mod, filename,
        g_opts.target_arch == ARCH_X64 ? ARCH_X64 : ARCH_X86,
        parameter_origins, call);

    for (child_index = 0u;
         child_index < mod->debug_inline_call_count; ++child_index) {
        const ModuleDebugInlineCall* child =
            &mod->debug_inline_calls[child_index];
        if (child->parent_index_plus_one != call_index + 1u ||
            child->code_start < call->code_start ||
            child->code_end > call->code_end) {
            continue;
        }
        debug_emit_inline_call_tree(
            obj, info, files, file_count, filename, info_section, mod,
            function, function_decl, origins, origin_count, local_origins,
            parameter_origins, emitted,
            child_index);
    }
    section_add_byte(info, 0u);
}

static void debug_emit_inline_calls(
    ObjectFile* obj, ObjSection* info, const char* const* files,
    int file_count, const char* filename, int info_section,
    const Module* mod, const ModuleSymbol* function,
    const Decl* function_decl, DebugInlineOrigin* origins,
    size_t origin_count, DebugInlineLocalOrigin* local_origins,
    DebugInlineParameterOrigin* parameter_origins) {
    bool* emitted;
    if (!mod || mod->debug_inline_call_count == 0u) return;
    if (mod->debug_inline_call_count > SIZE_MAX / sizeof(*emitted)) {
        rcc_fatal("DWARF inline-call emission table is too large");
    }
    emitted = rcc_alloc(mod->debug_inline_call_count * sizeof(*emitted));
    memset(emitted, 0,
           mod->debug_inline_call_count * sizeof(*emitted));
    for (size_t index = 0u;
         index < mod->debug_inline_call_count; ++index) {
        const ModuleDebugInlineCall* call = &mod->debug_inline_calls[index];
        if (call->caller == function_decl &&
            call->parent_index_plus_one == 0u) {
            debug_emit_inline_call_tree(
                obj, info, files, file_count, filename, info_section, mod,
                function, function_decl, origins, origin_count,
                local_origins, parameter_origins, emitted,
                index);
        }
    }
    /* If an inner range could not be nested in its recorded parent (for
     * example, because the parent emitted no machine code), preserve that
     * attribution at the nearest representable function scope. */
    for (size_t index = 0u;
         index < mod->debug_inline_call_count; ++index) {
        const ModuleDebugInlineCall* call = &mod->debug_inline_calls[index];
        if (call->caller == function_decl && !emitted[index]) {
            debug_emit_inline_call_tree(
                obj, info, files, file_count, filename, info_section, mod,
                function, function_decl, origins, origin_count,
                local_origins, parameter_origins, emitted,
                index);
        }
    }
    rcc_free(emitted);
}

static void module_emit_debug_info(ObjectFile* obj, Module* mod,
                                   const char* filename) {
    const ModuleSymbol** functions;
    const char** files;
    DebugTypeContext types = {0};
    DebugTypePatchContext type_patches = {0};
    DebugInlineOrigin* inline_origins = NULL;
    DebugInlineLocalOrigin* inline_local_origins = NULL;
    DebugInlineParameterOrigin* inline_parameter_origins = NULL;
    size_t inline_origin_count = 0u;
    ObjSection* strings;
    ObjSection* abbrev;
    ObjSection* info;
    int function_count = 0;
    int global_count = 0;
    int typedef_count = 0;
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
        Type* typedef_type;
        if (!declaration || declaration->kind != DECL_TYPEDEF ||
            !declaration->name || !declaration->loc.filename ||
            declaration->loc.filename[0] == '\0' ||
            declaration->loc.line <= 0 || declaration->loc.column < 0) {
            continue;
        }
        typedef_type = declaration->typedef_type
            ? declaration->typedef_type : declaration->type;
        if (!typedef_type) continue;
        ++typedef_count;
        debug_type_collect(&types, typedef_type);
        debug_file_add(&files, &file_count, &file_capacity,
                       declaration->loc.filename);
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
    if (function_count == 0 && global_count == 0 && typedef_count == 0) {
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
    debug_collect_inline_files(mod, &files, &file_count, &file_capacity);
    if (mod->debug_inline_call_count > 0u) {
        if (mod->debug_inline_call_count >
            SIZE_MAX / sizeof(*inline_origins)) {
            rcc_fatal("DWARF inline-origin table is too large");
        }
        inline_origins = rcc_alloc(mod->debug_inline_call_count *
                                   sizeof(*inline_origins));
        for (size_t index = 0u; index < mod->debug_inline_call_count; ++index) {
            const Decl* callee = mod->debug_inline_calls[index].callee;
            if (!callee || !callee->loc.filename || callee->loc.line <= 0 ||
                !callee->type || callee->type->kind != TYPE_FUNC) {
                continue;
            }
            if (debug_inline_origin_find(inline_origins,
                                         inline_origin_count, callee)) {
                continue;
            }
            debug_collect_function_types(&types, (Decl*)callee);
            inline_origins[inline_origin_count].declaration = callee;
            inline_origins[inline_origin_count].die_offset = 0u;
            ++inline_origin_count;
        }
    }
    for (size_t type_index = 0u; type_index < types.count; ++type_index) {
        const Type* type = types.entries[type_index].type;
        if (!type || (type->kind != TYPE_STRUCT &&
                      type->kind != TYPE_UNION) || !type->cxx_class) {
            continue;
        }
        for (CxxTypeAlias* alias = type->cxx_class->type_aliases; alias;
             alias = alias->next) {
            if (!alias->type || debug_type_is_dependent(alias->type, 0u) ||
                !alias->loc.filename || alias->loc.filename[0] == '\0' ||
                alias->loc.line <= 0 || alias->loc.column < 0) {
                continue;
            }
            debug_file_add(&files, &file_count, &file_capacity,
                           alias->loc.filename);
        }
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
     * not claim that a call site was actually inlined. Source-level function
     * abbreviations also record whether the declaration has a prototype. */
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
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
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
    debug_line_uleb(abbrev, 0x27u);    /* DW_AT_prototyped */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
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
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
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
    debug_line_uleb(abbrev, 0x3eu);     /* DW_AT_encoding */
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
    debug_line_uleb(abbrev, 34u);
    debug_line_uleb(abbrev, 0x21u);     /* DW_TAG_subrange_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x37u);     /* DW_AT_count */
    debug_line_uleb(abbrev, 0x18u);     /* DW_FORM_exprloc */
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
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 36u);
    debug_line_uleb(abbrev, 0x04u);     /* DW_TAG_enumeration_type */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);     /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);     /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x0bu);     /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x6du);     /* DW_AT_enum_class */
    debug_line_uleb(abbrev, 0x0cu);     /* DW_FORM_flag */
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
    debug_line_uleb(abbrev, 35u);
    debug_line_uleb(abbrev, 0x28u);     /* DW_TAG_enumerator */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);     /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);     /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x1cu);     /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0fu);     /* DW_FORM_udata */
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
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
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
    debug_line_uleb(abbrev, 0x06u);     /* DW_FORM_data4 */
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
    /* C++ references are pointer-sized ABI carriers, but debuggers need the
     * source-level lvalue/rvalue distinction and the referred-to type. */
    debug_line_uleb(abbrev, 37u);
    debug_line_uleb(abbrev, 0x10u);    /* DW_TAG_reference_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 38u);
    debug_line_uleb(abbrev, 0x42u);    /* DW_TAG_rvalue_reference_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Scoped stack variables use a DWARF v4 location list so their frame
     * address is not advertised outside the lexical block's PC ranges. */
    debug_line_uleb(abbrev, 39u);
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
    debug_line_uleb(abbrev, 0x02u);    /* DW_AT_location */
    debug_line_uleb(abbrev, 0x17u);    /* DW_FORM_sec_offset */
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
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
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
    debug_line_uleb(abbrev, 0x27u);    /* DW_AT_prototyped */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
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
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
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
    debug_line_uleb(abbrev, 0x27u);    /* DW_AT_prototyped */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
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
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
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
    debug_line_uleb(abbrev, 0x27u);    /* DW_AT_prototyped */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
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
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
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
    debug_line_uleb(abbrev, 0x27u);    /* DW_AT_prototyped */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
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
    /* Optimized-away const scalar locals retain a source value without
     * claiming that a machine location still exists. */
    debug_line_uleb(abbrev, 54u);
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
    debug_line_uleb(abbrev, 0x1cu);    /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0du);    /* DW_FORM_sdata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Optimized-away unsigned scalar locals use the unsigned DWARF form so
     * values through UINT64_MAX remain exactly representable. */
    debug_line_uleb(abbrev, 55u);
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
    debug_line_uleb(abbrev, 0x1cu);    /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0fu);    /* DW_FORM_udata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 56u);
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
    debug_line_uleb(abbrev, 0x1cu);    /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0au);    /* DW_FORM_block1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Abstract inline origins retain declaration identity and function type;
     * each concrete call-site DIE below points back here. */
    debug_line_uleb(abbrev, 40u);
    debug_line_uleb(abbrev, 0x2eu);    /* DW_TAG_subprogram */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x3au);    /* DW_AT_decl_file */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x3bu);    /* DW_AT_decl_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x39u);    /* DW_AT_decl_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x6eu);    /* DW_AT_linkage_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x20u);    /* DW_AT_inline */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x27u);    /* DW_AT_prototyped */
    debug_line_uleb(abbrev, 0x0cu);    /* DW_FORM_flag */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 41u);
    debug_line_uleb(abbrev, 0x1du);    /* DW_TAG_inlined_subroutine */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x31u);    /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x11u);    /* DW_AT_low_pc */
    debug_line_uleb(abbrev, 0x01u);    /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0x12u);    /* DW_AT_high_pc */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x58u);    /* DW_AT_call_file */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x59u);    /* DW_AT_call_line */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0x57u);    /* DW_AT_call_column */
    debug_line_uleb(abbrev, 0x06u);    /* DW_FORM_data4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 42u);
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
    debug_line_uleb(abbrev, 43u);
    debug_line_uleb(abbrev, 0x1cu);    /* DW_TAG_inheritance */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x02u);    /* DW_AT_data_member_location */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x32u);    /* DW_AT_accessibility */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 44u);
    debug_line_uleb(abbrev, 0x16u);    /* DW_TAG_typedef */
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
    debug_line_uleb(abbrev, 45u);
    debug_line_uleb(abbrev, 0x1cu);    /* DW_TAG_inheritance */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x02u);    /* DW_AT_data_member_location */
    debug_line_uleb(abbrev, 0x18u);    /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0x4cu);    /* DW_AT_virtuality */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x32u);    /* DW_AT_accessibility */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 46u);
    debug_line_uleb(abbrev, 0x16u);    /* DW_TAG_typedef */
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
    debug_line_uleb(abbrev, 0x32u);    /* DW_AT_accessibility */
    debug_line_uleb(abbrev, 0x0bu);    /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 47u);
    debug_line_uleb(abbrev, 0x2fu);    /* DW_TAG_template_type_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 48u);
    debug_line_uleb(abbrev, 0x30u);    /* DW_TAG_template_value_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);    /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0du);    /* DW_FORM_sdata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 49u);
    debug_line_uleb(abbrev, 0x30u);    /* DW_TAG_template_value_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);    /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0fu);    /* DW_FORM_udata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* GCC's DWARF v4-compatible extension represents a template parameter
     * pack as a named container DIE (0x4107) whose children are the
     * individual, unnamed template argument DIEs. */
    debug_line_uleb(abbrev, 50u);
    debug_line_uleb(abbrev, 0x4107u);  /* DW_TAG_GNU_template_parameter_pack */
    section_add_byte(abbrev, 1u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 51u);
    debug_line_uleb(abbrev, 0x2fu);    /* DW_TAG_template_type_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 52u);
    debug_line_uleb(abbrev, 0x30u);    /* DW_TAG_template_value_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);    /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0du);    /* DW_FORM_sdata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 53u);
    debug_line_uleb(abbrev, 0x30u);    /* DW_TAG_template_value_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x49u);    /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);    /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);    /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0fu);    /* DW_FORM_udata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 54u);
    debug_line_uleb(abbrev, 0x4106u);  /* DW_TAG_GNU_template_template_param */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x03u);    /* DW_AT_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0x2110u); /* DW_AT_GNU_template_name */
    debug_line_uleb(abbrev, 0x0eu);    /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 55u);
    debug_line_uleb(abbrev, 0x4106u);  /* DW_TAG_GNU_template_template_param */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x2110u); /* DW_AT_GNU_template_name */
    debug_line_uleb(abbrev, 0x0eu);   /* DW_FORM_strp */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 57u);
    debug_line_uleb(abbrev, 0x1fu);   /* DW_TAG_ptr_to_member_type */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x0bu);   /* DW_AT_byte_size */
    debug_line_uleb(abbrev, 0x0bu);   /* DW_FORM_data1 */
    debug_line_uleb(abbrev, 0x49u);   /* DW_AT_type */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1du);   /* DW_AT_containing_type */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Concrete inlined scalar locals refer to their abstract declaration. */
    debug_line_uleb(abbrev, 60u);
    debug_line_uleb(abbrev, 0x34u);   /* DW_TAG_variable */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);   /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0du);   /* DW_FORM_sdata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 61u);
    debug_line_uleb(abbrev, 0x34u);   /* DW_TAG_variable */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);   /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0fu);   /* DW_FORM_udata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 62u);
    debug_line_uleb(abbrev, 0x34u);   /* DW_TAG_variable */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);   /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0au);   /* DW_FORM_block1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Concrete inlined signed scalar parameters carry their call value. */
    debug_line_uleb(abbrev, 63u);
    debug_line_uleb(abbrev, 0x05u);   /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);   /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0du);   /* DW_FORM_sdata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 64u);
    debug_line_uleb(abbrev, 0x05u);   /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);   /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0fu);   /* DW_FORM_udata */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 65u);
    debug_line_uleb(abbrev, 0x05u);   /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);   /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x0au);   /* DW_FORM_block1 */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 66u);
    debug_line_uleb(abbrev, 0x05u);   /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);   /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x01u);   /* DW_FORM_addr */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* Dynamic inline parameters can reuse a stable global/static source
     * object's location when its type exactly matches the formal parameter. */
    debug_line_uleb(abbrev, 67u);
    debug_line_uleb(abbrev, 0x05u);   /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x02u);   /* DW_AT_location */
    debug_line_uleb(abbrev, 0x18u);   /* DW_FORM_exprloc */
    debug_line_uleb(abbrev, 0u);
    debug_line_uleb(abbrev, 0u);
    /* A symbolic address passed by value is a DWARF constant, not a storage
     * location. The address-sized field below carries an object relocation. */
    debug_line_uleb(abbrev, 68u);
    debug_line_uleb(abbrev, 0x05u);   /* DW_TAG_formal_parameter */
    section_add_byte(abbrev, 0u);
    debug_line_uleb(abbrev, 0x31u);   /* DW_AT_abstract_origin */
    debug_line_uleb(abbrev, 0x13u);   /* DW_FORM_ref4 */
    debug_line_uleb(abbrev, 0x1cu);   /* DW_AT_const_value */
    debug_line_uleb(abbrev, 0x01u);   /* DW_FORM_addr */
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

    debug_emit_type_dies(info, strings, files, file_count, &types,
                         &type_patches);
    debug_type_patches_free(&type_patches);

    for (DeclList* item = mod->debug_ast ? mod->debug_ast->decls : NULL;
         item; item = item->next) {
        Decl* declaration = item->decl;
        Type* typedef_type;
        DebugTypeEntry* type_entry;
        int file_index;
        if (!declaration || declaration->kind != DECL_TYPEDEF ||
            !declaration->name || !declaration->loc.filename ||
            declaration->loc.filename[0] == '\0' ||
            declaration->loc.line <= 0 || declaration->loc.column < 0) {
            continue;
        }
        typedef_type = declaration->typedef_type
            ? declaration->typedef_type : declaration->type;
        type_entry = debug_type_find(&types, typedef_type);
        file_index = debug_line_file_index(
            files, file_count, declaration->loc.filename);
        if (!typedef_type || !type_entry || file_index <= 0) {
            rcc_fatal("DWARF typedef is missing its type or source file");
            return;
        }
        section_add_byte(info, 44u); /* DW_TAG_typedef */
        debug_line_u32(info, debug_str_add(strings, declaration->name));
        debug_type_ref(info, NULL, type_entry);
        debug_line_u32(info, (uint32_t)file_index);
        debug_line_u32(info, (uint32_t)declaration->loc.line);
        debug_line_u32(info, (uint32_t)declaration->loc.column);
    }

    for (size_t index = 0u; index < inline_origin_count; ++index) {
        const Decl* callee = inline_origins[index].declaration;
        DebugTypeEntry* return_type =
            debug_type_find(&types, callee->type->ret_type);
        const char* linkage_name = callee->link_name
            ? callee->link_name : callee->name;
        int file_index = debug_line_file_index(
            files, file_count, callee->loc.filename);
        if (!return_type || file_index <= 0 || callee->loc.column < 0) {
            rcc_fatal("DWARF inline origin is missing source/type metadata");
        }
        if (info->size > UINT32_MAX) {
            rcc_fatal("DWARF inline-origin DIE offset exceeds 32-bit range");
        }
        inline_origins[index].die_offset = (uint32_t)info->size;
        section_add_byte(info, 40u);
        debug_line_u32(info, debug_str_add(
            strings, debug_function_source_name(callee, callee->name)));
        debug_line_u32(info, (uint32_t)file_index);
        debug_line_u32(info, (uint32_t)callee->loc.line);
        debug_line_u32(info, (uint32_t)callee->loc.column);
        debug_line_u32(info, debug_str_add(
            strings, linkage_name ? linkage_name : callee->name));
        debug_line_u32(info, return_type->offset);
        section_add_byte(info, 1u); /* DW_INL_inlined */
        section_add_byte(info, callee->type->has_prototype ? 1u : 0u);
        for (const DeclList* parameter = callee->func_params; parameter;
             parameter = parameter->next) {
            const Decl* declaration = parameter->decl;
            DebugTypeEntry* parameter_type;
            int parameter_file_index;
            if (!declaration || declaration->kind != DECL_PARAM ||
                !declaration->name || declaration->loc.line <= 0 ||
                declaration->loc.column < 0) {
                continue;
            }
            parameter_type = debug_type_find(&types, declaration->type);
            parameter_file_index = debug_line_file_index(
                files, file_count, declaration->loc.filename);
            if (!parameter_type || parameter_file_index <= 0) continue;
            if (info->size > UINT32_MAX) {
                rcc_fatal("DWARF inline-parameter DIE offset exceeds 32-bit range");
            }
            DebugInlineParameterOrigin* parameter_origin =
                rcc_alloc(sizeof(*parameter_origin));
            parameter_origin->function = callee;
            parameter_origin->declaration = declaration;
            parameter_origin->die_offset = (uint32_t)info->size;
            parameter_origin->next = inline_parameter_origins;
            inline_parameter_origins = parameter_origin;
            section_add_byte(info, 42u);
            debug_line_u32(info, debug_str_add(strings, declaration->name));
            debug_line_u32(info, parameter_type->offset);
            debug_line_u32(info, (uint32_t)parameter_file_index);
            debug_line_u32(info, (uint32_t)declaration->loc.line);
            debug_line_u32(info, (uint32_t)declaration->loc.column);
        }
        debug_emit_inline_local_origin(
            info, strings, &types, files, file_count, callee,
            &inline_local_origins);
        section_add_byte(info, 0u); /* end of abstract-origin children */
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
        debug_line_u32(info, (uint32_t)file_index);
        debug_line_u32(info, function->source_line);
        debug_line_u32(info, function->source_column);
        section_add_byte(info, function->is_global ? 1u : 0u);
        debug_line_u32(info, debug_str_add(strings, function->name));
        if (return_type) debug_line_u32(info, return_type->offset);
        /* The current C/C++ x86 backends retain a frame pointer, so expose
         * the same EBP/RBP base used by stack-local locations.  Full CFI and
         * unwind ranges remain a separate debug/unwind feature. */
        debug_expr_breg(info, g_opts.target_arch == ARCH_X64
                                  ? ARCH_X64 : ARCH_X86, 0, 0u, false);
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
             * object-pointer ref, followed by prototype status, before that
             * child begins. */
            debug_line_u32(info,
                           (uint32_t)(info->size + 2u * sizeof(uint32_t) +
                                      (function_method ? 1u : 0u) + 1u));
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
        if (function_abbreviation == 2u ||
            function_abbreviation == 26u ||
            function_abbreviation == 28u ||
            function_abbreviation == 29u ||
            function_abbreviation == 30u) {
            section_add_byte(
                info, function_decl && function_decl->type &&
                          function_decl->type->kind == TYPE_FUNC &&
                          function_decl->type->has_prototype ? 1u : 0u);
        }
        if (function_decl && function_decl->func_template_owner) {
            CxxTemplate* templ = function_decl->func_template_owner;
            for (int parameter_index = 0;
                 parameter_index < templ->param_count;
                 ++parameter_index) {
                TemplateParam* parameter = &templ->params[parameter_index];
                DebugTypeEntry* parameter_type = NULL;
                if (!parameter->name) continue;
                if (parameter->is_pack) {
                    int pack_length = function_decl->func_template_pack_counts
                        ? function_decl->func_template_pack_counts[
                              parameter_index]
                        : function_decl->func_template_pack_count;
                    int pack_offset = function_decl->func_template_pack_offsets
                        ? function_decl->func_template_pack_offsets[
                              parameter_index]
                        : 0;
                    if (parameter->kind == TPARAM_TYPE &&
                        function_decl->func_template_pack_count >= 0) {
                        section_add_byte(info, 50u);
                        debug_line_u32(
                            info, debug_str_add(strings, parameter->name));
                        for (int pack_index = 0;
                             pack_index < pack_length;
                             ++pack_index) {
                            Type* argument_type =
                                function_decl->func_template_pack_args[
                                    pack_offset + pack_index];
                            if (!argument_type || debug_type_is_dependent(
                                    argument_type, 0u)) {
                                continue;
                            }
                            parameter_type = debug_type_find(
                                &types, argument_type);
                            if (!parameter_type) continue;
                            section_add_byte(info, 51u);
                            debug_type_ref(info, NULL, parameter_type);
                        }
                        section_add_byte(info, 0u);
                    } else if (
                        parameter->kind == TPARAM_NONTYPE &&
                        function_decl->func_template_pack_count >= 0) {
                        Type* value_type =
                            debug_template_value_parameter_type(
                                templ, parameter,
                                function_decl->func_template_args,
                                function_decl->func_template_arg_count);
                        if (!debug_type_has_integral_constant_encoding(
                                value_type)) {
                            continue;
                        }
                        parameter_type = debug_type_find(
                            &types, value_type);
                        if (!parameter_type) {
                            rcc_fatal(
                                "DWARF function-template value-pack type was not collected");
                            return;
                        }
                        section_add_byte(info, 50u);
                        debug_line_u32(
                            info, debug_str_add(strings, parameter->name));
                        for (int value_index = 0;
                             value_index < pack_length;
                             ++value_index) {
                            if (!function_decl->func_template_pack_values ||
                                !function_decl->func_template_pack_value_present ||
                                !function_decl->func_template_pack_value_present[
                                    pack_offset + value_index]) {
                                continue;
                            }
                            section_add_byte(
                                info, value_type->is_unsigned
                                    ? 53u : 52u);
                            debug_type_ref(info, NULL, parameter_type);
                            if (value_type->is_unsigned) {
                                debug_line_uleb(
                                    info, (uint64_t)
                                        function_decl->func_template_pack_values[
                                            pack_offset + value_index]);
                            } else {
                                debug_line_sleb(
                                    info,
                                    function_decl->func_template_pack_values[
                                        pack_offset + value_index]);
                            }
                        }
                        section_add_byte(info, 0u);
                    }
                    continue;
                }
                if (parameter_index >=
                    function_decl->func_template_arg_count) {
                    continue;
                }
                if (parameter->kind == TPARAM_TEMPLATE) {
                    Type* template_argument = function_decl->func_template_args
                        ? function_decl->func_template_args[parameter_index]
                        : NULL;
                    const char* template_name =
                        debug_template_argument_name(template_argument);
                    if (!template_name || template_name[0] == '\0') {
                        rcc_fatal(
                            "DWARF function-template template argument is missing");
                        return;
                    }
                    section_add_byte(info, 54u);
                    debug_line_u32(
                        info, debug_str_add(strings, parameter->name));
                    debug_line_u32(
                        info, debug_str_add(strings, template_name));
                } else if (parameter->kind == TPARAM_TYPE) {
                    Type* argument_type = function_decl->func_template_args
                        ? function_decl->func_template_args[parameter_index]
                        : NULL;
                    if (!argument_type ||
                        debug_type_is_dependent(argument_type, 0u)) {
                        continue;
                    }
                    parameter_type = debug_type_find(&types, argument_type);
                    if (!parameter_type) {
                        rcc_fatal(
                            "DWARF function-template argument type was not collected");
                        return;
                    }
                    section_add_byte(info, 47u);
                    debug_line_u32(
                        info, debug_str_add(strings, parameter->name));
                    debug_type_ref(info, NULL, parameter_type);
                } else if (parameter->kind == TPARAM_NONTYPE &&
                           function_decl->func_template_value_args &&
                           function_decl->func_template_value_present &&
                           function_decl->func_template_value_present[
                               parameter_index]) {
                    Type* value_type =
                        debug_template_value_parameter_type(
                            templ, parameter,
                            function_decl->func_template_args,
                            function_decl->func_template_arg_count);
                    if (!debug_type_has_integral_constant_encoding(
                            value_type)) {
                        continue;
                    }
                    parameter_type = debug_type_find(&types,
                                                     value_type);
                    if (!parameter_type) {
                        rcc_fatal(
                            "DWARF function-template value type was not collected");
                        return;
                    }
                    section_add_byte(
                        info, value_type->is_unsigned ? 49u : 48u);
                    debug_line_u32(
                        info, debug_str_add(strings, parameter->name));
                    debug_type_ref(info, NULL, parameter_type);
                    if (value_type->is_unsigned) {
                        debug_line_uleb(
                            info, (uint64_t)function_decl->func_template_value_args[
                                parameter_index]);
                    } else {
                        debug_line_sleb(
                            info, function_decl->func_template_value_args[
                                parameter_index]);
                    }
                }
            }
        }
        debug_emit_function_locals(
            obj, info, strings, &types, files, file_count, mod, filename,
            info_section, function,
            g_opts.target_arch == ARCH_X64 ? ARCH_X64 : ARCH_X86);
        debug_emit_inline_calls(
            obj, info, files, file_count, filename, info_section, mod,
            function, function_decl, inline_origins, inline_origin_count,
            inline_local_origins, inline_parameter_origins);
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
    rcc_free(inline_origins);
    while (inline_local_origins) {
        DebugInlineLocalOrigin* next = inline_local_origins->next;
        rcc_free(inline_local_origins);
        inline_local_origins = next;
    }
    while (inline_parameter_origins) {
        DebugInlineParameterOrigin* next = inline_parameter_origins->next;
        rcc_free(inline_parameter_origins);
        inline_parameter_origins = next;
    }
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

static void debug_frame_emit_saved_offsets(
    ObjSection* frame, const Module* mod, const ModuleSymbol* function,
    uint64_t function_end, uint64_t through_pc, uint32_t pointer_size) {
    for (size_t index = 0u; index < mod->debug_frame_save_count; ++index) {
        const ModuleDebugFrameSave* save = &mod->debug_frame_saves[index];
        uint64_t cfa_offset;
        if (save->save_pc < function->offset ||
            save->save_pc >= function_end || save->save_pc > through_pc) {
            continue;
        }
        if (save->dwarf_register > 63u ||
            save->frame_offset > UINT32_MAX - 2u * pointer_size) {
            rcc_fatal("DWARF saved-register location is invalid");
            return;
        }
        cfa_offset = (uint64_t)save->frame_offset + 2u * pointer_size;
        if (cfa_offset % pointer_size != 0u) {
            rcc_fatal("DWARF saved-register offset is not pointer aligned");
            return;
        }
        section_add_byte(frame, (uint8_t)(0x80u + save->dwarf_register));
        debug_line_uleb(frame, cfa_offset / pointer_size);
    }
}

static void debug_frame_emit_saved_restores(
    ObjSection* frame, const Module* mod, const ModuleSymbol* function,
    uint64_t function_end) {
    for (size_t index = 0u; index < mod->debug_frame_save_count; ++index) {
        const ModuleDebugFrameSave* save = &mod->debug_frame_saves[index];
        if (save->save_pc < function->offset ||
            save->save_pc >= function_end) continue;
        if (save->dwarf_register > 63u) {
            rcc_fatal("DWARF restored-register number is invalid");
            return;
        }
        section_add_byte(frame, (uint8_t)(0xc0u + save->dwarf_register));
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

/* RIN v3 keeps runtime unwind bytes in a loadable section.  The compiler's
 * existing producer is the DWARF .debug_frame encoder below, so retain one
 * canonical byte stream and copy it to the non-loadable debug section only
 * when debug information was requested.  Relocations must be copied too:
 * FDE initial locations are absolute pointers after the RIN loader applies
 * the object relocation. */
static ObjSection* module_copy_debug_frame_section(
    ObjectFile* obj, const ObjSection* source) {
    ObjSection* copy;
    int copy_index;
    if (!obj || !source) return NULL;
    copy = objfile_add_section(obj, ".debug_frame", SECT_DEBUG_FRAME, 0u);
    copy->align = source->align;
    section_add_data(copy, source->data, source->size);
    copy_index = objfile_section_index(obj, copy);
    if (copy_index < 0) return NULL;
    for (const ObjReloc* relocation = source->relocs;
         relocation; relocation = relocation->next) {
        objfile_add_reloc(obj, copy_index, relocation->offset,
                          relocation->symbol_name, relocation->type,
                          relocation->addend);
    }
    return copy;
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

    if (!obj || !mod || mod->symbol_count <= 0) return;
    functions = rcc_alloc((size_t)mod->symbol_count * sizeof(*functions));
    for (int index = 0; index < mod->symbol_count; ++index) {
        ModuleSymbol* symbol = &mod->symbols[index];
        if (!symbol->is_defined || symbol->section != MODULE_SYMBOL_CODE ||
            debug_frame_prologue_kind(mod, symbol) ==
                DEBUG_FRAME_PROLOGUE_NONE) continue;
        functions[function_count++] = symbol;
    }
    if (function_count == 0) {
        rcc_free(functions);
        return;
    }

    frame = objfile_add_section(obj, ".unwind", SECT_UNWIND,
                                SECT_FLAG_ALLOC);
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
    if (g_opts.target_arch == ARCH_X64) {
        static const uint8_t preserved_registers[] = {
            3u, 6u, 12u, 13u, 14u, 15u
        };
        for (size_t index = 0u;
             index < sizeof(preserved_registers); ++index) {
            section_add_byte(frame, 0x08u); /* DW_CFA_same_value */
            debug_line_uleb(frame, preserved_registers[index]);
        }
    } else {
        static const uint8_t preserved_registers[] = {3u, 5u, 6u, 7u};
        for (size_t index = 0u;
             index < sizeof(preserved_registers); ++index) {
            section_add_byte(frame, 0x08u); /* DW_CFA_same_value */
            debug_line_uleb(frame, preserved_registers[index]);
        }
    }
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
            section_add_byte(
                frame, (uint8_t)((uint64_t)function_size >> (byte * 8u)));
        }

        /* push fp: the saved frame pointer is two words below the new CFA. */
        debug_frame_advance(frame, 1u);
        section_add_byte(frame, 0x0eu); /* DW_CFA_def_cfa_offset */
        debug_line_uleb(frame, pointer_size * 2u);
        section_add_byte(frame, (uint8_t)(0x80u + frame_register));
        /* The pushed frame pointer is below both the return address and the
         * caller's CFA: CFA is SP + 2 words, so the saved FP is at CFA - 2
         * words.  The CIE's return-address rule remains CFA - 1 word. */
        debug_line_uleb(frame, 2u);
        if (prologue_kind == DEBUG_FRAME_PROLOGUE_I686_ALIGNED) {
            /* `and esp, mask` changes the stack base at PC 14, before the
             * saved EBP and return address are copied to the aligned stack.
             * During that window EAX and EDX hold those caller values, so
             * describe them as register rules rather than stale stack slots. */
            debug_frame_advance(frame, 13u);
            section_add_byte(frame, 0x0cu); /* DW_CFA_def_cfa */
            debug_line_uleb(frame, stack_register);
            debug_line_uleb(frame, pointer_size * 2u);
            section_add_byte(frame, 0x09u); /* DW_CFA_register */
            debug_line_uleb(frame, frame_register);
            debug_line_uleb(frame, 0u); /* saved EBP is in EAX */
            section_add_byte(frame, 0x09u); /* DW_CFA_register */
            debug_line_uleb(frame, return_register);
            debug_line_uleb(frame, 2u); /* return address is in EDX */
            /* Both values are now in their aligned stack slots. */
            debug_frame_advance(frame, 7u);
            section_add_byte(frame,
                             (uint8_t)(0x80u + frame_register));
            debug_line_uleb(frame, 2u);
            section_add_byte(frame,
                             (uint8_t)(0xc0u + return_register));
            /* Establish EBP as the stable CFA base after mov ebp,esp. */
            debug_frame_advance(frame, 2u);
        } else {
            /* mov fp, sp: subsequent locals use the stable frame register. */
            debug_frame_advance(frame, prologue_after_fp - 1u);
        }
        section_add_byte(frame, 0x0du); /* DW_CFA_def_cfa_register */
        debug_line_uleb(frame, frame_register);
        current_pc = prologue_after_fp;
        for (size_t save_index = 0u;
             save_index < mod->debug_frame_save_count; ++save_index) {
            const ModuleDebugFrameSave* save =
                &mod->debug_frame_saves[save_index];
            uint64_t relative_save_pc;
            if (save->save_pc < function->offset ||
                save->save_pc >= function_end) continue;
            relative_save_pc = save->save_pc - function->offset;
            if (relative_save_pc < current_pc ||
                relative_save_pc >= function_size) {
                rcc_fatal("DWARF saved-register PC is outside function '%s'",
                          function->name ? function->name : "<unnamed>");
                continue;
            }
            debug_frame_advance(frame, relative_save_pc - current_pc);
            debug_frame_emit_saved_offsets(
                frame, mod, function, function_end, save->save_pc,
                pointer_size);
            current_pc = relative_save_pc;
        }
        for (size_t epilogue_index = 0u;
             epilogue_index < mod->debug_frame_epilogue_count;
             ++epilogue_index) {
            const ModuleDebugFrameEpilogue* epilogue =
                &mod->debug_frame_epilogues[epilogue_index];
            uint64_t return_pc = epilogue->return_pc;
            uint64_t resume_pc = epilogue->resume_pc;
            uint64_t relative_pc;
            uint64_t relative_resume_pc;
            bool return_instruction_valid = false;
            if (return_pc <= function->offset ||
                return_pc >= function_end || resume_pc <= return_pc ||
                resume_pc > function_end || resume_pc > mod->code.size) {
                continue;
            }
            if (mod->code.data[return_pc] == 0xc3u &&
                resume_pc == return_pc + 1u) {
                return_instruction_valid = true;
            } else if (mod->code.data[return_pc] == 0xc2u &&
                       resume_pc == return_pc + 3u) {
                return_instruction_valid = true;
            }
            if (!return_instruction_valid) continue;
            relative_pc = return_pc - function->offset;
            relative_resume_pc = resume_pc - function->offset;
            if (relative_pc <= current_pc ||
                relative_pc >= function_size ||
                relative_resume_pc <= relative_pc ||
                relative_resume_pc > function_size) {
                continue;
            }
            debug_frame_advance(frame, relative_pc - current_pc);
            section_add_byte(frame, 0x0du); /* DW_CFA_def_cfa_register */
            debug_line_uleb(frame, stack_register);
            /* `leave` restores ESP to the return-address slot. */
            section_add_byte(frame, 0x0eu); /* DW_CFA_def_cfa_offset */
            debug_line_uleb(frame, pointer_size);
            section_add_byte(frame, (uint8_t)(0xc0u + frame_register));
            debug_frame_emit_saved_restores(
                frame, mod, function, function_end);
            current_pc = relative_pc;
            ++emitted_epilogues;

            /* A later branch target may resume the function after this
             * non-terminal return.  Restore the normal frame-body rules at
             * the return instruction's exact end address. */
            if (relative_resume_pc < function_size) {
                debug_frame_advance(frame, relative_resume_pc - current_pc);
                section_add_byte(frame, 0x0du);
                debug_line_uleb(frame, frame_register);
                section_add_byte(frame, 0x0eu);
                debug_line_uleb(frame, pointer_size * 2u);
                section_add_byte(frame,
                                 (uint8_t)(0x80u + frame_register));
                debug_line_uleb(frame, 2u);
                debug_frame_emit_saved_offsets(
                    frame, mod, function, function_end, function_end,
                    pointer_size);
                current_pc = relative_resume_pc;
            }
        }
        if (!mod->debug_verified_backend && emitted_epilogues == 0u &&
            after_leave > prologue_after_fp) {
            debug_frame_advance(frame, after_leave - current_pc);
            section_add_byte(frame, 0x0du); /* DW_CFA_def_cfa_register */
            debug_line_uleb(frame, stack_register);
            section_add_byte(frame, 0x0eu); /* DW_CFA_def_cfa_offset */
            debug_line_uleb(frame, pointer_size);
            section_add_byte(frame, (uint8_t)(0xc0u + frame_register));
            debug_frame_emit_saved_restores(
                frame, mod, function, function_end);
        }

        if (frame->size - fde_offset - 4u > UINT32_MAX) {
            rcc_fatal("DWARF FDE is too large");
        }
        debug_line_patch_u32(frame, fde_offset,
                             (uint32_t)(frame->size - fde_offset - 4u));
    }
    if (g_opts.debug_info && !module_copy_debug_frame_section(obj, frame)) {
        rcc_fatal("failed to copy RIN unwind data to DWARF debug_frame");
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
        uint64_t relocation_width = (mr->is_tls_index_module ||
                                     mr->is_tls_index_offset ||
                                     (!mr->is_relative && mr->is_64bit))
            ? 8u : 4u;
        RelocType type = mr->is_tls_index_module
            ? RELOC_TLSINDEX_MODULE64
            : mr->is_tls_index_offset ? RELOC_TLSINDEX_OFFSET64
            : mr->is_tls ? RELOC_TLSOFF32S : mr->is_got
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
