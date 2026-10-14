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

#define CXX_MANGLE_BUFFER_SIZE 8192u

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
    if (strcmp(name, "operator<=>") == 0) return "ss";
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
    static const char user_literal_prefix[] = "operator\"\"";
    size_t len;
    if (name && strncmp(name, user_literal_prefix,
                        sizeof(user_literal_prefix) - 1u) == 0) {
        const char* suffix = name + sizeof(user_literal_prefix) - 1u;
        size_t suffix_length = strlen(suffix);
        if (suffix_length == 0u || *suffix != '_') {
            rcc_fatal("C++ user-defined literal name is invalid");
        }
        if (*pos + suffix_length + 34u >= CXX_MANGLE_BUFFER_SIZE) {
            rcc_fatal("C++ mangled name is too long");
        }
        /* Itanium ABI integer literal operator: `li` + length + suffix. */
        buf[(*pos)++] = 'l';
        buf[(*pos)++] = 'i';
        *pos += (size_t)snprintf(buf + *pos,
                                 CXX_MANGLE_BUFFER_SIZE - *pos, "%lu%s",
                                 (unsigned long)suffix_length, suffix);
        return;
    }
    if (operator_code) {
        len = strlen(operator_code);
        if (*pos + len >= CXX_MANGLE_BUFFER_SIZE) {
            rcc_fatal("C++ mangled name is too long");
        }
        memcpy(buf + *pos, operator_code, len);
        *pos += len;
        return;
    }
    len = strlen(name);
    if (*pos + 32u + len >= CXX_MANGLE_BUFFER_SIZE) {
        rcc_fatal("C++ mangled name is too long");
    }
    *pos += (size_t)snprintf(buf + *pos,
                             CXX_MANGLE_BUFFER_SIZE - *pos, "%lu%s",
                             (unsigned long)len, name);
}

typedef struct CxxMangleContext {
    Type** class_types;
    size_t class_type_count;
    size_t class_type_index_base;
} CxxMangleContext;

static void cxx_mangle_type_append(char* buf, size_t* pos, Type* type);
static void cxx_mangle_type_append_context(
    char* buf, size_t* pos, Type* type, CxxMangleContext* context);
static void cxx_mangle_type_char(char* buf, size_t* pos, char value);
static void mangle_class_name(char* buf, size_t* pos, CxxClass* cls);
static size_t cxx_mangle_find_class_substitution(
    const CxxMangleContext* context, Type* type);
static void cxx_mangle_record_class_type(CxxMangleContext* context,
                                         Type* type);

static void cxx_mangle_enum_name(char* buf, size_t* pos, Type* type) {
    const char* namespace_name;
    if (!type || !type->enum_tag || type->enum_tag[0] == '\0') {
        rcc_fatal("C++ anonymous enum cannot be mangled as a type");
    }
    namespace_name = type->cxx_namespace;
    if (!namespace_name || namespace_name[0] == '\0') {
        mangle_name(buf, pos, type->enum_tag);
        return;
    }
    cxx_mangle_type_char(buf, pos, 'N');
    while (*namespace_name) {
        const char* separator = strstr(namespace_name, "::");
        size_t component_length = separator
            ? (size_t)(separator - namespace_name) : strlen(namespace_name);
        char component[512];
        if (component_length == 0u || component_length >= sizeof(component)) {
            rcc_fatal("C++ enum namespace component is invalid for mangling");
        }
        memcpy(component, namespace_name, component_length);
        component[component_length] = '\0';
        mangle_name(buf, pos, component);
        if (!separator) break;
        namespace_name = separator + 2;
    }
    mangle_name(buf, pos, type->enum_tag);
    cxx_mangle_type_char(buf, pos, 'E');
}

static Type* cxx_mangle_template_value_type(
    CxxTemplate* templ, const TemplateParam* parameter,
    Type* const* arguments,
    int argument_count, int argument_slot) {
    Type* value_type = NULL;
    Type* declared_type = parameter ? parameter->type : NULL;
    if (arguments && argument_slot >= 0 && argument_slot < argument_count) {
        value_type = arguments[argument_slot];
        if (value_type && !value_type->cxx_dependent) return value_type;
    }
    if (declared_type && declared_type->cxx_dependent_alias_template) {
        Type* substituted = rcc_cxx_template_substitute_type(
            templ, declared_type, (Type**)arguments, argument_count,
            NULL, NULL);
        if (substituted && !substituted->cxx_dependent) return substituted;
    }
    if (declared_type && declared_type->cxx_dependent &&
        declared_type->cxx_dependent_member_name &&
        declared_type->cxx_template_param_index >= 0 && arguments &&
        declared_type->cxx_template_param_index < argument_count) {
        Type* owner = arguments[
            declared_type->cxx_template_param_index];
        CxxClass* access_context = templ && templ->func_def &&
                templ->func_def->func_method_owner
            ? templ->func_def->func_method_owner->cxx_class
            : templ ? templ->templated_class : NULL;
        Type* nested_type = NULL;
        bool ambiguous = false;
        bool accessible = false;
        if (owner && owner->cxx_class &&
            cxx_class_resolve_nested_type_path(
                owner->cxx_class,
                declared_type->cxx_dependent_member_name,
                access_context, &nested_type, &ambiguous, &accessible) &&
            nested_type) {
            return nested_type;
        }
    }
    if (declared_type && declared_type->cxx_dependent &&
        declared_type->cxx_template_param_index >= 0 && arguments &&
        declared_type->cxx_template_param_index < argument_count) {
        Type* substituted = arguments[
            declared_type->cxx_template_param_index];
        if (substituted && !substituted->cxx_dependent) return substituted;
    }
    return value_type ? value_type : declared_type;
}

/* A template-template argument is encoded as a template-name, not as a type.
 * In particular, namespace-qualified names use Itanium's nested-name form. */
static void mangle_template_argument_name(char* buf, size_t* pos,
                                          Type* argument) {
    CxxNamespace* namespace_stack[32];
    CxxTemplate* actual_template = argument ? argument->cxx_template : NULL;
    int namespace_count = 0;
    if (!actual_template || !actual_template->name) {
        rcc_fatal("C++ template-template argument is missing its template name");
    }
    for (CxxNamespace* current = actual_template->ns;
         current && current->name; current = current->parent) {
        if (namespace_count >=
            (int)(sizeof(namespace_stack) / sizeof(namespace_stack[0]))) {
            rcc_fatal("C++ template-template namespace is too deep to mangle");
        }
        namespace_stack[namespace_count++] = current;
    }
    if (namespace_count > 0) {
        cxx_mangle_type_char(buf, pos, 'N');
        for (int index = namespace_count - 1; index >= 0; --index) {
            mangle_name(buf, pos, namespace_stack[index]->name);
        }
    }
    mangle_name(buf, pos, actual_template->name);
    if (namespace_count > 0) cxx_mangle_type_char(buf, pos, 'E');
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
    if (cls) mangle_class_name(buf, pos, cls);
}

/* Itanium encodes member-function cv/ref qualifiers before the nested class
 * name (for example `_ZNKR1S1fEv`), not between the class and method names. */
static void mangle_nested_method_prefix(char* buf, size_t* pos,
                                        CxxNamespace* ns, CxxClass* cls,
                                        bool is_const,
                                        bool is_volatile,
                                        CxxRefQualifier ref_qualifier) {
    CxxNamespace* ns_stack[32];
    int ns_count = 0;
    buf[(*pos)++] = '_';
    buf[(*pos)++] = 'Z';
    if (!ns && !cls) return;
    buf[(*pos)++] = 'N';
    if (is_volatile) buf[(*pos)++] = 'V';
    if (is_const) buf[(*pos)++] = 'K';
    if (ref_qualifier == CXX_REF_QUAL_LVALUE) buf[(*pos)++] = 'R';
    else if (ref_qualifier == CXX_REF_QUAL_RVALUE) buf[(*pos)++] = 'O';
    for (CxxNamespace* current = ns; current && current->name;
         current = current->parent) {
        if (ns_count < (int)(sizeof(ns_stack) / sizeof(ns_stack[0]))) {
            ns_stack[ns_count++] = current;
        }
    }
    for (int index = ns_count - 1; index >= 0; --index) {
        mangle_name(buf, pos, ns_stack[index]->name);
    }
    if (cls) mangle_class_name(buf, pos, cls);
}

static void cxx_mangle_type_char(char* buf, size_t* pos, char value) {
    if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
        rcc_fatal("C++ type name is too long");
    }
    buf[(*pos)++] = value;
}

/* Use the source template and its complete argument list for class members.
 * The parser's internal `.__instanceN` tag is intentionally useful for AST
 * ownership, but N is translation-unit order and therefore cannot identify an
 * inline variable or an in-class method across object files. */
static void mangle_class_name(char* buf, size_t* pos, CxxClass* cls) {
    CxxTemplate* tmpl;
    Type** template_args;
    int template_arg_count;
    int64_t* template_value_args;
    bool* template_value_present;
    bool expanded_pack;
    int pack_parameter_index = -1;
    CxxMangleContext template_argument_context = { 0 };
    template_argument_context.class_type_index_base = 1u;
    if (!cls) return;
    tmpl = cls->template_identity_tmpl
        ? cls->template_identity_tmpl : cls->templ;
    template_args = cls->template_identity_tmpl
        ? cls->template_identity_args : cls->template_args;
    template_arg_count = cls->template_identity_tmpl
        ? cls->template_identity_arg_count : cls->template_arg_count;
    template_value_args = cls->template_identity_tmpl
        ? cls->template_identity_value_args : cls->template_value_args;
    template_value_present = cls->template_identity_tmpl
        ? cls->template_identity_value_present : cls->template_value_present;
    if (cls->type && cls->type->cxx_scope_identity) {
        mangle_name(buf, pos, cls->type->cxx_scope_identity);
        mangle_name(buf, pos, cls->name);
        return;
    }
    if (tmpl) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            if (!tmpl->params[index].is_pack) continue;
            if (pack_parameter_index >= 0 ||
                index != tmpl->param_count - 1) {
                pack_parameter_index = -2;
                break;
            }
            pack_parameter_index = index;
        }
    }
    expanded_pack = pack_parameter_index >= 0;
    if (!tmpl || !tmpl->name ||
        ((!expanded_pack && template_arg_count != tmpl->param_count) ||
         (expanded_pack && template_arg_count < pack_parameter_index) ||
         (template_arg_count > 0 && !template_args))) {
        mangle_name(buf, pos, cls->name);
        return;
    }
    mangle_name(buf, pos, tmpl->name);
    if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
        rcc_fatal("C++ template class name is too long");
    }
    buf[(*pos)++] = 'I';
    for (int index = 0; expanded_pack && index < pack_parameter_index;
         ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind == TPARAM_TYPE) {
            cxx_mangle_type_append_context(
                buf, pos, template_args[index], &template_argument_context);
        } else if (parameter->kind == TPARAM_TEMPLATE) {
            mangle_template_argument_name(buf, pos, template_args[index]);
        } else if (parameter->kind == TPARAM_NONTYPE) {
            int written;
            if (!template_value_present || !template_value_present[index] ||
                !template_value_args) {
                rcc_fatal("C++ template class value argument is missing");
            }
            cxx_mangle_type_char(buf, pos, 'L');
            cxx_mangle_type_append_context(
                buf, pos,
                cxx_mangle_template_value_type(
                    tmpl, parameter, template_args, template_arg_count,
                    index),
                &template_argument_context);
            if (template_value_args[index] < 0) {
                uint64_t magnitude = (uint64_t)(
                    -(template_value_args[index] + 1)) + 1u;
                written = snprintf(buf + *pos,
                                   CXX_MANGLE_BUFFER_SIZE - *pos, "n%lluE",
                                   (unsigned long long)magnitude);
            } else {
                written = snprintf(buf + *pos,
                                   CXX_MANGLE_BUFFER_SIZE - *pos, "%lldE",
                                   (long long)template_value_args[index]);
            }
            if (written < 0 ||
                (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
                rcc_fatal("C++ template class name is too long");
            }
            *pos += (size_t)written;
        }
    }
    if (expanded_pack) {
        if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
            rcc_fatal("C++ template class name is too long");
        }
        buf[(*pos)++] = 'J';
        for (int pack_index = 0;
             pack_index < template_arg_count - pack_parameter_index;
             ++pack_index) {
            int written;
            if (tmpl->params[pack_parameter_index].kind == TPARAM_TYPE) {
                cxx_mangle_type_append_context(
                    buf, pos,
                    template_args[pack_parameter_index + pack_index],
                    &template_argument_context);
            } else if (tmpl->params[pack_parameter_index].kind ==
                       TPARAM_TEMPLATE) {
                mangle_template_argument_name(
                    buf, pos, template_args[pack_parameter_index + pack_index]);
            } else {
                if (!template_value_present ||
                    !template_value_present[pack_parameter_index + pack_index] ||
                    !template_value_args) {
                    rcc_fatal("C++ template class value pack argument is missing");
                }
                if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
                    rcc_fatal("C++ template class name is too long");
                }
                buf[(*pos)++] = 'L';
                cxx_mangle_type_append_context(
                    buf, pos,
                    cxx_mangle_template_value_type(
                        tmpl, &tmpl->params[pack_parameter_index],
                        template_args, template_arg_count,
                        pack_parameter_index + pack_index),
                    &template_argument_context);
                if (template_value_args[pack_parameter_index + pack_index] < 0) {
                    uint64_t magnitude =
                        (uint64_t)(-(
                            template_value_args[pack_parameter_index + pack_index] +
                            1)) + 1u;
                    written = snprintf(
                        buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos, "n%lluE",
                                       (unsigned long long)magnitude);
                } else {
                    written = snprintf(buf + *pos,
                                       CXX_MANGLE_BUFFER_SIZE - *pos, "%lldE",
                        (long long)template_value_args[
                            pack_parameter_index + pack_index]);
                }
                if (written < 0 ||
                    (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
                    rcc_fatal("C++ template class name is too long");
                }
                *pos += (size_t)written;
            }
        }
        if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
            rcc_fatal("C++ template class name is too long");
        }
        buf[(*pos)++] = 'E';
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (expanded_pack && index <= pack_parameter_index) continue;
        if (parameter->kind == TPARAM_TYPE) {
            cxx_mangle_type_append_context(
                buf, pos, template_args[index], &template_argument_context);
        } else if (parameter->kind == TPARAM_NONTYPE) {
            int written;
            if (!template_value_present || !template_value_present[index] ||
                !template_value_args) {
                rcc_fatal("C++ template class value argument is missing");
            }
            if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
                rcc_fatal("C++ template class name is too long");
            }
            buf[(*pos)++] = 'L';
            cxx_mangle_type_append_context(
                buf, pos,
                cxx_mangle_template_value_type(
                    tmpl, parameter, template_args, template_arg_count,
                    index),
                &template_argument_context);
            if (template_value_args[index] < 0) {
                uint64_t magnitude =
                    (uint64_t)(-(template_value_args[index] + 1)) + 1u;
                written = snprintf(buf + *pos,
                                   CXX_MANGLE_BUFFER_SIZE - *pos, "n%lluE",
                                   (unsigned long long)magnitude);
            } else {
                written = snprintf(buf + *pos,
                                   CXX_MANGLE_BUFFER_SIZE - *pos, "%lldE",
                                   (long long)template_value_args[index]);
            }
            if (written < 0 ||
                (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
                rcc_fatal("C++ template class name is too long");
            }
            *pos += (size_t)written;
        } else if (parameter->kind == TPARAM_TEMPLATE) {
            if (!template_args || !template_args[index] ||
                !template_args[index]->cxx_template) {
                rcc_fatal("C++ template class template argument is missing");
            }
            mangle_template_argument_name(buf, pos, template_args[index]);
        } else {
            rcc_fatal("unsupported C++ template class parameter");
        }
    }
    if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
        rcc_fatal("C++ template class name is too long");
    }
    buf[(*pos)++] = 'E';
}

static size_t cxx_mangle_find_class_substitution(
    const CxxMangleContext* context, Type* type) {
    if (!context || !type) return (size_t)-1;
    for (size_t index = 0; index < context->class_type_count; ++index) {
        if (type_is_compatible(context->class_types[index], type)) {
            return index + context->class_type_index_base;
        }
    }
    return (size_t)-1;
}

static void cxx_mangle_append_substitution(char* buf, size_t* pos,
                                           size_t index) {
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char encoded[sizeof(size_t) * 8u];
    size_t count = 0u;
    size_t value;
    cxx_mangle_type_char(buf, pos, 'S');
    if (index == 0u) {
        cxx_mangle_type_char(buf, pos, '_');
        return;
    }
    value = index - 1u;
    do {
        encoded[count++] = digits[value % 36u];
        value /= 36u;
    } while (value != 0u);
    while (count > 0u) {
        cxx_mangle_type_char(buf, pos, encoded[--count]);
    }
    cxx_mangle_type_char(buf, pos, '_');
}

static void cxx_mangle_record_class_type(CxxMangleContext* context,
                                         Type* type) {
    if (!context || !type ||
        cxx_mangle_find_class_substitution(context, type) != (size_t)-1) {
        return;
    }
    context->class_types = ast_arena_grow(
        context->class_types,
        context->class_type_count * sizeof(*context->class_types),
        (context->class_type_count + 1u) * sizeof(*context->class_types));
    context->class_types[context->class_type_count++] = type;
}

/* Append a type without recursively reusing the public static result buffer.
 * The function-signature context retains repeated class-type substitutions
 * across parameters, as required by the Itanium ABI. */
static void cxx_mangle_type_append_context(
    char* buf, size_t* pos, Type* type, CxxMangleContext* context) {
    if (!type) {
        cxx_mangle_type_char(buf, pos, 'v');
        return;
    }

    while (type->kind == TYPE_PTR && !type->cxx_is_member_pointer) {
        cxx_mangle_type_char(buf, pos,
                             type->is_reference
                                 ? (type->is_rvalue_reference ? 'O' : 'R')
                                 : 'P');
        type = type->base;
    }

    if (type->kind == TYPE_PTR && type->cxx_is_member_pointer) {
        size_t owner_substitution = cxx_mangle_find_class_substitution(
            context, type->cxx_member_pointer_owner);
        cxx_mangle_type_char(buf, pos, 'M');
        if (owner_substitution != (size_t)-1) {
            cxx_mangle_append_substitution(buf, pos, owner_substitution);
        } else {
            cxx_mangle_type_append_context(
                buf, pos, type->cxx_member_pointer_owner, context);
        }
        cxx_mangle_type_append_context(buf, pos, type->base, context);
        return;
    }

    if (type->kind == TYPE_FUNC) {
        if (type->is_volatile) cxx_mangle_type_char(buf, pos, 'V');
        if (type->is_const) cxx_mangle_type_char(buf, pos, 'K');
        if (type->function_is_noexcept) {
            cxx_mangle_type_char(buf, pos, 'D');
            cxx_mangle_type_char(buf, pos, 'o');
        }
    } else {
        if (type->is_volatile) cxx_mangle_type_char(buf, pos, 'V');
        if (type->is_const) cxx_mangle_type_char(buf, pos, 'K');
    }

    switch (type->kind) {
        case TYPE_VOID:   cxx_mangle_type_char(buf, pos, 'v'); break;
        case TYPE_BOOL:   cxx_mangle_type_char(buf, pos, 'b'); break;
        case TYPE_CHAR:
            if (type->is_char8) {
                cxx_mangle_type_char(buf, pos, 'D');
                cxx_mangle_type_char(buf, pos, 'u');
            } else if (type->is_plain_char) {
                cxx_mangle_type_char(buf, pos, 'c');
            } else {
                cxx_mangle_type_char(buf, pos,
                                     type->is_unsigned ? 'h' : 'a');
            }
            break;
        case TYPE_SHORT:
            if (type->is_char16) {
                cxx_mangle_type_char(buf, pos, 'D');
                cxx_mangle_type_char(buf, pos, 's');
            } else {
                cxx_mangle_type_char(buf, pos,
                                     type->is_unsigned ? 't' : 's');
            }
            break;
        case TYPE_INT:
            if (type->is_wchar) {
                cxx_mangle_type_char(buf, pos, 'w');
            } else if (type->is_char32) {
                cxx_mangle_type_char(buf, pos, 'D');
                cxx_mangle_type_char(buf, pos, 'i');
            } else {
                cxx_mangle_type_char(buf, pos,
                                     type->is_unsigned ? 'j' : 'i');
            }
            break;
        case TYPE_LONG:   cxx_mangle_type_char(buf, pos,
                              type->is_unsigned ? 'm' : 'l'); break;
        case TYPE_LLONG:  cxx_mangle_type_char(buf, pos,
                              type->is_unsigned ? 'y' : 'x'); break;
        case TYPE_FLOAT:  cxx_mangle_type_char(buf, pos, 'f'); break;
        case TYPE_DOUBLE: cxx_mangle_type_char(buf, pos, 'd'); break;
        case TYPE_VECTOR: {
            int written = snprintf(buf + *pos,
                                   CXX_MANGLE_BUFFER_SIZE - *pos, "Dv%d_",
                                   type->array_len);
            if (written < 0 ||
                (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
                rcc_fatal("C++ vector type name is too long");
            }
            *pos += (size_t)written;
            cxx_mangle_type_append_context(buf, pos, type->base, context);
            break;
        }
        case TYPE_NULLPTR:
            cxx_mangle_type_char(buf, pos, 'D');
            cxx_mangle_type_char(buf, pos, 'n');
            break;
        case TYPE_ENUM: {
            size_t substitution = cxx_mangle_find_class_substitution(
                context, type);
            if (substitution != (size_t)-1) {
                cxx_mangle_append_substitution(buf, pos, substitution);
            } else {
                cxx_mangle_enum_name(buf, pos, type);
                cxx_mangle_record_class_type(context, type);
            }
            break;
        }
        case TYPE_STRUCT:
        case TYPE_UNION:
            {
                size_t substitution = cxx_mangle_find_class_substitution(
                    context, type);
                if (substitution != (size_t)-1) {
                    cxx_mangle_append_substitution(buf, pos, substitution);
                    break;
                }
                if (!type->tag) {
                    rcc_fatal("C++ anonymous type cannot be mangled");
                }
                if (type->cxx_class) {
                    mangle_class_name(buf, pos, type->cxx_class);
                } else {
                    mangle_name(buf, pos, type->tag);
                }
                cxx_mangle_record_class_type(context, type);
            }
            break;
        case TYPE_ARRAY: {
            char base_mangled[CXX_MANGLE_BUFFER_SIZE];
            size_t base_pos = 0u;
            int written;
            cxx_mangle_type_append_context(
                base_mangled, &base_pos, type->base, context);
            base_mangled[base_pos] = '\0';
            written = type->array_len >= 0
                ? snprintf(buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos, "A%d%s",
                           type->array_len, base_mangled)
                : snprintf(buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                           "A_%s", base_mangled);
            if (written < 0 ||
                (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
                rcc_fatal("C++ array type name is too long");
            }
            *pos += (size_t)written;
            break;
        }
        case TYPE_FUNC:
            cxx_mangle_type_char(buf, pos, 'F');
            cxx_mangle_type_append_context(
                buf, pos, type->ret_type, context);
            if (type->params) {
                for (TypeParam* parameter = type->params; parameter;
                     parameter = parameter->next) {
                    cxx_mangle_type_append_context(
                        buf, pos, parameter->type, context);
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

static void cxx_mangle_type_append(char* buf, size_t* pos, Type* type) {
    cxx_mangle_type_append_context(buf, pos, type, NULL);
}

/* Mangle a type */
char* cxx_mangle_type(Type* type) {
    static char buf[CXX_MANGLE_BUFFER_SIZE];
    size_t pos = 0u;
    cxx_mangle_type_append(buf, &pos, type);
    buf[pos] = '\0';
    return buf;
}

/* Mangle a simple name with namespace */
char* cxx_mangle_name(const char* name, CxxNamespace* ns, CxxClass* cls) {
    static char buf[CXX_MANGLE_BUFFER_SIZE];
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

static char* cxx_mangle_member_template_function_name(
    Decl* func, CxxNamespace* ns, CxxClass* cls) {
    static char buf[CXX_MANGLE_BUFFER_SIZE];
    size_t pos = 0;
    bool is_const_method = func && func->type && func->type->is_const;
    bool is_volatile_method = func && func->type && func->type->is_volatile;
    CxxRefQualifier ref_qualifier = func && func->type
        ? func->type->function_ref_qualifier : CXX_REF_QUAL_NONE;
    if (cls && (is_const_method || is_volatile_method ||
                ref_qualifier != CXX_REF_QUAL_NONE)) {
        mangle_nested_method_prefix(buf, &pos, ns, cls, is_const_method,
                                    is_volatile_method, ref_qualifier);
    } else {
        mangle_nested_prefix(buf, &pos, ns, cls);
    }
    mangle_name(buf, &pos, func ? func->name : NULL);
    buf[pos] = '\0';
    return buf;
}

/* Mangle a function */
char* cxx_mangle_function(Decl* func, CxxNamespace* ns, CxxClass* cls) {
    static char buf[CXX_MANGLE_BUFFER_SIZE];
    size_t pos = 0;
    CxxMangleContext mangle_context = {0};
    bool is_const_method = false;
    bool is_volatile_method = false;
    CxxRefQualifier ref_qualifier = CXX_REF_QUAL_NONE;

    if (func && cls && func->func_is_cxx_method && func->type) {
        is_const_method = func->type->is_const;
        is_volatile_method = func->type->is_volatile;
        ref_qualifier = func->type->function_ref_qualifier;
    } else if (func && cls) {
        for (struct CxxMember* member = cls->members; member;
             member = member->next) {
            if (member->method && member->method->decl == func) {
                is_const_method = member->method->is_const;
                is_volatile_method = member->method->is_volatile;
                ref_qualifier = member->method->ref_qualifier;
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
        mangle_nested_method_prefix(buf, &pos, ns, cls, is_const_method,
                                    is_volatile_method, ref_qualifier);
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
        if (cls && (is_const_method || is_volatile_method ||
                    ref_qualifier != CXX_REF_QUAL_NONE)) {
            mangle_nested_method_prefix(buf, &pos, ns, cls,
                                        is_const_method, is_volatile_method,
                                        ref_qualifier);
            mangle_name(buf, &pos, func->name);
            buf[pos++] = 'E';
        } else {
            /* Get base mangled name */
            char* base = cxx_mangle_name(func->name, ns, cls);
            strcpy(buf, base);
            pos = strlen(buf);
        }
    }

    /* A member's nested-name prefix contributes substitutions before its
     * source parameter types.  Record the primary class and then the current
     * specialization so parameters that name the enclosing class use the
     * same Itanium substitutions as other C++ compilers. */
    if (cls && cls->templ && cls->templ->templated_class &&
        cls->templ->templated_class->type) {
        cxx_mangle_record_class_type(
            &mangle_context, cls->templ->templated_class->type);
    }
    if (cls && cls->type) {
        cxx_mangle_record_class_type(&mangle_context, cls->type);
    }

    /* Add parameter types */
    if (func->func_params) {
        for (DeclList* p = func->func_params; p; p = p->next) {
            char type_mangled[CXX_MANGLE_BUFFER_SIZE];
            size_t type_length = 0u;
            cxx_mangle_type_append_context(
                type_mangled, &type_length, p->decl->type,
                &mangle_context);
            if (type_length >= sizeof(type_mangled) ||
                pos + type_length >= sizeof(buf)) {
                rcc_fatal("C++ mangled function signature is too long");
            }
            type_mangled[type_length] = '\0';
            memcpy(buf + pos, type_mangled, type_length + 1u);
            pos += type_length;
        }
    } else {
        /* No parameters = void */
        buf[pos++] = 'v';
    }

    if (func && func->type && func->type->kind == TYPE_FUNC &&
        func->type->variadic) {
        if (pos + 1u >= sizeof(buf)) {
            rcc_fatal("C++ variadic function name is too long");
        }
        buf[pos++] = 'z';
    }

    buf[pos] = '\0';
    return buf;
}

static int cxx_function_template_type_parameter_index(
    CxxTemplate* tmpl, Type* type) {
    if (!tmpl || !type || type->kind != TYPE_STRUCT || !type->tag) {
        return -1;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_TYPE &&
            tmpl->params[index].name &&
            strcmp(type->tag, tmpl->params[index].name) == 0) {
            return index;
        }
    }
    return -1;
}

typedef struct CxxMemberTemplateMangleContext {
    int* parameter_substitutions;
    size_t next_substitution;
    Type** type_substitutions;
    size_t* type_substitution_indices;
    size_t type_substitution_count;
} CxxMemberTemplateMangleContext;

static void cxx_mangle_template_type_append(
    char* buf, size_t* pos, Type* type, CxxTemplate* tmpl,
    CxxMemberTemplateMangleContext* context);
static void cxx_mangle_template_type_append_impl(
    char* buf, size_t* pos, Type* type, CxxTemplate* tmpl,
    CxxMemberTemplateMangleContext* context);

static bool cxx_member_template_expression_supported(
    Expr* expression, CxxTemplate* tmpl) {
    int64_t value;
    if (!expression) return false;
    if (expr_eval_integer_constant(expression, &value)) return true;
    if (expression->kind == EXPR_IDENT && expression->ident_name && tmpl) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE && !parameter->is_pack &&
                parameter->name &&
                strcmp(parameter->name, expression->ident_name) == 0) {
                return true;
            }
        }
        return false;
    }
    if (expression->kind == EXPR_SIZEOF &&
        expression->sizeof_pack_name && tmpl) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack && parameter->name &&
                strcmp(parameter->name,
                       expression->sizeof_pack_name) == 0) {
                return true;
            }
        }
        return false;
    }
    if (expression->kind == EXPR_SIZEOF && expression->sizeof_type) {
        return cxx_function_template_type_parameter_index(
                   tmpl, expression->sizeof_type) >= 0;
    }
    if (expression->kind == EXPR_NEG || expression->kind == EXPR_NOT ||
        expression->kind == EXPR_BITNOT) {
        return cxx_member_template_expression_supported(
            expression->unary_operand, tmpl);
    }
    if (expression->kind == EXPR_ADD || expression->kind == EXPR_SUB ||
        expression->kind == EXPR_MUL || expression->kind == EXPR_DIV ||
        expression->kind == EXPR_MOD || expression->kind == EXPR_BITAND ||
        expression->kind == EXPR_BITOR || expression->kind == EXPR_BITXOR ||
        expression->kind == EXPR_LSHIFT || expression->kind == EXPR_RSHIFT ||
        expression->kind == EXPR_EQ || expression->kind == EXPR_NE ||
        expression->kind == EXPR_LT || expression->kind == EXPR_GT ||
        expression->kind == EXPR_LE || expression->kind == EXPR_GE ||
        expression->kind == EXPR_AND || expression->kind == EXPR_OR) {
        return cxx_member_template_expression_supported(
                   expression->binary_lhs, tmpl) &&
               cxx_member_template_expression_supported(
                   expression->binary_rhs, tmpl);
    }
    if (expression->kind == EXPR_COND) {
        return cxx_member_template_expression_supported(
                   expression->cond_test, tmpl) &&
               cxx_member_template_expression_supported(
                   expression->cond_then, tmpl) &&
               cxx_member_template_expression_supported(
                   expression->cond_else, tmpl);
    }
    if (expression->kind == EXPR_CAST) {
        return expression->cast_type &&
               cxx_member_template_expression_supported(
                   expression->cast_expr, tmpl);
    }
    return false;
}

static void cxx_member_template_expression_append(
    char* buf, size_t* pos, Expr* expression, CxxTemplate* tmpl,
    CxxMemberTemplateMangleContext* context) {
    int64_t value;
    const char* operation = NULL;
    if (expr_eval_integer_constant(expression, &value)) {
        char* type = cxx_mangle_type(
            expression->type ? expression->type : type_int);
        size_t type_length = type ? strlen(type) : 0u;
        if (!type || *pos + type_length + 32u >= CXX_MANGLE_BUFFER_SIZE) {
            rcc_fatal("C++ member template expression is too long");
        }
        buf[(*pos)++] = 'L';
        memcpy(buf + *pos, type, type_length);
        *pos += type_length;
        if (value < 0) {
            uint64_t magnitude = (uint64_t)(-(value + 1)) + 1u;
            *pos += (size_t)snprintf(
                buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos, "n%llu",
                (unsigned long long)magnitude);
        } else {
            *pos += (size_t)snprintf(
                buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos, "%lld",
                (long long)value);
        }
        buf[(*pos)++] = 'E';
        return;
    }
    if (expression->kind == EXPR_IDENT && expression->ident_name) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind != TPARAM_NONTYPE || parameter->is_pack ||
                !parameter->name ||
                strcmp(parameter->name, expression->ident_name) != 0) {
                continue;
            }
            if (*pos + 16u >= CXX_MANGLE_BUFFER_SIZE) {
                rcc_fatal("C++ member template expression is too long");
            }
            buf[(*pos)++] = 'T';
            if (index > 0) {
                int written = snprintf(buf + *pos,
                                       CXX_MANGLE_BUFFER_SIZE - *pos,
                                       "%d", index - 1);
                if (written < 0 || (size_t)written >=
                        CXX_MANGLE_BUFFER_SIZE - *pos) {
                    rcc_fatal("C++ member template parameter index is too long");
                }
                *pos += (size_t)written;
            }
            buf[(*pos)++] = '_';
            return;
        }
    }
    if (expression->kind == EXPR_SIZEOF &&
        expression->sizeof_pack_name) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (!parameter->is_pack || !parameter->name ||
                strcmp(parameter->name,
                       expression->sizeof_pack_name) != 0) {
                continue;
            }
            if (*pos + 16u >= CXX_MANGLE_BUFFER_SIZE) {
                rcc_fatal("C++ member template expression is too long");
            }
            buf[(*pos)++] = 's';
            buf[(*pos)++] = 'Z';
            buf[(*pos)++] = 'T';
            if (index > 0) {
                int written = snprintf(buf + *pos,
                                       CXX_MANGLE_BUFFER_SIZE - *pos,
                                       "%d", index - 1);
                if (written < 0 || (size_t)written >=
                        CXX_MANGLE_BUFFER_SIZE - *pos) {
                    rcc_fatal("C++ member template parameter index is too long");
                }
                *pos += (size_t)written;
            }
            buf[(*pos)++] = '_';
            return;
        }
    }
    if (expression->kind == EXPR_SIZEOF && expression->sizeof_type) {
        if (cxx_function_template_type_parameter_index(
                tmpl, expression->sizeof_type) >= 0) {
            if (*pos + 2u >= CXX_MANGLE_BUFFER_SIZE) {
                rcc_fatal("C++ member template expression is too long");
            }
            buf[(*pos)++] = 's';
            buf[(*pos)++] = 't';
            cxx_mangle_template_type_append(
                buf, pos, expression->sizeof_type, tmpl, context);
            return;
        }
    }
    if (expression->kind == EXPR_CAST) {
        const char* cast_operator = "cv";
        char* cast_type = cxx_mangle_type(expression->cast_type);
        size_t cast_type_length = cast_type ? strlen(cast_type) : 0u;
        switch (expression->cxx_cast_kind) {
            case CXX_CAST_STATIC: cast_operator = "sc"; break;
            case CXX_CAST_REINTERPRET: cast_operator = "rc"; break;
            case CXX_CAST_CONST: cast_operator = "cc"; break;
            case CXX_CAST_DYNAMIC: cast_operator = "dc"; break;
            case CXX_CAST_NONE: break;
        }
        if (!cast_type ||
            *pos + cast_type_length + strlen(cast_operator) >=
                CXX_MANGLE_BUFFER_SIZE) {
            rcc_fatal("C++ member template cast expression is too long");
        }
        size_t cast_operator_length = strlen(cast_operator);
        memcpy(buf + *pos, cast_operator, cast_operator_length);
        *pos += cast_operator_length;
        memcpy(buf + *pos, cast_type, cast_type_length);
        *pos += cast_type_length;
        cxx_member_template_expression_append(
            buf, pos, expression->cast_expr, tmpl, context);
        return;
    }
    if (expression->kind == EXPR_COND) {
        operation = "qu";
    } else if (expression->kind == EXPR_ADD) operation = "pl";
    else if (expression->kind == EXPR_SUB) operation = "mi";
    else if (expression->kind == EXPR_MUL) operation = "ml";
    else if (expression->kind == EXPR_DIV) operation = "dv";
    else if (expression->kind == EXPR_MOD) operation = "rm";
    else if (expression->kind == EXPR_BITAND) operation = "an";
    else if (expression->kind == EXPR_BITOR) operation = "or";
    else if (expression->kind == EXPR_BITXOR) operation = "eo";
    else if (expression->kind == EXPR_LSHIFT) operation = "ls";
    else if (expression->kind == EXPR_RSHIFT) operation = "rs";
    else if (expression->kind == EXPR_EQ) operation = "eq";
    else if (expression->kind == EXPR_NE) operation = "ne";
    else if (expression->kind == EXPR_LT) operation = "lt";
    else if (expression->kind == EXPR_GT) operation = "gt";
    else if (expression->kind == EXPR_LE) operation = "le";
    else if (expression->kind == EXPR_GE) operation = "ge";
    else if (expression->kind == EXPR_AND) operation = "aa";
    else if (expression->kind == EXPR_OR) operation = "oo";
    else if (expression->kind == EXPR_NEG) operation = "ng";
    else if (expression->kind == EXPR_NOT) operation = "nt";
    else if (expression->kind == EXPR_BITNOT) operation = "co";
    if (!operation) return;
    size_t operation_length = strlen(operation);
    if (*pos + operation_length >= CXX_MANGLE_BUFFER_SIZE) {
        rcc_fatal("C++ member template expression is too long");
    }
    memcpy(buf + *pos, operation, operation_length);
    *pos += operation_length;
    if (expression->kind == EXPR_NEG || expression->kind == EXPR_NOT ||
        expression->kind == EXPR_BITNOT) {
        cxx_member_template_expression_append(
            buf, pos, expression->unary_operand, tmpl, context);
    } else if (expression->kind == EXPR_COND) {
        cxx_member_template_expression_append(
            buf, pos, expression->cond_test, tmpl, context);
        cxx_member_template_expression_append(
            buf, pos, expression->cond_then, tmpl, context);
        cxx_member_template_expression_append(
            buf, pos, expression->cond_else, tmpl, context);
    } else {
        cxx_member_template_expression_append(
            buf, pos, expression->binary_lhs, tmpl, context);
        cxx_member_template_expression_append(
            buf, pos, expression->binary_rhs, tmpl, context);
    }
}

