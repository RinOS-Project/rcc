/*
 * RCC++ - RinOS C++ Compiler
 * C++ AST Implementation
 */

#include "ast_cxx.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* C++ semantic objects share the translation-unit arena with the C AST. */
#define rcc_alloc ast_arena_alloc
#define rcc_strdup ast_arena_strdup

/* Global namespace */
CxxNamespace* g_global_namespace = NULL;

/* ═══════════════════════════════════════
 * Name Mangling (Itanium C++ ABI style)
 * ═══════════════════════════════════════ */

static const char* cxx_operator_abi_code(const char* name) {
    if (!name) return NULL;
    if (strcmp(name, "operator new") == 0) return "nw";
    if (strcmp(name, "operator delete") == 0) return "dl";
    if (strcmp(name, "operator new[]") == 0) return "na";
    if (strcmp(name, "operator delete[]") == 0) return "da";
    if (strcmp(name, "operator=") == 0) return "aS";
    if (strcmp(name, "operator+") == 0) return "pl";
    if (strcmp(name, "operator-") == 0) return "mi";
    if (strcmp(name, "operator*") == 0) return "ml";
    if (strcmp(name, "operator/") == 0) return "dv";
    if (strcmp(name, "operator%") == 0) return "rm";
    if (strcmp(name, "operator++") == 0) return "pp";
    if (strcmp(name, "operator--") == 0) return "mm";
    if (strcmp(name, "operator&") == 0) return "an";
    if (strcmp(name, "operator|") == 0) return "or";
    if (strcmp(name, "operator^") == 0) return "eo";
    if (strcmp(name, "operator~") == 0) return "co";
    if (strcmp(name, "operator!") == 0) return "nt";
    if (strcmp(name, "operator==") == 0) return "eq";
    if (strcmp(name, "operator!=") == 0) return "ne";
    if (strcmp(name, "operator<") == 0) return "lt";
    if (strcmp(name, "operator>") == 0) return "gt";
    if (strcmp(name, "operator<=") == 0) return "le";
    if (strcmp(name, "operator>=") == 0) return "ge";
    if (strcmp(name, "operator&&") == 0) return "aa";
    if (strcmp(name, "operator||") == 0) return "oo";
    if (strcmp(name, "operator<<") == 0) return "ls";
    if (strcmp(name, "operator>>") == 0) return "rs";
    if (strcmp(name, "operator+=") == 0) return "pL";
    if (strcmp(name, "operator-=") == 0) return "mI";
    if (strcmp(name, "operator*=") == 0) return "mL";
    if (strcmp(name, "operator/=") == 0) return "dV";
    if (strcmp(name, "operator%=") == 0) return "rM";
    if (strcmp(name, "operator&=") == 0) return "aN";
    if (strcmp(name, "operator|=") == 0) return "oR";
    if (strcmp(name, "operator^=") == 0) return "eO";
    if (strcmp(name, "operator<<=") == 0) return "lS";
    if (strcmp(name, "operator>>=") == 0) return "rS";
    if (strcmp(name, "operator,") == 0) return "cm";
    if (strcmp(name, "operator->*") == 0) return "pm";
    if (strcmp(name, "operator.*") == 0) return "ds";
    if (strcmp(name, "operator->") == 0) return "pt";
    if (strcmp(name, "operator()") == 0) return "cl";
    if (strcmp(name, "operator[]") == 0) return "ix";
    if (strcmp(name, "operator co_await") == 0) return "aw";
    return NULL;
}

/* Mangle a length-prefixed name or a standard C++ operator name. */
static void mangle_name(char* buf, size_t* pos, const char* name) {
    const char* operator_code = cxx_operator_abi_code(name);
    size_t len;
    if (operator_code) {
        len = strlen(operator_code);
        if (*pos + len >= 256u) rcc_fatal("C++ mangled name is too long");
        memcpy(buf + *pos, operator_code, len);
        *pos += len;
        return;
    }
    len = strlen(name);
    if (*pos + 32u + len >= 256u) rcc_fatal("C++ mangled name is too long");
    *pos += (size_t)snprintf(buf + *pos, 256u - *pos, "%lu%s",
                             (unsigned long)len, name);
}

static void mangle_nested_prefix(char* buf, size_t* pos,
                                  CxxNamespace* ns, CxxClass* cls) {
    CxxNamespace* ns_stack[32];
    int ns_count = 0;

    buf[(*pos)++] = '_';
    buf[(*pos)++] = 'Z';
    if (!ns && !cls) return;
    buf[(*pos)++] = 'N';
    for (CxxNamespace* current = ns; current && current->name;
         current = current->parent) {
        if (ns_count < (int)(sizeof(ns_stack) / sizeof(ns_stack[0]))) {
            ns_stack[ns_count++] = current;
        }
    }
    for (int index = ns_count - 1; index >= 0; --index) {
        mangle_name(buf, pos, ns_stack[index]->name);
    }
    if (cls) mangle_name(buf, pos, cls->name);
}

static void cxx_mangle_type_append(char* buf, size_t* pos, Type* type);

static void cxx_mangle_type_char(char* buf, size_t* pos, char value) {
    if (*pos + 1u >= 256u) {
        rcc_fatal("C++ type name is too long");
    }
    buf[(*pos)++] = value;
}

/* Append a type without recursively reusing the public static result buffer.
 * A type can contain another function type (for example `int (*)(int)`), so
 * every recursive call must keep writing at the caller's current position. */
static void cxx_mangle_type_append(char* buf, size_t* pos, Type* type) {
    if (!type) {
        cxx_mangle_type_char(buf, pos, 'v');
        return;
    }

    while (type->kind == TYPE_PTR) {
        cxx_mangle_type_char(buf, pos,
                             type->is_reference
                                 ? (type->is_rvalue_reference ? 'O' : 'R')
                                 : 'P');
        type = type->base;
    }

    if (type->is_const) cxx_mangle_type_char(buf, pos, 'K');

    switch (type->kind) {
        case TYPE_VOID:   cxx_mangle_type_char(buf, pos, 'v'); break;
        case TYPE_BOOL:   cxx_mangle_type_char(buf, pos, 'b'); break;
        case TYPE_CHAR:   cxx_mangle_type_char(buf, pos,
                              type->is_unsigned ? 'h' : 'c'); break;
        case TYPE_SHORT:  cxx_mangle_type_char(buf, pos,
                              type->is_unsigned ? 't' : 's'); break;
        case TYPE_INT:    cxx_mangle_type_char(buf, pos,
                              type->is_unsigned ? 'j' : 'i'); break;
        case TYPE_LONG:   cxx_mangle_type_char(buf, pos,
                              type->is_unsigned ? 'm' : 'l'); break;
        case TYPE_LLONG:  cxx_mangle_type_char(buf, pos,
                              type->is_unsigned ? 'y' : 'x'); break;
        case TYPE_FLOAT:  cxx_mangle_type_char(buf, pos, 'f'); break;
        case TYPE_DOUBLE: cxx_mangle_type_char(buf, pos, 'd'); break;
        case TYPE_NULLPTR:
            cxx_mangle_type_char(buf, pos, 'D');
            cxx_mangle_type_char(buf, pos, 'n');
            break;
        case TYPE_STRUCT:
        case TYPE_UNION:
            if (!type->tag) rcc_fatal("C++ anonymous type cannot be mangled");
            mangle_name(buf, pos, type->tag);
            break;
        case TYPE_ARRAY: {
            char base_mangled[256];
            size_t base_pos = 0u;
            int written;
            cxx_mangle_type_append(base_mangled, &base_pos, type->base);
            base_mangled[base_pos] = '\0';
            written = type->array_len >= 0
                ? snprintf(buf + *pos, 256u - *pos, "A%d%s",
                           type->array_len, base_mangled)
                : snprintf(buf + *pos, 256u - *pos, "A_%s", base_mangled);
            if (written < 0 || (size_t)written >= 256u - *pos) {
                rcc_fatal("C++ array type name is too long");
            }
            *pos += (size_t)written;
            break;
        }
        case TYPE_FUNC:
            cxx_mangle_type_char(buf, pos, 'F');
            cxx_mangle_type_append(buf, pos, type->ret_type);
            if (type->params) {
                for (TypeParam* parameter = type->params; parameter;
                     parameter = parameter->next) {
                    cxx_mangle_type_append(buf, pos, parameter->type);
                }
            } else {
                cxx_mangle_type_char(buf, pos, 'v');
            }
            if (type->variadic) cxx_mangle_type_char(buf, pos, 'z');
            cxx_mangle_type_char(buf, pos, 'E');
            break;
        default:
            rcc_fatal("unsupported C++ type in name mangling");
    }
}

/* Mangle a type */
char* cxx_mangle_type(Type* type) {
    static char buf[256];
    size_t pos = 0u;
    cxx_mangle_type_append(buf, &pos, type);
    buf[pos] = '\0';
    return buf;
}

