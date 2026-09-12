/*
 * RAR - RinOS Archiver
 * Static library archive implementation
 */

#include "archive.h"
#include "objfile.h"
#include <inttypes.h>
#include <limits.h>
#include <string.h>

static bool archive_range(uint64_t offset, uint64_t size, uint64_t limit) {
    return offset <= limit && size <= limit - offset;
}

static bool archive_seek(FILE* file, uint64_t offset, int origin) {
    if (offset > (uint64_t)LONG_MAX) return false;
    return fseek(file, (long)offset, origin) == 0;
}

static uint64_t archive_tell(FILE* file) {
    long offset = ftell(file);
    return offset < 0 ? UINT64_MAX : (uint64_t)offset;
}

static bool archive_pad_to(FILE* file, uint64_t offset) {
    uint64_t current = archive_tell(file);
    if (current == UINT64_MAX || current > offset) return false;
    while (current++ < offset) {
        if (fputc(0, file) == EOF) return false;
    }
    return true;
}

/* ═══════════════════════════════════════
 * Archive Creation/Destruction
 * ═══════════════════════════════════════ */

Archive* archive_new(const char* filename) {
    Archive* ar = rcc_alloc(sizeof(Archive));
    ar->filename = rcc_strdup(filename);
    ar->members = NULL;
    ar->member_count = 0;
    ar->symbols = NULL;
    ar->symbol_count = 0;
    ar->strtab = rcc_alloc(256);
    ar->strtab[0] = '\0';
    ar->strtab_size = 1;
    ar->strtab_cap = 256;
    return ar;
}

void archive_free(Archive* ar) {
    if (!ar) return;

    /* Free members */
    ArchiveMember* m = ar->members;
    while (m) {
        ArchiveMember* next = m->next;
        rcc_free(m->name);
        rcc_free(m->data);
        rcc_free(m);
        m = next;
    }

    /* Free symbols */
    ArchiveSymbol* s = ar->symbols;
    while (s) {
        ArchiveSymbol* next = s->next;
        rcc_free(s->name);
        rcc_free(s);
        s = next;
    }

    rcc_free(ar->strtab);
    rcc_free(ar->filename);
    rcc_free(ar);
}

/* ═══════════════════════════════════════
 * String Table
 * ═══════════════════════════════════════ */

static uint32_t archive_add_string(Archive* ar, const char* str) {
    if (!str || !str[0]) return 0;

    size_t len = strlen(str) + 1;
    while (ar->strtab_size + len > ar->strtab_cap) {
        ar->strtab_cap *= 2;
        ar->strtab = rcc_realloc(ar->strtab, ar->strtab_cap);
    }

    uint32_t offset = ar->strtab_size;
    memcpy(ar->strtab + offset, str, len);
    ar->strtab_size += len;

    return offset;
}

/* ═══════════════════════════════════════
 * Member Operations
 * ═══════════════════════════════════════ */

bool archive_add_member(Archive* ar, const char* filename) {
    /* Read file */
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "rar: cannot open file: %s\n", filename);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || (uint64_t)size > SIZE_MAX) {
        fclose(f);
        fprintf(stderr, "rar: member is empty or exceeds the current memory limit: %s\n",
                filename);
        return false;
    }

    uint8_t* data = rcc_alloc(size);
    if (fread(data, size, 1, f) != 1) {
        fclose(f);
        rcc_free(data);
        fprintf(stderr, "rar: cannot read file: %s\n", filename);
        return false;
    }
    fclose(f);

    /* Create member */
    ArchiveMember* m = rcc_alloc(sizeof(ArchiveMember));

    /* Extract base name */
    const char* base = strrchr(filename, '/');
    if (!base) base = strrchr(filename, '\\');
    if (base) base++;
    else base = filename;

    m->name = rcc_strdup(base);
    m->data = data;
    m->size = (uint64_t)size;
    m->next = NULL;

    /* Append to list */
    if (!ar->members) {
        ar->members = m;
    } else {
        ArchiveMember* last = ar->members;
        while (last->next) last = last->next;
        last->next = m;
    }
    ar->member_count++;

    return true;
}