static void cxx_mangle_member_template_substitution(
    char* buf, size_t* pos, size_t index) {
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char encoded[sizeof(size_t) * 8u];
    size_t count = 0u;
    size_t value;
    if (*pos + 2u >= CXX_MANGLE_BUFFER_SIZE) {
        rcc_fatal("C++ member template signature is too long");
    }
    buf[(*pos)++] = 'S';
    if (index == 0u) {
        buf[(*pos)++] = '_';
        return;
    }
    value = index - 1u;
    do {
        encoded[count++] = digits[value % 36u];
        value /= 36u;
    } while (value != 0u);
    while (count > 0u) {
        if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
            rcc_fatal("C++ member template signature is too long");
        }
        buf[(*pos)++] = encoded[--count];
    }
    if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) {
        rcc_fatal("C++ member template signature is too long");
    }
    buf[(*pos)++] = '_';
}

static size_t cxx_mangle_member_template_substitution_base(
    CxxNamespace* ns, CxxClass* cls) {
    /* The enclosing class prefix and the completed member-name prefix are
     * already in the nested name before the function type is encoded. */
    size_t count = cls ? 2u : 0u;
    /* A class-template specialization contributes its template-name prefix
     * to the ABI substitution table in addition to the class and member
     * prefixes. */
    if (cls && cls->templ) ++count;
    for (CxxNamespace* current = ns; current && current->name;
         current = current->parent) {
        ++count;
    }
    return count;
}

static size_t cxx_mangle_free_template_substitution_base(CxxNamespace* ns) {
    if (!ns) return 1u;
    /* A nested namespace and the template function name precede the function
     * type in the Itanium substitution table. */
    return cxx_mangle_member_template_substitution_base(ns, NULL) + 1u;
}

static bool cxx_mangle_dependent_nested_type_append(
    char* buf, size_t* pos, Type* type) {
    const char* component;
    int parameter_index;
    if (!type || type->kind != TYPE_STRUCT || !type->cxx_dependent ||
        !type->cxx_dependent_member_name ||
        !*type->cxx_dependent_member_name ||
        type->cxx_template_param_index < 0) {
        return false;
    }
    parameter_index = type->cxx_template_param_index;
    component = type->cxx_dependent_member_name;
    while (*component) {
        const char* separator = strstr(component, "::");
        size_t component_length = separator
            ? (size_t)(separator - component) : strlen(component);
        if (component_length == 0u || (separator && !separator[2])) {
            return false;
        }
        component = separator ? separator + 2 : component + component_length;
    }
#define CXX_MANGLE_DEPENDENT_APPEND(value) do { \
    if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) { \
        rcc_fatal("C++ dependent nested type name is too long"); \
    } \
    buf[(*pos)++] = (value); \
} while (0)
    CXX_MANGLE_DEPENDENT_APPEND('N');
    CXX_MANGLE_DEPENDENT_APPEND('T');
    if (parameter_index > 0) {
        int written = snprintf(buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                               "%d", parameter_index - 1);
        if (written < 0 ||
            (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
            rcc_fatal("C++ dependent nested type parameter is too long");
        }
        *pos += (size_t)written;
    }
    CXX_MANGLE_DEPENDENT_APPEND('_');
    component = type->cxx_dependent_member_name;
    while (*component) {
        const char* separator = strstr(component, "::");
        size_t component_length = separator
            ? (size_t)(separator - component) : strlen(component);
        int written;
        written = snprintf(buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                           "%zu", component_length);
        if (written < 0 ||
            (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos ||
            component_length >= CXX_MANGLE_BUFFER_SIZE - *pos -
                                   (size_t)written) {
            rcc_fatal("C++ dependent nested type name is too long");
        }
        *pos += (size_t)written;
        memcpy(buf + *pos, component, component_length);
        *pos += component_length;
        if (!separator) break;
        component = separator + 2;
    }
    CXX_MANGLE_DEPENDENT_APPEND('E');
#undef CXX_MANGLE_DEPENDENT_APPEND
    return true;
}

static void cxx_mangle_template_type_append_impl(
    char* buf, size_t* pos, Type* type, CxxTemplate* tmpl,
    CxxMemberTemplateMangleContext* context) {
#define CXX_MANGLE_TEMPLATE_APPEND(value) do { \
    if (*pos + 1u >= CXX_MANGLE_BUFFER_SIZE) { \
        rcc_fatal("C++ member template signature is too long"); \
    } \
    buf[(*pos)++] = (value); \
} while (0)
    if (!type) {
        CXX_MANGLE_TEMPLATE_APPEND('v');
        return;
    }
    while (type->kind == TYPE_PTR && !type->cxx_is_member_pointer) {
        CXX_MANGLE_TEMPLATE_APPEND(
            type->is_reference
                ? (type->is_rvalue_reference ? 'O' : 'R') : 'P');
        type = type->base;
    }
    if (type->kind == TYPE_PTR && type->cxx_is_member_pointer) {
        CXX_MANGLE_TEMPLATE_APPEND('M');
        cxx_mangle_template_type_append(
            buf, pos, type->cxx_member_pointer_owner, tmpl, context);
        cxx_mangle_template_type_append(buf, pos, type->base, tmpl, context);
        return;
    }
    if (type->is_volatile) CXX_MANGLE_TEMPLATE_APPEND('V');
    if (type->is_const) CXX_MANGLE_TEMPLATE_APPEND('K');
    {
        int parameter_index =
            cxx_function_template_type_parameter_index(tmpl, type);
        if (parameter_index >= 0) {
            if (context && context->parameter_substitutions &&
                context->parameter_substitutions[parameter_index] >= 0) {
                cxx_mangle_member_template_substitution(
                    buf, pos,
                    (size_t)context->parameter_substitutions[parameter_index]);
                return;
            }
            CXX_MANGLE_TEMPLATE_APPEND('T');
            if (parameter_index > 0) {
                int written = snprintf(buf + *pos,
                                       CXX_MANGLE_BUFFER_SIZE - *pos,
                                       "%d", parameter_index - 1);
                if (written < 0 ||
                    (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
                    rcc_fatal("C++ member template parameter index is too long");
                }
                *pos += (size_t)written;
            }
            CXX_MANGLE_TEMPLATE_APPEND('_');
            if (context && context->parameter_substitutions) {
                context->parameter_substitutions[parameter_index] =
                    (int)context->next_substitution++;
            }
            return;
        }
    }
    if (type->kind == TYPE_ARRAY) {
        int written = type->array_len >= 0
            ? snprintf(buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                       "A%d_", type->array_len)
            : snprintf(buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos, "A_");
        if (written < 0 ||
            (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
            rcc_fatal("C++ member template array type is too long");
        }
        *pos += (size_t)written;
        cxx_mangle_template_type_append(
            buf, pos, type->base, tmpl, context);
        return;
    }
    if (type->kind == TYPE_FUNC) {
        if (type->function_is_noexcept) {
            CXX_MANGLE_TEMPLATE_APPEND('D');
            CXX_MANGLE_TEMPLATE_APPEND('o');
        }
        CXX_MANGLE_TEMPLATE_APPEND('F');
        cxx_mangle_template_type_append(
            buf, pos, type->ret_type, tmpl, context);
        if (type->params) {
            for (TypeParam* parameter = type->params; parameter;
                 parameter = parameter->next) {
                cxx_mangle_template_type_append(
                    buf, pos, parameter->type, tmpl, context);
            }
        } else {
            CXX_MANGLE_TEMPLATE_APPEND('v');
        }
        if (type->variadic) CXX_MANGLE_TEMPLATE_APPEND('z');
        CXX_MANGLE_TEMPLATE_APPEND('E');
        return;
    }
    if (cxx_mangle_dependent_nested_type_append(buf, pos, type)) {
        return;
    }
    if (type->kind == TYPE_STRUCT && type->cxx_dependent &&
        type->cxx_template_param_index >= 0 &&
        type->cxx_template_arg_count > 0 && type->cxx_template_args) {
        int parameter_index = type->cxx_template_param_index;
        CxxTemplate* signature = parameter_index < tmpl->param_count
            ? tmpl->params[parameter_index].template_signature : NULL;
        int64_t nested_values[32] = { 0 };
        int nested_value_parameter_indices[32];
        Expr* nested_value_expressions[32] = { NULL };
        bool encodable = parameter_index >= 0 &&
            parameter_index < tmpl->param_count &&
            tmpl->params[parameter_index].kind == TPARAM_TEMPLATE &&
            signature && type->cxx_template_arg_count <= 32 &&
            type->cxx_template_arg_count == signature->param_count &&
            type->cxx_template_value_args;
        for (int index = 0; index < 32; ++index) {
            nested_value_parameter_indices[index] = -1;
        }
        for (int index = 0; encodable &&
             index < type->cxx_template_arg_count; ++index) {
            TemplateParam* nested_parameter = &signature->params[index];
            if (nested_parameter->kind == TPARAM_TYPE) continue;
            if (nested_parameter->kind != TPARAM_NONTYPE ||
                !nested_parameter->type) {
                encodable = false;
                break;
            }
            Expr* expression = type->cxx_template_value_args[index];
            if (!expression) {
                encodable = false;
                break;
            }
            if (expr_eval_integer_constant(expression,
                                           &nested_values[index])) {
                continue;
            }
            if (expression->kind == EXPR_IDENT &&
                expression->ident_name) {
                for (int function_index = 0;
                     function_index < tmpl->param_count; ++function_index) {
                    TemplateParam* function_parameter =
                        &tmpl->params[function_index];
                    if (function_parameter->kind == TPARAM_NONTYPE &&
                        !function_parameter->is_pack &&
                        function_parameter->name &&
                        strcmp(function_parameter->name,
                               expression->ident_name) == 0) {
                        nested_value_parameter_indices[index] =
                            function_index;
                        break;
                    }
                }
            }
            if (nested_value_parameter_indices[index] < 0 &&
                cxx_member_template_expression_supported(expression, tmpl)) {
                nested_value_expressions[index] = expression;
            }
            if (nested_value_parameter_indices[index] < 0 &&
                !nested_value_expressions[index]) {
                encodable = false;
            }
        }
        if (encodable) {
            CXX_MANGLE_TEMPLATE_APPEND('T');
            if (parameter_index > 0) {
                int written = snprintf(buf + *pos,
                                       CXX_MANGLE_BUFFER_SIZE - *pos,
                                       "%d", parameter_index - 1);
                if (written < 0 ||
                    (size_t)written >= CXX_MANGLE_BUFFER_SIZE - *pos) {
                    rcc_fatal(
                        "C++ dependent template-template parameter is too long");
                    return;
                }
                *pos += (size_t)written;
            }
            CXX_MANGLE_TEMPLATE_APPEND('_');
            CXX_MANGLE_TEMPLATE_APPEND('I');
            for (int index = 0; index < type->cxx_template_arg_count;
                 ++index) {
                TemplateParam* nested_parameter = &signature->params[index];
                if (nested_parameter->kind == TPARAM_TYPE) {
                    cxx_mangle_template_type_append(
                        buf, pos, type->cxx_template_args[index], tmpl,
                        context);
                    continue;
                }
                if (nested_value_parameter_indices[index] >= 0) {
                    int value_parameter_index =
                        nested_value_parameter_indices[index];
                    CXX_MANGLE_TEMPLATE_APPEND('X');
                    CXX_MANGLE_TEMPLATE_APPEND('T');
                    if (value_parameter_index > 0) {
                        int written = snprintf(
                            buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                            "%d", value_parameter_index - 1);
                        if (written < 0 || (size_t)written >=
                                CXX_MANGLE_BUFFER_SIZE - *pos) {
                            rcc_fatal(
                                "C++ dependent template value parameter is too long");
                            return;
                        }
                        *pos += (size_t)written;
                    }
                    CXX_MANGLE_TEMPLATE_APPEND('_');
                    CXX_MANGLE_TEMPLATE_APPEND('E');
                } else if (nested_value_expressions[index]) {
                    CXX_MANGLE_TEMPLATE_APPEND('X');
                    cxx_member_template_expression_append(
                        buf, pos, nested_value_expressions[index], tmpl,
                        context);
                    CXX_MANGLE_TEMPLATE_APPEND('E');
                } else {
                    int64_t value = nested_values[index];
                    CXX_MANGLE_TEMPLATE_APPEND('L');
                    cxx_mangle_template_type_append(
                        buf, pos, nested_parameter->type, tmpl, context);
                    int written;
                    if (value < 0) {
                        uint64_t magnitude =
                            (uint64_t)(-(value + 1)) + 1u;
                        written = snprintf(
                            buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                            "n%lluE", (unsigned long long)magnitude);
                    } else {
                        written = snprintf(
                            buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                            "%lldE", (long long)value);
                    }
                    if (written < 0 || (size_t)written >=
                            CXX_MANGLE_BUFFER_SIZE - *pos) {
                        rcc_fatal(
                            "C++ dependent template value argument is too long");
                        return;
                    }
                    *pos += (size_t)written;
                }
            }
            CXX_MANGLE_TEMPLATE_APPEND('E');
            return;
        }
    }
    int64_t nested_value_arguments[32] = { 0 };
    int nested_value_parameter_indices[32];
    Expr* nested_value_expressions[32] = { NULL };
    for (int index = 0;
         index < (int)(sizeof(nested_value_parameter_indices) /
                       sizeof(nested_value_parameter_indices[0]));
         ++index) {
        nested_value_parameter_indices[index] = -1;
    }
    bool nested_value_arguments_encodable = type && type->cxx_template &&
        type->cxx_template_arg_count > 0 &&
        type->cxx_template_arg_count <=
            (int)(sizeof(nested_value_arguments) /
                  sizeof(nested_value_arguments[0]));
    if (nested_value_arguments_encodable) {
        for (int index = 0; index < type->cxx_template_arg_count; ++index) {
            if (index < type->cxx_template->param_count &&
                type->cxx_template->params[index].kind == TPARAM_NONTYPE) {
                Expr* value_expression = type->cxx_template_value_args
                    ? type->cxx_template_value_args[index] : NULL;
                if (!value_expression) {
                    nested_value_arguments_encodable = false;
                    break;
                }
                if (expr_eval_integer_constant(
                        value_expression, &nested_value_arguments[index])) {
                    continue;
                }
                if (value_expression->kind == EXPR_IDENT &&
                    value_expression->ident_name && tmpl) {
                    for (int parameter_index = 0;
                         parameter_index < tmpl->param_count;
                         ++parameter_index) {
                        TemplateParam* function_parameter =
                            &tmpl->params[parameter_index];
                        if (function_parameter->kind == TPARAM_NONTYPE &&
                            !function_parameter->is_pack &&
                            function_parameter->name &&
                            strcmp(function_parameter->name,
                                   value_expression->ident_name) == 0) {
                            nested_value_parameter_indices[index] =
                                parameter_index;
                            break;
                        }
                    }
                }
                if (nested_value_parameter_indices[index] < 0 &&
                    cxx_member_template_expression_supported(
                        value_expression, tmpl)) {
                    nested_value_expressions[index] = value_expression;
                }
                if (nested_value_parameter_indices[index] < 0) {
                    if (!nested_value_expressions[index]) {
                        nested_value_arguments_encodable = false;
                        break;
                    }
                }
            }
        }
    }
    if (type->kind == TYPE_STRUCT && type->cxx_template &&
        type->cxx_template_arg_count > 0 && type->cxx_template_args &&
        nested_value_arguments_encodable) {
        CxxNamespace* namespace_stack[32];
        int namespace_count = 0;
        for (CxxNamespace* current = type->cxx_template->ns;
             current && current->name; current = current->parent) {
            if (namespace_count >=
                (int)(sizeof(namespace_stack) / sizeof(namespace_stack[0]))) {
                rcc_fatal("C++ member template namespace is too deep to mangle");
            }
            namespace_stack[namespace_count++] = current;
        }
        if (namespace_count > 0) CXX_MANGLE_TEMPLATE_APPEND('N');
        for (int index = namespace_count - 1; index >= 0; --index) {
            mangle_name(buf, pos, namespace_stack[index]->name);
        }
        mangle_name(buf, pos, type->cxx_template->name);
        CXX_MANGLE_TEMPLATE_APPEND('I');
        /* The template-name prefix enters the Itanium substitution table
         * before its arguments; later repeated type parameters therefore use
         * the same indices as GCC/Clang. */
        if (context) {
            context->next_substitution += (size_t)namespace_count + 1u;
        }
        for (int index = 0; index < type->cxx_template_arg_count; ++index) {
            TemplateParam* parameter = index < type->cxx_template->param_count
                ? &type->cxx_template->params[index] : NULL;
            if (parameter && parameter->kind == TPARAM_NONTYPE) {
                int function_parameter_index =
                    nested_value_parameter_indices[index];
                if (function_parameter_index >= 0) {
                    CXX_MANGLE_TEMPLATE_APPEND('X');
                    CXX_MANGLE_TEMPLATE_APPEND('T');
                    if (function_parameter_index > 0) {
                        int written = snprintf(
                            buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                            "%d", function_parameter_index - 1);
                        if (written < 0 || (size_t)written >=
                                CXX_MANGLE_BUFFER_SIZE - *pos) {
                            rcc_fatal(
                                "C++ member template parameter index is too long");
                        }
                        *pos += (size_t)written;
                    }
                    CXX_MANGLE_TEMPLATE_APPEND('_');
                    CXX_MANGLE_TEMPLATE_APPEND('E');
                } else if (nested_value_expressions[index]) {
                    CXX_MANGLE_TEMPLATE_APPEND('X');
                    cxx_member_template_expression_append(
                        buf, pos, nested_value_expressions[index], tmpl,
                        context);
                    CXX_MANGLE_TEMPLATE_APPEND('E');
                } else {
                    int64_t value = nested_value_arguments[index];
                    CXX_MANGLE_TEMPLATE_APPEND('L');
                    cxx_mangle_template_type_append(
                        buf, pos, parameter->type, tmpl, context);
                    int written;
                    if (value < 0) {
                        uint64_t magnitude = (uint64_t)(-(value + 1)) + 1u;
                        written = snprintf(
                            buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                            "n%lluE", (unsigned long long)magnitude);
                    } else {
                        written = snprintf(
                            buf + *pos, CXX_MANGLE_BUFFER_SIZE - *pos,
                            "%lldE", (long long)value);
                    }
                    if (written < 0 || (size_t)written >=
                            CXX_MANGLE_BUFFER_SIZE - *pos) {
                        rcc_fatal("C++ member template argument is too long");
                    }
                    *pos += (size_t)written;
                }
            } else {
                cxx_mangle_template_type_append(
                    buf, pos, type->cxx_template_args[index], tmpl, context);
            }
        }
        CXX_MANGLE_TEMPLATE_APPEND('E');
        if (namespace_count > 0) CXX_MANGLE_TEMPLATE_APPEND('E');
        return;
    }
    {
        char* mangled = cxx_mangle_type(type);
        size_t length = strlen(mangled);
        if (*pos + length >= CXX_MANGLE_BUFFER_SIZE) {
            rcc_fatal("C++ member template signature is too long");
        }
        memcpy(buf + *pos, mangled, length);
        *pos += length;
    }
#undef CXX_MANGLE_TEMPLATE_APPEND
}

static void cxx_mangle_template_type_append(
    char* buf, size_t* pos, Type* type, CxxTemplate* tmpl,
    CxxMemberTemplateMangleContext* context) {
    bool is_substitutable_type = type &&
        (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY ||
         type->kind == TYPE_STRUCT || type->kind == TYPE_UNION ||
         type->kind == TYPE_ENUM ||
         type->kind == TYPE_FUNC) &&
        cxx_function_template_type_parameter_index(tmpl, type) < 0;

    if (context && is_substitutable_type) {
        for (size_t index = 0; index < context->type_substitution_count;
             ++index) {
            if (type_is_compatible(context->type_substitutions[index], type)) {
                cxx_mangle_member_template_substitution(
                    buf, pos, context->type_substitution_indices[index]);
                return;
            }
        }
    }
    cxx_mangle_template_type_append_impl(buf, pos, type, tmpl, context);
    if (context && is_substitutable_type) {
        size_t count = context->type_substitution_count;
        context->type_substitutions = ast_arena_grow(
            context->type_substitutions,
            sizeof(context->type_substitutions[0]) * count,
            sizeof(context->type_substitutions[0]) * (count + 1u));
        context->type_substitution_indices = ast_arena_grow(
            context->type_substitution_indices,
            sizeof(context->type_substitution_indices[0]) * count,
            sizeof(context->type_substitution_indices[0]) * (count + 1u));
        context->type_substitutions[count] = type;
        context->type_substitution_indices[count] =
            context->next_substitution++;
        ++context->type_substitution_count;
    }
}

static char* cxx_mangle_function_template(Decl* func, CxxNamespace* ns,
                                           CxxClass* cls, CxxTemplate* tmpl,
                                           Type** type_args,
                                           const int64_t* value_args,
                                           const bool* value_present) {
    static char buf[CXX_MANGLE_BUFFER_SIZE];
    char* base;
    size_t pos;
    CxxNamespace* mangling_namespace;
    bool has_value_argument = false;
    bool has_type_argument = false;
    bool encode_member_signature;
    CxxMemberTemplateMangleContext member_mangle_context = { 0 };
    CxxMangleContext template_argument_context = { 0 };

    if (!func || !tmpl) return cxx_mangle_function(func, ns, cls);
    template_argument_context.class_type_index_base = 1u;
    mangling_namespace = ns == g_global_namespace ? NULL : ns;
    encode_member_signature = tmpl->member_method &&
        (!func->name || strcmp(func->name, "operator conversion") != 0);
    if ((encode_member_signature || !tmpl->member_method) &&
        tmpl->param_count > 0) {
        member_mangle_context.parameter_substitutions = ast_arena_alloc(
            sizeof(*member_mangle_context.parameter_substitutions) *
            (size_t)tmpl->param_count);
        for (int index = 0; index < tmpl->param_count; ++index) {
            member_mangle_context.parameter_substitutions[index] = -1;
        }
        member_mangle_context.next_substitution = encode_member_signature
            ? cxx_mangle_member_template_substitution_base(
                  mangling_namespace, cls)
            : cxx_mangle_free_template_substitution_base(
                  mangling_namespace);
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_NONTYPE && value_present &&
            value_present[index]) {
            has_value_argument = true;
        } else if (tmpl->params[index].kind == TPARAM_TYPE && type_args &&
                   tmpl->params[index].is_pack &&
                   tmpl->pending_pack_count >= 0) {
            has_type_argument = true;
        } else if (tmpl->params[index].kind == TPARAM_NONTYPE &&
                   tmpl->params[index].is_pack &&
                   tmpl->pending_pack_count >= 0) {
            has_value_argument = true;
        } else if ((tmpl->params[index].kind == TPARAM_TYPE ||
                    tmpl->params[index].kind == TPARAM_TEMPLATE) &&
                   type_args && type_args[index]) {
            has_type_argument = true;
        }
    }
    if (!has_value_argument && !has_type_argument) {
        return cxx_mangle_function(func, ns, cls);
    }

    if (tmpl->member_method && func->name &&
        strcmp(func->name, "operator conversion") == 0) {
        size_t base_length;
        base = cxx_mangle_function(func, mangling_namespace, cls);
        base_length = strlen(base);
        if (!func->func_params && base_length > 0u &&
            base[base_length - 1u] == 'v') {
            --base_length;
        }
        if (base_length >= sizeof(buf) - 32u) {
            rcc_fatal("C++ template conversion function name is too long");
        }
        memcpy(buf, base, base_length);
        pos = base_length;
    } else if (tmpl->member_method) {
        base = cxx_mangle_member_template_function_name(
            func, mangling_namespace, cls);
        strcpy(buf, base);
        pos = strlen(buf);
    } else {
        base = cxx_mangle_name(func->name, mangling_namespace, cls);
        strcpy(buf, base);
        pos = strlen(buf);
        if (mangling_namespace && pos > 0u && buf[pos - 1u] == 'E') {
            /* Template arguments belong to the final unqualified name inside
             * a nested-name-specifier, before that name's closing `E`. */
            --pos;
        }
    }
    buf[pos++] = 'I';
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind == TPARAM_TEMPLATE) {
            if (!type_args || !type_args[index] ||
                !type_args[index]->cxx_template) {
                rcc_fatal(
                    "C++ function template-template argument is missing");
            }
            mangle_template_argument_name(buf, &pos, type_args[index]);
            continue;
        }
        if (parameter->kind == TPARAM_TYPE) {
            if (parameter->is_pack && tmpl->pending_pack_count >= 0) {
                if (pos + 2u >= sizeof(buf)) {
                    rcc_fatal("C++ template function name is too long");
                }
                buf[pos++] = 'J';
                for (int pack_index = 0;
                     pack_index < tmpl->pending_pack_count; ++pack_index) {
                    size_t old_pos = pos;
                    cxx_mangle_type_append_context(
                        buf, &pos, tmpl->pending_pack_args[pack_index],
                        &template_argument_context);
                    if (old_pos == pos || pos >= sizeof(buf) - 32u) {
                        rcc_fatal("C++ template function name is too long");
                    }
                }
                buf[pos++] = 'E';
                continue;
            }
            if (!type_args || !type_args[index]) {
                continue;
            }
            cxx_mangle_type_append_context(
                buf, &pos, type_args[index], &template_argument_context);
            if (pos >= sizeof(buf) - 32u) {
                rcc_fatal("C++ template function name is too long");
            }
            continue;
        }
        if (parameter->is_pack && tmpl->pending_pack_count >= 0) {
            if (pos + 2u + 32u *
                    (size_t)tmpl->pending_pack_count >= sizeof(buf) - 32u) {
                rcc_fatal("C++ template function name is too long");
            }
            buf[pos++] = 'J';
            for (int pack_index = 0;
                 pack_index < tmpl->pending_pack_count; ++pack_index) {
                buf[pos++] = 'L';
                cxx_mangle_type_append_context(
                    buf, &pos,
                    cxx_mangle_template_value_type(
                        tmpl, parameter, type_args, tmpl->param_count,
                        index),
                    &template_argument_context);
                if (!tmpl->pending_pack_value_present ||
                    !tmpl->pending_pack_value_present[pack_index]) {
                    rcc_fatal("C++ non-type parameter pack value is missing");
                }
                if (tmpl->pending_pack_values[pack_index] < 0) {
                    uint64_t magnitude = (uint64_t)(
                        -(tmpl->pending_pack_values[pack_index] + 1)) + 1u;
                    pos += (size_t)snprintf(
                        buf + pos, sizeof(buf) - pos, "n%lluE",
                        (unsigned long long)magnitude);
                } else {
                    pos += (size_t)snprintf(
                        buf + pos, sizeof(buf) - pos, "%lldE",
                        (long long)tmpl->pending_pack_values[pack_index]);
                }
            }
            buf[pos++] = 'E';
            continue;
        }
        if (parameter->kind == TPARAM_NONTYPE && value_present &&
            value_present[index]) {
            buf[pos++] = 'L';
            cxx_mangle_type_append_context(
                buf, &pos,
                cxx_mangle_template_value_type(
                    tmpl, parameter, type_args, tmpl->param_count, index),
                &template_argument_context);
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
    }
    buf[pos++] = 'E';
    if (encode_member_signature ||
        (!tmpl->member_method && mangling_namespace)) {
        if (pos + 1u >= sizeof(buf)) {
            rcc_fatal("C++ member template name is too long");
        }
        buf[pos++] = 'E';
    }
    if ((encode_member_signature || !tmpl->member_method) &&
        tmpl->func_def && tmpl->func_def->type) {
        cxx_mangle_template_type_append(
            buf, &pos, tmpl->func_def->type->ret_type, tmpl,
            &member_mangle_context);
    }
    {
        Decl* signature = (encode_member_signature || !tmpl->member_method) &&
                tmpl->func_def
            ? tmpl->func_def : func;
        DeclList* parameters = signature->func_params
            ? signature->func_params : func->func_params;
        if (parameters) {
            for (DeclList* parameter = parameters; parameter;
             parameter = parameter->next) {
                if (encode_member_signature || !tmpl->member_method) {
                    if (parameter->decl &&
                        parameter->decl->param_is_pack) {
                        if (pos + 2u >= sizeof(buf)) {
                            rcc_fatal(
                                "C++ member template signature is too long");
                        }
                        buf[pos++] = 'D';
                        buf[pos++] = 'p';
                    }
                    cxx_mangle_template_type_append(
                        buf, &pos, parameter->decl->type, tmpl,
                        &member_mangle_context);
                } else {
                    char* type_mangled =
                        cxx_mangle_type(parameter->decl->type);
                    size_t type_length = strlen(type_mangled);
                    if (pos + type_length >= sizeof(buf)) {
                        rcc_fatal("C++ template function signature is too long");
                    }
                    memcpy(buf + pos, type_mangled, type_length);
                    pos += type_length;
                }
            }
        } else {
            buf[pos++] = 'v';
        }
        if (signature->type && signature->type->variadic) {
            buf[pos++] = 'z';
        }
    }
    buf[pos] = '\0';
    return buf;
}

/* C++ variable-template specializations use the same template-argument
 * encoding as function templates, but have no function parameter suffix.
 * Keep the source name and every explicit argument in the symbol so two
 * specializations cannot alias one another in the bounded global ABI. */