/* Mangle a simple name with namespace */
char* cxx_mangle_name(const char* name, CxxNamespace* ns, CxxClass* cls) {
    static char buf[512];
    size_t pos = 0;

    /* _Z prefix for mangled names */
    mangle_nested_prefix(buf, &pos, ns, cls);

    /* Nested name indicator */
    if (ns || cls) {
        /* Member name */
        mangle_name(buf, &pos, name);

        buf[pos++] = 'E';  /* End nested name */
    } else {
        /* Simple name */
        mangle_name(buf, &pos, name);
    }

    buf[pos] = '\0';
    return buf;
}

/* Mangle a function */
char* cxx_mangle_function(Decl* func, CxxNamespace* ns, CxxClass* cls) {
    static char buf[1024];
    size_t pos = 0;
    bool is_const_method = false;

    if (func && cls) {
        for (struct CxxMember* member = cls->members; member;
             member = member->next) {
            if (member->method && member->method->decl == func) {
                is_const_method = member->method->is_const;
                break;
            }
        }
    }

    if (func && func->func_is_cxx_constructor && cls) {
        mangle_nested_prefix(buf, &pos, ns, cls);
        if (pos + 2u >= sizeof(buf)) {
            rcc_fatal("C++ constructor name is too long");
        }
        buf[pos++] = 'C';
        buf[pos++] = '1';
    } else if (func && func->func_is_cxx_destructor && cls) {
        mangle_nested_prefix(buf, &pos, ns, cls);
        if (pos + 2u >= sizeof(buf)) {
            rcc_fatal("C++ destructor name is too long");
        }
        buf[pos++] = 'D';
        buf[pos++] = '1';
    } else if (func && func->name &&
        strcmp(func->name, "operator conversion") == 0) {
        char* return_type;
        mangle_nested_prefix(buf, &pos, ns, cls);
        if (is_const_method) buf[pos++] = 'K';
        buf[pos++] = 'c';
        buf[pos++] = 'v';
        return_type = cxx_mangle_type(func->type ? func->type->ret_type : NULL);
        if (pos + strlen(return_type) + 2u >= sizeof(buf)) {
            rcc_fatal("C++ conversion operator name is too long");
        }
        strcpy(buf + pos, return_type);
        pos += strlen(return_type);
        buf[pos++] = 'E';
    } else {
        if (is_const_method && cls) {
            mangle_nested_prefix(buf, &pos, ns, cls);
            buf[pos++] = 'K';
            mangle_name(buf, &pos, func->name);
            buf[pos++] = 'E';
        } else {
            /* Get base mangled name */
            char* base = cxx_mangle_name(func->name, ns, cls);
            strcpy(buf, base);
            pos = strlen(buf);
        }
    }

    /* Add parameter types */
    if (func->func_params) {
        for (DeclList* p = func->func_params; p; p = p->next) {
            char* type_mangled = cxx_mangle_type(p->decl->type);
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", type_mangled);
        }
    } else {
        /* No parameters = void */
        buf[pos++] = 'v';
    }

    buf[pos] = '\0';
    return buf;
}

static char* cxx_mangle_function_template(Decl* func, CxxNamespace* ns,
                                           CxxClass* cls, CxxTemplate* tmpl,
                                           const int64_t* value_args,
                                           const bool* value_present) {
    static char buf[1024];
    char* base;
    size_t pos;
    bool has_value_argument = false;

    if (!func || !tmpl) return cxx_mangle_function(func, ns, cls);
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_NONTYPE && value_present &&
            value_present[index]) {
            has_value_argument = true;
            break;
        }
    }
    if (!has_value_argument) return cxx_mangle_function(func, ns, cls);

    base = cxx_mangle_name(func->name, ns, cls);
    strcpy(buf, base);
    pos = strlen(buf);
    buf[pos++] = 'I';
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind != TPARAM_NONTYPE || !value_present[index]) {
            continue;
        }
        buf[pos++] = 'L';
        {
            char* type_mangled = cxx_mangle_type(parameter->type);
            size_t type_length = strlen(type_mangled);
            if (pos + type_length >= sizeof(buf) - 32u) {
                rcc_fatal("C++ template function name is too long");
            }
            memcpy(buf + pos, type_mangled, type_length);
            pos += type_length;
        }
        if (value_args[index] < 0) {
            uint64_t magnitude = (uint64_t)(-(value_args[index] + 1)) + 1u;
            pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos,
                                    "n%lluE",
                                    (unsigned long long)magnitude);
        } else {
            pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%lldE",
                                    (long long)value_args[index]);
        }
    }
    buf[pos++] = 'E';
    if (func->func_params) {
        for (DeclList* parameter = func->func_params; parameter;
             parameter = parameter->next) {
            char* type_mangled = cxx_mangle_type(parameter->decl->type);
            pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%s",
                                    type_mangled);
        }
    } else {
        buf[pos++] = 'v';
    }
    buf[pos] = '\0';
    return buf;
}

/* ═══════════════════════════════════════
 * Class Operations (Core API)
 * ═══════════════════════════════════════ */

CxxClass* cxx_class_alloc(const char* name, bool is_struct) {
    CxxClass* cls = rcc_alloc(sizeof(CxxClass));
    cls->name = name ? rcc_strdup(name) : NULL;
    cls->is_struct = is_struct;
    cls->has_user_constructor = false;
    cls->has_nonpublic_field = false;
    cls->has_static_field = false;
    cls->has_field_initializer = false;
    cls->constructors = NULL;
    cls->bases = NULL;
    cls->base_count = 0;
    cls->base_offsets = NULL;
    cls->virtual_bases = NULL;
    cls->virtual_base_count = 0;
    cls->nonvirtual_size = 1;
    cls->members = NULL;
    cls->vtable = NULL;
    cls->vtable_size = 0;
    cls->secondary_vtables = NULL;
    cls->secondary_vtable_count = 0;
    cls->destructor_method = NULL;
    cls->type = type_struct(name);
    cls->type->cxx_class = cls;
    cls->size = 0;
    cls->align = 1;
    cls->fields = NULL;
    cls->ns = NULL;
    cls->templ = NULL;
    cls->template_args = NULL;
    cls->template_arg_count = 0;
    return cls;
}

void cxx_class_add_base_ptr(CxxClass* cls, CxxClass* base, AccessSpec access, bool is_virtual) {
    cls->bases = ast_arena_grow(
        cls->bases, sizeof(cls->bases[0]) * (size_t)cls->base_count,
        sizeof(cls->bases[0]) * (size_t)(cls->base_count + 1));
    cls->bases[cls->base_count].base = base;
    cls->bases[cls->base_count].base_name = NULL;
    cls->bases[cls->base_count].access = access;
    cls->bases[cls->base_count].is_virtual = is_virtual;
    cls->base_count++;
}

void cxx_class_add_member(CxxClass* cls, Decl* decl, AccessSpec access, bool is_static) {
    struct CxxMember* member = rcc_alloc(sizeof(struct CxxMember));
    member->access = access;
    member->decl = decl;
    member->method = NULL;
    member->is_static = is_static;
    member->is_virtual = false;
    member->is_pure_virtual = false;
    member->is_override = false;
    member->is_final = false;
    member->next = NULL;

    /* Append to member list */
    if (!cls->members) {
        cls->members = member;
    } else {
        struct CxxMember* m = cls->members;
        while (m->next) m = m->next;
        m->next = member;
    }
}

static int cxx_virtual_base_index(CxxClass* cls, CxxClass* base);
static void cxx_add_virtual_base(CxxClass* cls, CxxClass* base,
                                 bool public_path);