ArchiveMember* archive_find_member(Archive* ar, const char* name) {
    for (ArchiveMember* m = ar->members; m; m = m->next) {
        if (strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════
 * Symbol Table Building
 * ═══════════════════════════════════════ */

bool archive_build_symtab(Archive* ar) {
    int member_idx = 0;

    for (ArchiveMember* m = ar->members; m; m = m->next, member_idx++) {
        /* Check if it's an object file */
        if (m->size < sizeof(RoHeader)) continue;

        RoHeader* hdr = (RoHeader*)m->data;
        if (hdr->magic != RO_MAGIC || hdr->version != RO_VERSION ||
            hdr->header_size != sizeof(RoHeader) || hdr->file_size != m->size ||
            !archive_range(hdr->symbol_off,
                           (uint64_t)hdr->symbol_count * sizeof(RoSymbol),
                           m->size) ||
            !archive_range(hdr->strtab_off, hdr->strtab_size, m->size) ||
            hdr->strtab_size == 0u) continue;

        /* Read symbols from object file */
        if (hdr->symbol_count == 0) continue;

        /* Read string table */
        char* strtab = (char*)(m->data + hdr->strtab_off);

        /* Read symbols */
        RoSymbol* syms = (RoSymbol*)(m->data + hdr->symbol_off);
        for (uint32_t i = 0; i < hdr->symbol_count; i++) {
            RoSymbol* sym = &syms[i];

            /* Only add global defined symbols */
            if (sym->type != SYM_GLOBAL && sym->type != SYM_WEAK) continue;
            if (sym->section == 0) continue;  /* Undefined */

            if (sym->name >= hdr->strtab_size ||
                !memchr(strtab + sym->name, '\0',
                        (size_t)hdr->strtab_size - sym->name)) continue;

            const char* name = strtab + sym->name;

            /* Add to archive symbol table */
            ArchiveSymbol* as = rcc_alloc(sizeof(ArchiveSymbol));
            as->name = rcc_strdup(name);
            as->member_idx = member_idx;
            as->next = NULL;

            if (!ar->symbols) {
                ar->symbols = as;
            } else {
                ArchiveSymbol* last = ar->symbols;
                while (last->next) last = last->next;
                last->next = as;
            }
            ar->symbol_count++;
        }
    }

    return true;
}

int archive_find_symbol(Archive* ar, const char* name) {
    for (ArchiveSymbol* s = ar->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s->member_idx;
        }
    }
    return -1;
}

/* ═══════════════════════════════════════
 * Archive I/O
 * ═══════════════════════════════════════ */

bool archive_write(Archive* ar, const char* filename) {
    FILE* f = fopen(filename, "wb");
    uint64_t* member_offsets = NULL;
    uint32_t* member_names = NULL;
    uint32_t* symbol_names = NULL;
    if (!f) {
        fprintf(stderr, "rar: cannot create archive: %s\n", filename);
        return false;
    }

    /* Build symbol table */
    archive_build_symtab(ar);

    member_offsets = rcc_alloc(sizeof(uint64_t) * ar->member_count);
    member_names = rcc_alloc(sizeof(uint32_t) * ar->member_count);
    symbol_names = rcc_alloc(sizeof(uint32_t) * ar->symbol_count);
    int idx = 0;
    for (ArchiveMember* m = ar->members; m; m = m->next) {
        member_names[idx++] = archive_add_string(ar, m->name);
    }
    idx = 0;
    for (ArchiveSymbol* s = ar->symbols; s; s = s->next) {
        symbol_names[idx++] = archive_add_string(ar, s->name);
    }

    uint64_t member_off = sizeof(RaHeader);
    uint64_t offset = member_off +
        (uint64_t)ar->member_count * sizeof(RaMember);
    idx = 0;
    for (ArchiveMember* m = ar->members; m; m = m->next, idx++) {
        offset = (offset + 15u) & ~UINT64_C(15);
        member_offsets[idx] = offset;
        offset += m->size;
    }

    uint64_t symtab_off = (offset + 7u) & ~UINT64_C(7);
    uint64_t symtab_size = (uint64_t)ar->symbol_count * sizeof(RaSymbol);
    offset = symtab_off + symtab_size;
    uint64_t strtab_off = offset;
    uint64_t file_size = strtab_off + ar->strtab_size;

    RaHeader hdr = {0};
    hdr.magic = RA_MAGIC;
    hdr.version = RA_VERSION;
    hdr.header_size = sizeof(RaHeader);
    hdr.member_count = ar->member_count;
    hdr.symbol_count = ar->symbol_count;
    hdr.member_off = member_off;
    hdr.symtab_off = symtab_off;
    hdr.strtab_off = strtab_off;
    hdr.strtab_size = ar->strtab_size;
    hdr.file_size = file_size;
    fwrite(&hdr, sizeof(hdr), 1, f);

    idx = 0;
    for (ArchiveMember* m = ar->members; m; m = m->next, idx++) {
        RaMember mh = {0};
        mh.name = member_names[idx];
        mh.offset = member_offsets[idx];
        mh.size = m->size;
        if (m->size >= sizeof(RoHeader)) {
            const RoHeader* object = (const RoHeader*)m->data;
            if (object->magic == RO_MAGIC && object->version == RO_VERSION) {
                mh.arch = object->arch;
            }
        }
        fwrite(&mh, sizeof(mh), 1, f);
    }

    idx = 0;
    for (ArchiveMember* m = ar->members; m; m = m->next, idx++) {
        if (!archive_pad_to(f, member_offsets[idx])) goto write_failed;
        fwrite(m->data, m->size, 1, f);
    }

    if (!archive_pad_to(f, symtab_off)) goto write_failed;

    idx = 0;
    for (ArchiveSymbol* s = ar->symbols; s; s = s->next) {
        RaSymbol rs = {0};
        rs.name = symbol_names[idx++];
        rs.member = s->member_idx;
        fwrite(&rs, sizeof(rs), 1, f);
    }

    fwrite(ar->strtab, ar->strtab_size, 1, f);
    if (archive_tell(f) != file_size) goto write_failed;
    if (fclose(f) != 0) f = NULL;
    rcc_free(member_offsets);
    rcc_free(member_names);
    rcc_free(symbol_names);

    return f != NULL;

write_failed:
    if (f) fclose(f);
    rcc_free(member_offsets);
    rcc_free(member_names);
    rcc_free(symbol_names);
    fprintf(stderr, "rar: failed to write archive: %s\n", filename);
    return false;
}

Archive* archive_read(const char* filename) {
    FILE* f = fopen(filename, "rb");
    Archive* ar = NULL;
    RaMember* members = NULL;
    uint64_t actual_size;
    if (!f) return NULL;

    if (!archive_seek(f, 0, SEEK_END) ||
        (actual_size = archive_tell(f)) == UINT64_MAX ||
        !archive_seek(f, 0, SEEK_SET)) {
        fclose(f);
        return NULL;
    }
    RaHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    if (hdr.magic != RA_MAGIC || hdr.version != RA_VERSION ||
        hdr.header_size != sizeof(RaHeader) || hdr.flags != 0u ||
        hdr.file_size != actual_size || hdr.member_count > 65535u ||
        hdr.symbol_count > 1048576u || hdr.strtab_size == 0u ||
        hdr.strtab_size > UINT32_MAX ||
        !archive_range(hdr.member_off,
                       (uint64_t)hdr.member_count * sizeof(RaMember),
                       actual_size) ||
        !archive_range(hdr.symtab_off,
                       (uint64_t)hdr.symbol_count * sizeof(RaSymbol),
                       actual_size) ||
        !archive_range(hdr.strtab_off, hdr.strtab_size, actual_size)) {
        fclose(f);
        return NULL;
    }

    ar = archive_new(filename);
    ar->strtab = rcc_realloc(ar->strtab, (size_t)hdr.strtab_size);
    ar->strtab_size = (uint32_t)hdr.strtab_size;
    ar->strtab_cap = (uint32_t)hdr.strtab_size;
    if (!archive_seek(f, hdr.strtab_off, SEEK_SET) ||
        fread(ar->strtab, (size_t)hdr.strtab_size, 1, f) != 1 ||
        ar->strtab[0] != '\0') goto read_failed;

    members = rcc_alloc(sizeof(RaMember) * hdr.member_count);
    if (!archive_seek(f, hdr.member_off, SEEK_SET) ||
        (hdr.member_count != 0u &&
         fread(members, sizeof(RaMember), hdr.member_count, f) !=
            hdr.member_count)) goto read_failed;

    for (uint32_t i = 0; i < hdr.member_count; i++) {
        RaMember* mh = &members[i];
        if (mh->name >= hdr.strtab_size ||
            !memchr(ar->strtab + mh->name, '\0',
                    (size_t)hdr.strtab_size - mh->name) ||
            mh->arch > ARCH_X64 || mh->flags != 0u ||
            mh->size > SIZE_MAX ||
            !archive_range(mh->offset, mh->size, actual_size)) {
            goto read_failed;
        }

        ArchiveMember* m = rcc_alloc(sizeof(ArchiveMember));
        m->name = rcc_strdup(ar->strtab + mh->name);

        m->data = rcc_alloc((size_t)mh->size);
        m->size = mh->size;

        if (!archive_seek(f, mh->offset, SEEK_SET) ||
            (mh->size != 0u && fread(m->data, (size_t)mh->size, 1, f) != 1)) {
            rcc_free(m->name);
            rcc_free(m->data);
            rcc_free(m);
            goto read_failed;
        }

        m->next = NULL;

        if (!ar->members) {
            ar->members = m;
        } else {
            ArchiveMember* last = ar->members;
            while (last->next) last = last->next;
            last->next = m;
        }
        ar->member_count++;
    }

    if (!archive_seek(f, hdr.symtab_off, SEEK_SET)) goto read_failed;
    for (uint32_t i = 0; i < hdr.symbol_count; i++) {
        RaSymbol rs;
        if (fread(&rs, sizeof(rs), 1, f) != 1 ||
            rs.name >= hdr.strtab_size ||
            !memchr(ar->strtab + rs.name, '\0',
                    (size_t)hdr.strtab_size - rs.name) ||
            rs.member >= hdr.member_count || rs.reserved != 0u) {
            goto read_failed;
        }

        ArchiveSymbol* s = rcc_alloc(sizeof(ArchiveSymbol));
        s->name = rcc_strdup(ar->strtab + rs.name);
        s->member_idx = rs.member;
        s->next = NULL;

        if (!ar->symbols) {
            ar->symbols = s;
        } else {
            ArchiveSymbol* last = ar->symbols;
            while (last->next) last = last->next;
            last->next = s;
        }
        ar->symbol_count++;
    }

    rcc_free(members);
    fclose(f);

    return ar;

read_failed:
    rcc_free(members);
    archive_free(ar);
    fclose(f);
    return NULL;
}

/* ═══════════════════════════════════════
 * High-level Archiver Functions
 * ═══════════════════════════════════════ */

bool rar_create(const char* output, char** inputs, int count) {
    Archive* ar = archive_new(output);

    for (int i = 0; i < count; i++) {
        if (!archive_add_member(ar, inputs[i])) {
            archive_free(ar);
            return false;
        }
    }

    bool ok = archive_write(ar, output);
    archive_free(ar);
    return ok;
}

bool rar_list(const char* filename) {
    Archive* ar = archive_read(filename);
    if (!ar) {
        fprintf(stderr, "rar: cannot open archive: %s\n", filename);
        return false;
    }

    printf("Archive: %s\n", filename);
    printf("Members: %d\n", ar->member_count);
    printf("Symbols: %d\n", ar->symbol_count);
    printf("\n");

    printf("Members:\n");
    for (ArchiveMember* m = ar->members; m; m = m->next) {
        printf("  %-20s %8" PRIu64 " bytes\n", m->name, m->size);
    }

    printf("\nSymbols:\n");
    int idx = 0;
    for (ArchiveSymbol* s = ar->symbols; s; s = s->next, idx++) {
        /* Find member name */
        const char* member_name = "?";
        int i = 0;
        for (ArchiveMember* m = ar->members; m; m = m->next, i++) {
            if (i == s->member_idx) {
                member_name = m->name;
                break;
            }
        }
        printf("  %-30s -> %s\n", s->name, member_name);
    }

    archive_free(ar);
    return true;
}

bool rar_extract(const char* archive_file, const char* member_name, const char* output) {
    Archive* ar = archive_read(archive_file);
    if (!ar) {
        fprintf(stderr, "rar: cannot open archive: %s\n", archive_file);
        return false;
    }

    ArchiveMember* m = archive_find_member(ar, member_name);
    if (!m) {
        fprintf(stderr, "rar: member not found: %s\n", member_name);
        archive_free(ar);
        return false;
    }

    const char* out_file = output ? output : member_name;
    FILE* f = fopen(out_file, "wb");
    if (!f) {
        fprintf(stderr, "rar: cannot create file: %s\n", out_file);
        archive_free(ar);
        return false;
    }

    fwrite(m->data, m->size, 1, f);
    fclose(f);

    printf("rar: extracted '%s' -> '%s'\n", member_name, out_file);

    archive_free(ar);
    return true;
}