static char* cxx_mangle_variable_template(Decl* variable, CxxNamespace* ns,
                                           CxxTemplate* tmpl, Type** type_args,
                                           const int64_t* value_args,
                                           const bool* value_present) {
    static char buf[CXX_MANGLE_BUFFER_SIZE];
    char* base;
    size_t pos;

    if (!variable || !tmpl) return NULL;
    base = cxx_mangle_name(variable->name, ns, NULL);
    if (!base || strlen(base) + 2u >= sizeof(buf)) {
        rcc_fatal("C++ variable template name is too long");
    }
    strcpy(buf, base);
    pos = strlen(buf);
    buf[pos++] = 'I';
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->is_pack) {
            rcc_fatal("C++ variable template parameter packs are unsupported");
        }
        if (parameter->kind == TPARAM_TYPE) {
            if (!type_args || !type_args[index]) {
                rcc_fatal("C++ variable template type argument is missing");
            }
            char* type_mangled = cxx_mangle_type(type_args[index]);
            size_t type_length = strlen(type_mangled);
            if (pos + type_length >= sizeof(buf) - 2u) {
                rcc_fatal("C++ variable template name is too long");
            }
            memcpy(buf + pos, type_mangled, type_length);
            pos += type_length;
        } else if (parameter->kind == TPARAM_NONTYPE) {
            char* type_mangled;
            size_t type_length;
            if (!value_args || !value_present || !value_present[index]) {
                rcc_fatal("C++ variable template value argument is missing");
            }
            type_mangled = cxx_mangle_type(parameter->type);
            type_length = strlen(type_mangled);
            if (pos + type_length + 32u >= sizeof(buf)) {
                rcc_fatal("C++ variable template name is too long");
            }
            buf[pos++] = 'L';
            memcpy(buf + pos, type_mangled, type_length);
            pos += type_length;
            if (value_args[index] < 0) {
                uint64_t magnitude = (uint64_t)(-(value_args[index] + 1)) + 1u;
                pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos,
                                        "n%lluE",
                                        (unsigned long long)magnitude);
            } else {
                pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos,
                                        "%lldE", (long long)value_args[index]);
            }
        } else {
            rcc_fatal("unsupported C++ variable template parameter");
        }
    }
    if (pos + 2u >= sizeof(buf)) {
        rcc_fatal("C++ variable template name is too long");
    }
    buf[pos++] = 'E';
    buf[pos] = '\0';
    return buf;
}

/* ═══════════════════════════════════════
 * Class Operations (Core API)
 * ═══════════════════════════════════════ */

static bool cxx_method_parameter_lists_match(const CxxMethod* left,
                                             const CxxMethod* right) {
    DeclList* left_param;
    DeclList* right_param;
    if (!left || !right || !left->decl || !right->decl ||
        !left->decl->type || !right->decl->type) {
        return false;
    }
    /* The ordinary-method registration pass prepends the ABI-only `this`
     * parameter to Type::params.  Compare the source parameter lists so a
     * completed base class and a not-yet-registered derived class use the
     * same signature domain. */
    left_param = left->decl->func_params;
    right_param = right->decl->func_params;
    while (left_param && right_param) {
        if (!left_param->decl || !right_param->decl ||
            !type_is_compatible(left_param->decl->type,
                                right_param->decl->type)) {
            return false;
        }
        left_param = left_param->next;
        right_param = right_param->next;
    }
    return left_param == NULL && right_param == NULL;
}

bool cxx_method_virtual_signature_matches(const CxxMethod* derived,
                                          const CxxMethod* base) {
    const char* derived_name;
    const char* base_name;
    if (!derived || !base || derived->is_static || base->is_static ||
        !derived->decl || !base->decl) {
        return false;
    }
    derived_name = derived->source_name
        ? derived->source_name : derived->decl->name;
    base_name = base->source_name
        ? base->source_name : base->decl->name;
    return derived_name && base_name && strcmp(derived_name, base_name) == 0 &&
           derived->is_const == base->is_const &&
           derived->is_volatile == base->is_volatile &&
           derived->ref_qualifier == base->ref_qualifier &&
           cxx_method_parameter_lists_match(derived, base);
}

bool cxx_method_override_signature_matches(const CxxMethod* derived,
                                           const CxxMethod* base) {
    return cxx_method_virtual_signature_matches(derived, base) &&
           derived->decl->type && base->decl->type &&
           type_is_compatible(derived->decl->type->ret_type,
                              base->decl->type->ret_type);
}

CxxClass* cxx_class_alloc(const char* name, bool is_struct) {
    CxxClass* cls = rcc_alloc(sizeof(CxxClass));
    cls->name = name ? rcc_strdup(name) : NULL;
    cls->is_struct = is_struct;
    cls->is_final = false;
    cls->is_abstract = false;
    cls->has_user_constructor = false;
    cls->has_nonpublic_field = false;
    cls->has_static_field = false;
    cls->has_field_initializer = false;
    cls->constructors = NULL;
    cls->bases = NULL;
    cls->base_count = 0;
    cls->using_base_members = NULL;
    cls->using_base_member_count = 0;
    cls->base_offsets = NULL;
    cls->virtual_bases = NULL;
    cls->virtual_base_count = 0;
    cls->nonvirtual_size = 1;
    cls->virtual_base_pointer_offset = -1;
    cls->virtual_base_table_symbol = NULL;
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
    cls->explicit_alignment = 0;
    cls->pack_alignment = 0;
    cls->fields = NULL;
    cls->type_aliases = NULL;
    cls->alias_templates = NULL;
    cls->ns = NULL;
    cls->templ = NULL;
    cls->template_args = NULL;
    cls->template_arg_count = 0;
    cls->template_value_args = NULL;
    cls->template_value_present = NULL;
    cls->template_pack_count = 0;
    cls->template_pack_values = NULL;
    cls->template_pack_value_present = NULL;
    cls->template_identity_tmpl = NULL;
    cls->template_identity_args = NULL;
    cls->template_identity_arg_count = 0;
    cls->template_identity_value_args = NULL;
    cls->template_identity_value_present = NULL;
    return cls;
}

void cxx_class_add_base_ptr(CxxClass* cls, CxxClass* base, AccessSpec access, bool is_virtual) {
    cls->bases = ast_arena_grow(
        cls->bases, sizeof(cls->bases[0]) * (size_t)cls->base_count,
        sizeof(cls->bases[0]) * (size_t)(cls->base_count + 1));
    cls->bases[cls->base_count].base = base;
    cls->bases[cls->base_count].base_name = NULL;
    cls->bases[cls->base_count].type_pattern = NULL;
    cls->bases[cls->base_count].access = access;
    cls->bases[cls->base_count].is_virtual = is_virtual;
    cls->bases[cls->base_count].is_pack_expansion = false;
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

static int cxx_class_member_alignment(const CxxClass* cls, int alignment) {
    if (alignment <= 0) return alignment;
    if (cls->pack_alignment > 0 && alignment > cls->pack_alignment) {
        return cls->pack_alignment;
    }
    return alignment;
}

/* An empty non-polymorphic base may share the address of the complete object
 * and of its first data member.  Keep the initial implementation deliberately
 * narrow: bases with their own bases, virtual-base state, or a visible field
 * stay on the ordinary one-byte subobject path until their complete ABI rules
 * are modeled. */
static bool cxx_can_use_empty_base_optimization(const CxxClass* base) {
    return base && base->base_count == 0 && base->vtable_size == 0 &&
           base->virtual_base_count == 0 &&
           base->virtual_base_pointer_offset < 0 && base->size == 1 &&
           base->type && base->type->fields == NULL;
}

/* A base subobject and a non-static data member of the same type cannot share
 * an address.  Check both class-field representations used by the parser so
 * EBO never changes this observable layout rule. */
static bool cxx_has_same_type_member(const CxxClass* cls,
                                     const CxxClass* base) {
    if (!cls || !base) return false;
    for (TypeParam* field = cls->fields; field; field = field->next) {
        if (!field->is_static && field->type &&
            field->type->cxx_class == base) {
            return true;
        }
    }
    for (const struct CxxMember* member = cls->members;
         member; member = member->next) {
        if (!member->is_static && member->decl &&
            member->decl->kind == DECL_VAR && member->decl->type &&
            member->decl->type->cxx_class == base) {
            return true;
        }
    }
    return false;
}

/* Keep the first no_unique_address implementation deliberately narrow.  An
 * empty, trivial class has no storage that construction or destruction could
 * observe, so its data member can share the current offset. */
static bool cxx_can_use_no_unique_address_type(const Type* type) {
    const CxxClass* cls = type ? type->cxx_class : NULL;
    return type && cls && type->size == 1 && cls->base_count == 0 &&
           cls->vtable_size == 0 && cls->virtual_base_count == 0 &&
           cls->virtual_base_pointer_offset < 0 &&
           type->fields == NULL && !cls->has_user_constructor &&
           !cls->has_field_initializer && !cls->destructor_method;
}

static bool cxx_has_same_type_subobject(const CxxClass* cls,
                                        const Type* type,
                                        const TypeParam* excluded) {
    const CxxClass* target = type ? type->cxx_class : NULL;
    if (!cls || !target) return false;
    for (const TypeParam* field = cls->fields; field; field = field->next) {
        if (field != excluded && !field->is_static && field->type &&
            field->type->cxx_class == target) {
            return true;
        }
    }
    for (int i = 0; i < cls->base_count; ++i) {
        if (cls->bases[i].base == target) return true;
    }
    return false;
}

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
        max_align = cxx_class_member_alignment(cls, pointer_size);
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
            if (!has_virtual && offset == 0 &&
                cxx_can_use_empty_base_optimization(base) &&
                !cxx_has_same_type_member(cls, base)) {
                base_offsets[i] = 0;
                continue;
            }
            /* Align for base */
            int align = cxx_class_member_alignment(cls, base->align);
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
     * including virtual bases reached through a non-virtual intermediate.
     * C++ constructs virtual bases in depth-first, left-to-right order: a
     * base's virtual closure is visited before that base itself.  Keeping the
     * same order in the layout table also makes constructor side effects and
     * target offsets agree for intermediate virtual bases. */
    cls->virtual_bases = NULL;
    cls->virtual_base_count = 0;
    for (int i = 0; i < cls->base_count; ++i) {
        CxxClass* base = cls->bases[i].base;
        if (!base) continue;
        for (int nested = 0; nested < base->virtual_base_count; ++nested) {
            cxx_add_virtual_base(
                cls, base->virtual_bases[nested].base,
                cls->bases[i].access == ACCESS_PUBLIC &&
                base->virtual_bases[nested].public_path);
        }
        if (cls->bases[i].is_virtual) {
            cxx_add_virtual_base(
                cls, base, cls->bases[i].access == ACCESS_PUBLIC);
        }
    }

    /* Fields from fields list */
    for (TypeParam* f = cls->fields; f; f = f->next) {
        Type* type = f->type;
        int align;
        int size;
        TypeField* field;
        bool use_no_unique_address;

        /* Static data members have storage independent of every object and
         * are materialized as translation-unit declarations. */
        if (f->is_static) continue;

        if (!type || !type_is_complete(type) || type->kind == TYPE_FUNC ||
            type->kind == TYPE_VOID) {
            layout_complete = false;
            continue;
        }
        if (type->cxx_nontrivial) nontrivial = true;
        align = cxx_class_member_alignment(cls, type->align);
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
                field->virtual_base_owner = NULL;
                field->virtual_base_member_offset = -1;
                field->initializer = f->initializer;
                field->is_deprecated = f->is_deprecated;
                field->deprecated_message = f->deprecated_message;
                field->cxx_no_unique_address = false;
                field->cxx_access = f->cxx_access;
                field->cxx_declaring_class = cls;
                field->next = NULL;
                *field_tail = field;
                field_tail = &field->next;
            }
            bitfield_used += f->bit_width;
            if (align > max_align) max_align = align;
            continue;
        }
        bitfield_active = false;
        use_no_unique_address =
            f->cxx_no_unique_address && !f->initializer &&
            cxx_can_use_no_unique_address_type(type) &&
            !cxx_has_same_type_subobject(cls, type, f);

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
        field->virtual_base_owner = NULL;
        field->virtual_base_member_offset = -1;
        field->initializer = f->initializer;
        field->is_deprecated = f->is_deprecated;
        field->deprecated_message = f->deprecated_message;
        field->cxx_no_unique_address = use_no_unique_address;
        field->cxx_access = f->cxx_access;
        field->cxx_declaring_class = cls;
        field->next = NULL;
        *field_tail = field;
        field_tail = &field->next;
        if (!use_no_unique_address) offset += size;

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
            field->virtual_base_owner = NULL;
            field->virtual_base_member_offset = -1;
            field->initializer = base_field->initializer;
            field->is_deprecated = base_field->is_deprecated;
            field->deprecated_message = base_field->deprecated_message;
            field->cxx_no_unique_address = base_field->cxx_no_unique_address;
            field->cxx_access = access;
            field->cxx_declaring_class =
                base_field->cxx_declaring_class;
            field->next = NULL;
            *field_tail = field;
            field_tail = &field->next;
        }
    }

    /* Non-static data members */
    for (struct CxxMember* m = cls->members; m; m = m->next) {
        if (!m->is_static && m->decl->kind == DECL_VAR) {
            Type* type = m->decl->type;
            int align = cxx_class_member_alignment(cls, type->align);
            int size = type->size;

            /* Align */
            offset = (offset + align - 1) & ~(align - 1);
            m->decl->var_offset = offset;
            offset += size;

            if (align > max_align) max_align = align;
        }
    }

    /* Every class with a virtual-base closure carries a hidden pointer to a
     * table of offsets for the current most-derived object.  Keep it in the
     * non-virtual portion so a polymorphic class can retain its primary vptr
     * at offset zero.  A derived object initializes one table pointer for
     * itself and one for each virtual-base-bearing subobject. */
    cls->virtual_base_pointer_offset = -1;
    if (cls->virtual_base_count > 0) {
        int pointer_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
        int pointer_align = cxx_class_member_alignment(cls, pointer_size);
        offset = (offset + pointer_align - 1) & ~(pointer_align - 1);
        cls->virtual_base_pointer_offset = offset;
        offset += pointer_size;
        if (pointer_align > max_align) max_align = pointer_align;
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
        int base_align = cxx_class_member_alignment(cls, base->align);
        offset = (offset + base_align - 1) & ~(base_align - 1);
        cls->virtual_bases[virtual_index].offset = offset;
        offset += base->nonvirtual_size;
        if (base_align > max_align) max_align = base_align;
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
            field->virtual_base_owner = base;
            field->virtual_base_member_offset = base_field->offset;
            field->initializer = base_field->initializer;
            field->is_deprecated = base_field->is_deprecated;
            field->deprecated_message = base_field->deprecated_message;
            field->cxx_no_unique_address = base_field->cxx_no_unique_address;
            field->cxx_access = access;
            field->cxx_declaring_class =
                base_field->cxx_declaring_class;
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

void cxx_class_apply_explicit_alignment(CxxClass* cls, int alignment,
                                        SourceLoc loc) {
    int mask;
    int aligned_size;
    int aligned_nonvirtual_size;

    if (!cls || !cls->type || alignment <= 0 || alignment <= cls->align) {
        return;
    }
    if (alignment > 4096 || (alignment & (alignment - 1)) != 0) {
        rcc_error(loc,
                  "C++ class alignment must be a power of two no greater than 4096");
        return;
    }
    mask = alignment - 1;
    if (cls->size > INT_MAX - mask ||
        cls->nonvirtual_size > INT_MAX - mask) {
        rcc_error(loc, "C++ class alignment overflows the supported object size");
        return;
    }
    aligned_size = (cls->size + mask) & ~mask;
    aligned_nonvirtual_size = (cls->nonvirtual_size + mask) & ~mask;
    if (aligned_size <= 0 || aligned_nonvirtual_size <= 0) {
        rcc_error(loc, "C++ class alignment produced an invalid object size");
        return;
    }
    cls->align = alignment;
    cls->explicit_alignment = alignment;
    cls->size = aligned_size;
    cls->nonvirtual_size = aligned_nonvirtual_size;
    cls->type->align = alignment;
    cls->type->size = aligned_size;
    cls->type->has_explicit_alignment = true;
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

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} CxxTypeInfoIdentityBuilder;

static void cxx_typeinfo_identity_append(
    CxxTypeInfoIdentityBuilder* builder, const char* data, size_t length) {
    size_t needed;
    size_t capacity;
    if (length > SIZE_MAX - builder->length - 1u) {
        rcc_fatal("C++ typeinfo identity is too large");
    }
    needed = builder->length + length + 1u;
    if (needed > builder->capacity) {
        capacity = builder->capacity ? builder->capacity : 128u;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2u) {
                capacity = needed;
                break;
            }
            capacity *= 2u;
        }
        builder->data = ast_arena_grow(builder->data, builder->capacity,
                                       capacity);
        builder->capacity = capacity;
    }
    if (length != 0u) {
        memcpy(builder->data + builder->length, data, length);
        builder->length += length;
    }
    builder->data[builder->length] = '\0';
}

static void cxx_typeinfo_identity_append_char(
    CxxTypeInfoIdentityBuilder* builder, char value) {
    cxx_typeinfo_identity_append(builder, &value, 1u);
}

static void cxx_typeinfo_identity_append_number(
    CxxTypeInfoIdentityBuilder* builder, uint64_t value) {
    char digits[32];
    int length = snprintf(digits, sizeof(digits), "%llu",
                          (unsigned long long)value);
    if (length < 0 || (size_t)length >= sizeof(digits)) {
        rcc_fatal("C++ typeinfo identity integer formatting failed");
    }
    cxx_typeinfo_identity_append(builder, digits, (size_t)length);
    cxx_typeinfo_identity_append_char(builder, '_');
}

static void cxx_typeinfo_identity_append_signed(
    CxxTypeInfoIdentityBuilder* builder, int64_t value) {
    uint64_t magnitude;
    if (value < 0) {
        cxx_typeinfo_identity_append_char(builder, 'n');
        magnitude = (uint64_t)(-(value + 1)) + UINT64_C(1);
    } else {
        cxx_typeinfo_identity_append_char(builder, 'p');
        magnitude = (uint64_t)value;
    }
    cxx_typeinfo_identity_append_number(builder, magnitude);
}

static void cxx_typeinfo_identity_append_text(
    CxxTypeInfoIdentityBuilder* builder, const char* text) {
    static const char hex[] = "0123456789abcdef";
    size_t length;
    if (!text) {
        cxx_typeinfo_identity_append_char(builder, 'N');
        return;
    }
    length = strlen(text);
    cxx_typeinfo_identity_append_char(builder, 'S');
    cxx_typeinfo_identity_append_number(builder, (uint64_t)length);
    for (size_t index = 0; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        char encoded[2] = {hex[byte >> 4], hex[byte & 0x0fu]};
        cxx_typeinfo_identity_append(builder, encoded, sizeof(encoded));
    }
}

static void cxx_typeinfo_identity_append_namespace(
    CxxTypeInfoIdentityBuilder* builder, CxxNamespace* ns) {
    CxxNamespace* stack[32];
    size_t count = 0u;
    bool has_anonymous = false;
    for (CxxNamespace* current = ns; current;
         current = current->parent) {
        if (!current->name && !current->is_anonymous_namespace) break;
        if (count == sizeof(stack) / sizeof(stack[0])) {
            rcc_fatal("C++ typeinfo namespace nesting is too deep");
        }
        if (current->is_anonymous_namespace) has_anonymous = true;
        stack[count++] = current;
    }
    cxx_typeinfo_identity_append_char(builder, 'N');
    cxx_typeinfo_identity_append_number(builder, (uint64_t)count);
    while (count != 0u) {
        CxxNamespace* component = stack[--count];
        if (has_anonymous) {
            cxx_typeinfo_identity_append_char(
                builder, component->is_anonymous_namespace ? 'A' : 'N');
        }
        cxx_typeinfo_identity_append_text(
            builder, component->is_anonymous_namespace
                ? component->anonymous_typeinfo_identity : component->name);
    }
}

static void cxx_typeinfo_identity_append_template(
    CxxTypeInfoIdentityBuilder* builder, CxxTemplate* tmpl,
    CxxNamespace* fallback_namespace) {
    if (!tmpl || !tmpl->name) {
        rcc_fatal("C++ typeinfo template identity is incomplete");
    }
    cxx_typeinfo_identity_append_char(builder, 'M');
    cxx_typeinfo_identity_append_namespace(
        builder, tmpl->ns ? tmpl->ns : fallback_namespace);
    cxx_typeinfo_identity_append_text(builder, tmpl->name);
}

static void cxx_typeinfo_identity_append_class(
    CxxTypeInfoIdentityBuilder* builder, CxxClass* cls);

static void cxx_typeinfo_identity_append_type(
    CxxTypeInfoIdentityBuilder* builder, const Type* type);

static void cxx_typeinfo_identity_append_class(
    CxxTypeInfoIdentityBuilder* builder, CxxClass* cls) {
    CxxTemplate* tmpl;
    Type** arguments;
    int argument_count;
    int64_t* value_arguments;
    bool* value_present;
    if (!cls || !cls->name) {
        rcc_fatal("C++ typeinfo class identity is incomplete");
    }
    tmpl = cls->template_identity_tmpl
        ? cls->template_identity_tmpl : cls->templ;
    arguments = cls->template_identity_tmpl
        ? cls->template_identity_args : cls->template_args;
    argument_count = cls->template_identity_tmpl
        ? cls->template_identity_arg_count : cls->template_arg_count;
    value_arguments = cls->template_identity_tmpl
        ? cls->template_identity_value_args : cls->template_value_args;
    value_present = cls->template_identity_tmpl
        ? cls->template_identity_value_present : cls->template_value_present;

    cxx_typeinfo_identity_append_char(builder, 'C');
    cxx_typeinfo_identity_append_namespace(builder, cls->ns);
    cxx_typeinfo_identity_append_text(
        builder, tmpl && tmpl->name ? tmpl->name : cls->name);
    if (cls->type && cls->type->cxx_scope_identity) {
        cxx_typeinfo_identity_append_char(builder, 'L');
        cxx_typeinfo_identity_append_text(
            builder, cls->type->cxx_scope_identity);
    }
    if (!tmpl) {
        cxx_typeinfo_identity_append_char(builder, 'N');
        return;
    }
    if (argument_count < 0 ||
        (argument_count != 0 && !arguments &&
         (!tmpl->params || tmpl->params[0].kind != TPARAM_NONTYPE))) {
        rcc_fatal("C++ typeinfo template arguments are incomplete");
    }
    cxx_typeinfo_identity_append_char(builder, 'I');
    cxx_typeinfo_identity_append_number(builder, (uint64_t)argument_count);
    for (int index = 0; index < argument_count; ++index) {
        int parameter_index = index < tmpl->param_count
            ? index : tmpl->param_count - 1;
        TemplateParam* parameter;
        if (parameter_index < 0 || !tmpl->params ||
            (index >= tmpl->param_count &&
             !tmpl->params[parameter_index].is_pack)) {
            rcc_fatal("C++ typeinfo template argument count is invalid");
        }
        parameter = &tmpl->params[parameter_index];
        switch (parameter->kind) {
            case TPARAM_TYPE:
                if (!arguments || !arguments[index]) {
                    rcc_fatal("C++ typeinfo type argument is missing");
                }
                cxx_typeinfo_identity_append_type(builder, arguments[index]);
                break;
            case TPARAM_NONTYPE:
                if (!value_present || !value_present[index] ||
                    !value_arguments) {
                    rcc_fatal("C++ typeinfo value argument is missing");
                }
                cxx_typeinfo_identity_append_char(builder, 'V');
                cxx_typeinfo_identity_append_signed(
                    builder, value_arguments[index]);
                break;
            case TPARAM_TEMPLATE:
                if (!arguments || !arguments[index] ||
                    !arguments[index]->cxx_template) {
                    rcc_fatal("C++ typeinfo template-template argument is missing");
                }
                cxx_typeinfo_identity_append_template(
                    builder, arguments[index]->cxx_template, cls->ns);
                break;
            default:
                rcc_fatal("unsupported C++ typeinfo template parameter");
        }
    }
}

static void cxx_typeinfo_identity_append_type(
    CxxTypeInfoIdentityBuilder* builder, const Type* type) {
    const TypeParam* parameter;
    uint64_t parameter_count = 0u;
    unsigned qualifiers = 0u;
    static const char hex[] = "0123456789abcdef";
    if (!type) {
        cxx_typeinfo_identity_append_char(builder, '0');
        return;
    }
    cxx_typeinfo_identity_append_char(builder, 'T');
    cxx_typeinfo_identity_append_number(builder, (uint64_t)type->kind);
    if (type->is_const) qualifiers |= 1u;
    if (type->is_volatile) qualifiers |= 2u;
    if (type->is_atomic) qualifiers |= 4u;
    if (type->is_restrict) qualifiers |= 8u;
    cxx_typeinfo_identity_append_char(builder, hex[qualifiers]);
    cxx_typeinfo_identity_append_char(
        builder, type->is_unsigned ? 'u' : 's');
    if (type->kind == TYPE_CHAR) {
        cxx_typeinfo_identity_append_char(
            builder, type->is_plain_char ? 'p' :
                     type->is_char8 ? '8' : 'c');
    } else if (type->is_char16) {
        cxx_typeinfo_identity_append_char(builder, '6');
    } else if (type->is_char32) {
        cxx_typeinfo_identity_append_char(builder, '3');
    } else if (type->is_wchar) {
        cxx_typeinfo_identity_append_char(builder, 'w');
    }
    switch (type->kind) {
        case TYPE_PTR:
            if (type->is_reference) {
                cxx_typeinfo_identity_append_char(
                    builder, type->is_rvalue_reference ? 'O' : 'R');
            } else if (type->cxx_is_member_pointer) {
                cxx_typeinfo_identity_append_char(builder, 'M');
                cxx_typeinfo_identity_append_type(
                    builder, type->cxx_member_pointer_owner);
            } else {
                cxx_typeinfo_identity_append_char(builder, 'P');
            }
            cxx_typeinfo_identity_append_type(builder, type->base);
            break;
        case TYPE_ARRAY:
            cxx_typeinfo_identity_append_char(builder, 'A');
            cxx_typeinfo_identity_append_signed(
                builder, (int64_t)type->array_len);
            cxx_typeinfo_identity_append_type(builder, type->base);
            break;
        case TYPE_VECTOR:
            cxx_typeinfo_identity_append_char(builder, 'V');
            cxx_typeinfo_identity_append_signed(
                builder, (int64_t)type->array_len);
            cxx_typeinfo_identity_append_type(builder, type->base);
            break;
        case TYPE_STRUCT:
        case TYPE_UNION:
            if (type->cxx_class) {
                cxx_typeinfo_identity_append_class(
                    builder, type->cxx_class);
            } else {
                cxx_typeinfo_identity_append_char(
                    builder, type->kind == TYPE_STRUCT ? 'S' : 'U');
                cxx_typeinfo_identity_append_text(
                    builder, type->cxx_namespace);
                cxx_typeinfo_identity_append_text(builder, type->tag);
            }
            break;
        case TYPE_ENUM:
            cxx_typeinfo_identity_append_char(builder, 'E');
            cxx_typeinfo_identity_append_text(
                builder, type->cxx_namespace);
            cxx_typeinfo_identity_append_text(builder, type->enum_tag);
            cxx_typeinfo_identity_append_char(
                builder, type->enum_is_scoped ? 'C' : 'U');
            break;
        case TYPE_FUNC:
            cxx_typeinfo_identity_append_char(builder, 'F');
            cxx_typeinfo_identity_append_char(
                builder, type->has_prototype ? 'P' : 'N');
            cxx_typeinfo_identity_append_char(
                builder, type->variadic ? 'V' : 'N');
            cxx_typeinfo_identity_append_type(builder, type->ret_type);
            for (parameter = type->params; parameter;
                 parameter = parameter->next) {
                ++parameter_count;
            }
            cxx_typeinfo_identity_append_number(
                builder, parameter_count);
            for (parameter = type->params; parameter;
                 parameter = parameter->next) {
                cxx_typeinfo_identity_append_type(builder, parameter->type);
            }
            break;
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_SHORT:
        case TYPE_INT:
        case TYPE_LONG:
        case TYPE_LLONG:
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
        case TYPE_NULLPTR:
        case TYPE_VOID:
            break;
    }
}

static const char* cxx_typeinfo_name(CxxClass* cls) {
    static const char external_prefix[] = "__rcc_typeinfo_class_";
    static const char local_prefix[] = "__rcc_typeinfo_local_type_";
    CxxTypeInfoIdentityBuilder builder = {0};
    const char* prefix;
    char buffer[1024];
    char* class_name;
    bool has_local_scope;
    if (!cls || !cls->name) return NULL;
    has_local_scope = cls->type && cls->type->cxx_scope_identity;
    bool has_anonymous_namespace = false;
    for (CxxNamespace* ns = cls->ns; ns; ns = ns->parent) {
        if (ns->is_anonymous_namespace) {
            has_anonymous_namespace = true;
            break;
        }
        if (!ns->name) break;
    }
    if (cls->templ || cls->template_identity_tmpl ||
        has_anonymous_namespace || has_local_scope) {
        prefix = has_anonymous_namespace || has_local_scope
            ? local_prefix : external_prefix;
        cxx_typeinfo_identity_append(&builder, prefix, strlen(prefix));
        cxx_typeinfo_identity_append_class(&builder, cls);
        return rcc_intern(builder.data);
    }
    class_name = cxx_mangle_name(cls->name, cls->ns, NULL);
    if (snprintf(buffer, sizeof(buffer), "__rcc_typeinfo_%s", class_name) < 0 ||
        strlen(buffer) >= sizeof(buffer) - 1u) {
        rcc_fatal("C++ typeinfo symbol is too long");
    }
    return rcc_intern(buffer);
}

static const char* cxx_virtual_base_table_name(CxxClass* cls) {
    char buffer[1024];
    char* class_name;
    if (!cls || !cls->name) return NULL;
    class_name = cxx_mangle_name(cls->name, cls->ns, NULL);
    if (snprintf(buffer, sizeof(buffer), "__rcc_vbtable_%s", class_name) < 0 ||
        strlen(buffer) >= sizeof(buffer) - 1u) {
        rcc_fatal("C++ virtual-base table symbol is too long");
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

static const char* cxx_virtual_secondary_vtable_name(CxxClass* cls,
                                                     int virtual_base_index) {
    char buffer[1024];
    char* class_name;
    if (!cls || !cls->name) return NULL;
    class_name = cxx_mangle_name(cls->name, cls->ns, NULL);
    if (snprintf(buffer, sizeof(buffer), "__rcc_vtable_%s_vbase%d",
                 class_name, virtual_base_index) < 0 ||
        strlen(buffer) >= sizeof(buffer) - 1u) {
        rcc_fatal("C++ virtual-base vtable symbol is too long");
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

static const char* cxx_virtual_secondary_thunk_name(
    CxxClass* cls, int virtual_base_index, int slot) {
    char buffer[1024];
    char* class_name;
    if (!cls || !cls->name) return NULL;
    class_name = cxx_mangle_name(cls->name, cls->ns, NULL);
    if (snprintf(buffer, sizeof(buffer),
                 "__rcc_thunk_%s_vbase%d_slot%d", class_name,
                 virtual_base_index, slot) < 0 ||
        strlen(buffer) >= sizeof(buffer) - 1u) {
        rcc_fatal("C++ virtual-base thunk symbol is too long");
    }
    return rcc_intern(buffer);
}

static int cxx_vtable_find_slot(const CxxVtableEntry* entries, int count,
                                const CxxMethod* method) {
    if (!entries || !method) return -1;
    for (int index = 0; index < count; ++index) {
        if (entries[index].method &&
            cxx_method_virtual_signature_matches(method,
                                                 entries[index].method)) {
            return index;
        }
    }
    return -1;
}

bool cxx_class_is_abstract(const CxxClass* cls) {
    return cls && cls->is_abstract;
}

void cxx_class_build_vtable(CxxClass* cls) {
    CxxClass* primary_base;
    int vtable_size;
    if (!cls || !cls->type) return;

    /* Type identities are useful even for non-polymorphic target classes in
     * a dynamic_cast<T*>(source) search.  Allocate the symbol while the
     * namespace and specialization identity are still attached to the class. */
    cls->type->cxx_typeinfo_symbol = cxx_typeinfo_name(cls);
    cls->virtual_base_table_symbol = cls->virtual_base_count > 0
        ? cxx_virtual_base_table_name(cls) : NULL;

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
        slot = cxx_vtable_find_slot(cls->vtable, vtable_size, method);
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
        secondary->is_virtual_base = cls->bases[base_index].is_virtual;
        secondary->virtual_base_index = secondary->is_virtual_base
            ? cxx_virtual_base_index(cls, base) : -1;
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
                                        method);
            if (slot < 0) continue;
            method->is_virtual = true;
            member->is_virtual = true;
            secondary->entries[slot].method = method;
            secondary->entries[slot].entry_symbol =
                cxx_secondary_thunk_name(cls, base_index, slot);
        }
    }

    /* A virtual base reached through a non-virtual intermediate base still
     * owns a vptr in the complete object.  Give it a most-derived table so
     * RTTI can recover the complete object and virtual calls can dispatch
     * through the same fixed layout. */
    for (int virtual_index = 0;
         virtual_index < cls->virtual_base_count; ++virtual_index) {
        CxxClass* base = cls->virtual_bases[virtual_index].base;
        CxxSecondaryVtable* secondary;
        bool already_present = false;
        if (!base || base->vtable_size <= 0 || !base->vtable) continue;
        for (int index = 0; index < cls->secondary_vtable_count; ++index) {
            CxxSecondaryVtable* existing = &cls->secondary_vtables[index];
            if (existing->is_virtual_base &&
                existing->virtual_base_index == virtual_index) {
                already_present = true;
                break;
            }
        }
        if (already_present) continue;
        cls->secondary_vtables = ast_arena_grow(
            cls->secondary_vtables,
            sizeof(*cls->secondary_vtables) *
                (size_t)cls->secondary_vtable_count,
            sizeof(*cls->secondary_vtables) *
                (size_t)(cls->secondary_vtable_count + 1));
        secondary = &cls->secondary_vtables[cls->secondary_vtable_count++];
        secondary->base = base;
        secondary->base_index = -1;
        secondary->is_virtual_base = true;
        secondary->virtual_base_index = virtual_index;
        secondary->symbol = cxx_virtual_secondary_vtable_name(
            cls, virtual_index);
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
                                        method);
            if (slot < 0) continue;
            method->is_virtual = true;
            member->is_virtual = true;
            secondary->entries[slot].method = method;
            secondary->entries[slot].entry_symbol =
                cxx_virtual_secondary_thunk_name(cls, virtual_index, slot);
        }
    }

    cls->is_abstract = false;
    for (int slot = 0; slot < vtable_size; ++slot) {
        if (cls->vtable[slot].method &&
            cls->vtable[slot].method->is_pure_virtual) {
            cls->is_abstract = true;
            break;
        }
    }
    if (!cls->is_abstract) {
        for (int table_index = 0;
             table_index < cls->secondary_vtable_count && !cls->is_abstract;
             ++table_index) {
            CxxSecondaryVtable* table = &cls->secondary_vtables[table_index];
            for (int slot = 0; slot < table->size; ++slot) {
                if (table->entries[slot].method &&
                    table->entries[slot].method->is_pure_virtual) {
                    cls->is_abstract = true;
                    break;
                }
            }
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
    ns->is_inline_namespace = false;
    ns->is_anonymous_namespace = false;
    ns->anonymous_typeinfo_identity = NULL;
    ns->decls = NULL;
    ns->classes = NULL;
    ns->class_count = 0;
    ns->templates = NULL;
    ns->template_count = 0;
    ns->children = NULL;
    ns->next = NULL;
    ns->namespace_alias_names = NULL;
    ns->namespace_alias_targets = NULL;
    ns->namespace_alias_count = 0;
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

typedef struct {
    CxxNamespace* result;
    size_t matches;
} CxxNamespaceLookup;

/* Members of an inline namespace participate in lookup in its enclosing
 * namespace.  Only descend through inline children; ordinary nested
 * namespaces are separate lookup scopes.  Keep a match count so a name made
 * visible by multiple inline branches is rejected as ambiguous instead of
 * depending on sibling insertion order. */
static void cxx_namespace_lookup_collect(CxxNamespace* scope,
                                         const char* name,
                                         CxxNamespaceLookup* lookup) {
    CxxNamespace* child;
    if (!scope || !name || !lookup || lookup->matches > 1u) return;
    for (child = scope->children; child; child = child->next) {
        if (child->name && strcmp(child->name, name) == 0) {
            lookup->result = child;
            ++lookup->matches;
            if (lookup->matches > 1u) return;
        }
    }
    for (int index = 0; index < scope->namespace_alias_count; ++index) {
        if (scope->namespace_alias_names[index] &&
            strcmp(scope->namespace_alias_names[index], name) == 0) {
            lookup->result = scope->namespace_alias_targets[index];
            ++lookup->matches;
            if (lookup->matches > 1u) return;
        }
    }
    for (child = scope->children; child; child = child->next) {
        if (child->is_inline_namespace) {
            cxx_namespace_lookup_collect(child, name, lookup);
            if (lookup->matches > 1u) return;
        }
    }
}

CxxNamespace* cxx_namespace_lookup(CxxNamespace* root, const char* name) {
    CxxNamespaceLookup lookup = { 0 };
    if (!root || !name) return NULL;
    cxx_namespace_lookup_collect(root, name, &lookup);
    return lookup.matches == 1u ? lookup.result : NULL;
}

static bool cxx_name_has_final_component(const char* qualified_name,
                                         const char* name) {
    const char* component;
    if (!qualified_name || !name) return false;
    component = strrchr(qualified_name, ':');
    if (component && component > qualified_name && component[-1] == ':') {
        component++;
    } else {
        component = qualified_name;
    }
    return strcmp(component, name) == 0;
}

static bool cxx_namespace_has_direct_name(CxxNamespace* ns,
                                          const char* name) {
    if (!ns || !name) return false;
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        if (child->name && strcmp(child->name, name) == 0) return true;
    }
    for (DeclList* entry = ns->decls; entry; entry = entry->next) {
        if (entry->decl && cxx_name_has_final_component(entry->decl->name,
                                                        name)) {
            return true;
        }
    }
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* cls = ns->classes[index];
        if (cls && cls->name && strcmp(cls->name, name) == 0) return true;
    }
    for (int index = 0; index < ns->template_count; ++index) {
        CxxTemplate* tmpl = ns->templates[index];
        if (tmpl && tmpl->name && strcmp(tmpl->name, name) == 0) return true;
    }
    for (int index = 0; index < ns->namespace_alias_count; ++index) {
        if (ns->namespace_alias_names[index] &&
            strcmp(ns->namespace_alias_names[index], name) == 0) {
            return true;
        }
    }
    for (int index = 0; index < ns->using_declaration_count; ++index) {
        if (cxx_name_has_final_component(ns->using_declarations[index], name)) {
            return true;
        }
    }
    return false;
}

