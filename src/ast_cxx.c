/*
 * RCC++ - RinOS C++ Compiler
 * C++ AST Implementation
 */

#include "ast_cxx.h"
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

/* Mangle a length-prefixed name */
static void mangle_name(char* buf, size_t* pos, const char* name) {
    size_t len = strlen(name);
    *pos += snprintf(buf + *pos, 256 - *pos, "%lu%s", (unsigned long)len, name);
}

/* Mangle a type */
char* cxx_mangle_type(Type* type) {
    static char buf[256];
    size_t pos = 0;

    if (!type) {
        buf[0] = 'v';  /* void */
        buf[1] = '\0';
        return buf;
    }

    /* Handle pointers */
    while (type->kind == TYPE_PTR) {
        buf[pos++] = 'P';
        type = type->base;
    }

    /* Handle const */
    if (type->is_const) {
        buf[pos++] = 'K';
    }

    /* Base types */
    switch (type->kind) {
        case TYPE_VOID:   buf[pos++] = 'v'; break;
        case TYPE_BOOL:   buf[pos++] = 'b'; break;
        case TYPE_CHAR:
            if (type->is_unsigned)
                buf[pos++] = 'h';  /* unsigned char */
            else
                buf[pos++] = 'c';  /* char */
            break;
        case TYPE_SHORT:
            if (type->is_unsigned)
                buf[pos++] = 't';  /* unsigned short */
            else
                buf[pos++] = 's';  /* short */
            break;
        case TYPE_INT:
            if (type->is_unsigned)
                buf[pos++] = 'j';  /* unsigned int */
            else
                buf[pos++] = 'i';  /* int */
            break;
        case TYPE_LONG:
            if (type->is_unsigned)
                buf[pos++] = 'm';  /* unsigned long */
            else
                buf[pos++] = 'l';  /* long */
            break;
        case TYPE_LLONG:
            if (type->is_unsigned)
                buf[pos++] = 'y';  /* unsigned long long */
            else
                buf[pos++] = 'x';  /* long long */
            break;
        case TYPE_FLOAT:  buf[pos++] = 'f'; break;
        case TYPE_DOUBLE: buf[pos++] = 'd'; break;
        case TYPE_STRUCT:
        case TYPE_UNION:
            /* Named type */
            if (type->tag) {
                mangle_name(buf, &pos, type->tag);
            }
            break;
        default:
            buf[pos++] = '?';
            break;
    }

    buf[pos] = '\0';
    return buf;
}