void cxx_class_compute_layout(CxxClass* cls) {
    int offset = 0;
    int max_align = 1;
    bool layout_complete = true;
    bool bitfield_active = false;
    int bitfield_offset = 0;
    int bitfield_size = 0;
    int bitfield_alignment = 0;
    unsigned bitfield_used = 0u;
    TypeField** field_tail;
    int* base_offsets = NULL;
    bool nontrivial = cls->has_user_constructor || cls->has_field_initializer ||
                      cls->base_count > 0;

    cls->type->fields = NULL;
    field_tail = &cls->type->fields;

    /* Space for vptr if class has virtual functions.  A primary virtual base
     * owns the first vptr slot; derived objects reuse that slot for their
     * most-derived table. */
    bool has_virtual = false;
    CxxClass* primary_vtable_base = NULL;
    bool has_destructor = false;
    for (struct CxxMember* m = cls->members; m; m = m->next) {
        if (m->is_virtual) {
            has_virtual = true;
        }
        if (m->method && m->method->is_destructor) has_destructor = true;
    }
    for (int i = 0; i < cls->base_count; ++i) {
        CxxClass* base = cls->bases[i].base;
        if (base && !cls->bases[i].is_virtual && base->vtable_size > 0) {
            has_virtual = true;
            if (!primary_vtable_base) primary_vtable_base = base;
        }
    }

    if (has_virtual && !primary_vtable_base) {
        int pointer_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
        offset = pointer_size;  /* vptr */
        max_align = pointer_size;
    }

    if (cls->base_count > 0) {
        base_offsets = ast_arena_alloc(
            sizeof(*base_offsets) * (size_t)cls->base_count);
    }
    cls->base_offsets = base_offsets;

    /* Base class subobjects.  Keep the offset separately so inherited fields
     * can be appended after the derived fields; this preserves C++ name
     * hiding when a derived class declares a member with the same name. */
    for (int i = 0; i < cls->base_count; i++) {
        CxxClass* base = cls->bases[i].base;
        if (base && !cls->bases[i].is_virtual) {
            if (base == primary_vtable_base) {
                base_offsets[i] = 0;
                if (offset < base->nonvirtual_size) {
                    offset = base->nonvirtual_size;
                }
                if (base->align > max_align) max_align = base->align;
                continue;
            }
            /* Align for base */
            int align = base->align;
            offset = (offset + align - 1) & ~(align - 1);
            base_offsets[i] = offset;
            /* Base subobject */
            offset += base->nonvirtual_size;
            if (align > max_align) max_align = align;
        } else if (!base) {
            base_offsets[i] = -1;
            layout_complete = false;
        } else {
            base_offsets[i] = -1;
        }
    }

    /* Collect the virtual-base closure before assigning offsets.  A
     * most-derived object owns one subobject for each distinct virtual base,
     * including virtual bases reached through a non-virtual intermediate. */
    cls->virtual_bases = NULL;
    cls->virtual_base_count = 0;
    for (int i = 0; i < cls->base_count; ++i) {
        CxxClass* base = cls->bases[i].base;
        if (!base) continue;
        if (cls->bases[i].is_virtual) {
            cxx_add_virtual_base(
                cls, base, cls->bases[i].access == ACCESS_PUBLIC);
        }
        for (int nested = 0; nested < base->virtual_base_count; ++nested) {
            cxx_add_virtual_base(
                cls, base->virtual_bases[nested].base,
                cls->bases[i].access == ACCESS_PUBLIC &&
                base->virtual_bases[nested].public_path);
        }
    }

    /* Fields from fields list */
    for (TypeParam* f = cls->fields; f; f = f->next) {
        Type* type = f->type;
        int align;
        int size;
        TypeField* field;

        /* Static data members have storage independent of every object and
         * are materialized as translation-unit declarations. */
        if (f->is_static) continue;

        if (!type || !type_is_complete(type) || type->kind == TYPE_FUNC ||
            type->kind == TYPE_VOID) {
            layout_complete = false;
            continue;
        }
        if (type->cxx_nontrivial) nontrivial = true;
        align = type->align;
        size = type->size;

        if (f->is_bitfield) {
            unsigned storage_bits = (unsigned)size * 8u;
            if (f->bit_width == 0u) {
                bitfield_active = false;
                offset = (offset + align - 1) & ~(align - 1);
                if (align > max_align) max_align = align;
                continue;
            }
            if (!bitfield_active || bitfield_size != size ||
                bitfield_alignment != align ||
                bitfield_used + f->bit_width > storage_bits) {
                bitfield_active = true;
                bitfield_offset = (offset + align - 1) & ~(align - 1);
                bitfield_size = size;
                bitfield_alignment = align;
                bitfield_used = 0u;
                offset = bitfield_offset + size;
            }
            if (f->name) {
                field = ast_arena_alloc(sizeof(*field));
                field->name = f->name;
                field->type = type;
                field->offset = bitfield_offset;
                field->is_bitfield = true;
                field->bit_width = f->bit_width;
                field->bit_offset = bitfield_used;
                field->from_virtual_base = false;
                field->initializer = f->initializer;
                field->cxx_access = f->cxx_access;
                field->next = NULL;
                *field_tail = field;
                field_tail = &field->next;
            }
            bitfield_used += f->bit_width;
            if (align > max_align) max_align = align;
            continue;
        }
        bitfield_active = false;

        /* Align */
        offset = (offset + align - 1) & ~(align - 1);
        field = ast_arena_alloc(sizeof(*field));
        field->name = f->name;
        field->type = type;
        field->offset = offset;
        field->is_bitfield = false;
        field->bit_width = 0u;
        field->bit_offset = 0u;
        field->from_virtual_base = false;
        field->initializer = f->initializer;
        field->cxx_access = f->cxx_access;
        field->next = NULL;
        *field_tail = field;
        field_tail = &field->next;
        offset += size;

        if (align > max_align) max_align = align;
    }

    /* Expose inherited data members through the derived TypeField list.  The
     * actual storage remains in the base subobject at base_offsets[i]. */
    for (int i = 0; i < cls->base_count; i++) {
        CxxClass* base = cls->bases[i].base;
        if (!base || cls->bases[i].is_virtual || base_offsets[i] < 0) {
            continue;
        }
        for (TypeField* base_field = base->type->fields;
             base_field; base_field = base_field->next) {
            if (base_field->from_virtual_base) continue;
            TypeField* field = ast_arena_alloc(sizeof(*field));
            unsigned char access = base_field->cxx_access;
            if (cls->bases[i].access == ACCESS_PRIVATE) {
                access = ACCESS_PRIVATE;
            } else if (cls->bases[i].access == ACCESS_PROTECTED &&
                       access == ACCESS_PUBLIC) {
                access = ACCESS_PROTECTED;
            }
            field->name = base_field->name;
            field->type = base_field->type;
            field->offset = base_offsets[i] + base_field->offset;
            field->is_bitfield = base_field->is_bitfield;
            field->bit_width = base_field->bit_width;
            field->bit_offset = base_field->bit_offset;
            field->from_virtual_base = false;
            field->initializer = base_field->initializer;
            field->cxx_access = access;
            field->next = NULL;
            *field_tail = field;
            field_tail = &field->next;
        }
    }

    /* Non-static data members */
    for (struct CxxMember* m = cls->members; m; m = m->next) {
        if (!m->is_static && m->decl->kind == DECL_VAR) {
            Type* type = m->decl->type;
            int align = type->align;
            int size = type->size;

            /* Align */
            offset = (offset + align - 1) & ~(align - 1);
            m->decl->var_offset = offset;
            offset += size;

            if (align > max_align) max_align = align;
        }
    }

    /* Save the size used when this class is embedded as a non-virtual base;
     * virtual subobjects belong to the final most-derived object. */
    cls->nonvirtual_size = (offset + max_align - 1) & ~(max_align - 1);
    if (cls->nonvirtual_size == 0) cls->nonvirtual_size = 1;

    /* Materialize every distinct reachable virtual base once.  Only the
     * non-virtual portion is copied here; nested virtual bases have their own
     * entries in this table. */
    for (int virtual_index = 0;
         virtual_index < cls->virtual_base_count; ++virtual_index) {
        CxxClass* base = cls->virtual_bases[virtual_index].base;
        if (!base || !type_is_complete(base->type) ||
            base->nonvirtual_size <= 0 || base->align <= 0) {
            layout_complete = false;
            continue;
        }
        offset = (offset + base->align - 1) & ~(base->align - 1);
        cls->virtual_bases[virtual_index].offset = offset;
        offset += base->nonvirtual_size;
        if (base->align > max_align) max_align = base->align;
        for (TypeField* base_field = base->type->fields;
             base_field; base_field = base_field->next) {
            if (base_field->from_virtual_base) continue;
            TypeField* field = ast_arena_alloc(sizeof(*field));
            unsigned char access = base_field->cxx_access;
            if (!cls->virtual_bases[virtual_index].public_path) {
                access = ACCESS_PRIVATE;
            }
            field->name = base_field->name;
            field->type = base_field->type;
            field->offset = cls->virtual_bases[virtual_index].offset +
                            base_field->offset;
            field->is_bitfield = base_field->is_bitfield;
            field->bit_width = base_field->bit_width;
            field->bit_offset = base_field->bit_offset;
            field->from_virtual_base = true;
            field->initializer = base_field->initializer;
            field->cxx_access = access;
            field->next = NULL;
            *field_tail = field;
            field_tail = &field->next;
        }
    }

    /* Direct virtual-base entries use the same shared offset as the closure
     * table.  This keeps conversions and field layout on one source of truth. */
    for (int i = 0; i < cls->base_count; ++i) {
        if (cls->bases[i].is_virtual && cls->bases[i].base) {
            int virtual_index = cxx_virtual_base_index(
                cls, cls->bases[i].base);
            base_offsets[i] = virtual_index >= 0
                ? cls->virtual_bases[virtual_index].offset : -1;
        }
    }

    /* Final size with alignment padding */
    cls->size = (offset + max_align - 1) & ~(max_align - 1);
    if (cls->size == 0) cls->size = 1;  /* Empty class has size 1 */
    cls->align = max_align;
    cls->type->size = cls->size;
    cls->type->align = cls->align;
    cls->type->is_complete = layout_complete;
    cls->type->cxx_is_class = true;
    cls->type->cxx_nontrivial = nontrivial || has_virtual || has_destructor;
}