bool cxx_namespace_add_alias(CxxNamespace* ns, const char* name,
                             CxxNamespace* target) {
    /* A declaration in an inline namespace is visible in the enclosing
     * namespace, but that injected name does not prevent a namespace alias
     * declaration there.  If the spellings collide, ordinary qualified
     * lookup will correctly report the resulting ambiguity. */
    if (!ns || !name || !*name || !target ||
        cxx_namespace_has_direct_name(ns, name)) {
        return false;
    }
    ns->namespace_alias_names = ast_arena_grow(
        ns->namespace_alias_names,
        sizeof(*ns->namespace_alias_names) *
            (size_t)ns->namespace_alias_count,
        sizeof(*ns->namespace_alias_names) *
            (size_t)(ns->namespace_alias_count + 1));
    ns->namespace_alias_targets = ast_arena_grow(
        ns->namespace_alias_targets,
        sizeof(*ns->namespace_alias_targets) *
            (size_t)ns->namespace_alias_count,
        sizeof(*ns->namespace_alias_targets) *
            (size_t)(ns->namespace_alias_count + 1));
    ns->namespace_alias_names[ns->namespace_alias_count] =
        rcc_intern(name);
    ns->namespace_alias_targets[ns->namespace_alias_count] = target;
    ++ns->namespace_alias_count;
    return true;
}

void cxx_namespace_add_decl(CxxNamespace* ns, Decl* decl) {
    DeclList* node = rcc_alloc(sizeof(DeclList));
    node->decl = decl;
    node->next = ns->decls;
    ns->decls = node;
}

static int cxx_template_redeclaration_parameter_index(
    Type* type, const CxxTemplate* tmpl) {
    if (!type || !tmpl) return -1;
    if (!type->cxx_dependent_member_name &&
        type->cxx_template_arg_count == 0 &&
        (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) && type->tag) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            const TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->name &&
                strcmp(parameter->name, type->tag) == 0) {
                return index;
            }
        }
    }
    if (type->cxx_template_param_index < 0 ||
        type->cxx_template_param_index >= tmpl->param_count ||
        tmpl->params[type->cxx_template_param_index].kind != TPARAM_TYPE) {
        return -1;
    }
    return type->cxx_template_param_index;
}

static bool cxx_template_redeclaration_type_matches(
    Type* left, const CxxTemplate* left_template,
    Type* right, const CxxTemplate* right_template, int depth) {
    if (!left || !right || depth > 64 || left->kind != right->kind ||
        left->is_const != right->is_const ||
        left->is_volatile != right->is_volatile ||
        left->is_atomic != right->is_atomic ||
        left->is_restrict != right->is_restrict ||
        left->is_reference != right->is_reference ||
        left->is_rvalue_reference != right->is_rvalue_reference ||
        left->is_unsigned != right->is_unsigned ||
        left->cxx_is_member_pointer != right->cxx_is_member_pointer ||
        left->cxx_dependent_alias_template !=
            right->cxx_dependent_alias_template) {
        return false;
    }
    if (left->cxx_dependent || right->cxx_dependent) {
        int left_index = cxx_template_redeclaration_parameter_index(
            left, left_template);
        int right_index = cxx_template_redeclaration_parameter_index(
            right, right_template);
        if (!left->cxx_dependent || !right->cxx_dependent ||
            left_index < 0 || right_index != left_index ||
            ((left->cxx_dependent_member_name ||
              right->cxx_dependent_member_name) &&
             (!left->cxx_dependent_member_name ||
              !right->cxx_dependent_member_name ||
              strcmp(left->cxx_dependent_member_name,
                     right->cxx_dependent_member_name) != 0)) ||
            left->cxx_template_arg_count != right->cxx_template_arg_count ||
            (left->cxx_template_arg_count > 0 &&
             (!left->cxx_template_args || !right->cxx_template_args))) {
            return false;
        }
        for (int index = 0; index < left->cxx_template_arg_count; ++index) {
            bool left_pack_expansion =
                left->cxx_template_arg_pack_expansions &&
                left->cxx_template_arg_pack_expansions[index];
            bool right_pack_expansion =
                right->cxx_template_arg_pack_expansions &&
                right->cxx_template_arg_pack_expansions[index];
            if (left_pack_expansion != right_pack_expansion ||
                !cxx_template_redeclaration_type_matches(
                    left->cxx_template_args[index], left_template,
                    right->cxx_template_args[index], right_template,
                    depth + 1)) {
                return false;
            }
        }
        return true;
    }
    if (left->cxx_is_member_pointer &&
        !cxx_template_redeclaration_type_matches(
            left->cxx_member_pointer_owner, left_template,
            right->cxx_member_pointer_owner, right_template, depth + 1)) {
        return false;
    }
    switch (left->kind) {
        case TYPE_PTR:
        case TYPE_ARRAY:
        case TYPE_VECTOR:
            if (left->kind == TYPE_ARRAY &&
                left->array_len != right->array_len) return false;
            if (left->kind == TYPE_VECTOR &&
                (left->size != right->size ||
                 left->array_len != right->array_len)) return false;
            return cxx_template_redeclaration_type_matches(
                left->base, left_template, right->base, right_template,
                depth + 1);
        case TYPE_FUNC: {
            TypeParam* left_parameter = left->params;
            TypeParam* right_parameter = right->params;
            if (left->variadic != right->variadic ||
                left->has_prototype != right->has_prototype ||
                !cxx_template_redeclaration_type_matches(
                    left->ret_type, left_template, right->ret_type,
                    right_template, depth + 1)) {
                return false;
            }
            while (left_parameter && right_parameter) {
                if (!cxx_template_redeclaration_type_matches(
                        left_parameter->type, left_template,
                        right_parameter->type, right_template, depth + 1)) {
                    return false;
                }
                left_parameter = left_parameter->next;
                right_parameter = right_parameter->next;
            }
            return !left_parameter && !right_parameter;
        }
        default:
            return type_is_compatible(left, right);
    }
}

static bool cxx_template_template_signature_matches(
    const CxxTemplate* left, const CxxTemplate* right, int depth) {
    if (!left || !right || depth > 32 ||
        left->param_count != right->param_count) {
        return false;
    }
    for (int index = 0; index < left->param_count; ++index) {
        const TemplateParam* left_parameter = &left->params[index];
        const TemplateParam* right_parameter = &right->params[index];
        if (left_parameter->kind != right_parameter->kind ||
            left_parameter->is_pack != right_parameter->is_pack) {
            return false;
        }
        if (left_parameter->kind == TPARAM_TYPE) continue;
        if (left_parameter->kind == TPARAM_NONTYPE) {
            if (!cxx_template_redeclaration_type_matches(
                    left_parameter->type, left,
                    right_parameter->type, right, 0)) {
                return false;
            }
            continue;
        }
        if (left_parameter->kind != TPARAM_TEMPLATE ||
            !cxx_template_template_signature_matches(
                left_parameter->template_signature,
                right_parameter->template_signature, depth + 1)) {
            return false;
        }
    }
    return true;
}

static int cxx_template_redeclaration_parameter_named(
    const CxxTemplate* tmpl, const char* name) {
    if (!tmpl || !name) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].name &&
            strcmp(tmpl->params[index].name, name) == 0) {
            return index;
        }
    }
    return -1;
}

typedef struct CxxRequiresParameterScope {
    const DeclList* parameters;
    const struct CxxRequiresParameterScope* parent;
} CxxRequiresParameterScope;

static bool cxx_template_redeclaration_requires_parameter_location(
    const CxxRequiresParameterScope* scope, const Expr* expression,
    int* scope_depth, int* parameter_index) {
    int current_depth = 0;
    if (!expression) return false;
    for (; scope; scope = scope->parent, ++current_depth) {
        int index = 0;
        for (const DeclList* parameter = scope->parameters; parameter;
             parameter = parameter->next, ++index) {
            if (parameter->decl && parameter->decl == expression->ident_decl) {
                *scope_depth = current_depth;
                *parameter_index = index;
                return true;
            }
        }
        index = 0;
        for (const DeclList* parameter = scope->parameters; parameter;
             parameter = parameter->next, ++index) {
            if (parameter->decl && parameter->decl->name &&
                expression->ident_name &&
                strcmp(parameter->decl->name, expression->ident_name) == 0) {
                *scope_depth = current_depth;
                *parameter_index = index;
                return true;
            }
        }
    }
    return false;
}

static bool cxx_template_redeclaration_requires_parameter_matches(
    const CxxRequiresParameterScope* left_scope, const Expr* left,
    const CxxRequiresParameterScope* right_scope, const Expr* right,
    bool* is_local) {
    int left_depth = -1;
    int right_depth = -1;
    int left_index = -1;
    int right_index = -1;
    bool left_local = cxx_template_redeclaration_requires_parameter_location(
        left_scope, left, &left_depth, &left_index);
    bool right_local = cxx_template_redeclaration_requires_parameter_location(
        right_scope, right, &right_depth, &right_index);
    *is_local = left_local || right_local;
    return !*is_local ||
        (left_local && right_local && left_depth == right_depth &&
         left_index == right_index);
}

static bool cxx_template_redeclaration_expr_matches(
    const Expr* left, const CxxTemplate* left_template,
    const CxxRequiresParameterScope* left_requires_params,
    const Expr* right, const CxxTemplate* right_template,
    const CxxRequiresParameterScope* right_requires_params, int depth);

static bool cxx_template_redeclaration_expr_list_matches(
    const ExprList* left, const CxxTemplate* left_template,
    const CxxRequiresParameterScope* left_requires_params,
    const ExprList* right, const CxxTemplate* right_template,
    const CxxRequiresParameterScope* right_requires_params, int depth) {
    while (left && right) {
        if (!cxx_template_redeclaration_expr_matches(
                left->expr, left_template, left_requires_params,
                right->expr, right_template, right_requires_params, depth + 1)) {
            return false;
        }
        left = left->next;
        right = right->next;
    }
    return !left && !right;
}

static bool cxx_template_redeclaration_initializer_list_matches(
    const ExprList* left, const CxxTemplate* left_template,
    const CxxRequiresParameterScope* left_requires_params,
    const ExprList* right, const CxxTemplate* right_template,
    const CxxRequiresParameterScope* right_requires_params, int depth) {
    while (left && right) {
        if (left->designator_kind != right->designator_kind ||
            (left->designator_kind == INIT_DESIGNATOR_INDEX &&
             left->designator_index != right->designator_index) ||
            (left->designator_kind == INIT_DESIGNATOR_FIELD &&
             (!left->designator_field || !right->designator_field ||
              strcmp(left->designator_field,
                     right->designator_field) != 0)) ||
            !cxx_template_redeclaration_expr_matches(
                left->expr, left_template, left_requires_params,
                right->expr, right_template, right_requires_params,
                depth + 1)) {
            return false;
        }
        left = left->next;
        right = right->next;
    }
    return !left && !right;
}

typedef enum {
    CXX_REDECL_REQUIREMENT_EXPRESSION,
    CXX_REDECL_REQUIREMENT_NESTED,
    CXX_REDECL_REQUIREMENT_TYPE,
    CXX_REDECL_REQUIREMENT_COMPOUND,
} CxxRedeclarationRequirementKind;

typedef struct {
    const ExprList* expressions;
    const ExprList* nested;
    const TypeList* types;
    const CxxCompoundRequirement* compounds;
} CxxRedeclarationRequirementCursor;

typedef struct {
    CxxRedeclarationRequirementKind kind;
    const Expr* expression;
    Type* type;
    const CxxCompoundRequirement* compound;
    SourceLoc loc;
} CxxRedeclarationRequirement;

static bool cxx_template_redeclaration_loc_before(SourceLoc left,
                                                   SourceLoc right) {
    return left.line < right.line ||
        (left.line == right.line && left.column < right.column);
}

static bool cxx_template_redeclaration_next_requirement(
    const CxxRedeclarationRequirementCursor* cursor,
    CxxRedeclarationRequirement* requirement) {
    bool found = false;
    if (cursor->expressions) {
        requirement->kind = CXX_REDECL_REQUIREMENT_EXPRESSION;
        requirement->expression = cursor->expressions->expr;
        requirement->type = NULL;
        requirement->compound = NULL;
        requirement->loc = cursor->expressions->expr->loc;
        found = true;
    }
    if (cursor->nested && (!found || cxx_template_redeclaration_loc_before(
            cursor->nested->expr->loc, requirement->loc))) {
        requirement->kind = CXX_REDECL_REQUIREMENT_NESTED;
        requirement->expression = cursor->nested->expr;
        requirement->type = NULL;
        requirement->compound = NULL;
        requirement->loc = cursor->nested->expr->loc;
        found = true;
    }
    if (cursor->types && (!found || cxx_template_redeclaration_loc_before(
            cursor->types->loc, requirement->loc))) {
        requirement->kind = CXX_REDECL_REQUIREMENT_TYPE;
        requirement->expression = NULL;
        requirement->type = cursor->types->type;
        requirement->compound = NULL;
        requirement->loc = cursor->types->loc;
        found = true;
    }
    if (cursor->compounds && (!found || cxx_template_redeclaration_loc_before(
            cursor->compounds->loc, requirement->loc))) {
        requirement->kind = CXX_REDECL_REQUIREMENT_COMPOUND;
        requirement->expression = NULL;
        requirement->type = NULL;
        requirement->compound = cursor->compounds;
        requirement->loc = cursor->compounds->loc;
        found = true;
    }
    return found;
}

static void cxx_template_redeclaration_advance_requirement(
    CxxRedeclarationRequirementCursor* cursor,
    CxxRedeclarationRequirementKind kind) {
    switch (kind) {
        case CXX_REDECL_REQUIREMENT_EXPRESSION:
            cursor->expressions = cursor->expressions->next;
            break;
        case CXX_REDECL_REQUIREMENT_NESTED:
            cursor->nested = cursor->nested->next;
            break;
        case CXX_REDECL_REQUIREMENT_TYPE:
            cursor->types = cursor->types->next;
            break;
        case CXX_REDECL_REQUIREMENT_COMPOUND:
            cursor->compounds = cursor->compounds->next;
            break;
    }
}

static bool cxx_template_redeclaration_requires_matches(
    const Expr* left, const CxxTemplate* left_template,
    const CxxRequiresParameterScope* left_outer_requires_params,
    const Expr* right, const CxxTemplate* right_template,
    const CxxRequiresParameterScope* right_outer_requires_params, int depth) {
    const DeclList* left_parameter;
    const DeclList* right_parameter;
    CxxRequiresParameterScope left_scope;
    CxxRequiresParameterScope right_scope;
    CxxRedeclarationRequirementCursor left_cursor;
    CxxRedeclarationRequirementCursor right_cursor;
    CxxRedeclarationRequirement left_requirement;
    CxxRedeclarationRequirement right_requirement;
    if (!left || !right || depth > 64 ||
        left->kind != EXPR_CXX_REQUIRES ||
        right->kind != EXPR_CXX_REQUIRES) {
        return false;
    }
    left_parameter = left->cxx_requires_params;
    right_parameter = right->cxx_requires_params;
    while (left_parameter && right_parameter) {
        if (!left_parameter->decl || !right_parameter->decl ||
            left_parameter->decl->param_is_pack !=
                right_parameter->decl->param_is_pack ||
            !cxx_template_redeclaration_type_matches(
                left_parameter->decl->type, left_template,
                right_parameter->decl->type, right_template, depth + 1)) {
            return false;
        }
        left_parameter = left_parameter->next;
        right_parameter = right_parameter->next;
    }
    if (left_parameter || right_parameter) return false;

    left_scope.parameters = left->cxx_requires_params;
    left_scope.parent = left_outer_requires_params;
    right_scope.parameters = right->cxx_requires_params;
    right_scope.parent = right_outer_requires_params;
    left_cursor.expressions = left->cxx_requires_items;
    left_cursor.nested = left->cxx_requires_nested;
    left_cursor.types = left->cxx_requires_types;
    left_cursor.compounds = left->cxx_requires_compound;
    right_cursor.expressions = right->cxx_requires_items;
    right_cursor.nested = right->cxx_requires_nested;
    right_cursor.types = right->cxx_requires_types;
    right_cursor.compounds = right->cxx_requires_compound;
    while (left_cursor.expressions || left_cursor.nested || left_cursor.types ||
           left_cursor.compounds) {
        if (!cxx_template_redeclaration_next_requirement(
                &left_cursor, &left_requirement) ||
            !cxx_template_redeclaration_next_requirement(
                &right_cursor, &right_requirement) ||
            left_requirement.kind != right_requirement.kind) {
            return false;
        }
        switch (left_requirement.kind) {
            case CXX_REDECL_REQUIREMENT_EXPRESSION:
            case CXX_REDECL_REQUIREMENT_NESTED:
                if (!cxx_template_redeclaration_expr_matches(
                        left_requirement.expression, left_template, &left_scope,
                        right_requirement.expression, right_template,
                        &right_scope, depth + 1)) {
                    return false;
                }
                break;
            case CXX_REDECL_REQUIREMENT_TYPE:
                if (!cxx_template_redeclaration_type_matches(
                        left_requirement.type, left_template,
                        right_requirement.type, right_template, depth + 1)) {
                    return false;
                }
                break;
            case CXX_REDECL_REQUIREMENT_COMPOUND: {
                const CxxCompoundRequirement* left_compound =
                    left_requirement.compound;
                const CxxCompoundRequirement* right_compound =
                    right_requirement.compound;
                if (left_compound->is_noexcept != right_compound->is_noexcept ||
                    left_compound->return_type_convertible !=
                        right_compound->return_type_convertible ||
                    (!!left_compound->return_type !=
                        !!right_compound->return_type) ||
                    !cxx_template_redeclaration_expr_matches(
                        left_compound->expr, left_template, &left_scope,
                        right_compound->expr, right_template, &right_scope,
                        depth + 1) ||
                    (left_compound->return_type &&
                     !cxx_template_redeclaration_type_matches(
                        left_compound->return_type, left_template,
                        right_compound->return_type, right_template,
                        depth + 1))) {
                    return false;
                }
                break;
            }
        }
        cxx_template_redeclaration_advance_requirement(
            &left_cursor, left_requirement.kind);
        cxx_template_redeclaration_advance_requirement(
            &right_cursor, right_requirement.kind);
    }
    return !right_cursor.expressions && !right_cursor.nested &&
        !right_cursor.types && !right_cursor.compounds;
}

static bool cxx_template_redeclaration_expr_matches(
    const Expr* left, const CxxTemplate* left_template,
    const CxxRequiresParameterScope* left_requires_params,
    const Expr* right, const CxxTemplate* right_template,
    const CxxRequiresParameterScope* right_requires_params, int depth) {
    if (!left || !right || depth > 64 || left->kind != right->kind) {
        return false;
    }
    switch (left->kind) {
        case EXPR_INT_LIT:
            return left->int_val == right->int_val && left->type &&
                right->type && cxx_template_redeclaration_type_matches(
                    left->type, left_template, right->type, right_template,
                    depth + 1);
        case EXPR_FLOAT_LIT:
            return left->float_val == right->float_val && left->type &&
                right->type && cxx_template_redeclaration_type_matches(
                    left->type, left_template, right->type, right_template,
                    depth + 1);
        case EXPR_CHAR_LIT:
            return expr_char_code_unit(left) ==
                    expr_char_code_unit(right) && left->type &&
                right->type && cxx_template_redeclaration_type_matches(
                    left->type, left_template, right->type, right_template,
                    depth + 1);
        case EXPR_STRING_LIT:
            return left->str_val && right->str_val &&
                strcmp(left->str_val, right->str_val) == 0 && left->type &&
                right->type && cxx_template_redeclaration_type_matches(
                    left->type, left_template, right->type, right_template,
                    depth + 1);
        case EXPR_IDENT: {
            bool is_local;
            if (!cxx_template_redeclaration_requires_parameter_matches(
                    left_requires_params, left, right_requires_params, right,
                    &is_local)) return false;
            if (is_local) return true;
            int left_index = cxx_template_redeclaration_parameter_named(
                left_template, left->ident_name);
            int right_index = cxx_template_redeclaration_parameter_named(
                right_template, right->ident_name);
            if (left_index >= 0 || right_index >= 0) {
                return left_index >= 0 && right_index == left_index &&
                    left_template->params[left_index].kind ==
                        right_template->params[right_index].kind;
            }
            return left->ident_name && right->ident_name &&
                strcmp(left->ident_name, right->ident_name) == 0 &&
                (!left->ident_decl || !right->ident_decl ||
                 left->ident_decl == right->ident_decl);
        }
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return left->member_name && right->member_name &&
                strcmp(left->member_name, right->member_name) == 0 &&
                cxx_template_redeclaration_expr_matches(
                    left->member_base, left_template, left_requires_params,
                    right->member_base, right_template,
                    right_requires_params, depth + 1);
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
        case EXPR_NOEXCEPT:
            return cxx_template_redeclaration_expr_matches(
                left->unary_operand, left_template, left_requires_params,
                right->unary_operand, right_template, right_requires_params,
                depth + 1);
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            if (left->sizeof_pack_name || right->sizeof_pack_name) {
                Expr left_pack = {.ident_name = left->sizeof_pack_name};
                Expr right_pack = {.ident_name = right->sizeof_pack_name};
                bool is_local;
                if (!cxx_template_redeclaration_requires_parameter_matches(
                        left_requires_params, &left_pack,
                        right_requires_params, &right_pack, &is_local)) {
                    return false;
                }
                if (is_local) return true;
                int left_index = cxx_template_redeclaration_parameter_named(
                    left_template, left->sizeof_pack_name);
                int right_index = cxx_template_redeclaration_parameter_named(
                    right_template, right->sizeof_pack_name);
                return left_index >= 0 && right_index == left_index &&
                    left_template->params[left_index].is_pack &&
                    right_template->params[right_index].is_pack;
            }
            if (left->sizeof_type || right->sizeof_type) {
                return cxx_template_redeclaration_type_matches(
                    left->sizeof_type, left_template,
                    right->sizeof_type, right_template, depth + 1);
            }
            return cxx_template_redeclaration_expr_matches(
                left->unary_operand, left_template, left_requires_params,
                right->unary_operand, right_template, right_requires_params,
                depth + 1);
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
        case EXPR_SPACESHIP:
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            return cxx_template_redeclaration_expr_matches(
                       left->binary_lhs, left_template, left_requires_params,
                       right->binary_lhs, right_template,
                       right_requires_params, depth + 1) &&
                cxx_template_redeclaration_expr_matches(
                       left->binary_rhs, left_template,
                       left_requires_params, right->binary_rhs,
                       right_template, right_requires_params, depth + 1);
        case EXPR_INDEX:
            return cxx_template_redeclaration_expr_matches(
                       left->index_base, left_template, left_requires_params,
                       right->index_base, right_template,
                       right_requires_params, depth + 1) &&
                cxx_template_redeclaration_expr_matches(
                       left->index_expr, left_template, left_requires_params,
                       right->index_expr, right_template,
                       right_requires_params, depth + 1);
        case EXPR_COND:
            return cxx_template_redeclaration_expr_matches(
                       left->cond_test, left_template,
                       left_requires_params, right->cond_test,
                       right_template, right_requires_params, depth + 1) &&
                cxx_template_redeclaration_expr_matches(
                       left->cond_then, left_template,
                       left_requires_params, right->cond_then,
                       right_template, right_requires_params, depth + 1) &&
                cxx_template_redeclaration_expr_matches(
                       left->cond_else, left_template,
                       left_requires_params, right->cond_else,
                       right_template, right_requires_params, depth + 1);
        case EXPR_CALL:
            if (left->cxx_concept_template != right->cxx_concept_template ||
                left->call_is_new != right->call_is_new ||
                left->call_is_delete != right->call_is_delete) {
                return false;
            }
            if (left->call_is_new &&
                (left->call_new_value_init != right->call_new_value_init ||
                 left->call_new_is_array != right->call_new_is_array ||
                 left->call_new_brace_init != right->call_new_brace_init ||
                 (!!left->call_new_type != !!right->call_new_type) ||
                 (left->call_new_type &&
                  !cxx_template_redeclaration_type_matches(
                      left->call_new_type, left_template,
                      right->call_new_type, right_template, depth + 1)) ||
                 (!!left->call_new_count != !!right->call_new_count) ||
                 (left->call_new_count &&
                  !cxx_template_redeclaration_expr_matches(
                      left->call_new_count, left_template,
                      left_requires_params, right->call_new_count,
                      right_template, right_requires_params, depth + 1)) ||
                 !cxx_template_redeclaration_expr_list_matches(
                     left->call_new_args, left_template, left_requires_params,
                     right->call_new_args, right_template,
                     right_requires_params, depth + 1))) {
                return false;
            }
            return (!left->call_is_delete ||
                    left->call_delete_is_array ==
                        right->call_delete_is_array) &&
                cxx_template_redeclaration_expr_matches(
                    left->call_func, left_template,
                    left_requires_params, right->call_func,
                    right_template, right_requires_params, depth + 1) &&
                cxx_template_redeclaration_expr_list_matches(
                    left->call_args, left_template,
                    left_requires_params, right->call_args,
                    right_template, right_requires_params, depth + 1);
        case EXPR_CAST:
            return left->cxx_cast_kind == right->cxx_cast_kind &&
                cxx_template_redeclaration_type_matches(
                       left->cast_type, left_template,
                       right->cast_type, right_template, depth + 1) &&
                cxx_template_redeclaration_expr_matches(
                       left->cast_expr, left_template,
                       left_requires_params, right->cast_expr,
                       right_template, right_requires_params, depth + 1);
        case EXPR_COMPOUND:
            return left->compound_value_init ==
                       right->compound_value_init &&
                left->compound_copy_init == right->compound_copy_init &&
                left->compound_paren_init == right->compound_paren_init &&
                left->compound_designator_wrapper ==
                    right->compound_designator_wrapper &&
                (!!left->compound_type == !!right->compound_type) &&
                (!left->compound_type ||
                 cxx_template_redeclaration_type_matches(
                     left->compound_type, left_template,
                     right->compound_type, right_template, depth + 1)) &&
                cxx_template_redeclaration_initializer_list_matches(
                    left->compound_init, left_template,
                    left_requires_params, right->compound_init,
                    right_template, right_requires_params, depth + 1);
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
            return (!!left->va_second_operand ==
                    !!right->va_second_operand) &&
                cxx_template_redeclaration_expr_matches(
                    left->va_list_operand, left_template,
                    left_requires_params, right->va_list_operand,
                    right_template, right_requires_params, depth + 1) &&
                (!left->va_second_operand ||
                 cxx_template_redeclaration_expr_matches(
                     left->va_second_operand, left_template,
                     left_requires_params, right->va_second_operand,
                     right_template, right_requires_params, depth + 1));
        case EXPR_VA_ARG:
            return (!!left->va_arg_type == !!right->va_arg_type) &&
                (!left->va_arg_type ||
                 cxx_template_redeclaration_type_matches(
                     left->va_arg_type, left_template, right->va_arg_type,
                     right_template, depth + 1)) &&
                cxx_template_redeclaration_expr_matches(
                    left->va_list_operand, left_template,
                    left_requires_params, right->va_list_operand,
                    right_template, right_requires_params, depth + 1);
        case EXPR_CXX_TYPEID:
            return left->cxx_typeid_is_type == right->cxx_typeid_is_type &&
                (!!left->cxx_typeid_operand_type ==
                 !!right->cxx_typeid_operand_type) &&
                (!left->cxx_typeid_operand_type ||
                 cxx_template_redeclaration_type_matches(
                     left->cxx_typeid_operand_type, left_template,
                     right->cxx_typeid_operand_type, right_template,
                     depth + 1)) &&
                (!!left->cxx_typeid_operand ==
                 !!right->cxx_typeid_operand) &&
                (!left->cxx_typeid_operand ||
                 cxx_template_redeclaration_expr_matches(
                     left->cxx_typeid_operand, left_template,
                     left_requires_params, right->cxx_typeid_operand,
                     right_template, right_requires_params, depth + 1));
        case EXPR_CXX_FOLD: {
            if (left->cxx_fold_operator != right->cxx_fold_operator ||
                left->cxx_fold_left != right->cxx_fold_left ||
                (!!left->cxx_fold_init != !!right->cxx_fold_init) ||
                (!!left->cxx_fold_pattern != !!right->cxx_fold_pattern) ||
                (!!left->cxx_fold_pack_name !=
                 !!right->cxx_fold_pack_name)) {
                return false;
            }
            if (left->cxx_fold_pack_name) {
                Expr left_pack = {
                    .ident_name = left->cxx_fold_pack_name};
                Expr right_pack = {
                    .ident_name = right->cxx_fold_pack_name};
                bool is_local;
                if (!cxx_template_redeclaration_requires_parameter_matches(
                        left_requires_params, &left_pack,
                        right_requires_params, &right_pack, &is_local)) {
                    return false;
                }
                if (!is_local) {
                    int left_index = cxx_template_redeclaration_parameter_named(
                        left_template, left->cxx_fold_pack_name);
                    int right_index = cxx_template_redeclaration_parameter_named(
                        right_template, right->cxx_fold_pack_name);
                    if (left_index < 0 || right_index != left_index ||
                        !left_template->params[left_index].is_pack ||
                        !right_template->params[right_index].is_pack) {
                        return false;
                    }
                }
            }
            return (!left->cxx_fold_init ||
                    cxx_template_redeclaration_expr_matches(
                        left->cxx_fold_init, left_template,
                        left_requires_params, right->cxx_fold_init,
                        right_template, right_requires_params, depth + 1)) &&
                (!left->cxx_fold_pattern ||
                 cxx_template_redeclaration_expr_matches(
                     left->cxx_fold_pattern, left_template,
                     left_requires_params, right->cxx_fold_pattern,
                     right_template, right_requires_params, depth + 1));
        }
        case EXPR_CXX_REQUIRES:
            return cxx_template_redeclaration_requires_matches(
                left, left_template, left_requires_params,
                right, right_template, right_requires_params, depth + 1);
        default:
            return false;
    }
}

bool cxx_template_constraint_mapping_matches(
    const Expr* left, const CxxTemplate* left_template,
    const Expr* right, const CxxTemplate* right_template) {
    return cxx_template_redeclaration_expr_matches(
        left, left_template, NULL, right, right_template, NULL, 0);
}

bool cxx_template_type_parameter_mapping_matches(
    Type* left, const CxxTemplate* left_template,
    Type* right, const CxxTemplate* right_template) {
    return cxx_template_redeclaration_type_matches(
        left, left_template, right, right_template, 0);
}

