/*
 * RAR - RinOS Archiver
 * Static library archive format (.ra)
 */

#ifndef ARCHIVE_H
#define ARCHIVE_H

#include "rcc.h"
#include "objfile.h"

#define RA_MAGIC 0x0A324152u /* "RA2\n" */
#define RA_VERSION 0x00020000u

/* Archive file header */
typedef struct {
    uint32_t magic;
    uint32_t version;       /* Archive format version */
    uint32_t header_size;
    uint32_t flags;
    uint32_t member_count;  /* Number of members */
    uint32_t symbol_count;
    uint64_t member_off;
    uint64_t symtab_off;    /* Offset to symbol table */
    uint64_t strtab_off;    /* Offset to string table */
    uint64_t strtab_size;   /* Size of string table */
    uint64_t file_size;
} RaHeader;

/* Archive member header */
typedef struct {
    uint32_t name;          /* Offset in string table */
    uint16_t arch;
    uint16_t flags;
    uint64_t offset;        /* Offset in archive */
    uint64_t size;          /* Size of member */
    uint64_t mtime;         /* Reproducible archives use zero */
    uint8_t identity[16];
} RaMember;

/* Archive symbol table entry */
typedef struct {
    uint32_t name;          /* Offset in string table */
    uint32_t member;        /* Member index */
    uint64_t reserved;
} RaSymbol;

#if defined(__cplusplus)
static_assert(sizeof(RaHeader) == 64, "RaHeader v2 ABI drift");
static_assert(sizeof(RaMember) == 48, "RaMember v2 ABI drift");
static_assert(sizeof(RaSymbol) == 16, "RaSymbol v2 ABI drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(RaHeader) == 64, "RaHeader v2 ABI drift");
_Static_assert(sizeof(RaMember) == 48, "RaMember v2 ABI drift");
_Static_assert(sizeof(RaSymbol) == 16, "RaSymbol v2 ABI drift");
#endif

/* In-memory archive representation */
typedef struct ArchiveMember {
    char* name;             /* Member file name */
    uint8_t* data;          /* Member data */
    uint32_t size;          /* Member size */
    struct ArchiveMember* next;
} ArchiveMember;

typedef struct ArchiveSymbol {
    char* name;             /* Symbol name */
    int member_idx;         /* Which member defines it */
    struct ArchiveSymbol* next;
} ArchiveSymbol;

typedef struct Archive {
    char* filename;
    ArchiveMember* members;
    int member_count;
    ArchiveSymbol* symbols;
    int symbol_count;

    /* String table */
    char* strtab;
    uint32_t strtab_size;
    uint32_t strtab_cap;
} Archive;

/* Archive functions */
Archive* archive_new(const char* filename);
void archive_free(Archive* ar);

/* Member operations */
bool archive_add_member(Archive* ar, const char* filename);
bool archive_add_object(Archive* ar, ObjectFile* obj, const char* name);
ArchiveMember* archive_find_member(Archive* ar, const char* name);

/* I/O operations */
bool archive_write(Archive* ar, const char* filename);
Archive* archive_read(const char* filename);

/* Symbol table operations */
bool archive_build_symtab(Archive* ar);
int archive_find_symbol(Archive* ar, const char* name);

/* High-level archiver function */
bool rar_create(const char* output, char** inputs, int count);
bool rar_extract(const char* archive, const char* member, const char* output);
bool rar_list(const char* archive);

#endif /* ARCHIVE_H */