static CxxClass* cxx_primary_vtable_base(CxxClass* cls) {
    if (!cls) return NULL;
    for (int i = 0; i < cls->base_count; ++i) {
        CxxClass* base = cls->bases[i].base;
        if (base && !cls->bases[i].is_virtual && base->vtable_size > 0) {
            return base;
        }
    }

    return NULL;
}

static const char* cxx_vtable_name(CxxClass* cls) {
    char buffer[1024];
    char* class_name;
    if (!cls || !cls->name) return NULL;
    class_name = cxx_mangle_name(cls->name, cls->ns, NULL);
    if (snprintf(buffer, sizeof(buffer), "__rcc_vtable_%s", class_name) < 0 ||
        strlen(buffer) >= sizeof(buffer) - 1u) {
        rcc_fatal("C++ vtable symbol is too long");
    }
    return rcc_intern(buffer);
}

static const char* cxx_secondary_vtable_name(CxxClass* cls,
                                             int base_index) {
    char buffer[1024];
    char* class_name;
    if (!cls || !cls->name) return NULL;
    class_name = cxx_mangle_name(cls->name, cls->ns, NULL);
    if (snprintf(buffer, sizeof(buffer), "__rcc_vtable_%s_base%d",
                 class_name, base_index) < 0 ||
        strlen(buffer) >= sizeof(buffer) - 1u) {
        rcc_fatal("C++ secondary vtable symbol is too long");
    }
    return rcc_intern(buffer);
}

static const char* cxx_secondary_thunk_name(CxxClass* cls,
                                            int base_index, int slot) {
    char buffer[1024];
    char* class_name;
    if (!cls || !cls->name) return NULL;
    class_name = cxx_mangle_name(cls->name, cls->ns, NULL);
    if (snprintf(buffer, sizeof(buffer), "__rcc_thunk_%s_base%d_slot%d",
                 class_name, base_index, slot) < 0 ||
        strlen(buffer) >= sizeof(buffer) - 1u) {
        rcc_fatal("C++ secondary vtable thunk symbol is too long");
    }
    return rcc_intern(buffer);
}

static int cxx_vtable_find_slot(const CxxVtableEntry* entries, int count,
                                const char* name) {
    if (!entries || !name) return -1;
    for (int index = 0; index < count; ++index) {
        if (entries[index].name &&
            strcmp(entries[index].name, name) == 0) {
            return index;
        }
    }
    return -1;
}

void cxx_class_build_vtable(CxxClass* cls) {
    CxxClass* primary_base;
    int vtable_size;
    if (!cls || !cls->type) return;

    primary_base = cxx_primary_vtable_base(cls);
    vtable_size = primary_base ? primary_base->vtable_size : 0;
    cls->vtable = vtable_size > 0
        ? ast_arena_alloc(sizeof(*cls->vtable) * (size_t)vtable_size) : NULL;
    for (int index = 0; index < vtable_size; ++index) {
        cls->vtable[index] = primary_base->vtable[index];
    }

    /* Build the Itanium-style primary table. */
    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        CxxMethod* method = member->method;
        int slot = -1;
        if (!method || !method->decl || method->is_static ||
            method->is_constructor || method->is_destructor) {
            continue;
        }
        for (int index = 0; index < vtable_size; ++index) {
            if (cls->vtable[index].name &&
                strcmp(cls->vtable[index].name, method->decl->name) == 0) {
                slot = index;
                break;
            }
        }
        if (slot >= 0) {
            method->is_virtual = true;
            member->is_virtual = true;
        } else if (method->is_virtual) {
            slot = vtable_size++;
            cls->vtable = ast_arena_grow(
                cls->vtable,
                sizeof(*cls->vtable) * (size_t)(vtable_size - 1),
                sizeof(*cls->vtable) * (size_t)vtable_size);
        } else {
            continue;
        }
        method->vtable_index = slot;
        cls->vtable[slot].name = method->decl->name;
        cls->vtable[slot].method = method;
        cls->vtable[slot].offset = slot *
            (g_opts.target_arch == ARCH_X64 ? 8 : 4);
        cls->vtable[slot].entry_symbol = NULL;
    }

    /* A fixed-offset polymorphic secondary base owns a distinct vptr in the
     * complete object.  Copy its slots and replace only overridden entries;
     * each replacement is entered through a real adjusting thunk emitted by
     * the target-specific backend. */
    for (int base_index = 0; base_index < cls->base_count; ++base_index) {
        CxxClass* base = cls->bases[base_index].base;
        CxxSecondaryVtable* secondary;
        if (!base || base == primary_base ||
            base->vtable_size <= 0 ||
            !base->vtable) {
            continue;
        }
        cls->secondary_vtables = ast_arena_grow(
            cls->secondary_vtables,
            sizeof(*cls->secondary_vtables) *
                (size_t)cls->secondary_vtable_count,
            sizeof(*cls->secondary_vtables) *
                (size_t)(cls->secondary_vtable_count + 1));
        secondary = &cls->secondary_vtables[cls->secondary_vtable_count++];
        secondary->base = base;
        secondary->base_index = base_index;
        secondary->symbol = cxx_secondary_vtable_name(cls, base_index);
        secondary->size = base->vtable_size;
        secondary->entries = ast_arena_alloc(
            sizeof(*secondary->entries) * (size_t)secondary->size);
        for (int slot = 0; slot < secondary->size; ++slot) {
            secondary->entries[slot] = base->vtable[slot];
            secondary->entries[slot].entry_symbol = NULL;
        }
        for (struct CxxMember* member = cls->members; member;
             member = member->next) {
            CxxMethod* method = member->method;
            int slot;
            if (!method || !method->decl || method->is_static ||
                method->is_constructor || method->is_destructor) {
                continue;
            }
            slot = cxx_vtable_find_slot(secondary->entries, secondary->size,
                                        method->decl->name);
            if (slot < 0) continue;
            method->is_virtual = true;
            member->is_virtual = true;
            secondary->entries[slot].method = method;
            secondary->entries[slot].entry_symbol =
                cxx_secondary_thunk_name(cls, base_index, slot);
        }
    }

    cls->vtable_size = vtable_size;
    cls->type->cxx_vtable_size = vtable_size;
    cls->type->cxx_vtable_symbol = vtable_size > 0
        ? cxx_vtable_name(cls) : NULL;
}

/* ═══════════════════════════════════════
 * Namespace Operations (Core API)
 * ═══════════════════════════════════════ */

CxxNamespace* cxx_namespace_alloc(const char* name, CxxNamespace* parent) {
    CxxNamespace* ns = rcc_alloc(sizeof(CxxNamespace));
    ns->name = name ? rcc_strdup(name) : NULL;
    ns->parent = parent;
    ns->decls = NULL;
    ns->classes = NULL;
    ns->class_count = 0;
    ns->templates = NULL;
    ns->template_count = 0;
    ns->children = NULL;
    ns->next = NULL;
    ns->using_namespaces = NULL;
    ns->using_namespace_count = 0;
    ns->using_declarations = NULL;
    ns->using_declaration_count = 0;

    /* Link to parent */
    if (parent) {
        ns->next = parent->children;
        parent->children = ns;
    }

    return ns;
}

CxxNamespace* cxx_namespace_lookup(CxxNamespace* root, const char* name) {
    if (!root || !name) return NULL;

    for (CxxNamespace* ns = root->children; ns; ns = ns->next) {
        if (ns->name && strcmp(ns->name, name) == 0) {
            return ns;
        }
    }
    return NULL;
}

void cxx_namespace_add_decl(CxxNamespace* ns, Decl* decl) {
    DeclList* node = rcc_alloc(sizeof(DeclList));
    node->decl = decl;
    node->next = ns->decls;
    ns->decls = node;
}

void cxx_namespace_add_template(CxxNamespace* ns, CxxTemplate* tmpl) {
    if (!ns || !tmpl) return;
    tmpl->ns = ns;
    ns->templates = ast_arena_grow(
        ns->templates, sizeof(CxxTemplate*) * (size_t)ns->template_count,
        sizeof(CxxTemplate*) * (size_t)(ns->template_count + 1));
    ns->templates[ns->template_count++] = tmpl;
}

void cxx_namespace_add_using_namespace(CxxNamespace* ns, CxxNamespace* target) {
    if (!ns || !target) return;
    ns->using_namespaces = ast_arena_grow(
        ns->using_namespaces,
        sizeof(CxxNamespace*) * (size_t)ns->using_namespace_count,
        sizeof(CxxNamespace*) * (size_t)(ns->using_namespace_count + 1));
    ns->using_namespaces[ns->using_namespace_count++] = target;
}

void cxx_namespace_add_using_decl(CxxNamespace* ns, const char* qualified_name) {
    if (!ns || !qualified_name || !*qualified_name) return;
    ns->using_declarations = ast_arena_grow(
        ns->using_declarations,
        sizeof(const char*) * (size_t)ns->using_declaration_count,
        sizeof(const char*) * (size_t)(ns->using_declaration_count + 1));
    ns->using_declarations[ns->using_declaration_count++] =
        rcc_intern(qualified_name);
}