static bool cxx_function_template_redeclaration_matches(
    const CxxTemplate* left, const CxxTemplate* right) {
    Type* left_type;
    Type* right_type;
    TypeParam* left_parameter;
    TypeParam* right_parameter;
    DeclList* left_declaration_parameter;
    DeclList* right_declaration_parameter;
    if (!left || !right || left->kind != TMPL_FUNCTION ||
        right->kind != TMPL_FUNCTION || left->is_concept ||
        right->is_concept || !left->func_def || !right->func_def ||
        !left->name || !right->name || strcmp(left->name, right->name) != 0 ||
        left->param_count != right->param_count ||
        (!!left->constraint != !!right->constraint) ||
        (left->constraint &&
         !cxx_template_redeclaration_expr_matches(
             left->constraint,
             left->constraint_context ? left->constraint_context : left,
             NULL,
             right->constraint,
             right->constraint_context ? right->constraint_context : right,
             NULL,
             0)) ||
        left->function_lowering != right->function_lowering ||
        left->is_noexcept != right->is_noexcept ||
        (!!left->func_def->func_noexcept_expr !=
         !!right->func_def->func_noexcept_expr) ||
        (left->func_def->func_noexcept_expr &&
         !cxx_template_redeclaration_expr_matches(
             left->func_def->func_noexcept_expr, left, NULL,
             right->func_def->func_noexcept_expr, right, NULL, 0)) ||
        left->is_constexpr != right->is_constexpr ||
        left->func_def->func_is_consteval !=
            right->func_def->func_is_consteval) {
        return false;
    }
    for (int index = 0; index < left->param_count; ++index) {
        const TemplateParam* left_template_parameter = &left->params[index];
        const TemplateParam* right_template_parameter = &right->params[index];
        if (left_template_parameter->kind != right_template_parameter->kind ||
            left_template_parameter->is_pack !=
                right_template_parameter->is_pack) {
            return false;
        }
        if (left_template_parameter->kind == TPARAM_TYPE) continue;
        if (left_template_parameter->kind == TPARAM_NONTYPE) {
            if (left_template_parameter->is_pack ||
                !cxx_template_redeclaration_type_matches(
                    left_template_parameter->type, left,
                    right_template_parameter->type, right, 0)) {
                return false;
            }
            continue;
        }
        if (left_template_parameter->kind != TPARAM_TEMPLATE ||
            !cxx_template_template_signature_matches(
                left_template_parameter->template_signature,
                right_template_parameter->template_signature, 0)) {
            return false;
        }
    }
    left_type = left->func_def->type;
    right_type = right->func_def->type;
    if (!left_type || !right_type || left_type->kind != TYPE_FUNC ||
        right_type->kind != TYPE_FUNC ||
        !cxx_template_redeclaration_type_matches(
            left_type->ret_type, left, right_type->ret_type, right, 0)) {
        return false;
    }
    if (left_type->variadic != right_type->variadic ||
        left_type->has_prototype != right_type->has_prototype) {
        return false;
    }
    left_parameter = left_type->params;
    right_parameter = right_type->params;
    left_declaration_parameter = left->func_def->func_params;
    right_declaration_parameter = right->func_def->func_params;
    while (left_parameter && right_parameter) {
        if ((left_declaration_parameter &&
             left_declaration_parameter->decl &&
             left_declaration_parameter->decl->param_is_pack) !=
            (right_declaration_parameter &&
             right_declaration_parameter->decl &&
             right_declaration_parameter->decl->param_is_pack)) {
            return false;
        }
        if (!cxx_template_redeclaration_type_matches(
                left_parameter->type, left, right_parameter->type, right, 0)) {
            return false;
        }
        left_parameter = left_parameter->next;
        right_parameter = right_parameter->next;
        left_declaration_parameter = left_declaration_parameter
            ? left_declaration_parameter->next : NULL;
        right_declaration_parameter = right_declaration_parameter
            ? right_declaration_parameter->next : NULL;
    }
    return !left_parameter && !right_parameter &&
           !left_declaration_parameter && !right_declaration_parameter;
}

bool cxx_function_template_friend_signature_matches(
    const CxxTemplate* candidate, const CxxTemplate* current,
    const char* function_name, Type* function_type,
    DeclList* function_parameters, bool is_consteval,
    Expr* noexcept_expression) {
    Decl signature_declaration = {0};
    CxxTemplate signature_template;
    if (!candidate || !current || !function_name || !function_type ||
        function_type->kind != TYPE_FUNC) {
        return false;
    }
    signature_declaration.kind = DECL_FUNC;
    signature_declaration.name = function_name;
    signature_declaration.type = function_type;
    signature_declaration.func_params = function_parameters;
    signature_declaration.func_is_consteval = is_consteval;
    signature_declaration.func_noexcept_expr = noexcept_expression;
    signature_template = *current;
    signature_template.name = function_name;
    signature_template.func_def = &signature_declaration;
    return cxx_function_template_redeclaration_matches(
        candidate, &signature_template);
}

static void cxx_template_merge_friend_access(CxxTemplate* target,
                                             const CxxTemplate* source) {
    for (CxxFriendAccess* grant = source->friend_access; grant;
         grant = grant->next) {
        bool present = false;
        for (CxxFriendAccess* existing = target->friend_access; existing;
             existing = existing->next) {
            if (existing->owner == grant->owner) {
                present = true;
                break;
            }
        }
        if (!present) {
            CxxFriendAccess* copy = ast_arena_alloc(sizeof(*copy));
            copy->owner = grant->owner;
            copy->next = target->friend_access;
            target->friend_access = copy;
        }
    }
}

static bool cxx_template_has_default_argument(const CxxTemplate* tmpl) {
    if (!tmpl) return false;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].has_default) return true;
    }
    return false;
}

static bool cxx_template_redefines_default_argument(
    const CxxTemplate* left, const CxxTemplate* right) {
    if (!left || !right || left->param_count != right->param_count) {
        return false;
    }
    for (int index = 0; index < left->param_count; ++index) {
        if (left->params[index].has_default &&
            right->params[index].has_default) {
            return true;
        }
    }
    return false;
}

static void cxx_template_merge_default_arguments(CxxTemplate* target,
                                                const CxxTemplate* source) {
    if (!target || !source || target->param_count != source->param_count) {
        return;
    }
    for (int index = 0; index < target->param_count; ++index) {
        TemplateParam* target_parameter = &target->params[index];
        const TemplateParam* source_parameter = &source->params[index];
        if (target_parameter->has_default || !source_parameter->has_default) {
            continue;
        }
        target_parameter->has_default = true;
        if (target_parameter->kind == TPARAM_TYPE ||
            target_parameter->kind == TPARAM_TEMPLATE) {
            target_parameter->default_type = source_parameter->default_type;
        } else if (target_parameter->kind == TPARAM_NONTYPE) {
            target_parameter->default_value = source_parameter->default_value;
            target_parameter->default_context =
                source_parameter->default_context;
        }
    }
}

void cxx_namespace_add_template(CxxNamespace* ns, CxxTemplate* tmpl) {
    if (!ns || !tmpl) return;
    tmpl->ns = ns;
    if (tmpl->friend_access && tmpl->func_def &&
        !tmpl->func_def->func_body &&
        cxx_template_has_default_argument(tmpl)) {
        rcc_error(tmpl->func_def->loc,
                  "friend function template default arguments require a definition");
        return;
    }
    for (int index = 0; index < ns->template_count; ++index) {
        CxxTemplate* existing = ns->templates[index];
        if ((!existing->friend_access && !tmpl->friend_access) ||
            !cxx_function_template_redeclaration_matches(existing, tmpl)) {
            continue;
        }
        if (existing->is_hidden_friend && !tmpl->friend_access &&
            cxx_template_has_default_argument(tmpl)) {
            rcc_error(tmpl->func_def->loc,
                      "cannot add a default template argument to the first namespace declaration of hidden friend template '%s'",
                      tmpl->name ? tmpl->name : "<function template>");
            return;
        }
        if ((existing->is_hidden_friend && existing->func_def &&
             existing->func_def->func_body &&
             cxx_template_has_default_argument(existing)) ||
            (tmpl->friend_access && tmpl->func_def &&
             tmpl->func_def->func_body &&
             cxx_template_has_default_argument(tmpl))) {
            rcc_error(tmpl->func_def->loc,
                      "friend function template with a default template argument cannot be redeclared");
            return;
        }
        if (cxx_template_redefines_default_argument(existing, tmpl)) {
            rcc_error(tmpl->func_def->loc,
                      "redefinition of default template argument for '%s'",
                      tmpl->name ? tmpl->name : "<function template>");
            return;
        }
        bool had_friend_declaration = existing->friend_access != NULL;
        bool has_friend_declaration = tmpl->friend_access != NULL;
        cxx_template_merge_friend_access(existing, tmpl);
        cxx_template_merge_default_arguments(existing, tmpl);
        if (tmpl->constraint) {
            existing->constraint = tmpl->constraint;
            existing->constraint_context = tmpl->constraint_context;
        }
        if (had_friend_declaration && !has_friend_declaration) {
            existing->is_hidden_friend = false;
        }
        if (existing->func_def->func_body && tmpl->func_def->func_body) {
            rcc_error(tmpl->func_def->loc,
                      "redefinition of function template '%s'", tmpl->name);
            return;
        }
        if (!existing->func_def->func_body && tmpl->func_def->func_body) {
            if (existing->instance_count != 0) {
                rcc_error(tmpl->func_def->loc,
                          "function template '%s' is defined after an earlier "
                          "specialization was instantiated",
                          tmpl->name);
                return;
            }
            cxx_template_merge_default_arguments(tmpl, existing);
            existing->func_def = tmpl->func_def;
            existing->params = tmpl->params;
            existing->param_count = tmpl->param_count;
            existing->is_constexpr = tmpl->is_constexpr;
            existing->is_noexcept = tmpl->is_noexcept;
        }
        return;
    }
    tmpl->is_hidden_friend = tmpl->friend_access != NULL;
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
    tmpl->friend_access = NULL;
    tmpl->is_hidden_friend = false;
    tmpl->kind = TMPL_CLASS;
    tmpl->class_def = NULL;
    tmpl->member_method = NULL;
    tmpl->alias_type = NULL;
    tmpl->enclosing_template = NULL;
    tmpl->is_constexpr = false;
    tmpl->is_noexcept = false;
    tmpl->is_concept = false;
    tmpl->constraint = NULL;
    tmpl->constraint_context = NULL;
    tmpl->function_lowering = TMPL_FUNCTION_NONE;
    tmpl->function_constant = 0;
    tmpl->templated_class = NULL;
    tmpl->local_classes = NULL;
    tmpl->local_class_count = 0;
    tmpl->is_local_class_template = false;
    tmpl->local_class_pattern = NULL;
    tmpl->local_class_instance = NULL;
    tmpl->primary_template = NULL;
    tmpl->specializations = NULL;
    tmpl->specialization_count = 0;
    tmpl->specialization_args = NULL;
    tmpl->specialization_value_args = NULL;
    tmpl->specialization_arg_count = 0;
    tmpl->instances = NULL;
    tmpl->instance_count = 0;
    tmpl->pending_pack_args = NULL;
    tmpl->pending_pack_values = NULL;
    tmpl->pending_pack_value_present = NULL;
    tmpl->pending_pack_count = -1;
    tmpl->active_pack_parameters = NULL;
    return tmpl;
}

static int template_type_parameter_index(CxxTemplate* tmpl, Type* type) {
    if (!tmpl || !type) {
        return -1;
    }
    if (type->kind != TYPE_STRUCT || !type->tag) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_TYPE &&
            tmpl->params[index].name &&
            strcmp(tmpl->params[index].name, type->tag) == 0) {
            return index;
        }
    }
    return -1;
}

/* Parameter packs may be written as a type pattern such as `auto*...`.  The
 * ordinary substitution helper must still distinguish that pattern from a
 * complete dependent type, so only pack-parameter discovery unwraps the
 * pointer/array carrier. */
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

static int template_parameter_pack_index_nested(CxxTemplate* tmpl,
                                                Type* type);

static int template_parameter_pack_index_merge(int current, int nested) {
    if (nested == -1) return current;
    if (current == -1) return nested;
    return current == nested ? current : -2;
}

static int template_parameter_pack_index_expression(CxxTemplate* tmpl,
                                                    Expr* expression) {
    int result = -1;
    if (!tmpl || !expression) return -1;
    if (expression->kind == EXPR_IDENT && expression->ident_name) {
        int index = template_value_parameter_index(tmpl, expression);
        return index >= 0 && tmpl->params[index].is_pack ? index : -1;
    }
    if (expression->sizeof_pack_name) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack && parameter->name &&
                strcmp(parameter->name,
                       expression->sizeof_pack_name) == 0) {
                result = template_parameter_pack_index_merge(result, index);
            }
        }
    }
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
        case EXPR_NOEXCEPT:
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->unary_operand));
            return template_parameter_pack_index_merge(
                result, template_parameter_pack_index_nested(
                            tmpl, expression->sizeof_type));
        case EXPR_CAST:
            return template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->cast_expr));
        case EXPR_CXX_TYPEID:
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_nested(
                            tmpl, expression->cxx_typeid_operand_type));
            return template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->cxx_typeid_operand));
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
        case EXPR_SPACESHIP:
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->binary_lhs));
            return template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->binary_rhs));
        case EXPR_COND:
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->cond_test));
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->cond_then));
            return template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->cond_else));
        case EXPR_CALL:
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->call_func));
            for (ExprList* argument = expression->call_args; argument;
                 argument = argument->next) {
                result = template_parameter_pack_index_merge(
                    result, template_parameter_pack_index_expression(
                                tmpl, argument->expr));
            }
            return result;
        case EXPR_INDEX:
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->index_base));
            return template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->index_expr));
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->member_base));
        case EXPR_COMPOUND:
            for (ExprList* item = expression->compound_init; item;
                 item = item->next) {
                result = template_parameter_pack_index_merge(
                    result, template_parameter_pack_index_expression(
                                tmpl, item->expr));
            }
            return result;
        case EXPR_GENERIC:
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->generic_control));
            for (GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                result = template_parameter_pack_index_merge(
                    result, template_parameter_pack_index_expression(
                                tmpl, association->expr));
            }
            return result;
        case EXPR_CXX_FOLD:
            result = template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->cxx_fold_init));
            return template_parameter_pack_index_merge(
                result, template_parameter_pack_index_expression(
                            tmpl, expression->cxx_fold_pattern));
        case EXPR_CXX_REQUIRES:
            for (ExprList* item = expression->cxx_requires_nested; item;
                 item = item->next) {
                result = template_parameter_pack_index_merge(
                    result, template_parameter_pack_index_expression(
                                tmpl, item->expr));
            }
            return result;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_CXX_THIS:
        case EXPR_IDENT:
            return result;
    }
    return result;
}

static int template_parameter_pack_index_nested(CxxTemplate* tmpl,
                                                Type* type) {
    int index = -1;
    if (!tmpl || !type) return -1;
    if (type->kind == TYPE_ARRAY) {
        index = template_value_parameter_index(tmpl, type->array_bound);
        if (index >= 0 && !tmpl->params[index].is_pack) index = -1;
        if (index >= 0) return index;
    }
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) {
        int nested = template_parameter_pack_index_nested(tmpl, type->base);
        if (nested == -2) return -2;
        if (nested >= 0) index = nested;
    }
    if (type->kind == TYPE_STRUCT) {
        int direct = template_type_parameter_index(tmpl, type);
        if (direct >= 0 && !tmpl->params[direct].is_pack) direct = -1;
        if (direct >= 0) {
            if (index >= 0 && index != direct) return -2;
            index = direct;
        }
    }
    if (type->kind == TYPE_FUNC) {
        int nested = template_parameter_pack_index_nested(tmpl,
                                                          type->ret_type);
        if (nested == -2) return -2;
        if (nested >= 0) {
            if (index >= 0 && index != nested) return -2;
            index = nested;
        }
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            nested = template_parameter_pack_index_nested(tmpl,
                                                           parameter->type);
            if (nested == -2) return -2;
            if (nested >= 0) {
                if (index >= 0 && index != nested) return -2;
                index = nested;
            }
        }
    }
    for (int nested_index = 0;
         nested_index < type->cxx_template_arg_count; ++nested_index) {
        int nested = type->cxx_template_args
            ? template_parameter_pack_index_nested(
                  tmpl, type->cxx_template_args[nested_index])
            : -1;
        if (nested == -2) return -2;
        if (nested < 0 && type->cxx_template_value_args) {
            nested = template_parameter_pack_index_expression(
                tmpl, type->cxx_template_value_args[nested_index]);
        }
        if (nested >= 0) {
            if (index >= 0 && index != nested) return -2;
            index = nested;
        }
    }
    return index;
}

static Expr* template_clone_expr(CxxTemplate* tmpl, Expr* expression,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present);
static Type* template_substitute_dependent_alias_template_type(
    CxxTemplate* tmpl, Type* type, Type** args, int arg_count,
    const int64_t* value_args, const bool* value_present);

static Type* template_substitute_type(CxxTemplate* tmpl, Type* type,
                                      Type** args, int arg_count,
                                      const int64_t* value_args,
                                      const bool* value_present) {
    Type* replacement;
    Type* base;
    Type* member_pointer_owner;
    int array_len;
    Expr* array_bound;
    int index;

    if (!type) return NULL;
    if (type->cxx_dependent_alias_template) {
        return template_substitute_dependent_alias_template_type(
            tmpl, type, args, arg_count, value_args, value_present);
    }
    if (type->cxx_dependent && type->cxx_template_param_index >= 0 &&
        type->cxx_template_param_index < arg_count && args &&
        args[type->cxx_template_param_index] && tmpl &&
        type->cxx_template_param_index < tmpl->param_count &&
        tmpl->params[type->cxx_template_param_index].kind ==
            TPARAM_TEMPLATE &&
        type->cxx_template_arg_count == 0) {
        TemplateParam* parameter =
            &tmpl->params[type->cxx_template_param_index];
        replacement = args[type->cxx_template_param_index];
        if (replacement->cxx_dependent && !replacement->cxx_template) {
            return replacement;
        }
        if (!replacement->cxx_template ||
            !rcc_parser_cxx_template_template_argument_matches(
                parameter, replacement->cxx_template)) {
            rcc_error((SourceLoc){"<template-template-argument>", 0, 0},
                      "template-template argument does not match its "
                      "parameter list");
            return type_int;
        }
        if (type->is_const || type->is_volatile) {
            Type* qualified = ast_arena_alloc(sizeof(*qualified));
            *qualified = *replacement;
            qualified->is_const = qualified->is_const || type->is_const;
            qualified->is_volatile = qualified->is_volatile ||
                                     type->is_volatile;
            return qualified;
        }
        return replacement;
    }
    if (type->cxx_dependent_member_name && type->cxx_template &&
        type->cxx_template_arg_count > 0) {
        Type* owner_pattern = ast_arena_alloc(sizeof(*owner_pattern));
        Type* owner;
        Type* nested_type = NULL;
        bool alias_ambiguous = false;
        bool alias_accessible = false;
        CxxClass* access_context;
        bool nested_path_resolved;

        *owner_pattern = *type;
        owner_pattern->cxx_dependent_member_name = NULL;
        owner = template_substitute_type(
            tmpl, owner_pattern, args, arg_count, value_args, value_present);
        if (owner && owner->cxx_dependent) {
            Type* unresolved = ast_arena_alloc(sizeof(*unresolved));
            *unresolved = *type;
            unresolved->cxx_class = owner->cxx_class;
            return unresolved;
        }
        access_context = tmpl ? tmpl->active_class_instance : NULL;
        if (!access_context && tmpl && tmpl->func_def &&
            tmpl->func_def->func_method_owner) {
            access_context =
                tmpl->func_def->func_method_owner->cxx_class;
        }
        if (!access_context && tmpl) access_context = tmpl->templated_class;
        nested_path_resolved = owner && owner->cxx_class &&
            cxx_class_resolve_nested_type_path(
                owner->cxx_class, type->cxx_dependent_member_name,
                access_context, &nested_type, &alias_ambiguous,
                &alias_accessible);
        if (nested_path_resolved && nested_type) {
            return template_substitute_type(
                tmpl, nested_type, args, arg_count, value_args,
                value_present);
        }
        {
            Type* unresolved = ast_arena_alloc(sizeof(*unresolved));
            *unresolved = *type;
            unresolved->cxx_class = owner ? owner->cxx_class : type->cxx_class;
            unresolved->cxx_template = NULL;
            unresolved->cxx_template_args = NULL;
            unresolved->cxx_template_value_args = NULL;
            unresolved->cxx_template_arg_count = 0;
            unresolved->cxx_template_param_index = -1;
            return unresolved;
        }
    }
    if (tmpl && type->cxx_class) {
        if (tmpl->active_class_instance &&
            tmpl->templated_class == type->cxx_class) {
            replacement = tmpl->active_class_instance->type;
            if (type->is_const || type->is_volatile) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *replacement;
                qualified->is_const = qualified->is_const || type->is_const;
                qualified->is_volatile = qualified->is_volatile ||
                                          type->is_volatile;
                return qualified;
            }
            return replacement;
        }
        if (tmpl->local_class_pattern == type->cxx_class &&
            tmpl->local_class_instance) {
            replacement = tmpl->local_class_instance->type;
            if (type->is_const || type->is_volatile) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *replacement;
                qualified->is_const = qualified->is_const || type->is_const;
                qualified->is_volatile = qualified->is_volatile ||
                                          type->is_volatile;
                return qualified;
            }
            return replacement;
        }
        for (int local_index = 0;
             local_index < tmpl->local_class_count; ++local_index) {
            CxxLocalClassTemplate* local = &tmpl->local_classes[local_index];
            CxxClass* instance;
            Type** saved_pack_args;
            int64_t* saved_pack_values;
            bool* saved_pack_value_present;
            int saved_pack_count;
            if (!local->pattern || local->pattern != type->cxx_class ||
                !local->templ) {
                continue;
            }
            saved_pack_args = local->templ->pending_pack_args;
            saved_pack_values = local->templ->pending_pack_values;
            saved_pack_value_present =
                local->templ->pending_pack_value_present;
            saved_pack_count = local->templ->pending_pack_count;
            local->templ->pending_pack_args = tmpl->pending_pack_args;
            local->templ->pending_pack_values = tmpl->pending_pack_values;
            local->templ->pending_pack_value_present =
                tmpl->pending_pack_value_present;
            local->templ->pending_pack_count = tmpl->pending_pack_count;
            instance = rcc_cxx_instantiate_class_template(
                local->templ, args, value_args, value_present, arg_count,
                (SourceLoc){"<local-class-template>", 0, 0});
            local->templ->pending_pack_args = saved_pack_args;
            local->templ->pending_pack_values = saved_pack_values;
            local->templ->pending_pack_value_present =
                saved_pack_value_present;
            local->templ->pending_pack_count = saved_pack_count;
            if (!instance) return NULL;
            replacement = instance->type;
            if (type->is_const || type->is_volatile) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *replacement;
                qualified->is_const = qualified->is_const || type->is_const;
                qualified->is_volatile = qualified->is_volatile ||
                                          type->is_volatile;
                return qualified;
            }
            return replacement;
        }
    }
    if (type->cxx_dependent && type->cxx_dependent_member_name &&
        type->cxx_template_param_index >= 0 &&
        type->cxx_template_param_index < arg_count &&
        args[type->cxx_template_param_index]) {
        Type* owner = args[type->cxx_template_param_index];
        bool alias_ambiguous = false;
        bool alias_accessible = false;
        if (owner->cxx_dependent) {
            Type* unresolved = ast_arena_alloc(sizeof(*unresolved));
            *unresolved = *type;
            unresolved->cxx_class = owner->cxx_class;
            /* Keep the nested name dependent when substitution itself still
             * yields a dependent owner.  A concrete missing/private alias is
             * distinguished by the cleared parameter index below. */
            if (owner->cxx_template_param_index >= 0) {
                unresolved->cxx_template_param_index =
                    owner->cxx_template_param_index;
            }
            return unresolved;
        }
        CxxClass* access_context = tmpl ? tmpl->active_class_instance : NULL;
        if (!access_context && tmpl && tmpl->func_def &&
            tmpl->func_def->func_method_owner) {
            access_context =
                tmpl->func_def->func_method_owner->cxx_class;
        }
        if (!access_context && tmpl) access_context = tmpl->templated_class;
        Type* nested_type = NULL;
        bool nested_path_resolved = owner->cxx_class &&
            cxx_class_resolve_nested_type_path(
                owner->cxx_class, type->cxx_dependent_member_name,
                access_context, &nested_type, &alias_ambiguous,
                &alias_accessible);
        if (nested_path_resolved && nested_type) {
            return template_substitute_type(
                tmpl, nested_type, args, arg_count, value_args,
                value_present);
        }
        {
            Type* unresolved = ast_arena_alloc(sizeof(*unresolved));
            *unresolved = *type;
            unresolved->cxx_class = owner->cxx_class;
            unresolved->cxx_template_param_index = -1;
            return unresolved;
        }
    }
    index = template_type_parameter_index(tmpl, type);
    if (index >= 0 && index < arg_count && args[index]) {
        replacement = args[index];
        if (replacement != type && replacement->cxx_dependent) {
            Type* resolved = template_substitute_type(
                tmpl, replacement, args, arg_count, value_args,
                value_present);
            if (resolved) replacement = resolved;
        }
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

    /* Function-template parameters can occur inside an already-instantiated
     * class-template-id. Substitute the primary-template identity arguments
     * before the overload resolver checks the resulting function type. */
    if ((type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        type->cxx_class && type->cxx_class->template_identity_tmpl &&
        type->cxx_class->template_identity_arg_count > 0) {
        CxxClass* class_instance = type->cxx_class;
        CxxTemplate* identity_template =
            class_instance->template_identity_tmpl;
        int identity_argument_count =
            class_instance->template_identity_arg_count;
        int identity_pack_index = -1;
        bool identity_pack_supported = true;
        for (int parameter_index = 0;
             parameter_index < identity_template->param_count;
             ++parameter_index) {
            TemplateParam* parameter =
                &identity_template->params[parameter_index];
            if (!parameter->is_pack) continue;
            if (identity_pack_index >= 0 ||
                parameter_index != identity_template->param_count - 1 ||
                parameter->kind != TPARAM_TYPE) {
                identity_pack_supported = false;
                break;
            }
            identity_pack_index = parameter_index;
        }
        bool type_only_identity = identity_pack_supported &&
            (identity_pack_index >= 0
                 ? identity_argument_count >= identity_pack_index
                 : identity_argument_count == identity_template->param_count);
        bool identity_changed = false;
        Type** identity_arguments = ast_arena_alloc(
            sizeof(*identity_arguments) * (size_t)identity_argument_count);
        int64_t* identity_values = ast_arena_alloc(
            sizeof(*identity_values) * (size_t)identity_argument_count);
        bool* identity_value_present = ast_arena_alloc(
            sizeof(*identity_value_present) * (size_t)identity_argument_count);
        memset(identity_arguments, 0,
               sizeof(*identity_arguments) * (size_t)identity_argument_count);
        memset(identity_values, 0,
               sizeof(*identity_values) * (size_t)identity_argument_count);
        memset(identity_value_present, 0,
               sizeof(*identity_value_present) * (size_t)identity_argument_count);
        for (int parameter_index = 0; type_only_identity &&
             parameter_index < identity_template->param_count;
             ++parameter_index) {
            if (identity_template->params[parameter_index].kind !=
                TPARAM_TYPE) {
                type_only_identity = false;
            }
        }
        for (int argument_index = 0; type_only_identity &&
             argument_index < identity_argument_count; ++argument_index) {
            int parameter_index = identity_pack_index >= 0 &&
                    argument_index >= identity_pack_index
                ? identity_pack_index : argument_index;
            if (parameter_index >= identity_template->param_count ||
                identity_template->params[parameter_index].kind !=
                    TPARAM_TYPE ||
                !class_instance->template_identity_args ||
                !class_instance->template_identity_args[argument_index]) {
                type_only_identity = false;
                break;
            }
            identity_arguments[argument_index] = template_substitute_type(
                tmpl,
                class_instance->template_identity_args[argument_index],
                args, arg_count, value_args, value_present);
            if (!identity_arguments[argument_index]) return NULL;
            identity_changed = identity_changed ||
                identity_arguments[argument_index] !=
                    class_instance->template_identity_args[argument_index];
        }
        if (type_only_identity && identity_changed) {
            bool identity_still_dependent = false;
            for (int argument_index = 0;
                 argument_index < identity_argument_count;
                 ++argument_index) {
                if (identity_arguments[argument_index]->cxx_dependent) {
                    identity_still_dependent = true;
                    break;
                }
            }
            if (identity_still_dependent) {
                Type* dependent_type = ast_arena_alloc(sizeof(*dependent_type));
                *dependent_type = *type;
                dependent_type->cxx_class = NULL;
                dependent_type->cxx_dependent = true;
                dependent_type->cxx_template = identity_template;
                dependent_type->cxx_template_param_index = -1;
                dependent_type->cxx_template_args = ast_arena_alloc(
                    sizeof(Type*) * (size_t)identity_argument_count);
                memcpy(dependent_type->cxx_template_args, identity_arguments,
                       sizeof(Type*) * (size_t)identity_argument_count);
                dependent_type->cxx_template_arg_count =
                    identity_argument_count;
                return dependent_type;
            }
            CxxClass* substituted_class = rcc_cxx_instantiate_class_template(
                identity_template, identity_arguments, identity_values,
                identity_value_present, identity_argument_count,
                (SourceLoc){"<template>", 0, 0});
            if (!substituted_class || !substituted_class->type) return NULL;
            replacement = substituted_class->type;
            if (type->is_const || type->is_volatile || type->is_atomic ||
                type->is_restrict) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *replacement;
                qualified->is_const = type->is_const;
                qualified->is_volatile = type->is_volatile;
                qualified->is_atomic = type->is_atomic;
                qualified->is_restrict = type->is_restrict;
                return qualified;
            }
            return replacement;
        }
    }

    /* A class-template specialization appearing in a function-template body
     * is incomplete until the outer template arguments are substituted.  Keep
     * the template and its argument patterns on the dependent carrier so the
     * real class instance, including its field layout, is materialized here. */
    if (type->cxx_dependent && type->cxx_template &&
        type->cxx_template_arg_count > 0) {
        Type* nested_arguments[32] = { NULL };
        int64_t nested_values[32] = { 0 };
        bool nested_value_present[32] = { false };
        Expr* nested_value_expressions[32] = { NULL };
        bool nested_pack_expansions[32] = { false };
        int nested_count = 0;
        int pack_index = -1;
        bool still_dependent = false;
        CxxClass* instantiated;
        if (type->cxx_template_arg_count >
            (int)(sizeof(nested_arguments) / sizeof(nested_arguments[0]))) {
            rcc_error((SourceLoc){"<template>", 0, 0},
                      "dependent class template argument limit exceeded");
            return NULL;
        }
        for (int parameter_index = 0;
             parameter_index < type->cxx_template->param_count;
             ++parameter_index) {
            if (!type->cxx_template->params[parameter_index].is_pack) continue;
            if (pack_index >= 0 ||
                parameter_index != type->cxx_template->param_count - 1) {
                pack_index = -2;
                break;
            }
            pack_index = parameter_index;
        }
        for (int nested_index = 0;
             nested_index < type->cxx_template_arg_count; ++nested_index) {
            int parameter_index = pack_index >= 0 &&
                    nested_index >= pack_index
                ? pack_index : nested_index;
            TemplateParam* parameter = parameter_index >= 0 &&
                    parameter_index < type->cxx_template->param_count
                ? &type->cxx_template->params[parameter_index] : NULL;
            bool is_pack_expansion =
                type->cxx_template_arg_pack_expansions &&
                type->cxx_template_arg_pack_expansions[nested_index];
            if (is_pack_expansion) {
                Type* pattern_argument = type->cxx_template_args
                    ? type->cxx_template_args[nested_index] : NULL;
                int expansion_pack_index =
                    template_parameter_pack_index_nested(tmpl,
                                                         pattern_argument);
                if (pack_index < 0 || nested_index < pack_index ||
                    nested_index != type->cxx_template_arg_count - 1 ||
                    !parameter || parameter->kind != TPARAM_TYPE ||
                    !parameter->is_pack || !tmpl ||
                    expansion_pack_index < 0 ||
                    expansion_pack_index >= tmpl->param_count ||
                    !tmpl->params[expansion_pack_index].is_pack) {
                    rcc_error((SourceLoc){"<template>", 0, 0},
                              "dependent class-template pack expansion has "
                              "an invalid parameter-pack pattern");
                    return NULL;
                }
                bool value_pack = tmpl->params[expansion_pack_index].kind ==
                    TPARAM_NONTYPE;
                if (tmpl->params[expansion_pack_index].kind != TPARAM_TYPE &&
                    !value_pack) {
                    rcc_error((SourceLoc){"<template>", 0, 0},
                              "dependent class-template pack expansion has "
                              "an unsupported parameter kind");
                    return NULL;
                }
                if (tmpl->pending_pack_count < 0) {
                    if (nested_count >=
                        (int)(sizeof(nested_arguments) /
                              sizeof(nested_arguments[0]))) {
                        rcc_error((SourceLoc){"<template>", 0, 0},
                                  "dependent class template argument limit "
                                  "exceeded");
                        return NULL;
                    }
                    nested_arguments[nested_count] = template_substitute_type(
                        tmpl, pattern_argument, args, arg_count,
                        value_args, value_present);
                    if (!nested_arguments[nested_count]) return NULL;
                    nested_pack_expansions[nested_count] = true;
                    still_dependent = true;
                    ++nested_count;
                    continue;
                }
                if (tmpl->pending_pack_count > 0 &&
                    (value_pack
                         ? (!tmpl->pending_pack_values ||
                            !tmpl->pending_pack_value_present)
                         : !tmpl->pending_pack_args)) {
                    rcc_error((SourceLoc){"<template>", 0, 0},
                              value_pack
                                  ? "class-template pack expansion is missing "
                                    "its deduced non-type arguments"
                                  : "class-template pack expansion is missing "
                                    "its deduced type arguments");
                    return NULL;
                }
                for (int pack_value = 0;
                     pack_value < tmpl->pending_pack_count; ++pack_value) {
                    Type* expanded_arguments[32] = { NULL };
                    int64_t expanded_values[32] = { 0 };
                    bool expanded_value_present[32] = { false };
                    if (nested_count >=
                        (int)(sizeof(nested_arguments) /
                              sizeof(nested_arguments[0])) ||
                        arg_count < 0 ||
                        (arg_count > 0 && !args) ||
                        arg_count > (int)(sizeof(expanded_arguments) /
                                          sizeof(expanded_arguments[0]))) {
                        rcc_error((SourceLoc){"<template>", 0, 0},
                                  "expanded class template argument limit "
                                  "exceeded");
                        return NULL;
                    }
                    if (arg_count > 0) {
                        memcpy(expanded_arguments, args,
                               sizeof(Type*) * (size_t)arg_count);
                        if (value_args && value_present) {
                            memcpy(expanded_values, value_args,
                                   sizeof(int64_t) * (size_t)arg_count);
                            memcpy(expanded_value_present, value_present,
                                   sizeof(bool) * (size_t)arg_count);
                        }
                    }
                    if (value_pack) {
                        expanded_values[expansion_pack_index] =
                            tmpl->pending_pack_values[pack_value];
                        expanded_value_present[expansion_pack_index] =
                            tmpl->pending_pack_value_present[pack_value];
                    } else {
                        expanded_arguments[expansion_pack_index] =
                            tmpl->pending_pack_args[pack_value];
                    }
                    nested_arguments[nested_count] = template_substitute_type(
                        tmpl, pattern_argument, expanded_arguments, arg_count,
                        expanded_values, expanded_value_present);
                    if (!nested_arguments[nested_count]) return NULL;
                    still_dependent = still_dependent ||
                        nested_arguments[nested_count]->cxx_dependent;
                    ++nested_count;
                }
                continue;
            }
            if (nested_count >=
                (int)(sizeof(nested_arguments) /
                      sizeof(nested_arguments[0]))) {
                rcc_error((SourceLoc){"<template>", 0, 0},
                          "dependent class template argument limit exceeded");
                return NULL;
            }
            if (parameter && parameter->kind == TPARAM_NONTYPE) {
                Expr* value_expression = type->cxx_template_value_args
                    ? type->cxx_template_value_args[nested_index] : NULL;
                Expr* substituted_expression;
                if (!value_expression) {
                    rcc_error((SourceLoc){"<template>", 0, 0},
                              "dependent class-template non-type argument "
                              "is missing");
                    return NULL;
                }
                substituted_expression = template_clone_expr(
                    tmpl, value_expression, args, arg_count, value_args,
                    value_present);
                if (!substituted_expression ||
                    !rcc_parser_cxx_eval_template_integer_expression(
                        substituted_expression, tmpl, value_args,
                        value_present, &nested_values[nested_count])) {
                    rcc_error(value_expression->loc,
                              "dependent class-template non-type argument "
                              "must be an integer constant expression");
                    return NULL;
                }
                nested_value_expressions[nested_count] = value_expression;
                nested_arguments[nested_count] = parameter->type
                    ? parameter->type : type_int;
                nested_value_present[nested_count] = true;
            } else {
                if (!parameter) {
                    rcc_error((SourceLoc){"<template>", 0, 0},
                              "dependent class template argument count does "
                              "not match its parameter list");
                    return NULL;
                }
                nested_arguments[nested_count] = template_substitute_type(
                    tmpl, type->cxx_template_args[nested_index], args,
                    arg_count, value_args, value_present);
                if (!nested_arguments[nested_count]) return NULL;
                still_dependent = still_dependent ||
                    nested_arguments[nested_count]->cxx_dependent;
            }
            ++nested_count;
        }
        if (still_dependent) {
            Type* dependent_type = ast_arena_alloc(sizeof(*dependent_type));
            *dependent_type = *type;
            dependent_type->cxx_class = NULL;
            dependent_type->cxx_dependent = true;
            dependent_type->cxx_template_args = nested_count > 0
                ? ast_arena_alloc(sizeof(Type*) * (size_t)nested_count) : NULL;
            dependent_type->cxx_template_value_args = nested_count > 0
                ? ast_arena_alloc(sizeof(Expr*) * (size_t)nested_count) : NULL;
            dependent_type->cxx_template_arg_pack_expansions =
                type->cxx_template_arg_pack_expansions && nested_count > 0
                    ? ast_arena_alloc(sizeof(bool) * (size_t)nested_count)
                    : NULL;
            if (nested_count > 0) {
                memcpy(dependent_type->cxx_template_args, nested_arguments,
                       sizeof(Type*) * (size_t)nested_count);
                memcpy(dependent_type->cxx_template_value_args,
                       nested_value_expressions,
                       sizeof(Expr*) * (size_t)nested_count);
                if (dependent_type->cxx_template_arg_pack_expansions) {
                    memcpy(dependent_type->cxx_template_arg_pack_expansions,
                           nested_pack_expansions,
                           sizeof(bool) * (size_t)nested_count);
                }
            }
            dependent_type->cxx_template_arg_count = nested_count;
            return dependent_type;
        }
        instantiated = rcc_cxx_instantiate_class_template(
            type->cxx_template, nested_arguments, nested_values,
            nested_value_present, nested_count,
            (SourceLoc){"<template>", 0, 0});
        return instantiated ? instantiated->type : NULL;
    }
    if (type->cxx_dependent && type->cxx_template_param_index >= 0 &&
        type->cxx_template_arg_count > 0) {
        Type* template_argument;
        CxxTemplate* actual_template;
        TemplateParam* outer_parameter = NULL;
        Type* nested_arguments[32] = { NULL };
        int64_t nested_values[32] = { 0 };
        bool nested_value_present[32] = { false };
        int nested_count = type->cxx_template_arg_count;
        if (nested_count > (int)(sizeof(nested_arguments) /
                                 sizeof(nested_arguments[0])) ||
            type->cxx_template_param_index >= arg_count || !args ||
            !(template_argument = args[type->cxx_template_param_index]) ||
            !(actual_template = template_argument->cxx_template) ||
            !tmpl || type->cxx_template_param_index >= tmpl->param_count ||
            (outer_parameter = &tmpl->params[type->cxx_template_param_index])->kind !=
                TPARAM_TEMPLATE ||
            !outer_parameter->template_signature ||
            outer_parameter->template_signature->param_count != nested_count ||
            actual_template->kind != TMPL_CLASS ||
            actual_template->param_count != nested_count) {
            rcc_error((SourceLoc){"<template>", 0, 0},
                      "template-template argument cannot be instantiated");
            return NULL;
        }
        for (int nested_index = 0; nested_index < nested_count;
             ++nested_index) {
            TemplateParam* signature_parameter =
                &outer_parameter->template_signature->params[nested_index];
            TemplateParam* actual_parameter =
                &actual_template->params[nested_index];
            if (signature_parameter->kind != actual_parameter->kind ||
                signature_parameter->is_pack != actual_parameter->is_pack) {
                rcc_error((SourceLoc){"<template>", 0, 0},
                          "template-template argument does not match its "
                          "parameter list");
                return NULL;
            }
            if (signature_parameter->kind == TPARAM_NONTYPE &&
                (!signature_parameter->type || !actual_parameter->type ||
                 !type_is_compatible(signature_parameter->type,
                                     actual_parameter->type))) {
                rcc_error((SourceLoc){"<template>", 0, 0},
                          "template-template argument does not match its "
                          "parameter list");
                return NULL;
            }
            if (actual_parameter->kind == TPARAM_NONTYPE) {
                Expr* value_expression = type->cxx_template_value_args
                    ? type->cxx_template_value_args[nested_index] : NULL;
                Expr* substituted_expression;
                if (!value_expression) {
                    rcc_error((SourceLoc){"<template>", 0, 0},
                              "dependent template-template non-type argument "
                              "is missing");
                    return NULL;
                }
                substituted_expression = template_clone_expr(
                    tmpl, value_expression, args, arg_count, value_args,
                    value_present);
                if (!substituted_expression ||
                    !expr_eval_integer_constant(substituted_expression,
                                                 &nested_values[nested_index])) {
                    rcc_error(value_expression->loc,
                              "dependent template-template non-type argument "
                              "must be an integer constant expression");
                    return NULL;
                }
                nested_arguments[nested_index] = actual_parameter->type
                    ? actual_parameter->type : type_int;
                nested_value_present[nested_index] = true;
            } else {
                nested_arguments[nested_index] = template_substitute_type(
                    tmpl, type->cxx_template_args[nested_index], args,
                    arg_count, value_args, value_present);
                if (!nested_arguments[nested_index]) return NULL;
            }
        }
        {
            CxxClass* instantiated = rcc_cxx_instantiate_class_template(
                actual_template, nested_arguments, nested_values,
                nested_value_present, nested_count,
                (SourceLoc){"<template>", 0, 0});
            return instantiated ? instantiated->type : NULL;
        }
    }

    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) {
        base = template_substitute_type(
            tmpl, type->base, args, arg_count, value_args, value_present);
        member_pointer_owner = type->cxx_is_member_pointer
            ? template_substitute_type(
                  tmpl, type->cxx_member_pointer_owner, args, arg_count,
                  value_args, value_present)
            : type->cxx_member_pointer_owner;
        /* Reference collapsing is part of substitution, not a later ABI
         * repair.  A forwarding-reference pattern such as `T&&` deduces T as
         * `U&` for an lvalue argument, so substituting the placeholder must
         * produce U& rather than the impossible nested `U& &&` carrier. */
        if (type->kind == TYPE_PTR && type->is_reference && base &&
            base->kind == TYPE_PTR && base->is_reference) {
            return base;
        }
        array_len = type->array_len;
        array_bound = type->array_bound;
        if (type->kind == TYPE_ARRAY && value_args && value_present &&
            type->array_bound) {
            Expr* substituted_bound = template_clone_expr(
                tmpl, type->array_bound, args, arg_count, value_args,
                value_present);
            int64_t value;
            if (substituted_bound &&
                expr_eval_integer_constant(substituted_bound, &value)) {
                if (value <= 0 || value > INT_MAX) {
                    rcc_error(type->array_bound->loc,
                              "non-type template array bound is out of range");
                    return NULL;
                }
                array_len = (int)value;
                array_bound = NULL;
            } else if (substituted_bound) {
                array_bound = substituted_bound;
            }
        }
        if (base != type->base ||
            member_pointer_owner != type->cxx_member_pointer_owner ||
            array_len != type->array_len ||
            array_bound != type->array_bound) {
            Type* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *type;
            copy->base = base;
            copy->cxx_member_pointer_owner = member_pointer_owner;
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
    } else if (type->kind == TYPE_FUNC) {
        Type* return_type = template_substitute_type(
            tmpl, type->ret_type, args, arg_count, value_args,
            value_present);
        TypeParam* parameters = NULL;
        TypeParam** tail = &parameters;
        bool changed = return_type != type->ret_type;
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            TypeParam* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *parameter;
            copy->type = template_substitute_type(
                tmpl, parameter->type, args, arg_count, value_args,
                value_present);
            copy->next = NULL;
            if (copy->type != parameter->type) changed = true;
            *tail = copy;
            tail = &copy->next;
        }
        if (changed) {
            Type* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *type;
            copy->ret_type = return_type;
            copy->params = parameters;
            return copy;
        }
    }
    return type;
}