/* Mangle a simple name with namespace */
char* cxx_mangle_name(const char* name, CxxNamespace* ns, CxxClass* cls) {
    static char buf[512];
    size_t pos = 0;

    /* _Z prefix for mangled names */
    buf[pos++] = '_';
    buf[pos++] = 'Z';

    /* Nested name indicator */
    if (ns || cls) {
        buf[pos++] = 'N';

        /* Namespace components */
        CxxNamespace* ns_stack[32];
        int ns_count = 0;
        for (CxxNamespace* n = ns; n && n->name; n = n->parent) {
            if (ns_count < 32) {
                ns_stack[ns_count++] = n;
            }
        }
        for (int i = ns_count - 1; i >= 0; i--) {
            mangle_name(buf, &pos, ns_stack[i]->name);
        }

        /* Class name */
        if (cls) {
            mangle_name(buf, &pos, cls->name);
        }

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

    /* Get base mangled name */
    char* base = cxx_mangle_name(func->name, ns, cls);
    strcpy(buf, base);
    size_t pos = strlen(buf);

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
    cls->bases = NULL;
    cls->base_count = 0;
    cls->members = NULL;
    cls->vtable = NULL;
    cls->vtable_size = 0;
    cls->type = type_struct(name);
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
    cls->bases[cls->base_count].access = access;
    cls->bases[cls->base_count].is_virtual = is_virtual;
    cls->base_count++;
}

void cxx_class_add_member(CxxClass* cls, Decl* decl, AccessSpec access, bool is_static) {
    struct CxxMember* member = rcc_alloc(sizeof(struct CxxMember));
    member->access = access;
    member->decl = decl;
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

void cxx_class_compute_layout(CxxClass* cls) {
    int offset = 0;
    int max_align = 1;
    bool layout_complete = true;
    TypeField** field_tail;

    cls->type->fields = NULL;
    field_tail = &cls->type->fields;

    /* Space for vptr if class has virtual functions */
    bool has_virtual = false;
    for (struct CxxMember* m = cls->members; m; m = m->next) {
        if (m->is_virtual) {
            has_virtual = true;
            break;
        }
    }

    if (has_virtual) {
        int pointer_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
        offset = pointer_size;  /* vptr */
        max_align = pointer_size;
    }

    /* Base class subobjects */
    for (int i = 0; i < cls->base_count; i++) {
        CxxClass* base = cls->bases[i].base;
        if (base && !cls->bases[i].is_virtual) {
            /* Align for base */
            int align = base->align;
            offset = (offset + align - 1) & ~(align - 1);
            /* Base subobject */
            offset += base->size;
            if (align > max_align) max_align = align;
        } else {
            layout_complete = false;
        }
    }

    /* Fields from fields list */
    for (TypeParam* f = cls->fields; f; f = f->next) {
        Type* type = f->type;
        int align;
        int size;
        TypeField* field;

        if (!type || !type_is_complete(type) || type->kind == TYPE_FUNC ||
            type->kind == TYPE_VOID) {
            layout_complete = false;
            continue;
        }
        align = type->align;
        size = type->size;

        /* Align */
        offset = (offset + align - 1) & ~(align - 1);
        field = ast_arena_alloc(sizeof(*field));
        field->name = f->name;
        field->type = type;
        field->offset = offset;
        field->next = NULL;
        *field_tail = field;
        field_tail = &field->next;
        offset += size;

        if (align > max_align) max_align = align;
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

    /* Final size with alignment padding */
    cls->size = (offset + max_align - 1) & ~(max_align - 1);
    if (cls->size == 0) cls->size = 1;  /* Empty class has size 1 */
    cls->align = max_align;
    cls->type->size = cls->size;
    cls->type->align = cls->align;
    cls->type->is_complete = layout_complete;
}

void cxx_class_build_vtable(CxxClass* cls) {
    int vtable_index = 0;

    /* Inherit base class vtable entries */
    for (int i = 0; i < cls->base_count; i++) {
        CxxClass* base = cls->bases[i].base;
        if (base && base->vtable_size > vtable_index) {
            vtable_index = base->vtable_size;
        }
    }

    /* Add/override virtual functions */
    for (struct CxxMember* m = cls->members; m; m = m->next) {
        if (m->is_virtual && m->decl->kind == DECL_FUNC) {
            bool found_override = false;

            /* Check if overriding a base class function */
            for (int i = 0; i < cls->base_count && !found_override; i++) {
                CxxClass* base = cls->bases[i].base;
                if (!base) continue;
                for (int j = 0; j < base->vtable_size; j++) {
                    if (strcmp(base->vtable[j].name, m->decl->name) == 0) {
                        /* Override existing slot */
                        found_override = true;
                        break;
                    }
                }
            }

            if (!found_override) {
                /* New virtual function - allocate slot */
                vtable_index++;
            }
        }
    }

    cls->vtable_size = vtable_index;
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
    ns->templates = ast_arena_grow(
        ns->templates, sizeof(CxxTemplate*) * (size_t)ns->template_count,
        sizeof(CxxTemplate*) * (size_t)(ns->template_count + 1));
    ns->templates[ns->template_count++] = tmpl;
}

/* ═══════════════════════════════════════
 * Template Operations (Core API)
 * ═══════════════════════════════════════ */

CxxTemplate* cxx_template_alloc(const char* name, TemplateParam* params, int count) {
    CxxTemplate* tmpl = rcc_alloc(sizeof(CxxTemplate));
    tmpl->name = name ? rcc_strdup(name) : NULL;
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
    tmpl->templated_class = NULL;
    tmpl->instances = NULL;
    tmpl->instance_count = 0;
    return tmpl;
}

void* cxx_template_instantiate(CxxTemplate* tmpl, Type** args, int arg_count) {
    /* Check if already instantiated */
    for (int i = 0; i < tmpl->instance_count; i++) {
        bool match = true;
        if (tmpl->instances[i].arg_count != arg_count) continue;

        for (int j = 0; j < arg_count; j++) {
            /* Simple type comparison */
            if (tmpl->instances[i].args[j] != args[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return tmpl->instances[i].instantiated;
        }
    }

    /* TODO: Actually instantiate template */
    /* This would involve substituting template parameters in the AST */

    return NULL;
}

/* ═══════════════════════════════════════
 * C++ Initialization
 * ═══════════════════════════════════════ */

void cxx_init(void) {
    if (!g_global_namespace) {
        g_global_namespace = cxx_namespace_alloc(NULL, NULL);
    }
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
    cls->bases[cls->base_count].access = access;
    cls->bases[cls->base_count].is_virtual = false;
    cls->base_count++;
    (void)base_name;  /* TODO: Store name for later resolution */
}

/* Add field to class */
void cxx_class_add_field(CxxClass* cls, const char* name, Type* type, AccessSpec access) {
    /* Create field as TypeParam (reusing existing structure) */
    TypeParam* field = rcc_alloc(sizeof(TypeParam));
    field->name = name ? rcc_strdup(name) : NULL;
    field->type = type;
    field->next = NULL;
    (void)access;

    /* Append to fields list */
    if (!cls->fields) {
        cls->fields = field;
    } else {
        TypeParam* f = cls->fields;
        while (f->next) f = f->next;
        f->next = field;
    }
}

/* Add method to class */
void cxx_class_add_method(CxxClass* cls, CxxMethod* method) {
    /* Create member wrapper */
    struct CxxMember* member = rcc_alloc(sizeof(struct CxxMember));
    member->access = method->access;
    member->decl = method->decl;
    member->is_static = method->is_static;
    member->is_virtual = method->is_virtual;
    member->is_pure_virtual = method->is_pure_virtual;
    member->is_override = method->is_override;
    member->is_final = method->is_final;
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
        tp->next = NULL;
        *parameter_tail = tp;
        parameter_tail = &tp->next;
    }
    Type* func_type = type_func(return_type, tparams, false);

    /* Create declaration */
    method->decl = decl_func(name, func_type, params, body, loc);
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
    tmpl->param_count++;
}