/* ═══════════════════════════════════════
 * Template Operations (Core API)
 * ═══════════════════════════════════════ */

CxxTemplate* cxx_template_alloc(const char* name, TemplateParam* params, int count) {
    CxxTemplate* tmpl = rcc_alloc(sizeof(CxxTemplate));
    tmpl->name = name ? rcc_strdup(name) : NULL;
    tmpl->ns = NULL;
    if (count > 0 && params) {
        tmpl->params = rcc_alloc(sizeof(TemplateParam) * count);
        memcpy(tmpl->params, params, sizeof(TemplateParam) * count);
    } else {
        tmpl->params = NULL;
    }
    tmpl->param_count = count;
    tmpl->kind = TMPL_CLASS;
    tmpl->class_def = NULL;
    tmpl->is_constexpr = false;
    tmpl->is_noexcept = false;
    tmpl->constraint = NULL;
    tmpl->function_lowering = TMPL_FUNCTION_NONE;
    tmpl->function_constant = 0;
    tmpl->templated_class = NULL;
    tmpl->specializations = NULL;
    tmpl->specialization_count = 0;
    tmpl->specialization_args = NULL;
    tmpl->specialization_arg_count = 0;
    tmpl->instances = NULL;
    tmpl->instance_count = 0;
    return tmpl;
}

static int template_type_parameter_index(CxxTemplate* tmpl, Type* type) {
    if (!tmpl || !type || type->kind != TYPE_STRUCT || !type->tag) {
        return -1;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_TYPE &&
            tmpl->params[index].name &&
            strcmp(tmpl->params[index].name, type->tag) == 0) {
            return index;
        }
    }
    return -1;
}

static int template_value_parameter_index(CxxTemplate* tmpl,
                                           Expr* bound_expression) {
    if (!tmpl || !bound_expression || bound_expression->kind != EXPR_IDENT ||
        !bound_expression->ident_name) {
        return -1;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_NONTYPE &&
            tmpl->params[index].name &&
            strcmp(tmpl->params[index].name,
                   bound_expression->ident_name) == 0) {
            return index;
        }
    }
    return -1;
}

static Type* template_substitute_type(CxxTemplate* tmpl, Type* type,
                                      Type** args, int arg_count,
                                      const int64_t* value_args,
                                      const bool* value_present) {
    Type* replacement;
    Type* base;
    int array_len;
    Expr* array_bound;
    int index;

    if (!type) return NULL;
    index = template_type_parameter_index(tmpl, type);
    if (index >= 0 && index < arg_count && args[index]) {
        replacement = args[index];
        if ((type->is_const && !replacement->is_const) ||
            (type->is_volatile && !replacement->is_volatile)) {
            Type* qualified = ast_arena_alloc(sizeof(*qualified));
            *qualified = *replacement;
            qualified->is_const = qualified->is_const || type->is_const;
            qualified->is_volatile = qualified->is_volatile ||
                                     type->is_volatile;
            replacement = qualified;
        }
        return replacement;
    }

    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) {
        base = template_substitute_type(
            tmpl, type->base, args, arg_count, value_args, value_present);
        array_len = type->array_len;
        array_bound = type->array_bound;
        if (type->kind == TYPE_ARRAY && value_args && value_present &&
            type->array_bound) {
            int value_index = template_value_parameter_index(
                tmpl, type->array_bound);
            if (value_index >= 0 && value_present[value_index]) {
                int64_t value = value_args[value_index];
                if (value <= 0 || value > INT_MAX) {
                    rcc_error(type->array_bound->loc,
                              "non-type template array bound is out of range");
                    return NULL;
                }
                array_len = (int)value;
                array_bound = NULL;
            }
        }
        if (base != type->base || array_len != type->array_len ||
            array_bound != type->array_bound) {
            Type* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *type;
            copy->base = base;
            if (copy->kind == TYPE_ARRAY) {
                copy->array_len = array_len;
                copy->array_bound = array_bound;
            }
            if (copy->kind == TYPE_ARRAY && copy->array_len > 0) {
                copy->size = base->size * copy->array_len;
                copy->align = base->align;
            }
            return copy;
        }
    } else if (type->kind == TYPE_FUNC) {
        Type* return_type = template_substitute_type(
            tmpl, type->ret_type, args, arg_count, value_args,
            value_present);
        TypeParam* params = NULL;
        TypeParam** tail = &params;
        bool changed = return_type != type->ret_type;
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            TypeParam* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *parameter;
            copy->type = template_substitute_type(
                tmpl, parameter->type, args, arg_count, value_args,
                value_present);
            copy->next = NULL;
            changed = changed || copy->type != parameter->type;
            *tail = copy;
            tail = &copy->next;
        }
        if (changed) {
            Type* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *type;
            copy->ret_type = return_type;
            copy->params = params;
            return copy;
        }
    }
    return type;
}

static Expr* template_clone_expr(CxxTemplate* tmpl, Expr* expression,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present);

static ExprList* template_clone_expr_list(CxxTemplate* tmpl, ExprList* list,
                                           Type** args, int arg_count,
                                           const int64_t* value_args,
                                           const bool* value_present) {
    ExprList* result = NULL;
    ExprList** tail = &result;
    for (; list; list = list->next) {
        ExprList* copy = ast_arena_alloc(sizeof(*copy));
        *copy = *list;
        copy->expr = template_clone_expr(tmpl, list->expr, args, arg_count,
                                          value_args, value_present);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return result;
}

static GenericAssociation* template_clone_associations(
        CxxTemplate* tmpl, GenericAssociation* list, Type** args,
    int arg_count, const int64_t* value_args, const bool* value_present) {
    GenericAssociation* result = NULL;
    GenericAssociation** tail = &result;
    for (; list; list = list->next) {
        GenericAssociation* copy = ast_arena_alloc(sizeof(*copy));
        *copy = *list;
        copy->type = template_substitute_type(
            tmpl, list->type, args, arg_count, value_args, value_present);
        copy->expr = template_clone_expr(tmpl, list->expr, args, arg_count,
                                          value_args, value_present);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return result;
}

static Expr* template_clone_expr(CxxTemplate* tmpl, Expr* expression,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present) {
    Expr* copy;
    if (!expression) return NULL;
    if (expression->kind == EXPR_IDENT && tmpl && value_present &&
        expression->ident_name) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE &&
                parameter->name && value_present[index] &&
                strcmp(parameter->name, expression->ident_name) == 0) {
                copy = expr_int(value_args[index], expression->loc);
                copy->type = parameter->type ? parameter->type : type_int;
                return copy;
            }
        }
    }
    copy = ast_arena_alloc(sizeof(*copy));
    *copy = *expression;
    copy->type = template_substitute_type(
        tmpl, expression->type, args, arg_count, value_args, value_present);
    copy->cxx_move_assignment = NULL;
    copy->cxx_close_call = NULL;
    switch (expression->kind) {
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            copy->unary_operand = template_clone_expr(
                tmpl, expression->unary_operand, args, arg_count,
                value_args, value_present);
            copy->sizeof_type = template_substitute_type(
                tmpl, expression->sizeof_type, args, arg_count,
                value_args, value_present);
            break;
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
        case EXPR_ASSIGN:
        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN:
        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN:
        case EXPR_COMMA:
            copy->binary_lhs = template_clone_expr(
                tmpl, expression->binary_lhs, args, arg_count,
                value_args, value_present);
            copy->binary_rhs = template_clone_expr(
                tmpl, expression->binary_rhs, args, arg_count,
                value_args, value_present);
            break;
        case EXPR_COND:
            copy->cond_test = template_clone_expr(
                tmpl, expression->cond_test, args, arg_count,
                value_args, value_present);
            copy->cond_then = template_clone_expr(
                tmpl, expression->cond_then, args, arg_count,
                value_args, value_present);
            copy->cond_else = template_clone_expr(
                tmpl, expression->cond_else, args, arg_count,
                value_args, value_present);
            break;
        case EXPR_CALL:
            copy->call_func = template_clone_expr(
                tmpl, expression->call_func, args, arg_count,
                value_args, value_present);
            copy->call_args = template_clone_expr_list(
                tmpl, expression->call_args, args, arg_count,
                value_args, value_present);
            copy->call_method = NULL;
            copy->call_is_virtual = false;
            copy->call_virtual_index = -1;
            copy->call_virtual_object = NULL;
            copy->call_is_new = expression->call_is_new;
            copy->call_new_value_init = expression->call_new_value_init;
            copy->call_new_is_array = expression->call_new_is_array;
            copy->call_new_default_member_initializers = false;
            copy->call_new_type = template_substitute_type(
                tmpl, expression->call_new_type, args, arg_count,
                value_args, value_present);
            copy->call_new_count = template_clone_expr(
                tmpl, expression->call_new_count, args, arg_count,
                value_args, value_present);
            copy->call_new_args = template_clone_expr_list(
                tmpl, expression->call_new_args, args, arg_count,
                value_args, value_present);
            /* Constructor selection is semantic and must be redone for the
             * substituted class specialization. */
            copy->call_new_constructor = NULL;
            copy->call_is_delete = expression->call_is_delete;
            copy->call_delete_is_array = expression->call_delete_is_array;
            copy->call_delete_destructor = NULL;
            /* Destructor lookup is semantic and must be redone for the
             * substituted class specialization. */
            copy->call_delete_cleanup = NULL;
            copy->call_delete_cleanup_field = NULL;
            copy->call_delete_cleanup_invalid = 0;
            break;
        case EXPR_INDEX:
            copy->index_base = template_clone_expr(
                tmpl, expression->index_base, args, arg_count,
                value_args, value_present);
            copy->index_expr = template_clone_expr(
                tmpl, expression->index_expr, args, arg_count,
                value_args, value_present);
            break;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            copy->member_base = template_clone_expr(
                tmpl, expression->member_base, args, arg_count,
                value_args, value_present);
            copy->member_field = NULL;
            break;
        case EXPR_CAST:
            copy->cast_expr = template_clone_expr(
                tmpl, expression->cast_expr, args, arg_count,
                value_args, value_present);
            copy->cast_type = template_substitute_type(
                tmpl, expression->cast_type, args, arg_count,
                value_args, value_present);
            break;
        case EXPR_COMPOUND:
            copy->compound_type = template_substitute_type(
                tmpl, expression->compound_type, args, arg_count,
                value_args, value_present);
            copy->compound_init = template_clone_expr_list(
                tmpl, expression->compound_init, args, arg_count,
                value_args, value_present);
            copy->compound_static_symbol = NULL;
            break;
        case EXPR_GENERIC:
            copy->generic_control = template_clone_expr(
                tmpl, expression->generic_control, args, arg_count,
                value_args, value_present);
            copy->generic_associations = template_clone_associations(
                tmpl, expression->generic_associations, args, arg_count,
                value_args, value_present);
            break;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            copy->va_list_operand = template_clone_expr(
                tmpl, expression->va_list_operand, args, arg_count,
                value_args, value_present);
            copy->va_second_operand = template_clone_expr(
                tmpl, expression->va_second_operand, args, arg_count,
                value_args, value_present);
            copy->va_arg_type = template_substitute_type(
                tmpl, expression->va_arg_type, args, arg_count,
                value_args, value_present);
            copy->va_arg_result_offset = expression->va_arg_result_offset;
            break;
        default:
            break;
    }
    return copy;
}