static Type* template_substitute_dependent_alias_template_type(
    CxxTemplate* tmpl, Type* type, Type** args, int arg_count,
    const int64_t* value_args, const bool* value_present) {
    SourceLoc loc = {"<dependent-alias-template>", 0, 0};
    int owner_index = type ? type->cxx_template_param_index : -1;
    Type* owner;
    CxxClass* owner_class;
    CxxClass* declaring_class = NULL;
    CxxClass* access_context;
    CxxClassAliasTemplate* alias_entry;
    CxxTemplate* alias_template;
    CxxTemplate* owner_template;
    Type** alias_arguments;
    int64_t* alias_values;
    bool* alias_value_present;
    int owner_argument_count = 0;
    int total_argument_count;
    int explicit_argument_count;
    size_t slot_count;
    bool ambiguous = false;
    bool accessible = false;
    bool unresolved_template_argument = false;

    if (!type || !type->cxx_dependent_alias_template || !args) {
        return type;
    }
    if (type->cxx_dependent_alias_owner) {
        owner = template_substitute_type(
            tmpl, type->cxx_dependent_alias_owner, args, arg_count,
            value_args, value_present);
    } else {
        if (owner_index < 0 || owner_index >= arg_count ||
            !args[owner_index]) {
            return type;
        }
        owner = args[owner_index];
    }
    if (!owner) return NULL;
    if (owner->cxx_dependent) {
        Type* unresolved = ast_arena_alloc(sizeof(*unresolved));
        *unresolved = *type;
        unresolved->cxx_dependent_alias_owner = owner;
        unresolved->cxx_class = owner->cxx_class;
        if (owner->cxx_template_param_index >= 0) {
            unresolved->cxx_template_param_index =
                owner->cxx_template_param_index;
        }
        if (type->cxx_template_arg_count > 0 && type->cxx_template_args) {
            int count = type->cxx_template_arg_count;
            unresolved->cxx_template_args = ast_arena_alloc(
                sizeof(*unresolved->cxx_template_args) * (size_t)count);
            unresolved->cxx_template_value_args = ast_arena_alloc(
                sizeof(*unresolved->cxx_template_value_args) *
                (size_t)count);
            for (int index = 0; index < count; ++index) {
                Expr* expression = type->cxx_template_value_args
                    ? type->cxx_template_value_args[index] : NULL;
                unresolved->cxx_template_args[index] = expression
                    ? type->cxx_template_args[index]
                    : template_substitute_type(
                          tmpl, type->cxx_template_args[index], args,
                          arg_count, value_args, value_present);
                unresolved->cxx_template_value_args[index] = expression
                    ? template_clone_expr(tmpl, expression, args, arg_count,
                                          value_args, value_present)
                    : NULL;
            }
        }
        return unresolved;
    }

    owner_class = owner->cxx_class;
    if (!owner_class) {
        rcc_error(loc,
                  "dependent nested alias template '%s' requires a class "
                  "owner after substitution",
                  type->cxx_dependent_member_name
                      ? type->cxx_dependent_member_name : "<unnamed>");
        return type_int;
    }
    access_context = tmpl ? tmpl->active_class_instance : NULL;
    if (!access_context && tmpl && tmpl->func_def &&
        tmpl->func_def->func_method_owner) {
        access_context = tmpl->func_def->func_method_owner->cxx_class;
    }
    if (!access_context && tmpl) access_context = tmpl->templated_class;
    alias_entry = cxx_class_find_inherited_alias_template(
        owner_class, type->cxx_dependent_member_name, access_context,
        &declaring_class, &ambiguous, &accessible);
    if (!alias_entry || !alias_entry->declaration || ambiguous) {
        rcc_error(loc,
                  ambiguous
                      ? "nested alias template '%s' is ambiguous in class '%s'"
                      : "class '%s' has no nested alias template '%s'",
                  ambiguous
                      ? (type->cxx_dependent_member_name
                             ? type->cxx_dependent_member_name : "<unnamed>")
                      : (owner_class->name ? owner_class->name : "<unnamed>"),
                  ambiguous
                      ? (owner_class->name ? owner_class->name : "<unnamed>")
                      : (type->cxx_dependent_member_name
                             ? type->cxx_dependent_member_name : "<unnamed>"));
        return type_int;
    }
    if (!accessible) {
        rcc_error(loc, "nested alias template '%s' is inaccessible in class '%s'",
                  type->cxx_dependent_member_name
                      ? type->cxx_dependent_member_name : "<unnamed>",
                  owner_class->name ? owner_class->name : "<unnamed>");
        return type_int;
    }
    alias_template = alias_entry->declaration;
    if (alias_template->param_count < 0 ||
        type->cxx_template_arg_count < 0 ||
        type->cxx_template_arg_count > alias_template->param_count) {
        rcc_error(loc, "nested alias template '%s' has an invalid argument count",
                  alias_template->name ? alias_template->name : "<alias>");
        return type_int;
    }
    explicit_argument_count = type->cxx_template_arg_count;
    owner_template = declaring_class ? declaring_class->templ
                                     : owner_class->templ;
    if (alias_template->enclosing_template &&
        owner_template == alias_template->enclosing_template) {
        CxxClass* alias_owner = declaring_class ? declaring_class : owner_class;
        owner_argument_count = alias_owner->template_arg_count;
        if (owner_argument_count != owner_template->param_count ||
            (owner_argument_count > 0 && !alias_owner->template_args)) {
            rcc_error(loc, "nested alias-template owner arguments are incomplete");
            return type_int;
        }
    } else if (alias_template->enclosing_template) {
        rcc_error(loc,
                  "nested alias template '%s' is not attached to its owner "
                  "specialization",
                  alias_template->name ? alias_template->name : "<alias>");
        return type_int;
    }
    if (owner_argument_count > INT_MAX - alias_template->param_count) {
        rcc_error(loc, "nested alias template argument count overflow");
        return type_int;
    }
    total_argument_count = alias_template->param_count + owner_argument_count;
    slot_count = total_argument_count > 0 ? (size_t)total_argument_count : 1u;
    if (slot_count > SIZE_MAX / sizeof(*alias_arguments) ||
        slot_count > SIZE_MAX / sizeof(*alias_values) ||
        slot_count > SIZE_MAX / sizeof(*alias_value_present)) {
        rcc_error(loc, "nested alias template argument storage size overflow");
        return type_int;
    }
    alias_arguments = ast_arena_alloc(sizeof(*alias_arguments) * slot_count);
    alias_values = ast_arena_alloc(sizeof(*alias_values) * slot_count);
    alias_value_present =
        ast_arena_alloc(sizeof(*alias_value_present) * slot_count);
    memset(alias_arguments, 0, sizeof(*alias_arguments) * slot_count);
    memset(alias_values, 0, sizeof(*alias_values) * slot_count);
    memset(alias_value_present, 0,
           sizeof(*alias_value_present) * slot_count);
    if (owner_argument_count > 0) {
        CxxClass* alias_owner = declaring_class ? declaring_class : owner_class;
        memcpy(alias_arguments + alias_template->param_count,
               alias_owner->template_args,
               sizeof(*alias_arguments) * (size_t)owner_argument_count);
        if (alias_owner->template_value_args &&
            alias_owner->template_value_present) {
            memcpy(alias_values + alias_template->param_count,
                   alias_owner->template_value_args,
                   sizeof(*alias_values) * (size_t)owner_argument_count);
            memcpy(alias_value_present + alias_template->param_count,
                   alias_owner->template_value_present,
                   sizeof(*alias_value_present) *
                       (size_t)owner_argument_count);
        }
    }

    for (int index = 0; index < alias_template->param_count; ++index) {
        TemplateParam* parameter = &alias_template->params[index];
        Expr* source_value = type->cxx_template_value_args &&
                index < explicit_argument_count
            ? type->cxx_template_value_args[index] : NULL;
        if (index < explicit_argument_count) {
            if (!type->cxx_template_args) {
                rcc_error(loc, "nested alias template arguments are missing");
                return type_int;
            }
            if (parameter->kind == TPARAM_TYPE) {
                if (source_value) {
                    rcc_error(loc,
                              "nested alias-template argument %d must be a type",
                              index + 1);
                    return type_int;
                }
                alias_arguments[index] = template_substitute_type(
                    tmpl, type->cxx_template_args[index], args, arg_count,
                    value_args, value_present);
                if (!alias_arguments[index]) return NULL;
            } else if (parameter->kind == TPARAM_NONTYPE) {
                Expr* substituted_value;
                int64_t value;
                if (!source_value) {
                    rcc_error(loc,
                              "nested alias-template argument %d must be a "
                              "non-type value",
                              index + 1);
                    return type_int;
                }
                substituted_value = template_clone_expr(
                    tmpl, source_value, args, arg_count, value_args,
                    value_present);
                if (!substituted_value ||
                    (!expr_eval_integer_constant(substituted_value, &value) &&
                     !rcc_parser_cxx_eval_template_integer_expression(
                         substituted_value, tmpl, value_args, value_present,
                         &value))) {
                    rcc_error(source_value->loc,
                              "nested alias-template non-type argument must "
                              "be a constant expression after substitution");
                    return type_int;
                }
                alias_values[index] = value;
                alias_value_present[index] = true;
                alias_arguments[index] = template_substitute_type(
                    alias_template, parameter->type, alias_arguments,
                    total_argument_count, alias_values,
                    alias_value_present);
            } else {
                Type* template_argument;
                if (source_value) {
                    rcc_error(loc,
                              "nested alias-template template argument must "
                              "name a class template");
                    return type_int;
                }
                template_argument = template_substitute_type(
                    tmpl, type->cxx_template_args[index], args, arg_count,
                    value_args, value_present);
                if (!template_argument) return NULL;
                if (template_argument->cxx_dependent &&
                    !template_argument->cxx_template) {
                    alias_arguments[index] = template_argument;
                    unresolved_template_argument = true;
                    continue;
                }
                if (!template_argument->cxx_template ||
                    !rcc_parser_cxx_template_template_argument_matches(
                        parameter, template_argument->cxx_template)) {
                    rcc_error(loc,
                              "nested alias-template template argument does "
                              "not match its parameter list");
                    return type_int;
                }
                alias_arguments[index] = template_argument;
            }
        } else if (parameter->has_default &&
                   parameter->kind == TPARAM_TYPE &&
                   parameter->default_type) {
            alias_arguments[index] = template_substitute_type(
                alias_template, parameter->default_type, alias_arguments,
                total_argument_count, alias_values, alias_value_present);
        } else if (parameter->has_default &&
                   parameter->kind == TPARAM_NONTYPE &&
                   parameter->default_value) {
            Expr* default_value = template_clone_expr(
                alias_template, parameter->default_value, alias_arguments,
                total_argument_count, alias_values, alias_value_present);
            int64_t value;
            if (!default_value ||
                (!expr_eval_integer_constant(default_value, &value) &&
                 !rcc_parser_cxx_eval_template_integer_expression(
                     default_value, alias_template, alias_values,
                     alias_value_present, &value))) {
                rcc_error(loc,
                          "nested alias-template non-type default is not a "
                          "constant expression");
                return type_int;
            }
            alias_values[index] = value;
            alias_value_present[index] = true;
            alias_arguments[index] = template_substitute_type(
                alias_template, parameter->type, alias_arguments,
                total_argument_count, alias_values, alias_value_present);
        } else if (parameter->has_default &&
                   parameter->kind == TPARAM_TEMPLATE &&
                   parameter->default_type) {
            Type* template_argument = template_substitute_type(
                alias_template, parameter->default_type, alias_arguments,
                total_argument_count, alias_values, alias_value_present);
            if (!template_argument) return NULL;
            if (template_argument->cxx_dependent &&
                !template_argument->cxx_template) {
                alias_arguments[index] = template_argument;
                unresolved_template_argument = true;
                continue;
            }
            if (!template_argument->cxx_template ||
                !rcc_parser_cxx_template_template_argument_matches(
                    parameter, template_argument->cxx_template)) {
                rcc_error(loc,
                          "nested alias-template template default does not "
                          "match its parameter list");
                return type_int;
            }
            alias_arguments[index] = template_argument;
        } else {
            rcc_error(loc, "nested alias template '%s' expects %d argument(s), got %d",
                      alias_template->name ? alias_template->name : "<alias>",
                      alias_template->param_count, explicit_argument_count);
            return type_int;
        }
    }
    if (unresolved_template_argument) {
        Type* unresolved = ast_arena_alloc(sizeof(*unresolved));
        *unresolved = *type;
        unresolved->cxx_dependent_alias_owner = owner;
        if (explicit_argument_count > 0 && type->cxx_template_args) {
            unresolved->cxx_template_args = ast_arena_alloc(
                sizeof(*unresolved->cxx_template_args) *
                (size_t)explicit_argument_count);
            unresolved->cxx_template_value_args = ast_arena_alloc(
                sizeof(*unresolved->cxx_template_value_args) *
                (size_t)explicit_argument_count);
            for (int index = 0; index < explicit_argument_count; ++index) {
                Expr* expression = type->cxx_template_value_args
                    ? type->cxx_template_value_args[index] : NULL;
                unresolved->cxx_template_args[index] = expression
                    ? type->cxx_template_args[index]
                    : template_substitute_type(
                          tmpl, type->cxx_template_args[index], args,
                          arg_count, value_args, value_present);
                unresolved->cxx_template_value_args[index] = expression
                    ? template_clone_expr(tmpl, expression, args, arg_count,
                                          value_args, value_present)
                    : NULL;
            }
        }
        return unresolved;
    }
    if (!rcc_parser_cxx_template_constraint_satisfied(
            alias_template, alias_arguments, alias_values,
            alias_value_present, loc)) {
        return type_int;
    }
    return template_substitute_type(
        alias_template, alias_template->alias_type, alias_arguments,
        total_argument_count, alias_values, alias_value_present);
}

Type* rcc_cxx_template_substitute_type(
    CxxTemplate* tmpl, Type* type, Type** arguments, int argument_count,
    const int64_t* value_args, const bool* value_present) {
    return template_substitute_type(
        tmpl, type, arguments, argument_count, value_args, value_present);
}

static Expr* template_clone_expr(CxxTemplate* tmpl, Expr* expression,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present);
static Decl* template_clone_decl(CxxTemplate* tmpl, Decl* declaration,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present);