static Decl* template_clone_decl(CxxTemplate* tmpl, Decl* declaration,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present) {
    Decl* copy;
    if (!declaration) return NULL;
    copy = ast_arena_alloc(sizeof(*copy));
    *copy = *declaration;
    copy->type = template_substitute_type(
        tmpl, declaration->type, args, arg_count, value_args, value_present);
    copy->param_default = template_clone_expr(
        tmpl, declaration->param_default, args, arg_count,
        value_args, value_present);
    if (declaration->kind == DECL_VAR) {
        copy->var_init = template_clone_expr(
            tmpl, declaration->var_init, args, arg_count,
            value_args, value_present);
        copy->var_cleanup = NULL;
        copy->var_cleanups = NULL;
    }
    return copy;
}

static Stmt* template_clone_stmt(CxxTemplate* tmpl, Stmt* statement,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present);

static StmtList* template_clone_stmt_list(CxxTemplate* tmpl, StmtList* list,
                                           Type** args, int arg_count,
                                           const int64_t* value_args,
                                           const bool* value_present) {
    StmtList* result = NULL;
    StmtList** tail = &result;
    for (; list; list = list->next) {
        StmtList* copy = ast_arena_alloc(sizeof(*copy));
        copy->stmt = template_clone_stmt(tmpl, list->stmt, args, arg_count,
                                          value_args, value_present);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return result;
}

static Stmt* template_clone_stmt(CxxTemplate* tmpl, Stmt* statement,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present) {
    Stmt* copy;
    if (!statement) return NULL;
    copy = ast_arena_alloc(sizeof(*copy));
    *copy = *statement;
    switch (statement->kind) {
        case STMT_EXPR:
            copy->expr = template_clone_expr(
                tmpl, statement->expr, args, arg_count,
                value_args, value_present);
            break;
        case STMT_BLOCK:
            copy->block_stmts = template_clone_stmt_list(
                tmpl, statement->block_stmts, args, arg_count,
                value_args, value_present);
            break;
        case STMT_IF:
            copy->if_cond = template_clone_expr(
                tmpl, statement->if_cond, args, arg_count,
                value_args, value_present);
            copy->if_then = template_clone_stmt(
                tmpl, statement->if_then, args, arg_count,
                value_args, value_present);
            copy->if_else = template_clone_stmt(
                tmpl, statement->if_else, args, arg_count,
                value_args, value_present);
            break;
        case STMT_WHILE:
        case STMT_DO:
            copy->while_cond = template_clone_expr(
                tmpl, statement->while_cond, args, arg_count,
                value_args, value_present);
            copy->while_body = template_clone_stmt(
                tmpl, statement->while_body, args, arg_count,
                value_args, value_present);
            break;
        case STMT_FOR:
            copy->for_init = template_clone_stmt(
                tmpl, statement->for_init, args, arg_count,
                value_args, value_present);
            copy->for_cond = template_clone_expr(
                tmpl, statement->for_cond, args, arg_count,
                value_args, value_present);
            copy->for_inc = template_clone_expr(
                tmpl, statement->for_inc, args, arg_count,
                value_args, value_present);
            copy->for_body = template_clone_stmt(
                tmpl, statement->for_body, args, arg_count,
                value_args, value_present);
            break;
        case STMT_SWITCH:
            copy->switch_expr = template_clone_expr(
                tmpl, statement->switch_expr, args, arg_count,
                value_args, value_present);
            copy->switch_body = template_clone_stmt(
                tmpl, statement->switch_body, args, arg_count,
                value_args, value_present);
            break;
        case STMT_CASE:
            copy->case_val = template_clone_expr(
                tmpl, statement->case_val, args, arg_count,
                value_args, value_present);
            copy->case_stmt = template_clone_stmt(
                tmpl, statement->case_stmt, args, arg_count,
                value_args, value_present);
            break;
        case STMT_DEFAULT:
            copy->default_stmt = template_clone_stmt(
                tmpl, statement->default_stmt, args, arg_count,
                value_args, value_present);
            break;
        case STMT_RETURN:
            copy->return_val = template_clone_expr(
                tmpl, statement->return_val, args, arg_count,
                value_args, value_present);
            break;
        case STMT_LABEL:
            copy->label_stmt = template_clone_stmt(
                tmpl, statement->label_stmt, args, arg_count,
                value_args, value_present);
            break;
        case STMT_TRY: {
            CxxCatch** tail = &copy->try_catches;
            copy->try_body = template_clone_stmt(
                tmpl, statement->try_body, args, arg_count,
                value_args, value_present);
            copy->try_frame_offset = 0;
            copy->try_frame_size = 0;
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                CxxCatch* cloned = ast_arena_alloc(sizeof(*cloned));
                memset(cloned, 0, sizeof(*cloned));
                cloned->type = template_substitute_type(
                    tmpl, handler->type, args, arg_count,
                    value_args, value_present);
                cloned->name = handler->name;
                cloned->is_ellipsis = handler->is_ellipsis;
                cloned->body = template_clone_stmt(
                    tmpl, handler->body, args, arg_count,
                    value_args, value_present);
                if (cloned->body && cloned->body->kind == STMT_BLOCK &&
                    cloned->body->block_stmts &&
                    cloned->body->block_stmts->stmt->kind == STMT_DECL) {
                    cloned->parameter = cloned->body->block_stmts->stmt->decl;
                }
                cloned->next = NULL;
                *tail = cloned;
                tail = &cloned->next;
            }
            break;
        }
        case STMT_DECL:
            copy->decl = template_clone_decl(
                tmpl, statement->decl, args, arg_count,
                value_args, value_present);
            break;
        default:
            break;
    }
    return copy;
}

Expr* cxx_template_clone_expr(CxxTemplate* tmpl, Expr* expression,
                              Type** args, int arg_count) {
    return cxx_template_clone_expr_with_values(
        tmpl, expression, args, arg_count, NULL, NULL);
}

Expr* cxx_template_clone_expr_with_values(
    CxxTemplate* tmpl, Expr* expression, Type** args, int arg_count,
    const int64_t* value_args, const bool* value_present) {
    return template_clone_expr(tmpl, expression, args, arg_count,
                               value_args, value_present);
}

Stmt* cxx_template_clone_stmt(CxxTemplate* tmpl, Stmt* statement,
                               Type** args, int arg_count) {
    return cxx_template_clone_stmt_with_values(
        tmpl, statement, args, arg_count, NULL, NULL);
}