static bool template_type_contains_parameter_name(const Type* type,
                                                    const char* parameter_name,
                                                    int depth) {
    if (!type || depth > 64) return false;
    if (type->cxx_dependent && type->tag && parameter_name) {
        size_t parameter_length = strlen(parameter_name);
        if (strcmp(type->tag, parameter_name) == 0 ||
            (strncmp(type->tag, parameter_name, parameter_length) == 0 &&
             type->tag[parameter_length] == ':' &&
             type->tag[parameter_length + 1] == ':')) {
            return true;
        }
    }
    for (int index = 0; index < type->cxx_template_arg_count; ++index) {
        if (template_type_contains_parameter_name(
                type->cxx_template_args[index], parameter_name, depth + 1)) {
            return true;
        }
    }
    if (type->cxx_is_member_pointer &&
        template_type_contains_parameter_name(
            type->cxx_member_pointer_owner, parameter_name, depth + 1)) {
        return true;
    }
    switch (type->kind) {
        case TYPE_PTR:
        case TYPE_ARRAY:
        case TYPE_VECTOR:
            return template_type_contains_parameter_name(
                type->base, parameter_name, depth + 1);
        case TYPE_FUNC:
            if (template_type_contains_parameter_name(
                    type->ret_type, parameter_name, depth + 1)) {
                return true;
            }
            for (TypeParam* parameter = type->params; parameter;
                 parameter = parameter->next) {
                if (template_type_contains_parameter_name(
                        parameter->type, parameter_name, depth + 1)) {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

static bool template_expr_contains_type_parameter(Expr* expression,
                                                   const char* parameter_name,
                                                   int depth) {
    if (!expression || depth > 64) return false;
    if (template_type_contains_parameter_name(
            expression->type, parameter_name, depth + 1)) {
        return true;
    }
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
        case EXPR_NOEXCEPT:
            return template_expr_contains_type_parameter(
                expression->unary_operand, parameter_name, depth + 1);
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            return template_type_contains_parameter_name(
                       expression->sizeof_type, parameter_name, depth + 1) ||
                template_expr_contains_type_parameter(
                    expression->unary_operand, parameter_name, depth + 1);
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
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
            return template_expr_contains_type_parameter(
                       expression->binary_lhs, parameter_name, depth + 1) ||
                   template_expr_contains_type_parameter(
                       expression->binary_rhs, parameter_name, depth + 1);
        case EXPR_COND:
            return template_expr_contains_type_parameter(
                       expression->cond_test, parameter_name, depth + 1) ||
                   template_expr_contains_type_parameter(
                       expression->cond_then, parameter_name, depth + 1) ||
                   template_expr_contains_type_parameter(
                       expression->cond_else, parameter_name, depth + 1);
        case EXPR_CALL:
            if (template_expr_contains_type_parameter(
                    expression->call_func, parameter_name, depth + 1)) {
                return true;
            }
            for (ExprList* argument = expression->call_args; argument;
                 argument = argument->next) {
                if (template_expr_contains_type_parameter(
                        argument->expr, parameter_name, depth + 1)) {
                    return true;
                }
            }
            return false;
        case EXPR_INDEX:
            return template_expr_contains_type_parameter(
                       expression->index_base, parameter_name, depth + 1) ||
                   template_expr_contains_type_parameter(
                       expression->index_expr, parameter_name, depth + 1);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return template_expr_contains_type_parameter(
                expression->member_base, parameter_name, depth + 1);
        case EXPR_CAST:
            return template_type_contains_parameter_name(
                       expression->cast_type, parameter_name, depth + 1) ||
                   template_expr_contains_type_parameter(
                       expression->cast_expr, parameter_name, depth + 1);
        case EXPR_CXX_TYPEID:
            return template_type_contains_parameter_name(
                       expression->cxx_typeid_operand_type,
                       parameter_name, depth + 1) ||
                   template_expr_contains_type_parameter(
                       expression->cxx_typeid_operand, parameter_name,
                       depth + 1);
        case EXPR_CXX_FOLD:
            return template_expr_contains_type_parameter(
                       expression->cxx_fold_init, parameter_name,
                       depth + 1) ||
                   template_expr_contains_type_parameter(
                       expression->cxx_fold_pattern, parameter_name,
                       depth + 1);
        default:
            return false;
    }
}

static bool template_expr_contains_identifier(Expr* expression,
                                               const char* name) {
    if (!expression || !name) return false;
    switch (expression->kind) {
        case EXPR_IDENT:
            return expression->ident_name &&
                   strcmp(expression->ident_name, name) == 0;
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
        case EXPR_NOEXCEPT:
            return template_expr_contains_identifier(
                expression->unary_operand, name);
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
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
            return template_expr_contains_identifier(
                       expression->binary_lhs, name) ||
                   template_expr_contains_identifier(
                       expression->binary_rhs, name);
        case EXPR_COND:
            return template_expr_contains_identifier(
                       expression->cond_test, name) ||
                   template_expr_contains_identifier(
                       expression->cond_then, name) ||
                   template_expr_contains_identifier(
                       expression->cond_else, name);
        case EXPR_CALL:
            if (template_expr_contains_identifier(expression->call_func, name)) {
                return true;
            }
            for (ExprList* argument = expression->call_args; argument;
                 argument = argument->next) {
                if (template_expr_contains_identifier(argument->expr, name)) {
                    return true;
                }
            }
            return false;
        case EXPR_INDEX:
            return template_expr_contains_identifier(
                       expression->index_base, name) ||
                   template_expr_contains_identifier(
                       expression->index_expr, name);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return template_expr_contains_identifier(
                expression->member_base, name);
        case EXPR_CAST:
            return template_expr_contains_identifier(
                expression->cast_expr, name);
        case EXPR_COMPOUND:
            for (ExprList* item = expression->compound_init; item;
                 item = item->next) {
                if (template_expr_contains_identifier(item->expr, name)) {
                    return true;
                }
            }
            return false;
        case EXPR_GENERIC:
            if (template_expr_contains_identifier(
                    expression->generic_control, name)) {
                return true;
            }
            for (GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                if (template_expr_contains_identifier(association->expr, name)) {
                    return true;
                }
            }
            return false;
        case EXPR_CXX_FOLD:
            return template_expr_contains_identifier(
                expression->cxx_fold_init, name);
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            return template_expr_contains_identifier(
                       expression->va_list_operand, name) ||
                   template_expr_contains_identifier(
                       expression->va_second_operand, name);
        default:
            return false;
    }
}

static DeclList* template_pack_parameter_list(CxxTemplate* tmpl) {
    if (!tmpl) return NULL;
    if (tmpl->active_pack_parameters) return tmpl->active_pack_parameters;
    return tmpl->func_def ? tmpl->func_def->func_params : NULL;
}

static void template_replace_pack_identifier(Expr* expression,
                                              const char* pack_name,
                                              const char* argument_name) {
    if (!expression || !pack_name || !argument_name) return;
    switch (expression->kind) {
        case EXPR_IDENT:
            if (expression->ident_name &&
                strcmp(expression->ident_name, pack_name) == 0) {
                expression->ident_name = rcc_intern(argument_name);
                expression->ident_decl = NULL;
            }
            return;
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
        case EXPR_NOEXCEPT:
            template_replace_pack_identifier(
                expression->unary_operand, pack_name, argument_name);
            return;
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
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
            template_replace_pack_identifier(
                expression->binary_lhs, pack_name, argument_name);
            template_replace_pack_identifier(
                expression->binary_rhs, pack_name, argument_name);
            return;
        case EXPR_COND:
            template_replace_pack_identifier(
                expression->cond_test, pack_name, argument_name);
            template_replace_pack_identifier(
                expression->cond_then, pack_name, argument_name);
            template_replace_pack_identifier(
                expression->cond_else, pack_name, argument_name);
            return;
        case EXPR_CALL:
            template_replace_pack_identifier(
                expression->call_func, pack_name, argument_name);
            for (ExprList* argument = expression->call_args; argument;
                 argument = argument->next) {
                template_replace_pack_identifier(
                    argument->expr, pack_name, argument_name);
            }
            return;
        case EXPR_INDEX:
            template_replace_pack_identifier(
                expression->index_base, pack_name, argument_name);
            template_replace_pack_identifier(
                expression->index_expr, pack_name, argument_name);
            return;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            template_replace_pack_identifier(
                expression->member_base, pack_name, argument_name);
            return;
        case EXPR_CAST:
            template_replace_pack_identifier(
                expression->cast_expr, pack_name, argument_name);
            return;
        case EXPR_COMPOUND:
            for (ExprList* item = expression->compound_init; item;
                 item = item->next) {
                template_replace_pack_identifier(
                    item->expr, pack_name, argument_name);
            }
            return;
        case EXPR_GENERIC:
            template_replace_pack_identifier(
                expression->generic_control, pack_name, argument_name);
            for (GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                template_replace_pack_identifier(
                    association->expr, pack_name, argument_name);
            }
            return;
        case EXPR_CXX_FOLD:
            template_replace_pack_identifier(
                expression->cxx_fold_init, pack_name, argument_name);
            return;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            template_replace_pack_identifier(
                expression->va_list_operand, pack_name, argument_name);
            template_replace_pack_identifier(
                expression->va_second_operand, pack_name, argument_name);
            return;
        default:
            return;
    }
}

static Expr* template_clone_pack_pattern(
    CxxTemplate* tmpl, Expr* pattern, const char* pack_name, int index,
    Type** args, int arg_count, const int64_t* value_args,
    const bool* value_present, TemplateParam* value_pack) {
    char name[64];
    int written;
    Expr* copy;
    const int64_t* clone_value_args = value_args;
    const bool* clone_value_present = value_present;
    Type** clone_args = args;
    int64_t* expanded_value_args = NULL;
    bool* expanded_value_present = NULL;
    Type** expanded_type_args = NULL;
    DeclList* pack_parameters;
    int type_pack_index = -1;
    if (!pattern || !pack_name) return NULL;
    written = snprintf(name, sizeof(name), "__rcc_pack_arg_%d", index);
    if (written < 0 || (size_t)written >= sizeof(name)) return NULL;
    pack_parameters = template_pack_parameter_list(tmpl);
    for (DeclList* parameter = pack_parameters; parameter;
         parameter = parameter->next) {
        int candidate;
        if (!parameter->decl || !parameter->decl->param_is_pack ||
            !parameter->decl->name ||
            strcmp(parameter->decl->name, pack_name) != 0) {
            continue;
        }
        candidate = template_parameter_pack_index_nested(
            tmpl, parameter->decl->type);
        if (candidate >= 0 && candidate < tmpl->param_count &&
            tmpl->params[candidate].kind == TPARAM_TYPE &&
            tmpl->params[candidate].is_pack) {
            type_pack_index = candidate;
        }
        break;
    }
    if (type_pack_index < 0 && tmpl && tmpl->params) {
        for (int parameter_index = 0; parameter_index < tmpl->param_count;
             ++parameter_index) {
            TemplateParam* parameter = &tmpl->params[parameter_index];
            if (parameter->kind == TPARAM_TYPE && parameter->is_pack &&
                parameter->name && strcmp(parameter->name, pack_name) == 0) {
                type_pack_index = parameter_index;
                break;
            }
        }
    }
    if (type_pack_index >= 0) {
        if (!tmpl->pending_pack_args || index < 0 ||
            index >= tmpl->pending_pack_count || arg_count <= 0 || !args) {
            rcc_error(pattern->loc,
                      "C++ type parameter pack value is missing");
            return NULL;
        }
        expanded_type_args = ast_arena_alloc(
            sizeof(*expanded_type_args) * (size_t)arg_count);
        memcpy(expanded_type_args, args,
               sizeof(*expanded_type_args) * (size_t)arg_count);
        expanded_type_args[type_pack_index] = tmpl->pending_pack_args[index];
        clone_args = expanded_type_args;
    }
    if (value_pack) {
        int parameter_index;
        if (!tmpl || !tmpl->params || arg_count <= 0 || !value_args ||
            !value_present || !tmpl->pending_pack_value_present ||
            !tmpl->pending_pack_values || index < 0 ||
            index >= tmpl->pending_pack_count) {
            rcc_error(pattern->loc,
                      "C++ non-type parameter pack value is missing");
            return NULL;
        }
        parameter_index = (int)(value_pack - tmpl->params);
        if (parameter_index < 0 || parameter_index >= arg_count) {
            rcc_error(pattern->loc,
                      "C++ non-type parameter pack metadata is invalid");
            return NULL;
        }
        expanded_value_args = ast_arena_alloc(
            sizeof(*expanded_value_args) * (size_t)arg_count);
        expanded_value_present = ast_arena_alloc(
            sizeof(*expanded_value_present) * (size_t)arg_count);
        memcpy(expanded_value_args, value_args,
               sizeof(*expanded_value_args) * (size_t)arg_count);
        memcpy(expanded_value_present, value_present,
               sizeof(*expanded_value_present) * (size_t)arg_count);
        expanded_value_args[parameter_index] =
            tmpl->pending_pack_values[index];
        expanded_value_present[parameter_index] = true;
        clone_value_args = expanded_value_args;
        clone_value_present = expanded_value_present;
    }
    copy = template_clone_expr(tmpl, pattern, clone_args, arg_count,
                               clone_value_args, clone_value_present);
    if (!copy) return NULL;
    if (!value_pack) {
        template_replace_pack_identifier(copy, pack_name, name);
    }
    copy->cxx_pack_expansion = false;
    copy->cxx_pack_expansion_name = NULL;
    copy->cxx_pack_expansion_pattern = NULL;
    return copy;
}

Expr* cxx_template_clone_pack_expansion(
    CxxTemplate* tmpl, Expr* pattern, const char* pack_name, int pack_index,
    Type** args, int arg_count, const int64_t* value_args,
    const bool* value_present) {
    return template_clone_pack_pattern(tmpl, pattern, pack_name, pack_index,
                                       args, arg_count, value_args,
                                       value_present, NULL);
}

static Expr* template_clone_pack_fold(
    CxxTemplate* tmpl, Expr* expression, Type** args, int arg_count,
    const int64_t* value_args, const bool* value_present) {
    Expr* result = NULL;
    Expr* initializer = NULL;
    TemplateParam* value_pack = NULL;
    const char* pack_name = expression ? expression->cxx_fold_pack_name : NULL;
    int pattern_pack_matches = 0;
    DeclList* pack_parameters = template_pack_parameter_list(tmpl);
    if (tmpl && expression && !pack_name && expression->cxx_fold_pattern &&
        pack_parameters) {
        for (DeclList* parameter = pack_parameters;
             parameter; parameter = parameter->next) {
            if (parameter->decl && parameter->decl->param_is_pack &&
                parameter->decl->name &&
                template_expr_contains_identifier(
                    expression->cxx_fold_pattern, parameter->decl->name)) {
                pack_name = parameter->decl->name;
                ++pattern_pack_matches;
            }
        }
    }
    if (tmpl && expression && !pack_name && expression->cxx_fold_pattern) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack && parameter->name &&
                (template_expr_contains_identifier(
                     expression->cxx_fold_pattern, parameter->name) ||
                 (parameter->kind == TPARAM_TYPE &&
                  template_expr_contains_type_parameter(
                      expression->cxx_fold_pattern, parameter->name, 0)))) {
                pack_name = parameter->name;
                ++pattern_pack_matches;
            }
        }
    }
    if (!tmpl || !expression || !pack_name ||
        (expression->cxx_fold_pattern && pattern_pack_matches != 1)) {
        rcc_error(expression ? expression->loc : (SourceLoc){"<template>", 0, 0},
                  "C++ fold expression pattern must reference exactly one template parameter pack");
        return expression;
    }
    if (tmpl->pending_pack_count < 0) {
        rcc_error(expression->loc,
                  "C++ fold expression requires a function-template pack specialization");
        return expression;
    }
    if (expression->cxx_fold_init) {
        initializer = template_clone_expr(
            tmpl, expression->cxx_fold_init, args, arg_count,
            value_args, value_present);
    }
    if (tmpl->params) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE && parameter->is_pack &&
                parameter->name &&
                strcmp(parameter->name, pack_name) == 0) {
                value_pack = parameter;
                break;
            }
        }
    }
    if (tmpl->pending_pack_count == 0) {
        if (initializer) return initializer;
        if (expression->cxx_fold_operator == EXPR_AND) {
            result = expr_int(1, expression->loc);
        } else if (expression->cxx_fold_operator == EXPR_OR) {
            result = expr_int(0, expression->loc);
        } else {
            rcc_error(expression->loc,
                      "empty C++ fold has no identity for this operator");
            return expression;
        }
        result->type = type_int;
        return result;
    }
    if (expression->cxx_fold_left) {
        result = initializer;
        for (int index = 0; index < tmpl->pending_pack_count; ++index) {
            char name[64];
            int written = snprintf(name, sizeof(name), "__rcc_pack_arg_%d", index);
            Expr* item;
            if (written < 0 || (size_t)written >= sizeof(name)) {
                rcc_error(expression->loc,
                          "C++ fold parameter name is too long");
                return expression;
            }
            if (expression->cxx_fold_pattern) {
                item = template_clone_pack_pattern(
                    tmpl, expression->cxx_fold_pattern, pack_name, index,
                    args, arg_count, value_args, value_present, value_pack);
            } else if (value_pack) {
                if (!tmpl->pending_pack_value_present ||
                    !tmpl->pending_pack_value_present[index]) {
                    rcc_error(expression->loc,
                              "C++ non-type parameter pack value is missing");
                    return expression;
                }
                item = expr_int(tmpl->pending_pack_values[index],
                                expression->loc);
                item->type = value_pack->type ? value_pack->type : type_int;
            } else {
                item = expr_ident(rcc_intern(name), expression->loc);
            }
            if (!item) {
                rcc_error(expression->loc,
                          "C++ fold expression pattern could not be cloned");
                return expression;
            }
            if (!result) {
                result = item;
            } else {
                result = expr_binary(expression->cxx_fold_operator,
                                     result, item, expression->loc);
            }
        }
        return result;
    }
    result = initializer;
    for (int index = tmpl->pending_pack_count - 1; index >= 0; --index) {
        char name[64];
        int written = snprintf(name, sizeof(name), "__rcc_pack_arg_%d", index);
        Expr* item;
        if (written < 0 || (size_t)written >= sizeof(name)) {
            rcc_error(expression->loc,
                      "C++ fold parameter name is too long");
            return expression;
        }
        if (expression->cxx_fold_pattern) {
            item = template_clone_pack_pattern(
                tmpl, expression->cxx_fold_pattern, pack_name, index,
                args, arg_count, value_args, value_present, value_pack);
        } else if (value_pack) {
            if (!tmpl->pending_pack_value_present ||
                !tmpl->pending_pack_value_present[index]) {
                rcc_error(expression->loc,
                          "C++ non-type parameter pack value is missing");
                return expression;
            }
            item = expr_int(tmpl->pending_pack_values[index], expression->loc);
            item->type = value_pack->type ? value_pack->type : type_int;
        } else {
            item = expr_ident(rcc_intern(name), expression->loc);
        }
        if (!item) {
            rcc_error(expression->loc,
                      "C++ fold expression pattern could not be cloned");
            return expression;
        }
        if (!result) result = item;
        else result = expr_binary(expression->cxx_fold_operator,
                                  item, result, expression->loc);
    }
    return result;
}

static ExprList* template_clone_expr_list(CxxTemplate* tmpl, ExprList* list,
                                           Type** args, int arg_count,
                                           const int64_t* value_args,
                                           const bool* value_present) {
    ExprList* result = NULL;
    ExprList** tail = &result;
    for (; list; list = list->next) {
        if (list->expr && list->expr->cxx_pack_expansion) {
            const char* pack_name = list->expr->cxx_pack_expansion_name;
            int pack_matches = 0;
            if (list->designator_kind != INIT_DESIGNATOR_NONE) {
                rcc_error(list->expr->loc,
                          "C++ pack expansion cannot carry an initializer designator");
                continue;
            }
            DeclList* pack_parameters = template_pack_parameter_list(tmpl);
            if (!tmpl || tmpl->pending_pack_count < 0 ||
                !pack_parameters) {
                rcc_error(list->expr->loc,
                          "C++ pack expansion requires a function-template specialization");
                continue;
            }
            if (!pack_name && list->expr->cxx_pack_expansion_pattern) {
                for (DeclList* parameter = pack_parameters;
                     parameter; parameter = parameter->next) {
                    if (parameter->decl && parameter->decl->param_is_pack &&
                        parameter->decl->name &&
                        template_expr_contains_identifier(
                            list->expr->cxx_pack_expansion_pattern,
                            parameter->decl->name)) {
                        pack_name = parameter->decl->name;
                        ++pack_matches;
                    }
                }
                if (pack_matches != 1) {
                    rcc_error(list->expr->loc,
                              pack_matches == 0
                                  ? "C++ pack expansion pattern does not name a parameter pack"
                                  : "C++ pack expansion pattern names multiple parameter packs");
                    continue;
                }
            }
            {
                bool found = false;
                for (DeclList* parameter = pack_parameters;
                     parameter; parameter = parameter->next) {
                    if (parameter->decl && parameter->decl->param_is_pack &&
                        parameter->decl->name &&
                        pack_name &&
                        strcmp(parameter->decl->name, pack_name) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found || !pack_name) {
                    rcc_error(list->expr->loc,
                              "C++ pack expansion names a non-pack parameter");
                    continue;
                }
            }
            for (int index = 0; index < tmpl->pending_pack_count; ++index) {
                char name[64];
                int written = snprintf(name, sizeof(name),
                                       "__rcc_pack_arg_%d", index);
                ExprList* copy;
                if (written < 0 || (size_t)written >= sizeof(name)) {
                    rcc_error(list->expr->loc,
                              "C++ pack expansion parameter name is too long");
                    break;
                }
                copy = ast_arena_alloc(sizeof(*copy));
                *copy = *list;
                copy->expr = list->expr->cxx_pack_expansion_pattern
                    ? template_clone_pack_pattern(
                          tmpl, list->expr->cxx_pack_expansion_pattern,
                          pack_name, index, args, arg_count, value_args,
                          value_present, NULL)
                    : expr_ident(rcc_intern(name), list->expr->loc);
                if (!copy->expr) {
                    rcc_error(list->expr->loc,
                              "C++ pack expansion pattern could not be cloned");
                    break;
                }
                copy->next = NULL;
                *tail = copy;
                tail = &copy->next;
            }
            continue;
        }
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

static bool template_pack_is_deferred_member_pack(
    CxxTemplate* tmpl, const char* pack_name) {
    for (CxxTemplate* current = tmpl; current;
         current = current->enclosing_template) {
        for (int index = 0; current->params &&
                            index < current->param_count; ++index) {
            TemplateParam* parameter = &current->params[index];
            if (parameter->is_pack && parameter->name && pack_name &&
                strcmp(parameter->name, pack_name) == 0) {
                return false;
            }
        }
    }
    return tmpl && tmpl->kind == TMPL_CLASS && pack_name;
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

static bool template_eval_dependent_static_member(
    CxxTemplate* tmpl, Expr* expression, Type** args, int arg_count,
    int64_t* value, Type** value_type) {
    if (!tmpl || !expression || expression->kind != EXPR_MEMBER ||
        !expression->member_base ||
        expression->member_base->kind != EXPR_IDENT ||
        !expression->member_base->ident_name || !expression->member_name ||
        !args || !value) {
        return false;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        Type* owner_type;
        CxxClass* owner_class;
        if (parameter->kind != TPARAM_TYPE || !parameter->name ||
            strcmp(parameter->name,
                   expression->member_base->ident_name) != 0 ||
            index >= arg_count || !(owner_type = args[index]) ||
            !(owner_class = owner_type->cxx_class)) {
            continue;
        }
        for (TypeParam* field = owner_class->fields; field;
             field = field->next) {
            bool is_constexpr = false;
            if (!field->is_static || !field->name ||
                strcmp(field->name, expression->member_name) != 0 ||
                !field->initializer) {
                continue;
            }
            for (struct CxxMember* member = owner_class->members;
                 member; member = member->next) {
                Decl* declaration = member->decl;
                const char* declared_name;
                if (!member->is_static || !declaration ||
                    declaration->kind != DECL_VAR ||
                    !declaration->var_is_constexpr || !declaration->name) {
                    continue;
                }
                declared_name = strrchr(declaration->name, ':');
                declared_name = declared_name &&
                        declared_name > declaration->name &&
                        declared_name[-1] == ':'
                    ? declared_name + 1 : declaration->name;
                if (strcmp(declared_name, expression->member_name) == 0) {
                    is_constexpr = true;
                    break;
                }
            }
            if (is_constexpr &&
                expr_eval_integer_constant(field->initializer, value)) {
                if (value_type) *value_type = field->type;
                return true;
            }
            return false;
        }
        return false;
    }
    return false;
}

static Expr* template_clone_expr(CxxTemplate* tmpl, Expr* expression,
                                 Type** args, int arg_count,
                                 const int64_t* value_args,
                                 const bool* value_present) {
    Expr* copy;
    if (!expression) return NULL;
    if (expression->kind == EXPR_MEMBER) {
        int64_t value;
        Type* value_type = NULL;
        if (template_eval_dependent_static_member(
                tmpl, expression, args, arg_count, &value, &value_type)) {
            copy = expr_int(value, expression->loc);
            copy->type = value_type ? value_type : type_int;
            return copy;
        }
    }
    if (expression->kind == EXPR_CXX_FOLD) {
        if (template_pack_is_deferred_member_pack(
                tmpl, expression->cxx_fold_pack_name)) {
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->type = template_substitute_type(
                tmpl, expression->type, args, arg_count,
                value_args, value_present);
            copy->cxx_fold_init = template_clone_expr(
                tmpl, expression->cxx_fold_init, args, arg_count,
                value_args, value_present);
            copy->cxx_fold_pattern = template_clone_expr(
                tmpl, expression->cxx_fold_pattern, args, arg_count,
                value_args, value_present);
            return copy;
        }
        return template_clone_pack_fold(tmpl, expression, args, arg_count,
                                        value_args, value_present);
    }
    if (expression->kind == EXPR_SIZEOF && expression->sizeof_pack_name) {
        if (template_pack_is_deferred_member_pack(
                tmpl, expression->sizeof_pack_name)) {
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->type = template_substitute_type(
                tmpl, expression->type, args, arg_count,
                value_args, value_present);
            return copy;
        }
        if (!tmpl || tmpl->pending_pack_count < 0) {
            rcc_error(expression->loc,
                      "sizeof... requires a function-template pack specialization");
            return expression;
        }
        copy = expr_int(tmpl->pending_pack_count, expression->loc);
        copy->type = type_uint;
        return copy;
    }
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
        case EXPR_NOEXCEPT:
            copy->unary_operand = template_clone_expr(
                tmpl, expression->unary_operand, args, arg_count,
                value_args, value_present);
            copy->sizeof_type = template_substitute_type(
                tmpl, expression->sizeof_type, args, arg_count,
                value_args, value_present);
            copy->sizeof_pack_name = expression->sizeof_pack_name;
            if (expression->kind == EXPR_NOEXCEPT) {
                /* The operand may become a different overload after
                 * substitution; recompute the value in sema. */
                copy->cxx_noexcept_value_valid = false;
                copy->cxx_noexcept_value = false;
            }
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
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
            copy->call_abi_function_type = template_substitute_type(
                tmpl, expression->call_abi_function_type, args, arg_count,
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
        case EXPR_CXX_TYPEID:
            copy->cxx_typeid_operand_type = template_substitute_type(
                tmpl, expression->cxx_typeid_operand_type, args, arg_count,
                value_args, value_present);
            copy->cxx_typeid_operand = template_clone_expr(
                tmpl, expression->cxx_typeid_operand, args, arg_count,
                value_args, value_present);
            copy->cxx_typeid_symbol = NULL;
            copy->cxx_typeid_dynamic = false;
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
            if (expression->member_template_argument_count > 0 &&
                expression->member_template_arguments) {
                int source_count = expression->member_template_argument_count;
                int destination_count = source_count;
                bool expand_arguments = false;
                bool defer_expansion = false;
                for (int index = 0; index < source_count; ++index) {
                    const CxxParsedTemplateArgument* source =
                        &expression->member_template_arguments[index];
                    Expr* expansion = source->value_expression;
                    if (source->is_type || !expansion ||
                        !expansion->cxx_pack_expansion) {
                        continue;
                    }
                    expand_arguments = true;
                    if (!tmpl || tmpl->pending_pack_count < 0) {
                        defer_expansion = true;
                        continue;
                    }
                    if (tmpl->pending_pack_count > 32 ||
                        destination_count >
                            32 - tmpl->pending_pack_count) {
                        rcc_error(expansion->loc,
                                  "member template argument pack expansion "
                                  "exceeds the supported argument limit");
                        defer_expansion = true;
                        continue;
                    }
                    destination_count += tmpl->pending_pack_count - 1;
                }
                if (expand_arguments && !defer_expansion) {
                    for (int index = 0; index < source_count; ++index) {
                        const CxxParsedTemplateArgument* source =
                            &expression->member_template_arguments[index];
                        Expr* expansion = source->value_expression;
                        const char* pack_name;
                        TemplateParam* value_pack = NULL;
                        int pack_matches = 0;
                        if (source->is_type || !expansion ||
                            !expansion->cxx_pack_expansion) {
                            continue;
                        }
                        pack_name = expansion->cxx_pack_expansion_name;
                        if (tmpl && tmpl->params) {
                            for (int parameter_index = 0;
                                 parameter_index < tmpl->param_count;
                                 ++parameter_index) {
                                TemplateParam* parameter =
                                    &tmpl->params[parameter_index];
                                if (parameter->kind != TPARAM_NONTYPE ||
                                    !parameter->is_pack || !parameter->name ||
                                    (pack_name && strcmp(parameter->name,
                                                         pack_name) != 0) ||
                                    !template_expr_contains_identifier(
                                        expansion->cxx_pack_expansion_pattern
                                            ? expansion->cxx_pack_expansion_pattern
                                            : expansion,
                                        parameter->name)) {
                                    continue;
                                }
                                value_pack = parameter;
                                pack_name = parameter->name;
                                ++pack_matches;
                            }
                        }
                        if (pack_matches != 1 || !value_pack || !pack_name) {
                            rcc_error(expansion->loc,
                                      pack_matches == 0
                                          ? "member template argument pack "
                                            "expansion does not name a "
                                            "non-type parameter pack"
                                          : "member template argument pack "
                                            "expansion names multiple packs");
                            defer_expansion = true;
                        }
                    }
                }
                if (defer_expansion) destination_count = source_count;
                copy->member_template_argument_count = destination_count;
                copy->member_template_arguments = destination_count > 0
                    ? ast_arena_alloc(
                          sizeof(copy->member_template_arguments[0]) *
                          (size_t)destination_count)
                    : NULL;
                {
                    int destination_index = 0;
                    for (int index = 0; index < source_count; ++index) {
                        const CxxParsedTemplateArgument* source =
                            &expression->member_template_arguments[index];
                        Expr* expansion = source->value_expression;
                        if (!defer_expansion && !source->is_type &&
                            expansion && expansion->cxx_pack_expansion) {
                            const char* pack_name =
                                expansion->cxx_pack_expansion_name;
                            TemplateParam* value_pack = NULL;
                            for (int parameter_index = 0;
                                 tmpl && tmpl->params &&
                                 parameter_index < tmpl->param_count;
                                 ++parameter_index) {
                                TemplateParam* parameter =
                                    &tmpl->params[parameter_index];
                                if (parameter->kind == TPARAM_NONTYPE &&
                                    parameter->is_pack && parameter->name &&
                                    (!pack_name ||
                                     strcmp(parameter->name, pack_name) == 0) &&
                                    template_expr_contains_identifier(
                                        expansion->cxx_pack_expansion_pattern
                                            ? expansion->cxx_pack_expansion_pattern
                                            : expansion,
                                        parameter->name)) {
                                    pack_name = parameter->name;
                                    value_pack = parameter;
                                    break;
                                }
                            }
                            for (int pack_index = 0;
                                 pack_index < tmpl->pending_pack_count;
                                 ++pack_index) {
                                CxxParsedTemplateArgument* destination =
                                    &copy->member_template_arguments[
                                        destination_index++];
                                *destination = *source;
                                destination->type = NULL;
                                destination->value_expression =
                                    template_clone_pack_pattern(
                                        tmpl,
                                        expansion->cxx_pack_expansion_pattern
                                            ? expansion->cxx_pack_expansion_pattern
                                            : expansion,
                                        pack_name, pack_index, args, arg_count,
                                        value_args, value_present, value_pack);
                                destination->value_valid =
                                    destination->value_expression &&
                                    expr_eval_integer_constant(
                                        destination->value_expression,
                                        &destination->value);
                            }
                            continue;
                        }
                        CxxParsedTemplateArgument* destination =
                            &copy->member_template_arguments[destination_index++];
                        *destination = *source;
                        destination->type = template_substitute_type(
                            tmpl, source->type, args, arg_count, value_args,
                            value_present);
                        destination->value_expression = template_clone_expr(
                            tmpl, source->value_expression, args, arg_count,
                            value_args, value_present);
                        if (destination->value_expression &&
                            expr_eval_integer_constant(
                                destination->value_expression,
                                &destination->value)) {
                            destination->value_valid = true;
                        }
                    }
                }
            }
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
            copy->compound_paren_init = expression->compound_paren_init;
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
        case EXPR_CXX_REQUIRES:
            copy->cxx_requires_items = template_clone_expr_list(
                tmpl, expression->cxx_requires_items, args, arg_count,
                value_args, value_present);
            copy->cxx_requires_params = NULL;
            {
                DeclList** tail = &copy->cxx_requires_params;
                for (DeclList* parameter = expression->cxx_requires_params;
                     parameter; parameter = parameter->next) {
                    DeclList* parameter_copy = ast_arena_alloc(
                        sizeof(*parameter_copy));
                    parameter_copy->decl = template_clone_decl(
                        tmpl, parameter->decl, args, arg_count,
                        value_args, value_present);
                    parameter_copy->next = NULL;
                    *tail = parameter_copy;
                    tail = &parameter_copy->next;
                }
            }
            copy->cxx_requires_types = NULL;
            {
                TypeList** tail = &copy->cxx_requires_types;
                for (TypeList* requirement = expression->cxx_requires_types;
                     requirement; requirement = requirement->next) {
                    TypeList* requirement_copy = ast_arena_alloc(
                        sizeof(*requirement_copy));
                    requirement_copy->type = template_substitute_type(
                        tmpl, requirement->type, args, arg_count,
                        value_args, value_present);
                    requirement_copy->loc = requirement->loc;
                    requirement_copy->next = NULL;
                    *tail = requirement_copy;
                    tail = &requirement_copy->next;
                }
            }
            copy->cxx_requires_nested = template_clone_expr_list(
                tmpl, expression->cxx_requires_nested, args, arg_count,
                value_args, value_present);
            copy->cxx_requires_compound = NULL;
            {
                CxxCompoundRequirement** tail =
                    &copy->cxx_requires_compound;
                for (CxxCompoundRequirement* requirement =
                         expression->cxx_requires_compound;
                     requirement; requirement = requirement->next) {
                    CxxCompoundRequirement* requirement_copy =
                        ast_arena_alloc(sizeof(*requirement_copy));
                    requirement_copy->expr = template_clone_expr(
                        tmpl, requirement->expr, args, arg_count,
                        value_args, value_present);
                    requirement_copy->is_noexcept = requirement->is_noexcept;
                    requirement_copy->return_type = template_substitute_type(
                        tmpl, requirement->return_type, args, arg_count,
                        value_args, value_present);
                    requirement_copy->return_type_convertible =
                        requirement->return_type_convertible;
                    requirement_copy->loc = requirement->loc;
                    requirement_copy->next = NULL;
                    *tail = requirement_copy;
                    tail = &requirement_copy->next;
                }
            }
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
    if (declaration->kind == DECL_TYPEDEF) {
        /* A typedef stores its canonical underlying type separately from the
         * common Decl.type field; keep both views concrete in an instance. */
        copy->typedef_type = template_substitute_type(
            tmpl, declaration->typedef_type, args, arg_count,
            value_args, value_present);
        copy->type = copy->typedef_type;
    }
    copy->param_default = template_clone_expr(
        tmpl, declaration->param_default, args, arg_count,
        value_args, value_present);
    if (declaration->kind == DECL_FUNC) {
        copy->func_noexcept_expr = template_clone_expr(
            tmpl, declaration->func_noexcept_expr, args, arg_count,
            value_args, value_present);
    }
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
            if (statement->if_is_constexpr && copy->if_cond) {
                int64_t condition_value;
                if (expr_eval_integer_constant(copy->if_cond,
                                               &condition_value)) {
                    bool take_then = condition_value != 0;
                    copy->if_then = take_then
                        ? template_clone_stmt(
                              tmpl, statement->if_then, args, arg_count,
                              value_args, value_present)
                        : NULL;
                    copy->if_else = take_then
                        ? NULL
                        : template_clone_stmt(
                              tmpl, statement->if_else, args, arg_count,
                              value_args, value_present);
                    break;
                }
            }
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
        if (parameter->is_pack) {
            if (tmpl->instances[instance_index].pack_count !=
                    tmpl->pending_pack_count) {
                return false;
            }
            if (parameter->kind == TPARAM_TYPE) {
                for (int pack_index = 0;
                     pack_index < tmpl->pending_pack_count; ++pack_index) {
                    if (!tmpl->pending_pack_args ||
                        !tmpl->instances[instance_index].pack_args ||
                        !type_is_compatible(
                            tmpl->instances[instance_index].pack_args[pack_index],
                            tmpl->pending_pack_args[pack_index])) {
                        return false;
                    }
                }
            } else {
                for (int pack_index = 0;
                     pack_index < tmpl->pending_pack_count; ++pack_index) {
                    if (!tmpl->pending_pack_value_present ||
                        !tmpl->pending_pack_value_present[pack_index] ||
                        !tmpl->instances[instance_index].pack_value_present ||
                        !tmpl->instances[instance_index].pack_value_present[pack_index] ||
                        !tmpl->instances[instance_index].pack_values ||
                        !tmpl->pending_pack_values ||
                        tmpl->instances[instance_index].pack_values[pack_index] !=
                            tmpl->pending_pack_values[pack_index]) {
                        return false;
                    }
                }
            }
        } else if (parameter->kind == TPARAM_NONTYPE) {
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
        if (parameter->is_pack) {
            if (tmpl->pending_pack_count < 0 ||
                (tmpl->pending_pack_count > 0 && parameter->kind == TPARAM_TYPE &&
                 !tmpl->pending_pack_args) ||
                (tmpl->pending_pack_count > 0 && parameter->kind == TPARAM_NONTYPE &&
                 (!tmpl->pending_pack_values ||
                  !tmpl->pending_pack_value_present))) {
                rcc_error(template_loc,
                          "function template parameter pack arguments are missing");
                return NULL;
            }
        } else if (parameter->kind == TPARAM_NONTYPE) {
            Type* value_type = parameter->type
                ? template_substitute_type(
                      tmpl, parameter->type, args, arg_count, value_args,
                      value_present)
                : NULL;
            if (!value_present || !value_present[index] || !value_args ||
                !value_type ||
                (!type_is_integer(value_type) &&
                 value_type->kind != TYPE_ENUM)) {
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
        bool has_function_parameter_pack = false;

        for (DeclList* item = definition->func_params; item;
             item = item->next) {
            if (item->decl && item->decl->param_is_pack) {
                has_function_parameter_pack = true;
                int pack_index = template_parameter_pack_index_nested(
                    tmpl, item->decl->type);
                if (pack_index < 0 || !tmpl->params[pack_index].is_pack) {
                    rcc_error(item->decl->loc,
                              "function parameter pack is not bound to a template parameter pack");
                    return NULL;
                }
                for (int pack_value = 0;
                     pack_value < tmpl->pending_pack_count; ++pack_value) {
                    char generated_name[64];
                    const char* parameter_name = NULL;
                    Type* parameter_type;
                    int written = snprintf(generated_name,
                                           sizeof(generated_name),
                                           "__rcc_pack_arg_%d", pack_value);
                    if (written < 0 || (size_t)written >= sizeof(generated_name)) {
                        rcc_error(item->decl->loc,
                                  "function template pack parameter name is too long");
                        return NULL;
                    }
                    {
                        Type* expanded_arguments[32];
                        if (arg_count > (int)(sizeof(expanded_arguments) /
                                              sizeof(expanded_arguments[0]))) {
                            rcc_error(item->decl->loc,
                                      "function template pack substitution exceeds compiler limits");
                            return NULL;
                        }
                        int64_t expanded_values[32];
                        bool expanded_value_present[32];
                        memcpy(expanded_arguments, args,
                               sizeof(Type*) * (size_t)arg_count);
                        memcpy(expanded_values, value_args,
                               sizeof(int64_t) * (size_t)arg_count);
                        memcpy(expanded_value_present, value_present,
                               sizeof(bool) * (size_t)arg_count);
                        if (tmpl->params[pack_index].kind == TPARAM_TYPE) {
                            expanded_arguments[pack_index] =
                                tmpl->pending_pack_args[pack_value];
                        } else {
                            if (!tmpl->pending_pack_values ||
                                !tmpl->pending_pack_value_present ||
                                !tmpl->pending_pack_value_present[pack_value]) {
                                rcc_error(item->decl->loc,
                                          "function non-type parameter pack value is missing");
                                return NULL;
                            }
                            expanded_values[pack_index] =
                                tmpl->pending_pack_values[pack_value];
                            expanded_value_present[pack_index] = true;
                        }
                        parameter_type = template_substitute_type(
                            tmpl, item->decl->type, expanded_arguments,
                            arg_count, expanded_values,
                            expanded_value_present);
                    }
                    if (!parameter_type) {
                        rcc_error(item->decl->loc,
                                  "function template pack parameter substitution failed");
                        return NULL;
                    }
                    /* A named pack is not expanded into repeated identifiers
                     * until pack-expression lowering exists.  Generate stable
                     * names so any unsupported body reference is diagnosed by
                     * normal lookup instead of binding one element silently. */
                    if (item->decl->name) parameter_name = rcc_intern(generated_name);
                    Decl* parameter = decl_param(parameter_name, parameter_type,
                                                 0, item->decl->loc);
                    parameter->param_default = NULL;
                    decllist_append(&parameters, parameter);
                    TypeParam* type_parameter = ast_arena_alloc(sizeof(*type_parameter));
                    type_parameter->name = parameter->name;
                    type_parameter->type = parameter_type;
                    type_parameter->is_bitfield = false;
                    type_parameter->bit_width = 0u;
                    type_parameter->is_static = false;
                    type_parameter->initializer = NULL;
                    type_parameter->is_deprecated = false;
                    type_parameter->deprecated_message = NULL;
                    type_parameter->cxx_access = ACCESS_PUBLIC;
                    type_parameter->next = NULL;
                    *type_tail = type_parameter;
                    type_tail = &type_parameter->next;
                }
                continue;
            }
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
            type_parameter->initializer = NULL;
            type_parameter->is_deprecated = false;
            type_parameter->deprecated_message = NULL;
            type_parameter->cxx_access = ACCESS_PUBLIC;
            type_parameter->next = NULL;
            *type_tail = type_parameter;
            type_tail = &type_parameter->next;
        }

        Type* return_type = template_substitute_type(
            tmpl, definition->type ? definition->type->ret_type : NULL,
            args, arg_count, value_args, value_present);
        function_type = type_func(return_type, type_parameters,
                                  definition->type &&
                                      definition->type->variadic &&
                                      !has_function_parameter_pack);
        function_type->has_prototype = definition->type
            ? definition->type->has_prototype : true;
        function_type->function_is_noexcept = definition->func_is_noexcept;
        function_type->is_const = definition->type
            ? definition->type->is_const : false;
        function_type->is_volatile = definition->type
            ? definition->type->is_volatile : false;
        function_type->function_ref_qualifier = definition->type
            ? definition->type->function_ref_qualifier
            : CXX_REF_QUAL_NONE;
        instance = decl_func(definition->name, function_type, parameters,
                             template_clone_stmt(tmpl, definition->func_body,
                                                 args, arg_count, value_args,
                                                 value_present),
                             definition->loc);
        instance->func_is_inline = definition->func_is_inline;
        instance->is_weak = definition->is_weak;
        instance->func_is_constexpr = definition->func_is_constexpr;
        instance->func_is_consteval = definition->func_is_consteval;
        instance->func_is_noreturn = definition->func_is_noreturn;
        instance->func_is_nodiscard = definition->func_is_nodiscard;
        instance->func_is_deprecated = definition->func_is_deprecated;
        instance->func_deprecated_message =
            definition->func_deprecated_message;
        instance->func_is_noexcept = definition->func_is_noexcept;
        instance->func_noexcept_expr = template_clone_expr(
            tmpl, definition->func_noexcept_expr, args, arg_count,
            value_args, value_present);
        instance->func_is_auto_return = definition->func_is_auto_return;
        instance->func_is_decltype_auto_return =
            definition->func_is_decltype_auto_return;
        instance->func_cxx_namespace = tmpl->ns
            ? cxx_namespace_qualified_name(tmpl->ns) : NULL;
        instance->func_cxx_namespace_scope = tmpl->ns;
        instance->func_is_template_instance = true;
        instance->func_template_owner = tmpl;
        instance->func_is_cxx_method = definition->func_is_cxx_method;
        instance->func_method_owner = definition->func_method_owner;
        instance->func_template_arg_count = arg_count;
        instance->func_template_pack_count = -1;
        if (tmpl->pending_pack_count >= 0) {
            bool has_type_pack = false;
            for (int parameter_index = 0;
                 parameter_index < tmpl->param_count; ++parameter_index) {
                if (tmpl->params[parameter_index].is_pack &&
                    tmpl->params[parameter_index].kind == TPARAM_TYPE) {
                    has_type_pack = true;
                    break;
                }
            }
            instance->func_template_pack_count = tmpl->pending_pack_count;
            if (tmpl->pending_pack_count > 0 && has_type_pack) {
                instance->func_template_pack_args = ast_arena_alloc(
                    sizeof(instance->func_template_pack_args[0]) *
                    (size_t)tmpl->pending_pack_count);
                memcpy(instance->func_template_pack_args,
                       tmpl->pending_pack_args,
                       sizeof(instance->func_template_pack_args[0]) *
                           (size_t)tmpl->pending_pack_count);
            } else if (tmpl->pending_pack_count > 0) {
                instance->func_template_pack_values = ast_arena_alloc(
                    sizeof(instance->func_template_pack_values[0]) *
                    (size_t)tmpl->pending_pack_count);
                instance->func_template_pack_value_present = ast_arena_alloc(
                    sizeof(instance->func_template_pack_value_present[0]) *
                    (size_t)tmpl->pending_pack_count);
                memcpy(instance->func_template_pack_values,
                       tmpl->pending_pack_values,
                       sizeof(instance->func_template_pack_values[0]) *
                           (size_t)tmpl->pending_pack_count);
                memcpy(instance->func_template_pack_value_present,
                       tmpl->pending_pack_value_present,
                       sizeof(instance->func_template_pack_value_present[0]) *
                           (size_t)tmpl->pending_pack_count);
            }
        }
        if (arg_count > 0) {
            instance->func_template_args = ast_arena_alloc(
                sizeof(instance->func_template_args[0]) *
                (size_t)arg_count);
            memcpy(instance->func_template_args, args,
                   sizeof(instance->func_template_args[0]) *
                       (size_t)arg_count);
            if (value_args) {
                instance->func_template_value_args = ast_arena_alloc(
                    sizeof(instance->func_template_value_args[0]) *
                    (size_t)arg_count);
                memcpy(instance->func_template_value_args, value_args,
                       sizeof(instance->func_template_value_args[0]) *
                           (size_t)arg_count);
            }
            if (value_present) {
                instance->func_template_value_present = ast_arena_alloc(
                    sizeof(instance->func_template_value_present[0]) *
                    (size_t)arg_count);
                memcpy(instance->func_template_value_present, value_present,
                       sizeof(instance->func_template_value_present[0]) *
                           (size_t)arg_count);
            }
        }
        instance->func_has_cxx_linkage = true;
        instance->func_friend_access = tmpl->friend_access;
        instance->link_name = rcc_intern(cxx_mangle_function_template(
            instance, tmpl->ns,
            tmpl->member_method ? tmpl->member_method->owner : NULL,
            tmpl, args, value_args,
            value_present));

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
        tmpl->instances[tmpl->instance_count].pack_args = NULL;
        tmpl->instances[tmpl->instance_count].pack_values = NULL;
        tmpl->instances[tmpl->instance_count].pack_value_present = NULL;
        tmpl->instances[tmpl->instance_count].pack_count =
            tmpl->pending_pack_count >= 0 ? tmpl->pending_pack_count : 0;
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
        if (tmpl->pending_pack_count > 0) {
            bool has_type_pack = false;
            for (int parameter_index = 0;
                 parameter_index < tmpl->param_count; ++parameter_index) {
                if (tmpl->params[parameter_index].is_pack &&
                    tmpl->params[parameter_index].kind == TPARAM_TYPE) {
                    has_type_pack = true;
                    break;
                }
            }
            if (has_type_pack) {
                tmpl->instances[tmpl->instance_count].pack_args =
                    ast_arena_alloc(sizeof(Type*) *
                                    (size_t)tmpl->pending_pack_count);
                memcpy(tmpl->instances[tmpl->instance_count].pack_args,
                       tmpl->pending_pack_args,
                       sizeof(Type*) * (size_t)tmpl->pending_pack_count);
            } else {
                tmpl->instances[tmpl->instance_count].pack_values =
                    ast_arena_alloc(sizeof(int64_t) *
                                    (size_t)tmpl->pending_pack_count);
                tmpl->instances[tmpl->instance_count].pack_value_present =
                    ast_arena_alloc(sizeof(bool) *
                                    (size_t)tmpl->pending_pack_count);
                memcpy(tmpl->instances[tmpl->instance_count].pack_values,
                       tmpl->pending_pack_values,
                       sizeof(int64_t) * (size_t)tmpl->pending_pack_count);
                memcpy(tmpl->instances[tmpl->instance_count].pack_value_present,
                       tmpl->pending_pack_value_present,
                       sizeof(bool) * (size_t)tmpl->pending_pack_count);
            }
        }
        tmpl->instances[tmpl->instance_count].arg_count = arg_count;
        tmpl->instances[tmpl->instance_count].instantiated = instance;
        ++tmpl->instance_count;
        return instance;
    }

    if (tmpl->kind == TMPL_VARIABLE && tmpl->var_def) {
        Decl* definition = tmpl->var_def;
        Type* variable_type = template_substitute_type(
            tmpl, definition->type, args, arg_count, value_args,
            value_present);
        Expr* initializer = template_clone_expr(
            tmpl, definition->var_init, args, arg_count, value_args,
            value_present);
        Decl* instance;
        char generated_name[128];
        int written;

        if (!variable_type) {
            rcc_error(definition->loc,
                      "variable template type substitution failed");
            return NULL;
        }
        written = snprintf(generated_name, sizeof(generated_name),
                           "__rcc_variable_template_%d_%s",
                           tmpl->instance_count, definition->name);
        if (written < 0 || (size_t)written >= sizeof(generated_name)) {
            rcc_error(definition->loc,
                      "variable template specialization name exceeds compiler limits");
            return NULL;
        }
        instance = decl_var(rcc_intern(generated_name), variable_type,
                            initializer, definition->loc);
        instance->storage = definition->storage;
        instance->is_weak = definition->is_weak;
        instance->var_is_thread_local = definition->var_is_thread_local;
        instance->var_is_constexpr = definition->var_is_constexpr;
        instance->var_is_constinit = definition->var_is_constinit;
        instance->var_is_inline = definition->var_is_inline;
        instance->var_is_deprecated = definition->var_is_deprecated;
        instance->var_deprecated_message = definition->var_deprecated_message;
        instance->link_name = rcc_intern(cxx_mangle_variable_template(
            definition, tmpl->ns, tmpl, args, value_args, value_present));

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
        tmpl->instances[tmpl->instance_count].pack_args = NULL;
        tmpl->instances[tmpl->instance_count].pack_values = NULL;
        tmpl->instances[tmpl->instance_count].pack_value_present = NULL;
        tmpl->instances[tmpl->instance_count].pack_count = 0;
        tmpl->instances[tmpl->instance_count].arg_count = arg_count;
        if (arg_count > 0) {
            tmpl->instances[tmpl->instance_count].value_args = ast_arena_alloc(
                sizeof(int64_t) * (size_t)arg_count);
            tmpl->instances[tmpl->instance_count].value_present = ast_arena_alloc(
                sizeof(bool) * (size_t)arg_count);
            memcpy(tmpl->instances[tmpl->instance_count].value_args,
                   value_args, sizeof(int64_t) * (size_t)arg_count);
            memcpy(tmpl->instances[tmpl->instance_count].value_present,
                   value_present, sizeof(bool) * (size_t)arg_count);
        }
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

const char* cxx_namespace_typeinfo_identity(CxxNamespace* ns) {
    CxxNamespace* stack[32];
    CxxTypeInfoIdentityBuilder builder = {0};
    size_t count = 0u;
    bool has_anonymous = false;
    const char* identity;
    if (!ns) return NULL;
    for (CxxNamespace* current = ns; current;
         current = current->parent) {
        if (!current->name && !current->is_anonymous_namespace) break;
        if (count == sizeof(stack) / sizeof(stack[0])) {
            rcc_fatal("C++ typeinfo namespace nesting is too deep");
        }
        if (current->is_anonymous_namespace) has_anonymous = true;
        stack[count++] = current;
    }
    if (!has_anonymous) return cxx_namespace_qualified_name(ns);
    cxx_typeinfo_identity_append_char(&builder, 'Q');
    cxx_typeinfo_identity_append_number(&builder, (uint64_t)count);
    while (count != 0u) {
        CxxNamespace* component = stack[--count];
        cxx_typeinfo_identity_append_char(
            &builder, component->is_anonymous_namespace ? 'A' : 'N');
        cxx_typeinfo_identity_append_text(
            &builder, component->is_anonymous_namespace
                ? component->anonymous_typeinfo_identity : component->name);
    }
    identity = rcc_intern(builder.data);
    return identity;
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
    cls->bases[cls->base_count].type_pattern = NULL;
    cls->bases[cls->base_count].access = access;
    cls->bases[cls->base_count].is_virtual = false;
    cls->bases[cls->base_count].is_pack_expansion = false;
    cls->base_count++;
}

void cxx_class_add_base_pattern(CxxClass* cls, Type* type_pattern,
                                const char* base_name, AccessSpec access,
                                bool is_virtual, bool is_pack_expansion) {
    int index;
    if (!cls || !type_pattern) return;
    index = cls->base_count++;
    cls->bases = ast_arena_grow(
        cls->bases, sizeof(cls->bases[0]) * (size_t)index,
        sizeof(cls->bases[0]) * (size_t)(index + 1));
    cls->bases[index].base = type_pattern->cxx_dependent
        ? NULL : type_pattern->cxx_class;
    cls->bases[index].base_name = base_name
        ? rcc_intern(base_name)
        : type_pattern->tag ? rcc_intern(type_pattern->tag) : NULL;
    cls->bases[index].type_pattern = type_pattern;
    cls->bases[index].access = access;
    cls->bases[index].is_virtual = is_virtual;
    cls->bases[index].is_pack_expansion = is_pack_expansion;
}

void cxx_class_add_using_base_member(CxxClass* cls, const char* base_name,
                                     const char* member_name,
                                     Type* base_type_pattern,
                                     AccessSpec access, SourceLoc loc) {
    if (!cls || !base_name || !member_name) return;
    cls->using_base_members = ast_arena_grow(
        cls->using_base_members,
        sizeof(cls->using_base_members[0]) *
            (size_t)cls->using_base_member_count,
        sizeof(cls->using_base_members[0]) *
            (size_t)(cls->using_base_member_count + 1));
    cls->using_base_members[cls->using_base_member_count].base_name =
        base_name;
    cls->using_base_members[cls->using_base_member_count].member_name =
        member_name;
    cls->using_base_members[cls->using_base_member_count].base_type_pattern =
        base_type_pattern;
    cls->using_base_members[cls->using_base_member_count].access = access;
    cls->using_base_members[cls->using_base_member_count].loc = loc;
    ++cls->using_base_member_count;
}

void cxx_class_add_friend_class(CxxClass* cls, const char* friend_name) {
    if (!cls || !friend_name || !*friend_name) return;
    cls->friend_class_names = ast_arena_grow(
        cls->friend_class_names,
        sizeof(cls->friend_class_names[0]) *
            (size_t)cls->friend_class_count,
        sizeof(cls->friend_class_names[0]) *
            (size_t)(cls->friend_class_count + 1));
    cls->friend_class_names[cls->friend_class_count++] = rcc_intern(friend_name);
}

void cxx_class_add_type_alias(CxxClass* cls, const char* name, Type* type,
                              AccessSpec access, SourceLoc loc) {
    CxxTypeAlias* alias;
    CxxTypeAlias** tail;
    if (!cls || !name || !*name || !type) return;
    if (cxx_class_find_alias_template(cls, name)) {
        rcc_error(loc, "duplicate nested type name '%s'", name);
        return;
    }
    alias = ast_arena_alloc(sizeof(*alias));
    alias->name = rcc_intern(name);
    alias->type = type;
    alias->access = access;
    alias->loc = loc;
    alias->next = NULL;
    tail = &cls->type_aliases;
    while (*tail) {
        if (strcmp((*tail)->name, alias->name) == 0) {
            rcc_error(loc, "duplicate nested type alias '%s'", name);
            return;
        }
        tail = &(*tail)->next;
    }
    *tail = alias;
}

CxxClassAliasTemplate* cxx_class_add_alias_template(
    CxxClass* cls, CxxTemplate* declaration, AccessSpec access,
    SourceLoc loc) {
    CxxClassAliasTemplate* alias_template;
    CxxClassAliasTemplate** tail;
    if (!cls || !declaration || declaration->kind != TMPL_ALIAS ||
        !declaration->name || !*declaration->name) {
        return NULL;
    }
    if (cxx_class_find_type_alias(cls, declaration->name)) {
        rcc_error(loc, "duplicate nested type name '%s'",
                  declaration->name);
        return NULL;
    }
    for (CxxClassAliasTemplate* current = cls->alias_templates; current;
         current = current->next) {
        if (current->declaration && current->declaration->name &&
            strcmp(current->declaration->name, declaration->name) == 0) {
            rcc_error(loc, "duplicate nested alias template '%s'",
                      declaration->name);
            return NULL;
        }
    }
    alias_template = ast_arena_alloc(sizeof(*alias_template));
    alias_template->declaration = declaration;
    alias_template->access = access;
    alias_template->next = NULL;
    tail = &cls->alias_templates;
    while (*tail) tail = &(*tail)->next;
    *tail = alias_template;
    return alias_template;
}

CxxClassAliasTemplate* cxx_class_find_alias_template(
    CxxClass* cls, const char* name) {
    if (!cls || !name) return NULL;
    for (CxxClassAliasTemplate* alias_template = cls->alias_templates;
         alias_template; alias_template = alias_template->next) {
        if (alias_template->declaration &&
            alias_template->declaration->name &&
            strcmp(alias_template->declaration->name, name) == 0) {
            return alias_template;
        }
    }
    return NULL;
}

CxxTypeAlias* cxx_class_find_type_alias(CxxClass* cls, const char* name) {
    if (!cls || !name) return NULL;
    for (CxxTypeAlias* alias = cls->type_aliases; alias;
         alias = alias->next) {
        if (strcmp(alias->name, name) == 0) return alias;
    }
    return NULL;
}

CxxTypeAlias* cxx_class_find_direct_public_base_type_alias(
    CxxClass* cls, const char* name, bool* ambiguous) {
    CxxTypeAlias* found = NULL;
    if (ambiguous) *ambiguous = false;
    if (!cls || !name) return NULL;
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        CxxTypeAlias* candidate;
        if (cls->bases[index].access != ACCESS_PUBLIC || !base) continue;
        candidate = cxx_class_find_type_alias(base, name);
        if (!candidate) continue;
        if (found && found != candidate) {
            if (ambiguous) *ambiguous = true;
            return NULL;
        }
        found = candidate;
    }
    return found;
}

typedef struct CxxTypeAliasClassPath {
    CxxClass* cls;
    const struct CxxTypeAliasClassPath* previous;
} CxxTypeAliasClassPath;

typedef struct CxxTypeAliasLookupResult {
    CxxTypeAlias* alias;
    CxxClass* declaring_class;
    AccessSpec access;
    bool found_non_alias;
    bool ambiguous;
} CxxTypeAliasLookupResult;

static bool cxx_type_alias_class_path_contains(
    const CxxTypeAliasClassPath* path, CxxClass* cls) {
    for (; path; path = path->previous) {
        if (path->cls == cls) return true;
    }
    return false;
}

static bool cxx_class_declares_non_alias_name(CxxClass* cls,
                                               const char* name) {
    for (struct CxxMember* member = cls ? cls->members : NULL; member;
         member = member->next) {
        const char* member_name = member->method
            ? member->method->source_name
            : (member->decl ? member->decl->name : NULL);
        if (member_name && strcmp(member_name, name) == 0) return true;
    }
    for (TypeParam* field = cls ? cls->fields : NULL; field;
         field = field->next) {
        if (field->name && strcmp(field->name, name) == 0) return true;
    }
    return false;
}

static CxxTypeAliasLookupResult cxx_class_lookup_type_alias_recursive(
    CxxClass* cls, const char* name, const CxxTypeAliasClassPath* path) {
    CxxTypeAliasLookupResult result = { 0 };
    CxxTypeAlias* own_alias;
    CxxTypeAliasClassPath current_path;
    if (!cls || !name) return result;
    if (cxx_type_alias_class_path_contains(path, cls)) {
        result.ambiguous = true;
        return result;
    }
    current_path.cls = cls;
    current_path.previous = path;
    own_alias = cxx_class_find_type_alias(cls, name);
    if (own_alias) {
        result.alias = own_alias;
        result.declaring_class = cls;
        result.access = own_alias->access;
        return result;
    }
    if (cxx_class_declares_non_alias_name(cls, name)) {
        result.found_non_alias = true;
        return result;
    }

    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        CxxTypeAliasLookupResult candidate;
        AccessSpec access;
        if (!base && cls->bases[index].type_pattern) {
            base = cls->bases[index].type_pattern->cxx_class;
        }
        if (!base) continue;
        candidate = cxx_class_lookup_type_alias_recursive(
            base, name, &current_path);
        if (candidate.ambiguous) {
            result.ambiguous = true;
            return result;
        }
        if (candidate.found_non_alias) {
            if (result.alias) {
                result.ambiguous = true;
                return result;
            }
            result.found_non_alias = true;
        }
        if (!candidate.alias) continue;
        if (result.found_non_alias) {
            result.ambiguous = true;
            return result;
        }
        access = candidate.access;
        if (cls->bases[index].access > access) {
            access = cls->bases[index].access;
        }
        if (!result.alias) {
            result.alias = candidate.alias;
            result.declaring_class = candidate.declaring_class;
            result.access = access;
        } else if (result.alias != candidate.alias) {
            result.ambiguous = true;
            return result;
        } else if (access < result.access) {
            /* The same declaration reached through a shared virtual base is
             * one lookup result; retain its least restrictive access path. */
            result.access = access;
        }
    }
    return result;
}

static bool cxx_class_is_same_or_derived_from_recursive(
    CxxClass* cls, CxxClass* target, const CxxTypeAliasClassPath* path) {
    CxxTypeAliasClassPath current_path;
    if (!cls || !target) return false;
    if (cls == target) return true;
    if (cxx_type_alias_class_path_contains(path, cls)) return false;
    current_path.cls = cls;
    current_path.previous = path;
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        if (!base && cls->bases[index].type_pattern) {
            base = cls->bases[index].type_pattern->cxx_class;
        }
        if (cxx_class_is_same_or_derived_from_recursive(
                base, target, &current_path)) {
            return true;
        }
    }
    return false;
}

bool cxx_class_is_same_or_derived_from(CxxClass* cls, CxxClass* target) {
    return cxx_class_is_same_or_derived_from_recursive(cls, target, NULL);
}

CxxTypeAlias* cxx_class_find_inherited_type_alias(
    CxxClass* cls, const char* name, CxxClass* access_context,
    bool* ambiguous, bool* accessible) {
    CxxTypeAliasLookupResult result;
    bool context_is_derived;
    if (ambiguous) *ambiguous = false;
    if (accessible) *accessible = false;
    result = cxx_class_lookup_type_alias_recursive(cls, name, NULL);
    if (ambiguous) *ambiguous = result.ambiguous;
    if (!result.alias || result.ambiguous) return NULL;
    if (accessible) {
        if (result.alias->access == ACCESS_PUBLIC &&
            result.access == ACCESS_PUBLIC) {
            *accessible = true;
        } else if (access_context == result.declaring_class) {
            *accessible = true;
        } else if (result.alias->access != ACCESS_PRIVATE &&
                   result.access != ACCESS_PRIVATE) {
            context_is_derived = cxx_class_is_same_or_derived_from(
                access_context, result.declaring_class);
            *accessible = context_is_derived;
        }
    }
    return result.alias;
}

bool cxx_class_resolve_nested_type_path(
    CxxClass* cls, const char* path, CxxClass* access_context,
    Type** resolved_type, bool* ambiguous, bool* accessible) {
    const char* component;
    CxxClass* owner = cls;
    unsigned depth = 0;

    if (resolved_type) *resolved_type = NULL;
    if (ambiguous) *ambiguous = false;
    if (accessible) *accessible = true;
    if (!owner || !path || !*path) return false;

    component = path;
    while (component && *component) {
        const char* separator = strstr(component, "::");
        size_t component_length = separator
            ? (size_t)(separator - component) : strlen(component);
        char component_name[512];
        bool component_ambiguous = false;
        bool component_accessible = false;
        CxxTypeAlias* alias;
        Type* nested_type;

        if (depth++ >= 64u || component_length == 0u ||
            component_length >= sizeof(component_name)) {
            return false;
        }
        memcpy(component_name, component, component_length);
        component_name[component_length] = '\0';
        alias = cxx_class_find_inherited_type_alias(
            owner, component_name, access_context,
            &component_ambiguous, &component_accessible);
        if (component_ambiguous) {
            if (ambiguous) *ambiguous = true;
            return false;
        }
        if (!alias) return false;
        if (!component_accessible) {
            if (accessible) *accessible = false;
            return false;
        }

        nested_type = alias->type;
        if (!separator) {
            if (resolved_type) *resolved_type = nested_type;
            return nested_type != NULL;
        }
        if (separator[2] == '\0' || !nested_type ||
            !nested_type->cxx_class) {
            return false;
        }
        owner = nested_type->cxx_class;
        component = separator + 2;
    }
    return false;
}

typedef struct CxxAliasTemplateClassPath {
    CxxClass* cls;
    const struct CxxAliasTemplateClassPath* previous;
} CxxAliasTemplateClassPath;

typedef struct CxxAliasTemplateLookupResult {
    CxxClassAliasTemplate* alias_template;
    CxxClass* declaring_class;
    AccessSpec access;
    AccessSpec inheritance_access;
    bool found_non_alias;
    bool ambiguous;
} CxxAliasTemplateLookupResult;

static bool cxx_alias_template_class_path_contains(
    const CxxAliasTemplateClassPath* path, CxxClass* cls) {
    for (; path; path = path->previous) {
        if (path->cls == cls) return true;
    }
    return false;
}

static CxxAliasTemplateLookupResult cxx_class_lookup_alias_template_recursive(
    CxxClass* cls, const char* name,
    const CxxAliasTemplateClassPath* path) {
    CxxAliasTemplateLookupResult result = { 0 };
    CxxClassAliasTemplate* own_alias;
    CxxClass* declaring_class = cls;
    CxxClassAliasTemplate* alias_template;
    CxxAliasTemplateClassPath current_path;
    if (!cls || !name) return result;
    if (cxx_alias_template_class_path_contains(path, cls)) {
        result.ambiguous = true;
        return result;
    }
    current_path.cls = cls;
    current_path.previous = path;

    own_alias = cxx_class_find_alias_template(cls, name);
    alias_template = own_alias;
    if (!alias_template && cls->templ && cls->templ->templated_class &&
        cls->templ->templated_class != cls) {
        alias_template = cxx_class_find_alias_template(
            cls->templ->templated_class, name);
    }
    if (alias_template) {
        result.alias_template = alias_template;
        result.declaring_class = declaring_class;
        result.access = alias_template->access;
        result.inheritance_access = ACCESS_PUBLIC;
        return result;
    }
    if (cxx_class_find_type_alias(cls, name) ||
        cxx_class_declares_non_alias_name(cls, name) ||
        (cls->templ && cls->templ->templated_class &&
         cls->templ->templated_class != cls &&
         (cxx_class_find_type_alias(cls->templ->templated_class, name) ||
          cxx_class_declares_non_alias_name(cls->templ->templated_class,
                                            name)))) {
        result.found_non_alias = true;
        return result;
    }

    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        CxxAliasTemplateLookupResult candidate;
        AccessSpec access;
        AccessSpec inheritance_access;
        if (!base && cls->bases[index].type_pattern) {
            base = cls->bases[index].type_pattern->cxx_class;
            if (!base && cls->bases[index].type_pattern->cxx_template) {
                base = cls->bases[index].type_pattern->cxx_template
                           ->templated_class;
            }
        }
        if (!base) continue;
        candidate = cxx_class_lookup_alias_template_recursive(
            base, name, &current_path);
        if (candidate.ambiguous) {
            result.ambiguous = true;
            return result;
        }
        if (candidate.found_non_alias) {
            if (result.alias_template) {
                result.ambiguous = true;
                return result;
            }
            result.found_non_alias = true;
        }
        if (!candidate.alias_template) continue;
        if (result.found_non_alias) {
            result.ambiguous = true;
            return result;
        }
        access = candidate.access;
        if (cls->bases[index].access > access) {
            access = cls->bases[index].access;
        }
        inheritance_access = candidate.inheritance_access;
        if (cls->bases[index].access > inheritance_access) {
            inheritance_access = cls->bases[index].access;
        }
        if (!result.alias_template) {
            result.alias_template = candidate.alias_template;
            result.declaring_class = candidate.declaring_class;
            result.access = access;
            result.inheritance_access = inheritance_access;
        } else if (result.alias_template != candidate.alias_template) {
            result.ambiguous = true;
            return result;
        } else if (access < result.access) {
            result.access = access;
        }
        if (inheritance_access < result.inheritance_access) {
            result.inheritance_access = inheritance_access;
        }
    }
    return result;
}

/* Protected class members are available to derived classes only when the
 * derivation path is accessible from the current access context.  In
 * particular, a descendant must not inherit access through a private base
 * edge declared by an intermediate class. */
static bool cxx_class_alias_access_context_is_derived_from(
    CxxClass* context, CxxClass* target, CxxClass* access_context,
    unsigned depth) {
    if (!context || !target || depth > 64u) return false;
    if (context == target) return true;
    for (int index = 0; index < context->base_count; ++index) {
        CxxClass* base = context->bases[index].base;
        if (!base && context->bases[index].type_pattern) {
            base = context->bases[index].type_pattern->cxx_class;
        }
        if (!base) continue;
        if (context != access_context &&
            context->bases[index].access == ACCESS_PRIVATE) {
            continue;
        }
        if (cxx_class_alias_access_context_is_derived_from(
                base, target, access_context, depth + 1u)) {
            return true;
        }
    }
    return false;
}

static bool cxx_class_friend_name_matches(const CxxClass* target,
                                          const CxxClass* candidate,
                                          const char* friend_name) {
    const char* normalized = friend_name;
    const char* namespace_name;
    char qualified_name[512];
    size_t namespace_length;
    size_t class_length;
    if (!target || !candidate || !candidate->name || !friend_name) {
        return false;
    }
    while (normalized[0] == ':' && normalized[1] == ':') normalized += 2;
    if (!strstr(normalized, "::")) {
        return target->ns == candidate->ns &&
               strcmp(normalized, candidate->name) == 0;
    }
    namespace_name = cxx_namespace_qualified_name(candidate->ns);
    namespace_length = namespace_name ? strlen(namespace_name) : 0u;
    class_length = strlen(candidate->name);
    if (namespace_length + (namespace_length ? 2u : 0u) + class_length >=
        sizeof(qualified_name)) {
        return false;
    }
    if (namespace_length) {
        memcpy(qualified_name, namespace_name, namespace_length);
        memcpy(qualified_name + namespace_length, "::", 2u);
        memcpy(qualified_name + namespace_length + 2u, candidate->name,
               class_length + 1u);
    } else {
        memcpy(qualified_name, candidate->name, class_length + 1u);
    }
    return strcmp(normalized, qualified_name) == 0;
}

static bool cxx_class_grants_friendship_to(const CxxClass* target,
                                           const CxxClass* candidate) {
    if (!target || !candidate) return false;
    for (int index = 0; index < target->friend_class_count; ++index) {
        if (cxx_class_friend_name_matches(
                target, candidate, target->friend_class_names[index])) {
            return true;
        }
    }
    return false;
}

CxxClassAliasTemplate* cxx_class_find_inherited_alias_template(
    CxxClass* cls, const char* name, CxxClass* access_context,
    CxxClass** declaring_class, bool* ambiguous, bool* accessible) {
    CxxAliasTemplateLookupResult result;
    if (declaring_class) *declaring_class = NULL;
    if (ambiguous) *ambiguous = false;
    if (accessible) *accessible = false;
    result = cxx_class_lookup_alias_template_recursive(cls, name, NULL);
    if (ambiguous) *ambiguous = result.ambiguous;
    if (declaring_class && result.alias_template && !result.ambiguous) {
        *declaring_class = result.declaring_class;
    }
    if (!result.alias_template || result.ambiguous) return NULL;
    if (accessible) {
        bool friend_of_lookup = cxx_class_grants_friendship_to(
            cls, access_context);
        bool friend_of_declarer = cxx_class_grants_friendship_to(
            result.declaring_class, access_context);
        if (result.alias_template->access == ACCESS_PUBLIC &&
            result.access == ACCESS_PUBLIC) {
            *accessible = true;
        } else if (access_context == result.declaring_class) {
            *accessible = true;
        } else if (access_context == cls &&
                   result.alias_template->access != ACCESS_PRIVATE) {
            /* A class may use a public/protected member inherited through
             * its own private base; that edge restricts descendants, not the
             * class that declared the inheritance. */
            *accessible = true;
        } else if (friend_of_lookup &&
                   (result.declaring_class == cls ||
                    result.alias_template->access != ACCESS_PRIVATE)) {
            /* A friend of the lookup class has that class's access to
             * inherited public/protected members and its own private aliases,
             * but not private aliases declared by a base. */
            *accessible = true;
        } else if (friend_of_declarer &&
                   result.inheritance_access == ACCESS_PUBLIC) {
            /* Friendship of a base grants access to its private aliases only
             * when the name is reachable through public inheritance. */
            *accessible = true;
        } else if (result.alias_template->access != ACCESS_PRIVATE &&
                   result.access != ACCESS_PRIVATE) {
            *accessible = cxx_class_alias_access_context_is_derived_from(
                access_context, result.declaring_class, access_context, 0u);
        }
    }
    return result.alias_template;
}

/* Add field to class */
void cxx_class_add_field_initializer(CxxClass* cls, const char* name,
                                     Type* type, AccessSpec access,
                                     Expr* initializer, bool is_bitfield,
                                     unsigned bit_width, bool is_static,
                                     bool is_deprecated,
                                     const char* deprecated_message,
                                     bool no_unique_address) {
    /* Create field as TypeParam (reusing existing structure) */
    TypeParam* field = rcc_alloc(sizeof(TypeParam));
    field->name = name ? rcc_strdup(name) : NULL;
    field->type = type;
    field->is_bitfield = is_bitfield;
    field->bit_width = bit_width;
    field->is_static = is_static;
    field->initializer = initializer;
    field->is_deprecated = is_deprecated;
    field->deprecated_message = deprecated_message;
    field->cxx_no_unique_address = no_unique_address;
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
        cls, name, type, access, NULL, false, 0u, false, false, NULL, false);
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
        tp->initializer = NULL;
        tp->is_deprecated = false;
        tp->deprecated_message = NULL;
        tp->cxx_no_unique_address = false;
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
    method->is_volatile = false;
    method->ref_qualifier = CXX_REF_QUAL_NONE;
    method->is_constexpr = false;
    method->is_explicit = false;
    method->explicit_condition = NULL;
    method->is_noexcept = false;
    method->is_deleted = false;
    method->is_defaulted = false;
    method->is_constructor = false;
    method->is_destructor = false;
    method->member_template = NULL;
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
    if (!ns || !cls) return;
    for (int index = 0; index < ns->class_count; ++index) {
        if (ns->classes[index] == cls) return;
    }
    cls->ns = ns;
    cls->type->cxx_namespace = cxx_namespace_typeinfo_identity(ns);
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
    tmpl->params[tmpl->param_count].is_pack = false;
    tmpl->params[tmpl->param_count].has_default = false;
    tmpl->params[tmpl->param_count].template_signature = NULL;
    tmpl->params[tmpl->param_count].default_context = NULL;
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
    tmpl->params[tmpl->param_count].is_pack = false;
    tmpl->params[tmpl->param_count].has_default = false;
    tmpl->params[tmpl->param_count].template_signature = NULL;
    tmpl->params[tmpl->param_count].default_context = NULL;
    tmpl->params[tmpl->param_count].default_value = NULL;
    tmpl->param_count++;
}