Stmt* cxx_template_clone_stmt_with_values(
    CxxTemplate* tmpl, Stmt* statement, Type** args, int arg_count,
    const int64_t* value_args, const bool* value_present) {
    return template_clone_stmt(tmpl, statement, args, arg_count,
                               value_args, value_present);
}

static bool template_instance_matches(CxxTemplate* tmpl, int instance_index,
                                      Type** args,
                                      const int64_t* value_args,
                                      const bool* value_present,
                                      int arg_count) {
    if (!tmpl || instance_index < 0 ||
        instance_index >= tmpl->instance_count ||
        tmpl->instances[instance_index].arg_count != arg_count) {
        return false;
    }
    for (int index = 0; index < arg_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind == TPARAM_NONTYPE) {
            if (!value_present || !value_present[index] ||
                !tmpl->instances[instance_index].value_present ||
                !tmpl->instances[instance_index].value_present[index] ||
                !value_args ||
                tmpl->instances[instance_index].value_args[index] !=
                    value_args[index]) {
                return false;
            }
        } else if (!args || !args[index] ||
                   !type_is_compatible(
                       tmpl->instances[instance_index].args[index],
                       args[index])) {
            return false;
        }
    }
    return true;
}

void* cxx_template_instantiate_with_values(CxxTemplate* tmpl, Type** args,
                                            const int64_t* value_args,
                                            const bool* value_present,
                                            int arg_count) {
    SourceLoc template_loc = {"<template>", 0, 0};

    if (!tmpl || arg_count < 0 || arg_count != tmpl->param_count ||
        (arg_count > 0 && !args)) {
        return NULL;
    }

    for (int index = 0; index < arg_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind == TPARAM_NONTYPE) {
            if (!value_present || !value_present[index] || !value_args ||
                !parameter->type || !type_is_integer(parameter->type)) {
                rcc_error(template_loc,
                          "function template non-type argument %d requires "
                          "an integer constant",
                          index + 1);
                return NULL;
            }
        } else if (!args[index]) {
            rcc_error(template_loc,
                      "function template type argument %d is missing",
                      index + 1);
            return NULL;
        }
    }

    for (int index = 0; index < tmpl->instance_count; ++index) {
        if (template_instance_matches(tmpl, index, args, value_args,
                                       value_present, arg_count)) {
            return tmpl->instances[index].instantiated;
        }
    }

    if (tmpl->kind == TMPL_CLASS) {
        return rcc_cxx_instantiate_class_template(
            tmpl, args, value_args, value_present, arg_count, template_loc);
    }

    /* Function templates use the same parameter substitution model as class
     * templates.  Their body is deliberately retained: semantic analysis of
     * the cloned declaration rebinds identifiers to the cloned parameters. */
    if (tmpl->kind == TMPL_FUNCTION && tmpl->func_def) {
        Decl* definition = tmpl->func_def;
        DeclList* parameters = NULL;
        TypeParam* type_parameters = NULL;
        TypeParam** type_tail = &type_parameters;
        Type* function_type;
        Decl* instance;

        for (DeclList* item = definition->func_params; item;
             item = item->next) {
            Type* parameter_type = template_substitute_type(
                tmpl, item->decl->type, args, arg_count, value_args,
                value_present);
            Decl* parameter = decl_param(item->decl->name, parameter_type,
                                         item->decl->param_index,
                                         item->decl->loc);
            parameter->param_default = item->decl->param_default;
            decllist_append(&parameters, parameter);
            TypeParam* type_parameter = ast_arena_alloc(sizeof(*type_parameter));
            type_parameter->name = parameter->name;
            type_parameter->type = parameter_type;
            type_parameter->is_bitfield = false;
            type_parameter->bit_width = 0u;
            type_parameter->is_static = false;
            type_parameter->cxx_access = ACCESS_PUBLIC;
            type_parameter->next = NULL;
            *type_tail = type_parameter;
            type_tail = &type_parameter->next;
        }

        Type* return_type = template_substitute_type(
            tmpl, definition->type ? definition->type->ret_type : NULL,
            args, arg_count, value_args, value_present);
        function_type = type_func(return_type, type_parameters,
                                  definition->type && definition->type->variadic);
        function_type->has_prototype = definition->type
            ? definition->type->has_prototype : true;
        instance = decl_func(definition->name, function_type, parameters,
                             template_clone_stmt(tmpl, definition->func_body,
                                                 args, arg_count, value_args,
                                                 value_present),
                             definition->loc);
        instance->func_is_inline = definition->func_is_inline;
        instance->func_is_constexpr = definition->func_is_constexpr;
        instance->func_is_consteval = definition->func_is_consteval;
        instance->func_is_auto_return = definition->func_is_auto_return;
        instance->func_is_decltype_auto_return =
            definition->func_is_decltype_auto_return;
        instance->func_is_template_instance = true;
        instance->func_has_cxx_linkage = true;
        instance->link_name = rcc_intern(cxx_mangle_function_template(
            instance, tmpl->ns, NULL, tmpl, value_args, value_present));

        tmpl->instances = ast_arena_grow(
            tmpl->instances,
            sizeof(tmpl->instances[0]) * (size_t)tmpl->instance_count,
            sizeof(tmpl->instances[0]) * (size_t)(tmpl->instance_count + 1));
        tmpl->instances[tmpl->instance_count].args = ast_arena_alloc(
            sizeof(Type*) * (size_t)arg_count);
        memcpy(tmpl->instances[tmpl->instance_count].args, args,
               sizeof(Type*) * (size_t)arg_count);
        tmpl->instances[tmpl->instance_count].value_args = NULL;
        tmpl->instances[tmpl->instance_count].value_present = NULL;
        if (arg_count > 0) {
            tmpl->instances[tmpl->instance_count].value_args =
                ast_arena_alloc(sizeof(int64_t) * (size_t)arg_count);
            tmpl->instances[tmpl->instance_count].value_present =
                ast_arena_alloc(sizeof(bool) * (size_t)arg_count);
            memcpy(tmpl->instances[tmpl->instance_count].value_args,
                   value_args, sizeof(int64_t) * (size_t)arg_count);
            memcpy(tmpl->instances[tmpl->instance_count].value_present,
                   value_present, sizeof(bool) * (size_t)arg_count);
        }
        tmpl->instances[tmpl->instance_count].arg_count = arg_count;
        tmpl->instances[tmpl->instance_count].instantiated = instance;
        ++tmpl->instance_count;
        return instance;
    }

    return NULL;
}

/* Find a namespace from a source spelling such as `api::detail`.  Leading
 * global-scope qualification is accepted, while an empty spelling denotes
 * the supplied root. */
CxxNamespace* cxx_namespace_find(CxxNamespace* root, const char* qualified_name) {
    char buffer[512];
    char* component;
    char* next;
    CxxNamespace* current = root;

    if (!root || !qualified_name) return NULL;
    while (qualified_name[0] == ':' && qualified_name[1] == ':') {
        qualified_name += 2;
    }
    if (*qualified_name == '\0') return root;
    if (strlen(qualified_name) >= sizeof(buffer)) return NULL;
    strcpy(buffer, qualified_name);
    component = buffer;
    for (;;) {
        next = strstr(component, "::");
        if (next) *next = '\0';
        if (*component == '\0') return NULL;
        current = cxx_namespace_lookup(current, component);
        if (!current) return NULL;
        if (!next) return current;
        component = next + 2;
    }
}

static int cxx_virtual_base_index(CxxClass* cls, CxxClass* base) {
    if (!cls || !base) return -1;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        if (cls->virtual_bases[index].base == base) return index;
    }
    return -1;
}

static void cxx_add_virtual_base(CxxClass* cls, CxxClass* base,
                                 bool public_path) {
    int existing;
    if (!cls || !base) return;
    existing = cxx_virtual_base_index(cls, base);
    if (existing >= 0) {
        if (public_path) cls->virtual_bases[existing].public_path = true;
        return;
    }
    cls->virtual_bases = ast_arena_grow(
        cls->virtual_bases,
        sizeof(*cls->virtual_bases) * (size_t)cls->virtual_base_count,
        sizeof(*cls->virtual_bases) * (size_t)(cls->virtual_base_count + 1));
    cls->virtual_bases[cls->virtual_base_count].base = base;
    cls->virtual_bases[cls->virtual_base_count].offset = -1;
    cls->virtual_bases[cls->virtual_base_count].public_path = public_path;
    ++cls->virtual_base_count;
}

bool cxx_class_virtual_base_offset(CxxClass* cls, CxxClass* base,
                                   int* offset) {
    int index = cxx_virtual_base_index(cls, base);
    if (index < 0 || cls->virtual_bases[index].offset < 0) return false;
    if (offset) *offset = cls->virtual_bases[index].offset;
    return true;
}

/* Return the longest namespace prefix of a qualified declaration.  This also
 * handles class members (`api::Widget::run`), whose final components are not
 * namespaces. */
CxxNamespace* cxx_namespace_for_decl_name(CxxNamespace* root,
                                           const char* qualified_name) {
    char buffer[512];
    char* component;
    char* next;
    CxxNamespace* current;
    CxxNamespace* last;

    if (!root || !qualified_name) return root;
    while (qualified_name[0] == ':' && qualified_name[1] == ':') {
        qualified_name += 2;
    }
    if (strlen(qualified_name) >= sizeof(buffer)) return root;
    strcpy(buffer, qualified_name);
    current = root;
    last = root;
    component = buffer;
    for (;;) {
        next = strstr(component, "::");
        if (!next) break;
        *next = '\0';
        if (*component == '\0') break;
        current = cxx_namespace_lookup(current, component);
        if (!current) break;
        last = current;
        component = next + 2;
    }
    return last;
}

const char* cxx_namespace_qualified_name(CxxNamespace* ns) {
    CxxNamespace* stack[32];
    char buffer[512] = "";
    size_t length = 0u;
    int count = 0;
    if (!ns) return NULL;
    for (; ns && ns->name; ns = ns->parent) {
        if (count == (int)(sizeof(stack) / sizeof(stack[0]))) return NULL;
        stack[count++] = ns;
    }
    for (int index = count - 1; index >= 0; --index) {
        size_t part_length = strlen(stack[index]->name);
        if (part_length > sizeof(buffer) - 1u - length) return NULL;
        if (length != 0u) {
            memcpy(buffer + length, "::", 2u);
            length += 2u;
        }
        memcpy(buffer + length, stack[index]->name, part_length);
        length += part_length;
    }
    buffer[length] = '\0';
    return rcc_intern(buffer);
}

void* cxx_template_instantiate(CxxTemplate* tmpl, Type** args, int arg_count) {
    return cxx_template_instantiate_with_values(tmpl, args, NULL, NULL,
                                                arg_count);
}

/* ═══════════════════════════════════════
 * C++ Initialization
 * ═══════════════════════════════════════ */

void cxx_init(void) {
    if (!g_global_namespace) {
        g_global_namespace = cxx_namespace_alloc(NULL, NULL);
    }
}

CxxNamespace* cxx_namespace_global(void) {
    return g_global_namespace;
}

/* ═══════════════════════════════════════
 * Parser API (for parser_cxx.c)
 * ═══════════════════════════════════════ */

/* Class with source location */
CxxClass* cxx_class_new(const char* name, SourceLoc loc) {
    (void)loc;
    return cxx_class_alloc(name, false);
}

/* Add base class by name (deferred resolution) */
void cxx_class_add_base(CxxClass* cls, const char* base_name, AccessSpec access) {
    cls->bases = ast_arena_grow(
        cls->bases, sizeof(cls->bases[0]) * (size_t)cls->base_count,
        sizeof(cls->bases[0]) * (size_t)(cls->base_count + 1));
    cls->bases[cls->base_count].base = NULL;  /* Will be resolved later */
    cls->bases[cls->base_count].base_name =
        base_name ? rcc_intern(base_name) : NULL;
    cls->bases[cls->base_count].access = access;
    cls->bases[cls->base_count].is_virtual = false;
    cls->base_count++;
}

/* Add field to class */
void cxx_class_add_field_initializer(CxxClass* cls, const char* name,
                                     Type* type, AccessSpec access,
                                     Expr* initializer, bool is_bitfield,
                                     unsigned bit_width, bool is_static) {
    /* Create field as TypeParam (reusing existing structure) */
    TypeParam* field = rcc_alloc(sizeof(TypeParam));
    field->name = name ? rcc_strdup(name) : NULL;
    field->type = type;
    field->is_bitfield = is_bitfield;
    field->bit_width = bit_width;
    field->is_static = is_static;
    field->initializer = initializer;
    field->cxx_access = (unsigned char)access;
    field->next = NULL;

    /* Append to fields list */
    if (!cls->fields) {
        cls->fields = field;
    } else {
        TypeParam* f = cls->fields;
        while (f->next) f = f->next;
        f->next = field;
    }
}

void cxx_class_add_field(CxxClass* cls, const char* name, Type* type,
                         AccessSpec access) {
    cxx_class_add_field_initializer(
        cls, name, type, access, NULL, false, 0u, false);
}

/* Add method to class */
void cxx_class_add_method(CxxClass* cls, CxxMethod* method) {
    /* Create member wrapper */
    struct CxxMember* member = rcc_alloc(sizeof(struct CxxMember));
    member->access = method->access;
    member->decl = method->decl;
    member->method = method;
    member->is_static = method->is_static;
    member->is_virtual = method->is_virtual;
    member->is_pure_virtual = method->is_pure_virtual;
    member->is_override = method->is_override;
    member->is_final = method->is_final;
    member->next = NULL;

    if (method->is_destructor && !cls->destructor_method) {
        cls->destructor_method = method;
    }

    /* Append to member list */
    if (!cls->members) {
        cls->members = member;
    } else {
        struct CxxMember* m = cls->members;
        while (m->next) m = m->next;
        m->next = member;
    }
}

/* Create method */
CxxMethod* cxx_method_new(const char* name, Type* return_type, DeclList* params, Stmt* body, SourceLoc loc) {
    CxxMethod* method = rcc_alloc(sizeof(CxxMethod));

    /* Build function type */
    TypeParam* tparams = NULL;
    TypeParam** parameter_tail = &tparams;
    for (DeclList* p = params; p; p = p->next) {
        TypeParam* tp = rcc_alloc(sizeof(TypeParam));
        tp->name = p->decl->name;
        tp->type = p->decl->type;
        tp->is_bitfield = false;
        tp->bit_width = 0u;
        tp->is_static = false;
        tp->cxx_access = ACCESS_PUBLIC;
        tp->next = NULL;
        *parameter_tail = tp;
        parameter_tail = &tp->next;
    }
    Type* func_type = type_func(return_type, tparams, false);

    /* Create declaration */
    method->decl = decl_func(name, func_type, params, body, loc);
    method->source_name = name;
    method->owner = NULL;
    method->access = ACCESS_PUBLIC;
    method->is_static = false;
    method->is_virtual = false;
    method->is_pure_virtual = false;
    method->is_override = false;
    method->is_final = false;
    method->is_const = false;
    method->is_constexpr = false;
    method->is_explicit = false;
    method->is_noexcept = false;
    method->is_deleted = false;
    method->is_defaulted = false;
    method->is_constructor = false;
    method->is_destructor = false;
    method->vtable_index = -1;

    return method;
}

/* Namespace with source location */
CxxNamespace* cxx_namespace_new(const char* name, SourceLoc loc) {
    (void)loc;
    return cxx_namespace_alloc(name, NULL);
}

/* Add class to namespace */
void cxx_namespace_add_class(CxxNamespace* ns, CxxClass* cls) {
    cls->ns = ns;
    cls->type->cxx_namespace = cxx_namespace_qualified_name(ns);
    ns->classes = ast_arena_grow(
        ns->classes, sizeof(CxxClass*) * (size_t)ns->class_count,
        sizeof(CxxClass*) * (size_t)(ns->class_count + 1));
    ns->classes[ns->class_count++] = cls;
}

/* Add nested namespace */
void cxx_namespace_add_namespace(CxxNamespace* parent, CxxNamespace* child) {
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

/* Template with source location */
CxxTemplate* cxx_template_new(SourceLoc loc) {
    (void)loc;
    return cxx_template_alloc(NULL, NULL, 0);
}

/* Add type parameter to template */
void cxx_template_add_type_param(CxxTemplate* tmpl, const char* name) {
    tmpl->params = ast_arena_grow(
        tmpl->params, sizeof(TemplateParam) * (size_t)tmpl->param_count,
        sizeof(TemplateParam) * (size_t)(tmpl->param_count + 1));
    tmpl->params[tmpl->param_count].kind = TPARAM_TYPE;
    tmpl->params[tmpl->param_count].name = name ? rcc_strdup(name) : NULL;
    tmpl->params[tmpl->param_count].type = NULL;
    tmpl->params[tmpl->param_count].has_default = false;
    tmpl->params[tmpl->param_count].default_type = NULL;
    tmpl->param_count++;
}

/* Add value parameter to template */
void cxx_template_add_value_param(CxxTemplate* tmpl, const char* name, Type* type) {
    tmpl->params = ast_arena_grow(
        tmpl->params, sizeof(TemplateParam) * (size_t)tmpl->param_count,
        sizeof(TemplateParam) * (size_t)(tmpl->param_count + 1));
    tmpl->params[tmpl->param_count].kind = TPARAM_NONTYPE;
    tmpl->params[tmpl->param_count].name = name ? rcc_strdup(name) : NULL;
    tmpl->params[tmpl->param_count].type = type;
    tmpl->params[tmpl->param_count].has_default = false;
    tmpl->params[tmpl->param_count].default_value = NULL;
    tmpl->param_count++;
}
