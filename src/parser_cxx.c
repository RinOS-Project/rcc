/*
 * RCC++ - RinOS C++ Compiler
 * C++ specific parser extensions
 */

#include "rcc.h"
#include "token.h"
#include "ast.h"
#include "ast_cxx.h"
#include <limits.h>
#include <stdio.h>

/* External parser state (from parser.c) */
typedef struct {
    Token* cur;
    Token* prev;
} Parser;

extern Parser parser;
extern void rcc_parser_set_cxx_template_default_mode(bool enabled);
extern bool rcc_parser_cxx_split_template_close(void);

/* The current template is only needed while parsing dependent declarations;
 * instantiated types are resolved by the later template semantic phase. */
static CxxTemplate* active_template;
static CxxNamespace* active_namespace;
static CxxClass* active_class;
static CxxClass* active_template_class_definition;
static AST* active_ast;
static CxxFriendAccess* active_friend_access_context;
static CxxFriendAccess* active_friend_signature_candidates;

typedef struct CxxFriendSignatureAliasUse {
    CxxClass* owner;
    CxxClassAliasTemplate* alias_template;
    const char* name;
    struct CxxFriendSignatureAliasUse* next;
} CxxFriendSignatureAliasUse;

typedef struct CxxFriendSignatureContext {
    CxxFriendAccess* saved_access_context;
    CxxFriendAccess* saved_candidates;
    CxxFriendSignatureAliasUse* saved_alias_uses;
    bool saved_is_collecting;
} CxxFriendSignatureContext;

static CxxFriendSignatureAliasUse* active_friend_signature_alias_uses;
static bool active_friend_signature_collection;

typedef struct CxxPendingMemberPointerForm {
    Expr* expression;
    CxxClass* owner;
    const char* member_name;
    SourceLoc location;
    struct CxxPendingMemberPointerForm* next;
} CxxPendingMemberPointerForm;

static CxxPendingMemberPointerForm* pending_member_pointer_forms;

const char* rcc_parser_cxx_current_namespace_identity(void) {
    return cxx_namespace_typeinfo_identity(active_namespace);
}

/* A leading alignas belongs to a class declaration only when its complete
 * parenthesized argument list is followed by class-key.  Leave other
 * declarations to the common declaration parser so alignas on objects and
 * functions keeps its existing semantics. */
static bool cxx_leading_alignas_class_starts(void) {
    Token* token = parser.cur;

    while (token && token->type == TOK__ALIGNAS) {
        int depth = 0;
        token = token->next;
        if (!token || token->type != TOK_LPAREN) return false;
        do {
            if (token->type == TOK_LPAREN) {
                ++depth;
            } else if (token->type == TOK_RPAREN) {
                --depth;
            }
            token = token->next;
        } while (token && depth > 0);
        if (depth != 0) return false;
    }
    return token && (token->type == TOK_CLASS || token->type == TOK_STRUCT);
}

static const char* cxx_method_source_name(CxxMethod* method);
static CxxClass* find_class(const char* qualified_name);
static void cxx_record_friend_signature_alias_use(
    CxxClass* owner, const char* name,
    CxxClassAliasTemplate* alias_template) {
    for (CxxFriendSignatureAliasUse* use =
             active_friend_signature_alias_uses;
         use; use = use->next) {
        if (use->owner == owner && use->alias_template == alias_template &&
            use->name && name && strcmp(use->name, name) == 0) {
            return;
        }
    }
    CxxFriendSignatureAliasUse* use = ast_arena_alloc(sizeof(*use));
    use->owner = owner;
    use->alias_template = alias_template;
    use->name = name;
    use->next = active_friend_signature_alias_uses;
    active_friend_signature_alias_uses = use;
}

static CxxClassAliasTemplate* cxx_parser_find_inherited_alias_template(
    CxxClass* owner, const char* name, CxxClass* access_context,
    CxxClass** declaring_class, bool* ambiguous, bool* accessible);
static void cxx_complete_pending_member_pointer_forms(CxxClass* cls);

static bool cxx_inherited_nonfield_name(CxxClass* cls, const char* name,
                                        unsigned depth) {
    if (!cls || !name || depth > 32u) return false;
    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        const char* member_name = member->method
            ? cxx_method_source_name(member->method)
            : (member->decl ? member->decl->name : NULL);
        if (member_name && strcmp(member_name, name) == 0) {
            return true;
        }
    }
    for (TypeParam* field = cls->fields; field; field = field->next) {
        if (field->is_static && field->name &&
            strcmp(field->name, name) == 0) {
            return true;
        }
    }
    for (CxxTypeAlias* alias = cls->type_aliases; alias;
         alias = alias->next) {
        if (alias->name && strcmp(alias->name, name) == 0) return true;
    }
    for (int index = 0; index < cls->base_count; ++index) {
        if (cxx_inherited_nonfield_name(cls->bases[index].base, name,
                                        depth + 1u)) {
            return true;
        }
    }
    return false;
}

static bool cxx_fields_are_same_subobject(TypeField* left,
                                          TypeField* right) {
    if (!left || !right ||
        left->cxx_declaring_class != right->cxx_declaring_class ||
        left->from_virtual_base != right->from_virtual_base) {
        return false;
    }
    if (left->from_virtual_base) {
        return left->virtual_base_owner == right->virtual_base_owner &&
               left->virtual_base_member_offset ==
                   right->virtual_base_member_offset;
    }
    return left->offset == right->offset;
}

static bool cxx_fields_are_same_virtual_subobject(TypeField* left,
                                                  TypeField* right) {
    return left && right && left->from_virtual_base &&
           right->from_virtual_base &&
           left->cxx_declaring_class == right->cxx_declaring_class &&
           left->virtual_base_owner == right->virtual_base_owner &&
           left->virtual_base_member_offset ==
               right->virtual_base_member_offset;
}

static bool cxx_class_layout_ready(CxxClass* cls) {
    if (!cls) return false;
    if (cls->base_count == 0) return true;
    if (!cls->base_offsets) return false;
    for (int index = 0; index < cls->base_count; ++index) {
        if (cls->base_offsets[index] < 0) return false;
    }
    return true;
}

static int cxx_class_direct_base_index(CxxClass* cls, CxxClass* target) {
    int selected = -1;
    if (!cls || !target) return -1;
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        Type* pattern = cls->bases[index].type_pattern;
        const char* base_name = cls->bases[index].base_name;
        if (!base && pattern) base = pattern->cxx_class;
        if (base != target &&
            !(base_name && target->name &&
              strcmp(base_name, target->name) == 0)) {
            continue;
        }
        if (selected >= 0) return -1;
        selected = index;
    }
    return selected;
}

static bool cxx_field_matches_direct_base(CxxClass* aggregate,
                                          CxxClass* base,
                                          TypeField* base_field,
                                          TypeField* field) {
    int base_index;
    if (!aggregate || !base || !base_field || !field) return false;
    base_index = cxx_class_direct_base_index(aggregate, base);
    if (base_index < 0 || !field->name || !base_field->name ||
        strcmp(field->name, base_field->name) != 0 ||
        field->cxx_declaring_class != base_field->cxx_declaring_class) {
        return false;
    }
    if (base_field->from_virtual_base) {
        return field->from_virtual_base &&
               field->virtual_base_owner == base_field->virtual_base_owner &&
               field->virtual_base_member_offset ==
                   base_field->virtual_base_member_offset;
    }
    if (aggregate->bases[base_index].is_virtual) {
        return field->from_virtual_base &&
               field->virtual_base_owner == base &&
               field->virtual_base_member_offset == base_field->offset;
    }
    if (field->from_virtual_base) return false;
    if (!aggregate->base_offsets || aggregate->base_offsets[base_index] < 0) {
        /* In-class member bodies are parsed before the enclosing class's
         * complete layout assigns base offsets.  The caller still rejects
         * repeated matches, so this fallback remains unique-path only. */
        return true;
    }
    return field->offset == aggregate->base_offsets[base_index] +
                                base_field->offset;
}

static TypeField* cxx_class_lookup_data_field(
    CxxClass* cls, const char* name, bool* ambiguous, AccessSpec* access,
    CxxClass** access_owner, bool* used_using);

static TypeField* cxx_member_pointer_using_field(
    CxxClass* owner, const char* name, AccessSpec* access,
    CxxClass** access_owner, bool* declaration_found);

static TypeField* cxx_class_lookup_data_field(
    CxxClass* cls, const char* name, bool* ambiguous, AccessSpec* access,
    CxxClass** access_owner, bool* used_using) {
    TypeField* selected = NULL;
    CxxClass* selected_base = NULL;
    AccessSpec selected_access = ACCESS_PUBLIC;
    CxxClass* selected_access_owner = NULL;
    bool selected_using = false;
    bool using_found = false;
    bool layout_ready = cxx_class_layout_ready(cls);
    if (ambiguous) *ambiguous = false;
    if (used_using) *used_using = false;
    if (!cls || !cls->type || !name) return NULL;
    for (TypeField* field = cls->type->fields; field; field = field->next) {
        if (field->name && strcmp(field->name, name) == 0 &&
            field->cxx_declaring_class == cls) {
            if (access) *access = (AccessSpec)field->cxx_access;
            if (access_owner) *access_owner = cls;
            return field;
        }
    }

    selected = cxx_member_pointer_using_field(
        cls, name, &selected_access, &selected_access_owner, &using_found);
    if (using_found) {
        if (!selected && ambiguous) *ambiguous = true;
        if (selected) {
            if (access) *access = selected_access;
            if (access_owner) *access_owner = selected_access_owner;
            if (used_using) *used_using = true;
        }
        return selected;
    }

    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        const char* member_name = member->method
            ? cxx_method_source_name(member->method)
            : (member->decl ? member->decl->name : NULL);
        if (member_name && strcmp(member_name, name) == 0) return NULL;
    }
    for (TypeParam* field = cls->fields; field; field = field->next) {
        if (field->is_static && field->name &&
            strcmp(field->name, name) == 0) {
            return NULL;
        }
    }
    for (CxxTypeAlias* alias = cls->type_aliases; alias;
         alias = alias->next) {
        if (alias->name && strcmp(alias->name, name) == 0) return NULL;
    }

    for (int base_index = 0; base_index < cls->base_count; ++base_index) {
        CxxClass* base = cls->bases[base_index].base;
        TypeField* base_field;
        TypeField* mapped = NULL;
        AccessSpec base_access = ACCESS_PUBLIC;
        CxxClass* base_access_owner = NULL;
        bool base_ambiguous = false;
        bool base_used_using = false;
        int mapping_count = 0;
        if (!base && cls->bases[base_index].base_name) {
            base = find_class(cls->bases[base_index].base_name);
        }
        if (!base) continue;
        base_field = cxx_class_lookup_data_field(
            base, name, &base_ambiguous, &base_access,
            &base_access_owner, &base_used_using);
        if (base_ambiguous) {
            if (ambiguous) *ambiguous = true;
            return NULL;
        }
        if (!base_field) continue;
        for (TypeField* candidate = cls->type->fields; candidate;
             candidate = candidate->next) {
            if (cxx_field_matches_direct_base(cls, base, base_field,
                                              candidate)) {
                mapped = candidate;
                ++mapping_count;
            }
        }
        if (mapping_count == 0 && !layout_ready) {
            /* Member bodies may be parsed before inherited fields have been
             * copied into the incomplete class layout.  The base declaration
             * still identifies its member type and access metadata. */
            mapped = base_field;
            mapping_count = 1;
        }
        if (mapping_count != 1 || !mapped) {
            if (ambiguous) *ambiguous = true;
            return NULL;
        }
        if (cls->bases[base_index].access > base_access) {
            base_access = cls->bases[base_index].access;
            base_access_owner = cls;
        }
        if (!selected) {
            selected = mapped;
            selected_base = base;
            selected_access = base_access;
            selected_access_owner = base_access_owner;
            selected_using = base_used_using;
        } else if (!layout_ready) {
            if (selected_base != base &&
                !cxx_fields_are_same_virtual_subobject(selected, mapped)) {
                if (ambiguous) *ambiguous = true;
                return NULL;
            }
        } else if (!cxx_fields_are_same_subobject(selected, mapped)) {
                if (ambiguous) *ambiguous = true;
                return NULL;
        }
    }

    if (selected && cxx_inherited_nonfield_name(cls, name, 0u)) {
        if (ambiguous) *ambiguous = true;
        return NULL;
    }
    if (!selected) return NULL;
    if (access) *access = selected_access;
    if (access_owner) *access_owner = selected_access_owner;
    if (used_using) *used_using = selected_using;
    return selected;
}

static TypeField* cxx_member_pointer_using_field(
    CxxClass* owner, const char* name, AccessSpec* access,
    CxxClass** access_owner, bool* declaration_found) {
    TypeField* selected = NULL;
    CxxClass* selected_access_owner = NULL;
    if (declaration_found) *declaration_found = false;
    if (!owner || !name || !owner->type) return NULL;
    for (int index = 0; index < owner->using_base_member_count; ++index) {
        const char* using_name = owner->using_base_members[index].member_name;
        const char* base_name = owner->using_base_members[index].base_name;
        Type* base_pattern = owner->using_base_members[index].base_type_pattern;
        CxxClass* base = base_pattern ? base_pattern->cxx_class : NULL;
        TypeField* base_field;
        TypeField* owner_field = NULL;
        AccessSpec base_access = ACCESS_PUBLIC;
        CxxClass* base_access_owner = NULL;
        bool ambiguous_base_member = false;
        bool base_used_using = false;
        int owner_matches = 0;
        if (!using_name || strcmp(using_name, name) != 0) continue;
        if (declaration_found) *declaration_found = true;
        if (!base && base_name) base = find_class(base_name);
        if (!base) return NULL;
        if (cxx_class_direct_base_index(owner, base) < 0) return NULL;
        base_field = cxx_class_lookup_data_field(
            base, name, &ambiguous_base_member, &base_access,
            &base_access_owner, &base_used_using);
        if (!base_field || ambiguous_base_member) return NULL;
        for (TypeField* candidate = owner->type->fields;
             candidate; candidate = candidate->next) {
            if (cxx_field_matches_direct_base(owner, base, base_field,
                                              candidate)) {
                owner_field = candidate;
                ++owner_matches;
            }
        }
        if (owner_matches == 0 && !cxx_class_layout_ready(owner)) {
            owner_field = base_field;
            owner_matches = 1;
        }
        if (owner_matches != 1 || !owner_field || selected) return NULL;
        selected = owner_field;
        selected_access_owner = owner;
        if (access) *access = owner->using_base_members[index].access;
    }
    if (selected && access_owner) *access_owner = selected_access_owner;
    return selected;
}

static bool cxx_decl_leaf_matches(const char* qualified_name,
                                  const char* member_name) {
    const char* separator;
    const char* leaf;
    if (!qualified_name || !member_name) return false;
    separator = strrchr(qualified_name, ':');
    leaf = separator && separator > qualified_name && separator[-1] == ':'
        ? separator + 1 : qualified_name;
    return strcmp(leaf, member_name) == 0;
}

static void cxx_replace_pending_form(Expr* pending, Expr* resolved) {
    if (pending && resolved) *pending = *resolved;
}

static Expr* cxx_pending_address_of_decl(Decl* declaration,
                                         SourceLoc location) {
    Expr* identifier;
    Expr* address;
    if (!declaration || !declaration->name) return NULL;
    identifier = expr_ident(declaration->name, location);
    identifier->ident_decl = declaration;
    identifier->type = declaration->type;
    address = expr_unary(EXPR_ADDR, identifier, location);
    return address;
}

static void cxx_complete_pending_member_pointer_forms(CxxClass* cls) {
    CxxPendingMemberPointerForm** link = &pending_member_pointer_forms;
    while (*link) {
        CxxPendingMemberPointerForm* pending = *link;
        Expr* expression = pending->expression;
        TypeField* field = NULL;
        TypeField* declaring_field = NULL;
        Type* member_owner = NULL;
        AccessSpec access = ACCESS_PUBLIC;
        CxxClass* access_owner = NULL;
        bool used_using = false;
        bool ambiguous = false;
        int method_count = 0;
        int static_method_count = 0;
        TypeMethod* selected_method = NULL;
        TypeMethod* selected_static_method = NULL;
        if (pending->owner != cls) {
            link = &pending->next;
            continue;
        }

        field = cxx_class_lookup_data_field(
            cls, pending->member_name, &ambiguous, &access,
            &access_owner, &used_using);
        /* A dependent class-template field cannot be assigned a stable
         * pointer-to-member displacement until its specialization is laid
         * out. Preserve the designator for semantic resolution after cloning
         * instead of baking in the primary template's provisional offset. */
        if (active_template_class_definition == cls) {
            TypeParam* template_field;
            for (template_field = cls->fields; template_field;
                 template_field = template_field->next) {
                if (template_field->is_static || !template_field->name ||
                    strcmp(template_field->name,
                           pending->member_name) != 0) {
                    continue;
                }
                Type* member_pointer_type = type_ptr(template_field->type);
                type_cxx_member_pointer(member_pointer_type, cls->type);
                expression->type = member_pointer_type;
                expression->cxx_member_pointer_form = true;
                expression->cxx_member_pointer_form_deferred = true;
                expression->cxx_member_pointer_form_name =
                    pending->member_name;
                expression->cxx_member_pointer_form_access =
                    template_field->cxx_access;
                expression->cxx_member_pointer_form_declaring_class = cls;
                expression->cxx_member_pointer_form_designating_class = cls;
                break;
            }
            if (template_field) {
                *link = pending->next;
                continue;
            }
        }
        if (ambiguous) {
            rcc_error(pending->location,
                      "inherited data-member pointer form requires one unambiguous declaration");
        } else if (field && field->cxx_declaring_class && field->type) {
            CxxClass* declaring_class = field->cxx_declaring_class;
            member_owner = declaring_class->type;
            for (TypeField* candidate = member_owner
                     ? member_owner->fields : NULL;
                 candidate; candidate = candidate->next) {
                if (candidate->name &&
                    strcmp(candidate->name, pending->member_name) == 0 &&
                    candidate->cxx_declaring_class == declaring_class) {
                    declaring_field = candidate;
                    break;
                }
            }
            if (!declaring_field || field->is_bitfield ||
                declaring_field->is_bitfield ||
                field->type->is_reference) {
                rcc_error(pending->location,
                          "this data-member pointer form is unsupported");
            } else {
                Type* member_pointer_type = type_ptr(field->type);
                type_cxx_member_pointer(member_pointer_type, member_owner);
                expression->int_val = declaring_field->offset;
                expression->type = member_pointer_type;
                expression->cxx_member_pointer_form = true;
                expression->cxx_member_pointer_form_access = used_using
                    ? (unsigned char)access
                    : declaring_field->cxx_access;
                expression->cxx_member_pointer_form_declaring_class =
                    used_using && access_owner ? access_owner
                                               : declaring_class;
                expression->cxx_member_pointer_form_designating_class = cls;
            }
        } else {
            TypeParam* static_field = NULL;
            Decl* static_declaration = NULL;
            for (TypeParam* candidate = cls->fields;
                 candidate; candidate = candidate->next) {
                if (candidate->is_static && candidate->name &&
                    strcmp(candidate->name, pending->member_name) == 0) {
                    static_field = candidate;
                    break;
                }
            }
            if (static_field) {
                for (struct CxxMember* member = cls->members;
                     member; member = member->next) {
                    if (!member->method && member->decl &&
                        member->decl->kind == DECL_VAR &&
                        cxx_decl_leaf_matches(member->decl->name,
                                              pending->member_name)) {
                        static_declaration = member->decl;
                        break;
                    }
                }
                if (static_declaration) {
                    cxx_replace_pending_form(
                        expression,
                        cxx_pending_address_of_decl(static_declaration,
                                                    pending->location));
                } else {
                    rcc_error(pending->location,
                              "static data-member declaration is not available in this context");
                }
            } else {
                for (TypeMethod* method = cls->type->methods;
                     method; method = method->next) {
                    if (method->kind == TYPE_METHOD_FUNCTION &&
                        method->name &&
                        strcmp(method->name, pending->member_name) == 0 &&
                        method->function_decl) {
                        if (method->function_decl->func_this_param) {
                            selected_method = method;
                            ++method_count;
                        } else {
                            selected_static_method = method;
                            ++static_method_count;
                        }
                    }
                }
                if (method_count > 1) {
                    Expr* address = expr_unary(
                        EXPR_ADDR,
                        expr_ident(pending->member_name, pending->location),
                        pending->location);
                    address->cxx_member_pointer_form = true;
                    address->cxx_member_pointer_form_overload_set = true;
                    address->cxx_member_pointer_form_designating_class = cls;
                    cxx_replace_pending_form(expression, address);
                } else if (method_count == 1 && selected_method &&
                           selected_method->function_decl) {
                    Decl* function_decl = selected_method->function_decl;
                    if (!function_decl->link_name) {
                        rcc_error(
                            pending->location,
                            "pointer-to-member function has no emitted method symbol");
                    } else {
                        Expr* address = cxx_pending_address_of_decl(
                            function_decl, pending->location);
                        address->cxx_member_pointer_form = true;
                        address->cxx_member_pointer_form_access =
                            selected_method->cxx_access;
                        address->cxx_member_pointer_form_declaring_class =
                            selected_method->cxx_access_owner
                                ? selected_method->cxx_access_owner
                                : function_decl->func_method_owner
                                    ? function_decl->func_method_owner->cxx_class
                                    : cls;
                        address->cxx_member_pointer_form_designating_class =
                            cls;
                        address->cxx_member_function_pointer_method =
                            selected_method;
                        cxx_replace_pending_form(expression, address);
                    }
                } else if (method_count == 0 &&
                           static_method_count == 1 &&
                           selected_static_method &&
                           selected_static_method->function_decl) {
                    cxx_replace_pending_form(
                        expression,
                        cxx_pending_address_of_decl(
                            selected_static_method->function_decl,
                            pending->location));
                } else if (method_count == 0 && static_method_count > 1 &&
                           selected_static_method &&
                           selected_static_method->function_decl) {
                    Expr* identifier = expr_ident(
                        selected_static_method->function_decl->name,
                        pending->location);
                    cxx_replace_pending_form(
                        expression,
                        expr_unary(EXPR_ADDR, identifier,
                                   pending->location));
                } else {
                    rcc_error(pending->location,
                              "undefined identifier '%s::%s'",
                              cls->name ? cls->name : "<class>",
                              pending->member_name);
                }
            }
        }

        *link = pending->next;
    }
}

/* Parse `&Class::member` as a pointer-to-member constant.  Data members use
 * the bounded offset representation below.  The initial function-member
 * subset is restricted to one defined, non-virtual, non-overloaded method;
 * its ordinary code address is paired with the implicit object argument at
 * each call site. */
Expr* rcc_parse_cxx_member_pointer_address(void) {
    Token* cursor = parser.cur;
    Token* segments[32];
    size_t segment_count = 0u;
    char owner_name[256];
    size_t owner_length = 0u;
    Type* owner;
    CxxClass* owner_class;
    Type* member_owner;
    TypeField* field;
    TypeField* declaring_field = NULL;
    Expr* value;
    Type* member_pointer_type;
    SourceLoc loc;
    bool owner_is_active_class = false;
    bool direct_member = false;
    bool direct_nonstatic_method = false;
    bool inherited_nonstatic_method = false;
    bool direct_static_name = false;
    bool used_base_member = false;
    bool inherited_lookup_ambiguous = false;
    AccessSpec lookup_access = ACCESS_PUBLIC;
    CxxClass* lookup_access_owner = NULL;
    int matching_fields = 0;

    if (!cursor || cursor->type != TOK_IDENT) return NULL;
    while (cursor && cursor->type == TOK_IDENT) {
        if (segment_count >= sizeof(segments) / sizeof(segments[0])) {
            return NULL;
        }
        segments[segment_count++] = cursor;
        if (!cursor->next || cursor->next->type != TOK_SCOPE ||
            !cursor->next->next ||
            cursor->next->next->type != TOK_IDENT) {
            break;
        }
        cursor = cursor->next->next;
    }
    if (segment_count < 2u) return NULL;

    for (size_t index = 0u; index + 1u < segment_count; ++index) {
        size_t length = strlen(segments[index]->value.str_val);
        size_t separator = index + 2u < segment_count ? 2u : 0u;
        if (owner_length + length + separator >= sizeof(owner_name)) {
            return NULL;
        }
        memcpy(owner_name + owner_length, segments[index]->value.str_val,
               length);
        owner_length += length;
        if (separator) {
            memcpy(owner_name + owner_length, "::", 2u);
            owner_length += 2u;
        }
    }
    owner_name[owner_length] = '\0';
    owner = rcc_parser_lookup_type(owner_name);
    if (active_class && active_class->type && active_class->name &&
        strcmp(owner_name, active_class->name) == 0) {
        owner = active_class->type;
        owner_is_active_class = true;
    } else if (owner && active_class &&
               owner == active_class->type) {
        owner_is_active_class = true;
    }
    if (!owner || (owner->kind != TYPE_STRUCT &&
                   owner->kind != TYPE_UNION) ||
        (!owner->is_complete && !owner_is_active_class)) {
        return NULL;
    }
    owner_class = owner_is_active_class ? active_class : owner->cxx_class;

    if (owner_class) {
        const char* member_name =
            segments[segment_count - 1u]->value.str_val;
        for (TypeParam* declared = owner_class->fields;
             declared; declared = declared->next) {
            if (declared->name && strcmp(declared->name, member_name) == 0) {
                if (declared->is_static) direct_static_name = true;
                else direct_member = true;
                break;
            }
        }
        for (struct CxxMember* member = owner_class->members;
             member; member = member->next) {
            const char* declared_name = member->method
                ? member->method->source_name
                : (member->decl ? member->decl->name : NULL);
            if (!declared_name ||
                strcmp(declared_name, member_name) != 0) {
                continue;
            }
            if (member->is_static) direct_static_name = true;
            else direct_nonstatic_method = true;
        }
        for (CxxTypeAlias* alias = owner_class->type_aliases; alias;
             alias = alias->next) {
            if (alias->name && strcmp(alias->name, member_name) == 0) {
                direct_static_name = true;
            }
        }
        for (TypeMethod* method = owner->methods; method;
             method = method->next) {
            Decl* function = method->function_decl;
            CxxClass* declaring_class = function &&
                    function->func_method_owner
                ? function->func_method_owner->cxx_class : NULL;
            if (method->kind == TYPE_METHOD_FUNCTION && method->name &&
                strcmp(method->name, member_name) == 0 && function &&
                function->func_this_param && declaring_class &&
                declaring_class != owner_class) {
                inherited_nonstatic_method = true;
                break;
            }
        }
    }

    /* Member bodies are parsed before the complete class lookup set and
     * field offsets exist.  Defer a current-class member designator until
     * layout and method registration have published that information. */
    if (owner_is_active_class && !direct_static_name) {
        CxxPendingMemberPointerForm* pending;
        Expr* deferred;
        loc = segments[segment_count - 1u]->loc;
        parser.prev = segments[segment_count - 1u];
        parser.cur = parser.prev->next;
        deferred = expr_int(0, loc);
        deferred->type = type_int;
        pending = ast_arena_alloc(sizeof(*pending));
        pending->expression = deferred;
        pending->owner = owner_class;
        pending->member_name = rcc_intern(
            segments[segment_count - 1u]->value.str_val);
        pending->location = loc;
        pending->next = pending_member_pointer_forms;
        pending_member_pointer_forms = pending;
        return deferred;
    }

    field = NULL;
    if (!direct_static_name && !direct_nonstatic_method &&
        owner_class) {
        field = cxx_class_lookup_data_field(
            owner_class, segments[segment_count - 1u]->value.str_val,
            &inherited_lookup_ambiguous, &lookup_access,
            &lookup_access_owner, &used_base_member);
        if (inherited_lookup_ambiguous) {
            loc = segments[segment_count - 1u]->loc;
            parser.prev = segments[segment_count - 1u];
            parser.cur = parser.prev->next;
            rcc_error(loc,
                      "inherited data-member pointer form requires one unambiguous declaration");
            value = expr_int(0, loc);
            value->type = type_int;
            return value;
        }
    } else if (!direct_static_name && !direct_nonstatic_method) {
        field = owner->fields;
        while (field && strcmp(field->name, segments[segment_count - 1u]
                                               ->value.str_val) != 0) {
            field = field->next;
        }
    }
    /* The completed TypeField list still contains inherited storage even
     * when a declaration in the designating class hides that name.  In
     * particular, a directly declared member function hides a base data
     * member and must continue through member-function lookup below. */
    if (field && !direct_member && direct_nonstatic_method) field = NULL;
    if (!field) {
        if (direct_static_name && !direct_nonstatic_method &&
            !inherited_nonstatic_method) return NULL;
        if (direct_nonstatic_method || inherited_nonstatic_method) {
            TypeMethod* method = NULL;
            TypeMethod* selected_method = NULL;
            Decl* function_decl = NULL;
            int matching_methods = 0;
            const char* member_name =
                segments[segment_count - 1u]->value.str_val;
            for (TypeMethod* candidate = owner->methods; candidate;
                 candidate = candidate->next) {
                if (candidate->kind != TYPE_METHOD_FUNCTION ||
                    !candidate->name ||
                    strcmp(candidate->name, member_name) != 0 ||
                    !candidate->function_decl ||
                    !candidate->function_decl->func_this_param) {
                    continue;
                }
                ++matching_methods;
                selected_method = candidate;
                function_decl = candidate->function_decl;
            }
            method = selected_method;
            loc = segments[segment_count - 1u]->loc;
            parser.prev = segments[segment_count - 1u];
            parser.cur = parser.prev->next;
            if (matching_methods > 1) {
                value = expr_unary(
                    EXPR_ADDR, expr_ident(member_name, loc), loc);
                value->cxx_member_pointer_form = true;
                value->cxx_member_pointer_form_overload_set = true;
                value->cxx_member_pointer_form_designating_class =
                    owner_class;
                return value;
            }
            if (matching_methods != 1 || !method || !function_decl ||
                !function_decl->func_this_param ||
                !function_decl->link_name) {
                rcc_error(loc,
                          "pointer-to-member function requires one registered method overload");
                return NULL;
            }
            if (!function_decl->func_this_param->type ||
                !function_decl->func_this_param->type->base) {
                rcc_error(loc,
                          "pointer-to-member function has incomplete object qualification metadata");
                return NULL;
            }
            value = expr_unary(
                EXPR_ADDR,
                expr_ident(function_decl->name, loc), loc);
            value->unary_operand->ident_decl = function_decl;
            value->unary_operand->type = function_decl->type;
            value->cxx_member_pointer_form = true;
            value->cxx_member_pointer_form_access =
                method->cxx_access;
            value->cxx_member_pointer_form_declaring_class =
                method->cxx_access_owner
                    ? method->cxx_access_owner
                    : function_decl->func_method_owner
                        ? function_decl->func_method_owner->cxx_class
                        : owner_class;
            value->cxx_member_pointer_form_designating_class =
                owner_class;
            value->cxx_member_function_pointer_method = method;
            return value;
        }
        return NULL;
    }
    if (!direct_member && direct_nonstatic_method) {
        loc = segments[segment_count - 1u]->loc;
        parser.prev = segments[segment_count - 1u];
        parser.cur = parser.prev->next;
        rcc_error(loc,
                  "pointer-to-member function name could not be resolved");
        return NULL;
    }
    if (!direct_member && direct_static_name) {
        return NULL;
    }
    if (!direct_member) {
        if (owner_class) {
            matching_fields = 1;
        } else {
            matching_fields = 0;
            for (TypeField* candidate = owner->fields; candidate;
                 candidate = candidate->next) {
                if (!candidate->name ||
                    strcmp(candidate->name,
                           segments[segment_count - 1u]->value.str_val) != 0) {
                    continue;
                }
                field = candidate;
                ++matching_fields;
            }
        }
        if (matching_fields != 1 || !field ||
            !field->cxx_declaring_class) {
            loc = segments[segment_count - 1u]->loc;
            parser.prev = segments[segment_count - 1u];
            parser.cur = parser.prev->next;
            rcc_error(loc,
                      "inherited data-member pointer form requires one unambiguous declaration");
            value = expr_int(0, loc);
            value->type = type_int;
            return value;
        }
        member_owner = field->cxx_declaring_class->type;
        for (declaring_field = member_owner ? member_owner->fields : NULL;
             declaring_field; declaring_field = declaring_field->next) {
            if (declaring_field->name &&
                strcmp(declaring_field->name,
                       segments[segment_count - 1u]->value.str_val) == 0 &&
                declaring_field->cxx_declaring_class ==
                    field->cxx_declaring_class) {
                break;
            }
        }
        if (!member_owner || !declaring_field) {
            loc = segments[segment_count - 1u]->loc;
            parser.prev = segments[segment_count - 1u];
            parser.cur = parser.prev->next;
            rcc_error(loc,
                      "inherited data-member pointer declaration metadata is incomplete");
            value = expr_int(0, loc);
            value->type = type_int;
            return value;
        }
    } else {
        member_owner = owner;
        declaring_field = field;
    }
    if (declaring_field->is_bitfield || !field->type ||
        field->type->is_reference) {
        loc = segments[segment_count - 1u]->loc;
        parser.prev = segments[segment_count - 1u];
        parser.cur = parser.prev->next;
        rcc_error(loc,
                  "this data-member pointer form is unsupported");
        value = expr_int(0, loc);
        value->type = type_int;
        return value;
    }

    loc = segments[segment_count - 1u]->loc;
    parser.prev = segments[segment_count - 1u];
    parser.cur = parser.prev->next;
    value = expr_int(declaring_field->offset, loc);
    member_pointer_type = type_ptr(field->type);
    type_cxx_member_pointer(member_pointer_type, member_owner);
    value->type = member_pointer_type;
    value->cxx_member_pointer_form = true;
    value->cxx_member_pointer_form_access = used_base_member
        ? (unsigned char)lookup_access
        : declaring_field->cxx_access;
    value->cxx_member_pointer_form_declaring_class =
        used_base_member
            ? (lookup_access_owner ? lookup_access_owner : owner_class)
            : member_owner->cxx_class;
    value->cxx_member_pointer_form_designating_class = owner->cxx_class;
    return value;
}

void rcc_parser_validate_cxx_object_type(Type* type, SourceLoc loc) {
    Type* object_type = type;
    if (!object_type || object_type->is_reference) return;
    while (object_type && object_type->kind == TYPE_ARRAY) {
        object_type = object_type->base;
    }
    if (object_type && object_type->cxx_class &&
        cxx_class_is_abstract(object_type->cxx_class)) {
        rcc_error(loc, "cannot instantiate abstract class '%s'",
                  object_type->cxx_class->name
                      ? object_type->cxx_class->name : "<anonymous>");
    }
}

typedef struct CxxParserValueBinding {
    const char* name;
    Type* type;
    struct CxxParserValueBinding* next;
} CxxParserValueBinding;

typedef struct CxxLocalUsingBinding {
    const char* name;
    const char* target;
    bool namespace_import;
    struct CxxLocalUsingBinding* next;
} CxxLocalUsingBinding;

typedef struct CxxReferenceCapture {
    const char* name;
    struct CxxReferenceCapture* next;
} CxxReferenceCapture;

typedef struct CxxLambdaCaptureSpec {
    const char* name;
    SourceLoc loc;
    bool reference;
    Expr* initializer;
    struct CxxLambdaCaptureSpec* next;
} CxxLambdaCaptureSpec;

static CxxParserValueBinding* active_value_bindings;
static CxxLocalUsingBinding* active_local_using_bindings;
static CxxParserValueBinding* saved_value_bindings[32];
static int saved_value_binding_depth;
static CxxReferenceCapture* active_reference_captures;
static CxxReferenceCapture* saved_reference_captures[32];
static int saved_reference_capture_depth;
static unsigned cxx_lambda_counter;
static unsigned cxx_range_for_counter;
static unsigned cxx_structured_binding_counter;

static int cxx_class_pack_index(CxxTemplate* tmpl);
static Type* cxx_parser_value_type(const char* name);
static Type* cxx_decltype_member_type(Type* object_type,
                                      const char* member_name,
                                      SourceLoc loc);
static bool cxx_template_constraint_satisfied(
    CxxTemplate* tmpl, Type** arguments, const int64_t* values,
    const bool* value_present, SourceLoc loc, bool report_errors,
    bool* unsupported);

static bool cxx_type_is_aggregate(Type* type) {
    return ast_cxx_is_aggregate(type);
}

/* `T value(args)` is ambiguous with a function declaration when T is an
 * aggregate.  Keep the standard most-vexing-parse rule for parameter-shaped
 * lists, while still accepting expression-shaped C++20 aggregate
 * initialization such as `Pair value(1, 2)` and `Pair value(int(1), 2)`. */
static bool cxx_paren_looks_like_function_parameters(void) {
    Token* token;

    if (!parser.cur || !parser.cur->next ||
        parser.cur->next->type != TOK_LPAREN) return false;
    token = parser.cur->next->next;
    if (!token || token->type == TOK_RPAREN) return true;

    while (token->type == TOK_CONST || token->type == TOK_VOLATILE ||
           token->type == TOK_RESTRICT) {
        token = token->next;
    }
    if (!token) return false;

    switch (token->type) {
        case TOK_VOID:
        case TOK_BOOL:
        case TOK_CHAR8_T:
        case TOK_CHAR:
        case TOK_SHORT:
        case TOK_INT:
        case TOK_LONG:
        case TOK_SIGNED:
        case TOK_UNSIGNED:
        case TOK_FLOAT:
        case TOK_DOUBLE:
        case TOK___BUILTIN_VA_LIST:
        case TOK_STRUCT:
        case TOK_CLASS:
        case TOK_ENUM:
        case TOK_DECLTYPE:
        case TOK_AUTO:
            break;
        case TOK_IDENT:
            if (!rcc_parser_lookup_type(token->value.str_val)) return false;
            break;
        default:
            return false;
    }

    token = token->next;
    if (!token) return false;
    if (token->type == TOK_IDENT || token->type == TOK_STAR ||
        token->type == TOK_AMP || token->type == TOK_AND ||
        token->type == TOK_RPAREN || token->type == TOK_COMMA ||
        token->type == TOK_ELLIPSIS || token->type == TOK_LBRACKET) {
        return true;
    }
    if (token->type == TOK_LPAREN && token->next &&
        (token->next->type == TOK_STAR || token->next->type == TOK_AMP ||
         token->next->type == TOK_AND || token->next->type == TOK_IDENT)) {
        return true;
    }
    return false;
}

static bool cxx_standard_feature_tokens_valid(Token* head) {
    for (Token* token = head; token && token->type != TOK_EOF;
         token = token->next) {
        if (token->type == TOK_CONSTEVAL &&
            !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(token->loc, "consteval requires C++20 or newer");
        } else if (token->type == TOK_CONSTINIT &&
                   !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(token->loc, "constinit requires C++20 or newer");
        } else if (token->type == TOK_CONCEPT &&
                   !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(token->loc, "concept declarations require C++20 or newer");
        } else if (token->type == TOK_CHAR8_T &&
                   !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(token->loc, "char8_t requires C++20 or newer");
        } else if (token->type == TOK_SPACESHIP &&
                   !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(token->loc, "operator<=> requires C++20 or newer");
        } else if (token->type == TOK_REQUIRES &&
                   !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(token->loc, "requires-expressions and requires-clauses require C++20 or newer");
        } else if (token->type == TOK_IF && token->next &&
                   token->next->type == TOK_CONSTEXPR &&
                   !rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(token->loc, "if constexpr requires C++17 or newer");
        } else if (token->type == TOK_USING && token->next &&
                   token->next->type == TOK_ENUM &&
                   !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(token->loc, "using enum requires C++20 or newer");
        } else if (token->type == TOK_AUTO) {
            Token* next = token->next;
            if (next && (next->type == TOK_AMP || next->type == TOK_AND)) {
                next = next->next;
            }
            if (next && next->type == TOK_LBRACKET &&
                !rcc_parser_cxx_standard_at_least(17)) {
                rcc_error(token->loc, "structured bindings require C++17 or newer");
            }
        }
    }
    return g_error_count == 0;
}

/* Lambda init-captures are lowered as hidden call parameters.  Their
 * parameter type must be known while the lambda function declaration is
 * built, before the enclosing function is semantically analyzed.  Keep this
 * inference deliberately structural: expressions that need overload
 * resolution or a deferred template type are rejected instead of receiving a
 * guessed recovery type. */
static Type* cxx_lambda_capture_expression_type(Expr* expression) {
    Type* left;
    Type* right;
    if (!expression) return NULL;
    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_CHAR_LIT:
            return type_int;
        case EXPR_FLOAT_LIT:
            return expression->type && expression->type->kind == TYPE_FLOAT
                ? expression->type : type_double;
        case EXPR_IDENT:
            return cxx_parser_value_type(expression->ident_name);
        case EXPR_STRING_LIT:
            return type_array(type_char,
                              (int)expression->str_length + 1);
        case EXPR_CAST:
            return expression->cast_type;
        case EXPR_ADDR:
            left = cxx_lambda_capture_expression_type(
                expression->unary_operand);
            return left ? type_ptr(left) : NULL;
        case EXPR_DEREF:
            left = cxx_lambda_capture_expression_type(
                expression->unary_operand);
            return left && left->kind == TYPE_PTR ? left->base : NULL;
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            right = cxx_lambda_capture_expression_type(
                expression->binary_rhs);
            return right && right->kind == TYPE_PTR &&
                           right->cxx_is_member_pointer
                ? right->base : NULL;
        case EXPR_COND:
            left = cxx_lambda_capture_expression_type(expression->cond_then);
            right = cxx_lambda_capture_expression_type(expression->cond_else);
            return left && right && type_is_compatible(left, right)
                ? left : NULL;
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
            return cxx_lambda_capture_expression_type(expression->binary_lhs);
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_LE:
        case EXPR_GT:
        case EXPR_GE:
        case EXPR_AND:
        case EXPR_OR:
            return type_int;
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
            left = cxx_lambda_capture_expression_type(expression->binary_lhs);
            right = cxx_lambda_capture_expression_type(expression->binary_rhs);
            if (!left || !right) return NULL;
            if (left->kind == TYPE_DOUBLE || right->kind == TYPE_DOUBLE) {
                return type_double;
            }
            if (left->kind == TYPE_FLOAT || right->kind == TYPE_FLOAT) {
                return type_float;
            }
            return left;
        case EXPR_COMPOUND:
            return expression->compound_type;
        default:
            return NULL;
    }
}

static void cxx_parser_expr_loc(SourceLoc* location, const Expr* expression,
                                const SourceLoc* fallback) {
    if (!location) return;
    location->filename = NULL;
    location->line = 0;
    location->column = 0;
    if (expression) {
        location->filename = expression->loc.filename;
        location->line = expression->loc.line;
        location->column = expression->loc.column;
    } else if (fallback) {
        location->filename = fallback->filename;
        location->line = fallback->line;
        location->column = fallback->column;
    }
}

void rcc_parser_cxx_begin_function_parameters(DeclList* parameters) {
    if (saved_value_binding_depth >=
        (int)(sizeof(saved_value_bindings) / sizeof(saved_value_bindings[0]))) {
        rcc_fatal("C++ parser function nesting is too deep");
    }
    saved_value_bindings[saved_value_binding_depth++] = active_value_bindings;
    active_value_bindings = NULL;
    for (DeclList* item = parameters; item; item = item->next) {
        if (item->decl && item->decl->name) {
            CxxParserValueBinding* binding = ast_arena_alloc(sizeof(*binding));
            binding->name = item->decl->name;
            binding->type = item->decl->type;
            binding->next = active_value_bindings;
            active_value_bindings = binding;
        }
    }
}

void rcc_parser_cxx_end_function_parameters(void) {
    if (saved_value_binding_depth <= 0) {
        rcc_fatal("C++ parser function binding stack underflow");
    }
    active_value_bindings =
        saved_value_bindings[--saved_value_binding_depth];
}

void rcc_parser_cxx_add_value_binding(const char* name, Type* type) {
    CxxParserValueBinding* binding;
    if (!name || !*name || !type) return;
    for (binding = active_value_bindings; binding; binding = binding->next) {
        if (binding->name && strcmp(binding->name, name) == 0) return;
    }
    binding = ast_arena_alloc(sizeof(*binding));
    binding->name = name;
    binding->type = type;
    binding->next = active_value_bindings;
    active_value_bindings = binding;
}

static Type* cxx_parser_value_type(const char* name) {
    for (CxxParserValueBinding* binding = active_value_bindings;
         binding; binding = binding->next) {
        if (binding->name && name && strcmp(binding->name, name) == 0) {
            return binding->type;
        }
    }
    return NULL;
}

void* rcc_parser_cxx_using_scope_mark(void) {
    return active_local_using_bindings;
}

void rcc_parser_cxx_using_scope_restore(void* mark) {
    active_local_using_bindings = (CxxLocalUsingBinding*)mark;
}

static void cxx_parser_add_local_using(const char* name,
                                       const char* target,
                                       bool namespace_import,
                                       SourceLoc loc) {
    CxxLocalUsingBinding* binding;
    if (!target || !*target) {
        rcc_error(loc, "local using-declaration has no target");
        return;
    }
    binding = ast_arena_alloc(sizeof(*binding));
    binding->name = name;
    binding->target = target;
    binding->namespace_import = namespace_import;
    binding->next = active_local_using_bindings;
    active_local_using_bindings = binding;
}

const char* rcc_parser_cxx_resolve_local_using(const char* name,
                                               SourceLoc loc) {
    const char* resolved = NULL;
    char qualified[512];
    if (!name || !*name || cxx_parser_value_type(name)) return NULL;
    for (CxxLocalUsingBinding* binding = active_local_using_bindings;
         binding; binding = binding->next) {
        const char* candidate = NULL;
        if (!binding->namespace_import) {
            if (!binding->name || strcmp(binding->name, name) != 0) {
                continue;
            }
            candidate = binding->target;
        } else {
            size_t target_length = strlen(binding->target);
            if (target_length != 0u &&
                target_length + 2u + strlen(name) >= sizeof(qualified)) {
                rcc_error(loc, "local using target exceeds compiler limits");
                return rcc_intern("__rcc_invalid_local_using");
            }
            if (target_length == 0u) {
                candidate = rcc_intern(name);
            } else {
                memcpy(qualified, binding->target, target_length);
                memcpy(qualified + target_length, "::", 2u);
                strcpy(qualified + target_length + 2u, name);
                candidate = rcc_intern(qualified);
            }
        }
        if (!resolved) {
            resolved = candidate;
        } else if (strcmp(resolved, candidate) != 0) {
            rcc_error(loc, "ambiguous local using-declaration for '%s'", name);
            return rcc_intern("__rcc_invalid_local_using");
        }
    }
    return resolved ? rcc_intern(resolved) : NULL;
}

/* Function-template deduction happens while the source is still being
 * parsed, before the normal semantic pass has assigned types to every
 * expression.  Recover the type of the expression forms whose result is
 * already determined by the parsed AST.  Returning NULL is intentional: it
 * keeps genuinely dependent or unresolved expressions on the diagnostic
 * path instead of inventing a recovery type. */
static Type* cxx_parser_expression_type(Expr* expression) {
    Type* left;
    Type* right;
    Type* function_type;
    if (!expression) return NULL;
    if (expression->type) return expression->type;

    switch (expression->kind) {
        case EXPR_IDENT:
            if (expression->ident_decl && expression->ident_decl->type) {
                return expression->ident_decl->type;
            }
            left = cxx_parser_value_type(expression->ident_name);
            if (left) return left;
            for (DeclList* item = active_ast ? active_ast->decls : NULL;
                 item; item = item->next) {
                Decl* declaration = item->decl;
                const char* name = declaration ? declaration->name : NULL;
                const char* tail = name ? strrchr(name, ':') : NULL;
                tail = tail && tail > name && tail[-1] == ':'
                    ? tail + 1 : name;
                if (declaration && declaration->type &&
                    ((name && expression->ident_name &&
                      strcmp(name, expression->ident_name) == 0) ||
                     (tail && expression->ident_name &&
                      strcmp(tail, expression->ident_name) == 0))) {
                    return declaration->type;
                }
            }
            return NULL;

        case EXPR_CAST:
            return expression->cast_type;

        case EXPR_CALL:
            function_type = cxx_parser_expression_type(
                expression->call_func);
            if (function_type && function_type->kind == TYPE_PTR) {
                function_type = function_type->base;
            }
            return function_type && function_type->kind == TYPE_FUNC
                ? function_type->ret_type : NULL;

        case EXPR_ADDR:
            left = cxx_parser_expression_type(expression->unary_operand);
            return left ? type_ptr(left) : NULL;

        case EXPR_DEREF:
            left = cxx_parser_expression_type(expression->unary_operand);
            return left && left->kind == TYPE_PTR ? left->base : NULL;

        case EXPR_INDEX:
            left = cxx_parser_expression_type(expression->index_base);
            if (left && left->kind == TYPE_ARRAY) return left->base;
            return left && left->kind == TYPE_PTR ? left->base : NULL;

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            left = cxx_parser_expression_type(expression->member_base);
            if (expression->kind == EXPR_PTR_MEMBER && left &&
                left->kind == TYPE_PTR) {
                left = left->base;
            }
            for (TypeField* field = left ? left->fields : NULL;
                 field; field = field->next) {
                if (field->name && expression->member_name &&
                    strcmp(field->name, expression->member_name) == 0) {
                    return field->type;
                }
            }
            for (TypeMethod* method = left ? left->methods : NULL;
                 method; method = method->next) {
                if (method->name && expression->member_name &&
                    strcmp(method->name, expression->member_name) == 0) {
                    return method->function_decl &&
                               method->function_decl->type
                        ? method->function_decl->type
                        : type_func(method->return_type, NULL, false);
                }
            }
            return NULL;

        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            right = cxx_parser_expression_type(expression->binary_rhs);
            return right && right->kind == TYPE_PTR &&
                           right->cxx_is_member_pointer
                ? right->base : NULL;

        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            return cxx_parser_expression_type(expression->unary_operand);

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
            left = cxx_parser_expression_type(expression->binary_lhs);
            right = cxx_parser_expression_type(expression->binary_rhs);
            if ((expression->kind == EXPR_ADD ||
                 expression->kind == EXPR_SUB) && left && right) {
                if (left->kind == TYPE_PTR) return left;
                if (right->kind == TYPE_PTR && expression->kind == EXPR_ADD) {
                    return right;
                }
            }
            if (left && right && type_is_arithmetic(left) &&
                type_is_arithmetic(right)) {
                return type_common(left, right);
            }
            return left;

        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_AND:
        case EXPR_OR:
            return type_int;

        case EXPR_COND:
            left = cxx_parser_expression_type(expression->cond_then);
            right = cxx_parser_expression_type(expression->cond_else);
            if (left && right && type_is_arithmetic(left) &&
                type_is_arithmetic(right)) {
                return type_common(left, right);
            }
            return left && right && type_is_compatible(left, right)
                ? left : NULL;

        case EXPR_COMMA:
            return cxx_parser_expression_type(expression->binary_rhs);

        case EXPR_COMPOUND:
            return expression->compound_type;

        default:
            return NULL;
    }
}

/* Reference captures are represented by pointer parameters in the lowered
 * immediate-call ABI.  Rewrite an occurrence in the lambda body to a real
 * dereference so reads and writes observe the original object. */
Expr* rcc_parser_cxx_capture_expression(const char* name, SourceLoc loc) {
    for (CxxReferenceCapture* capture = active_reference_captures;
         capture; capture = capture->next) {
        if (capture->name && name && strcmp(capture->name, name) == 0) {
            return expr_unary(EXPR_DEREF, expr_ident(name, loc), loc);
        }
    }
    return NULL;
}

/* Parser utilities from parser.c */
static Token* peek(void) { return parser.cur; }
static Token* previous(void) { return parser.prev; }
static bool check(TokenType type) { return peek()->type == type; }
static bool at_end(void) { return check(TOK_EOF); }

static Token* advance(void) {
    if (!at_end()) {
        parser.prev = parser.cur;
        parser.cur = parser.cur->next;
    }
    return previous();
}

static bool match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

static Token* expect(TokenType type, const char* msg) {
    if (type == TOK_GT && check(TOK_RSHIFT)) {
        (void)rcc_parser_cxx_split_template_close();
    }
    if (check(type)) {
        return advance();
    }
    rcc_error(peek()->loc, "expected %s, got '%s'", msg, token_type_str(peek()->type));
    return NULL;
}

static ExprKind cxx_fold_operator_kind(TokenType token) {
    switch (token) {
        case TOK_PLUS: return EXPR_ADD;
        case TOK_MINUS: return EXPR_SUB;
        case TOK_STAR: return EXPR_MUL;
        case TOK_SLASH: return EXPR_DIV;
        case TOK_PERCENT: return EXPR_MOD;
        case TOK_AMP: return EXPR_BITAND;
        case TOK_PIPE: return EXPR_BITOR;
        case TOK_CARET: return EXPR_BITXOR;
        case TOK_LSHIFT: return EXPR_LSHIFT;
        case TOK_RSHIFT: return EXPR_RSHIFT;
        case TOK_EQ: return EXPR_EQ;
        case TOK_NE: return EXPR_NE;
        case TOK_LT: return EXPR_LT;
        case TOK_GT: return EXPR_GT;
        case TOK_LE: return EXPR_LE;
        case TOK_GE: return EXPR_GE;
        case TOK_AND: return EXPR_AND;
        case TOK_OR: return EXPR_OR;
        case TOK_COMMA: return EXPR_COMMA;
        default: return EXPR_INT_LIT;
    }
}

extern Expr* rcc_parse_cxx_fold_operand(void);

/* Claim only unary and binary fold spellings.  Other parenthesized expressions remain on
 * the common precedence parser; an unsupported fold operator is diagnosed
 * here instead of being reinterpreted as a scalar. */
Expr* rcc_parse_cxx_fold_expression(void) {
    SourceLoc loc;
    ExprKind operator_kind;
    const char* pack_name;
    Token* saved_cur;
    Token* saved_prev;

    if (!check(TOK_LPAREN) || !parser.cur->next) return NULL;
    /* A unary fold may use an expression pattern, for example
     * `((args + 1) + ...)`.  Try the parenthesized pattern first, then restore
     * the token cursor so ordinary parenthesized expressions keep the common
     * parser path. */
    if (parser.cur->next->type == TOK_LPAREN) {
        Expr* pattern;
        Expr* fold;
        saved_cur = parser.cur;
        saved_prev = parser.prev;
        loc = peek()->loc;
        advance(); /* outer ( */
        pattern = rcc_parse_cxx_fold_operand();
        operator_kind = cxx_fold_operator_kind(peek()->type);
        if (pattern && operator_kind != EXPR_INT_LIT) {
            advance();
            if (match(TOK_ELLIPSIS) && match(TOK_RPAREN)) {
                if (!rcc_parser_cxx_standard_at_least(17)) {
                    rcc_error(loc, "fold expressions require C++17 or newer");
                }
                fold = expr_cxx_fold(NULL, operator_kind, false, loc);
                fold->cxx_fold_pattern = pattern;
                return fold;
            }
        }
        parser.cur = saved_cur;
        parser.prev = saved_prev;
    }
    if (parser.cur->next->type == TOK_ELLIPSIS) {
        loc = peek()->loc;
        advance(); /* ( */
        advance(); /* ... */
        operator_kind = cxx_fold_operator_kind(peek()->type);
        if (operator_kind == EXPR_INT_LIT) {
            rcc_error(peek()->loc,
                      "unsupported C++ fold operator; expected a binary operator");
            if (!at_end()) advance();
        } else {
            advance();
        }
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc,
                      "C++ fold expression requires a parameter pack name");
            while (!check(TOK_RPAREN) && !at_end()) advance();
            expect(TOK_RPAREN, ")");
            return expr_cxx_fold(NULL, EXPR_ADD, true, loc);
        }
        pack_name = advance()->value.str_val;
        expect(TOK_RPAREN, ")");
        if (!rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(loc, "fold expressions require C++17 or newer");
        }
        return expr_cxx_fold(pack_name, operator_kind, true, loc);
    }

    /* A binary left fold has the spelling `(init op ... op pack)`. */
    if (parser.cur->next->next && parser.cur->next->next->next &&
        parser.cur->next->next->next->type == TOK_ELLIPSIS &&
        parser.cur->next->next->next->next &&
        parser.cur->next->next->next->next->next &&
        parser.cur->next->next->next->next->next->type == TOK_IDENT) {
        Expr* initializer;
        loc = peek()->loc;
        advance(); /* ( */
        initializer = rcc_parse_cxx_fold_operand();
        operator_kind = cxx_fold_operator_kind(peek()->type);
        if (operator_kind == EXPR_INT_LIT) {
            rcc_error(peek()->loc,
                      "unsupported C++ fold operator; expected a binary operator");
            if (!at_end()) advance();
        } else {
            advance();
        }
        expect(TOK_ELLIPSIS, "...");
        {
            ExprKind second_operator_kind = cxx_fold_operator_kind(peek()->type);
            if (second_operator_kind == EXPR_INT_LIT) {
                rcc_error(peek()->loc,
                          "unsupported C++ fold operator; expected a binary operator");
                if (!at_end()) advance();
            } else {
                advance();
            }
            if (second_operator_kind != operator_kind) {
                rcc_error(peek()->loc,
                          "C++ binary fold requires the same operator on both sides of ...");
            }
        }
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc,
                      "C++ fold expression requires a parameter pack name");
            while (!check(TOK_RPAREN) && !at_end()) advance();
            expect(TOK_RPAREN, ")");
            return expr_cxx_fold(NULL, EXPR_ADD, true, loc);
        }
        pack_name = advance()->value.str_val;
        expect(TOK_RPAREN, ")");
        {
            Expr* fold = expr_cxx_fold(pack_name, operator_kind, true, loc);
            fold->cxx_fold_init = initializer;
            if (!rcc_parser_cxx_standard_at_least(17)) {
                rcc_error(loc, "fold expressions require C++17 or newer");
            }
            return fold;
        }
    }

    /* A unary right fold has the spelling `(pack op ...)`.  Require the
     * ellipsis immediately before the closing parenthesis so ordinary
     * parenthesized expressions are left to the normal parser. */
    if (parser.cur->next->type != TOK_IDENT ||
        !parser.cur->next->next || !parser.cur->next->next->next ||
        parser.cur->next->next->next->type != TOK_ELLIPSIS) {
        return NULL;
    }

    /* A binary right fold has the spelling `(pack op ... op init)`. */
    if (parser.cur->next->next->next->next &&
        parser.cur->next->next->next->next->type != TOK_RPAREN) {
        Expr* initializer;
        ExprKind second_operator_kind;
        loc = peek()->loc;
        advance(); /* ( */
        pack_name = advance()->value.str_val;
        operator_kind = cxx_fold_operator_kind(peek()->type);
        if (operator_kind == EXPR_INT_LIT) {
            rcc_error(peek()->loc,
                      "unsupported C++ fold operator; expected a binary operator");
            if (!at_end()) advance();
        } else {
            advance();
        }
        expect(TOK_ELLIPSIS, "...");
        second_operator_kind = cxx_fold_operator_kind(peek()->type);
        if (second_operator_kind == EXPR_INT_LIT) {
            rcc_error(peek()->loc,
                      "unsupported C++ fold operator; expected a binary operator");
            if (!at_end()) advance();
        } else {
            advance();
        }
        if (second_operator_kind != operator_kind) {
            rcc_error(peek()->loc,
                      "C++ binary fold requires the same operator on both sides of ...");
        }
        initializer = rcc_parse_cxx_fold_operand();
        expect(TOK_RPAREN, ")");
        {
            Expr* fold = expr_cxx_fold(pack_name, operator_kind, false, loc);
            fold->cxx_fold_init = initializer;
            if (!rcc_parser_cxx_standard_at_least(17)) {
                rcc_error(loc, "fold expressions require C++17 or newer");
            }
            return fold;
        }
    }

    loc = peek()->loc;
    advance(); /* ( */
    pack_name = advance()->value.str_val;
    operator_kind = cxx_fold_operator_kind(peek()->type);
    if (operator_kind == EXPR_INT_LIT) {
        rcc_error(peek()->loc,
                  "unsupported C++ fold operator; expected a binary operator");
        if (!at_end()) advance();
    } else {
        advance();
    }
    expect(TOK_ELLIPSIS, "...");
    expect(TOK_RPAREN, ")");
    if (!rcc_parser_cxx_standard_at_least(17)) {
        rcc_error(loc, "fold expressions require C++17 or newer");
    }
    return expr_cxx_fold(pack_name, operator_kind, false, loc);
}

/* Forward declarations */
static Expr* parse_cxx_expression(void);
extern Expr* parse_expression(void);
extern Expr* parse_assignment_expression(void);
extern Expr* rcc_parser_parse_initializer(void);
extern Stmt* parse_declaration(void);
extern Type* rcc_parser_parse_cxx_declarator(Type* base_type,
                                              const char** name,
                                              DeclList** parameters);
extern bool rcc_parser_last_cxx_declarator_was_pack(void);
static Stmt* parse_cxx_statement(void);
static DeclList* parse_cxx_parameter_declarations(void);
static Type* parse_cxx_type_spec(void);
static Type* parse_class_template_specialization(CxxTemplate* tmpl,
                                                 SourceLoc loc);
static Type* parse_alias_template_specialization(CxxTemplate* tmpl,
                                                  SourceLoc loc,
                                                  Type* owner_type);
static bool cxx_parser_expression_is_lvalue(Expr* expression);
static Expr* parse_cxx_trailing_requires_clause(SourceLoc loc);
static Type* parse_cxx_lambda_auto_type(CxxTemplate* tmpl,
                                        int parameter_index,
                                        bool is_const, bool is_pointer,
                                        bool is_reference,
                                        bool is_rvalue_reference,
                                        SourceLoc loc);
static void resolve_class_bases(CxxClass* cls, SourceLoc loc);
static void validate_class_virtual_specifiers(CxxClass* cls, SourceLoc loc);
static bool cxx_class_has_unresolved_dependent_base(const CxxClass* cls);
static void cxx_resolve_known_class_bases(CxxClass* cls);
static bool cxx_constructor_dmi_value_is_lowerable(
    CxxClass* cls, TypeParam* initialized_field, Type* type,
    Expr* expression);
static CxxClass* find_class(const char* qualified_name);
static CxxTemplate* find_class_template(const char* qualified_name);
static CxxTemplate* find_alias_template(const char* qualified_name);
static CxxTemplate* find_template(const char* qualified_name, int kind);
static CxxTemplate* find_concept(const char* qualified_name);
static bool cxx_qualified_class_alias_template_starts(void);
static Type* cxx_lambda_function_type(Type* return_type, DeclList* params);
static bool consume_cxx_class_alias_template_owner(
    const char** owner_name_out);
static Type* parse_template_template_default(SourceLoc loc);
static bool is_active_template_type(const char* name);
static void add_namespace_declaration(AST* ast, CxxNamespace* ns,
                                      Decl* declaration);
static void parse_cxx_language_linkage(AST* ast, CxxNamespace* ns);
static int active_template_type_index(const char* name);
static int active_template_template_parameter_index(const char* name);
static bool eval_template_integer_expression(Expr* expression,
                                              CxxTemplate* tmpl,
                                              const int64_t* values,
                                              const bool* value_present,
                                              int64_t* result);
static bool cxx_expression_references_template_non_type_parameter(
    const Expr* expression, const CxxTemplate* tmpl);
static bool cxx_friend_function_name_matches(CxxNamespace* ns,
                                             const char* candidate_name,
                                             const char* name) {
    const char* namespace_name;
    char qualified_name[512];
    size_t namespace_length;
    if (!candidate_name || !name) return false;
    if (strcmp(candidate_name, name) == 0) return true;
    namespace_name = cxx_namespace_qualified_name(ns);
    if (!namespace_name || !*namespace_name) return false;
    namespace_length = strlen(namespace_name);
    if (namespace_length + 2u + strlen(name) >= sizeof(qualified_name)) {
        return false;
    }
    memcpy(qualified_name, namespace_name, namespace_length);
    memcpy(qualified_name + namespace_length, "::", 2u);
    strcpy(qualified_name + namespace_length + 2u, name);
    return strcmp(candidate_name, qualified_name) == 0;
}

static bool cxx_friend_signature_alias_syntax_at(Token* token) {
    for (int lookahead = 0; token && lookahead < 24;
         ++lookahead, token = token->next) {
        if (token->type == TOK_SCOPE && token->next &&
            token->next->type == TOK_IDENT && token->next->next &&
            token->next->next->type == TOK_LT) {
            return true;
        }
        if (token->type == TOK_SEMICOLON || token->type == TOK_LBRACE) {
            return false;
        }
    }
    return false;
}

static bool cxx_friend_signature_may_use_alias_type(void) {
    Token* token;
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;
    for (token = parser.cur; token; token = token->next) {
        if (token->type == TOK_LBRACE && parentheses == 0 && brackets == 0 &&
            braces == 0) {
            break;
        }
        if (token->type == TOK_SEMICOLON && parentheses == 0 &&
            brackets == 0 && braces == 0) {
            break;
        }
        if ((token->type == TOK_IDENT || token->type == TOK_SCOPE) &&
            cxx_friend_signature_alias_syntax_at(token)) {
            Token* saved_cur = parser.cur;
            Token* saved_prev = parser.prev;
            bool is_alias_type;
            parser.cur = token;
            is_alias_type = cxx_qualified_class_alias_template_starts();
            parser.cur = saved_cur;
            parser.prev = saved_prev;
            if (is_alias_type) return true;
        }
        switch (token->type) {
            case TOK_LPAREN: ++parentheses; break;
            case TOK_RPAREN:
                if (parentheses > 0) --parentheses;
                break;
            case TOK_LBRACKET: ++brackets; break;
            case TOK_RBRACKET:
                if (brackets > 0) --brackets;
                break;
            case TOK_LBRACE: ++braces; break;
            case TOK_RBRACE:
                if (braces > 0) --braces;
                break;
            default: break;
        }
    }
    return false;
}

static bool cxx_friend_function_signature_matches(
    CxxNamespace* ns, const Decl* candidate, const char* name,
    Type* return_type, DeclList* parameters) {
    const TypeParam* candidate_parameter;
    DeclList* parameter;
    bool variadic = false;
    if (!candidate || candidate->kind != DECL_FUNC || !candidate->name ||
        !cxx_friend_function_name_matches(ns, candidate->name, name) ||
        !candidate->type ||
        candidate->type->kind != TYPE_FUNC ||
        !type_is_compatible(candidate->type->ret_type, return_type)) {
        return false;
    }
    for (parameter = parameters; parameter; parameter = parameter->next) {
        if (parameter->decl && parameter->decl->param_is_pack) {
            variadic = true;
            break;
        }
    }
    if (candidate->type->variadic != variadic) return false;
    candidate_parameter = candidate->type->params;
    for (parameter = parameters; parameter;
         parameter = parameter->next, candidate_parameter =
             candidate_parameter ? candidate_parameter->next : NULL) {
        if (!parameter->decl || !candidate_parameter ||
            !type_is_compatible(candidate_parameter->type,
                                parameter->decl->type)) {
            return false;
        }
    }
    return candidate_parameter == NULL;
}

static CxxFriendAccess* cxx_merge_friend_access(CxxFriendAccess* result,
                                                const CxxFriendAccess* source) {
    for (const CxxFriendAccess* grant = source; grant; grant = grant->next) {
        bool already_present = false;
        if (!grant->owner) continue;
        for (CxxFriendAccess* existing = result; existing;
             existing = existing->next) {
            if (existing->owner == grant->owner) {
                already_present = true;
                break;
            }
        }
        if (!already_present) {
            CxxFriendAccess* copy = ast_arena_alloc(sizeof(*copy));
            copy->owner = grant->owner;
            copy->next = result;
            result = copy;
        }
    }
    return result;
}

static CxxFriendAccess* cxx_find_friend_function_access(
    const char* name, Type* return_type, DeclList* parameters,
    CxxFriendAccess* existing, bool is_consteval,
    Expr* noexcept_expression) {
    CxxNamespace* ns = active_namespace
        ? active_namespace : g_global_namespace;
    CxxTemplate* current_template = active_template &&
            active_template->kind == TMPL_FUNCTION
        ? active_template : NULL;
    Type* function_type = NULL;
    if (!ns || !name) return existing;
    if (!current_template) {
        for (DeclList* declaration = ns->decls; declaration;
             declaration = declaration->next) {
            Decl* candidate = declaration->decl;
            if (candidate && candidate->func_friend_access &&
                cxx_friend_function_signature_matches(
                    ns, candidate, name, return_type, parameters)) {
                existing = cxx_merge_friend_access(
                    existing, candidate->func_friend_access);
            }
        }
    } else {
        function_type = cxx_lambda_function_type(return_type, parameters);
        for (DeclList* parameter = parameters; parameter;
             parameter = parameter->next) {
            if (parameter->decl && parameter->decl->param_is_pack) {
                function_type->variadic = true;
                break;
            }
        }
    }
    for (int index = 0; index < ns->template_count; ++index) {
        CxxTemplate* candidate = ns->templates[index];
        if (candidate && candidate->kind == TMPL_FUNCTION &&
            candidate->friend_access && candidate->func_def &&
            (current_template
                 ? cxx_function_template_friend_signature_matches(
                       candidate, current_template, name, function_type,
                       parameters, is_consteval, noexcept_expression)
                 : cxx_friend_function_signature_matches(
                       ns, candidate->func_def, name, return_type,
                       parameters))) {
            existing = cxx_merge_friend_access(existing,
                                               candidate->friend_access);
        }
    }
    return existing;
}

static bool cxx_friend_function_type_matches(const Decl* candidate,
                                             CxxNamespace* ns,
                                             const char* name,
                                             Type* function_type) {
    return candidate && candidate->kind == DECL_FUNC &&
           cxx_friend_function_name_matches(ns, candidate->name, name) &&
           candidate->type &&
           function_type && candidate->type->kind == TYPE_FUNC &&
           function_type->kind == TYPE_FUNC &&
           type_is_compatible(candidate->type, function_type);
}

static CxxFriendAccess* cxx_find_friend_function_type_access(
    CxxNamespace* ns, const char* name, Type* function_type,
    CxxFriendAccess* existing) {
    if (!ns || !name || !function_type) return existing;
    for (DeclList* declaration = ns->decls; declaration;
         declaration = declaration->next) {
        Decl* candidate = declaration->decl;
        if (candidate && candidate->func_friend_access &&
            cxx_friend_function_type_matches(candidate, ns, name,
                                             function_type)) {
            existing = cxx_merge_friend_access(
                existing, candidate->func_friend_access);
        }
    }
    for (int index = 0; index < ns->template_count; ++index) {
        CxxTemplate* candidate = ns->templates[index];
        if (candidate && candidate->kind == TMPL_FUNCTION &&
            candidate->friend_access && candidate->func_def &&
            cxx_friend_function_type_matches(candidate->func_def, ns, name,
                                             function_type)) {
            existing = cxx_merge_friend_access(existing,
                                               candidate->friend_access);
        }
    }
    return existing;
}

static CxxFriendAccess* cxx_collect_friend_function_candidates(
    CxxNamespace* ns) {
    CxxFriendAccess* result = NULL;
    if (!ns) return NULL;
    for (DeclList* declaration = ns->decls; declaration;
         declaration = declaration->next) {
        Decl* candidate = declaration->decl;
        if (candidate && candidate->kind == DECL_FUNC &&
            candidate->func_friend_access) {
            result = cxx_merge_friend_access(
                result, candidate->func_friend_access);
        }
    }
    for (int index = 0; index < ns->template_count; ++index) {
        CxxTemplate* candidate = ns->templates[index];
        if (candidate && candidate->kind == TMPL_FUNCTION &&
            candidate->friend_access && candidate->func_def) {
            result = cxx_merge_friend_access(result,
                                             candidate->friend_access);
        }
    }
    return result;
}

static void* cxx_begin_friend_function_signature_access(bool file_scope) {
    CxxNamespace* ns = active_namespace
        ? active_namespace : g_global_namespace;
    CxxFriendAccess* candidates;
    CxxFriendSignatureContext* context;
    if (!file_scope || active_class || !ns ||
        !cxx_friend_signature_may_use_alias_type()) {
        return NULL;
    }
    candidates = cxx_collect_friend_function_candidates(ns);
    if (!candidates) return NULL;
    context = ast_arena_alloc(sizeof(*context));
    context->saved_access_context = active_friend_access_context;
    context->saved_candidates = active_friend_signature_candidates;
    context->saved_alias_uses = active_friend_signature_alias_uses;
    context->saved_is_collecting = active_friend_signature_collection;
    active_friend_signature_candidates = candidates;
    active_friend_signature_alias_uses = NULL;
    active_friend_signature_collection = true;
    return context;
}

static bool cxx_friend_signature_use_is_allowed(
    CxxFriendSignatureAliasUse* use, CxxFriendAccess* matching) {
    for (CxxFriendAccess* grant = matching; grant; grant = grant->next) {
        CxxClass* declaring_class = NULL;
        bool ambiguous = false;
        bool accessible = false;
        CxxClassAliasTemplate* alias_template =
            cxx_class_find_inherited_alias_template(
                use->owner, use->name, grant->owner, &declaring_class,
                &ambiguous, &accessible);
        if (alias_template == use->alias_template && !ambiguous &&
            accessible) {
            return true;
        }
    }
    return false;
}

static void cxx_validate_friend_function_signature_access(
    void* saved_context, const char* name, Type* return_type,
    DeclList* parameters, Type* function_type, SourceLoc loc,
    bool is_consteval, Expr* noexcept_expression) {
    CxxFriendSignatureContext* context = saved_context;
    CxxNamespace* ns = active_namespace
        ? active_namespace : g_global_namespace;
    CxxFriendAccess* matching = NULL;
    if (!context) return;
    if (active_friend_signature_alias_uses) {
        if (function_type && active_template &&
            active_template->kind == TMPL_FUNCTION) {
            matching = cxx_find_friend_function_access(
                name, return_type, parameters, matching, is_consteval,
                noexcept_expression);
        } else if (function_type) {
            matching = cxx_find_friend_function_type_access(
                ns, name, function_type, matching);
        } else if (name && return_type) {
            matching = cxx_find_friend_function_access(
                name, return_type, parameters, matching, is_consteval,
                noexcept_expression);
        }
        for (CxxFriendSignatureAliasUse* use =
                 active_friend_signature_alias_uses;
             use; use = use->next) {
            if (!cxx_friend_signature_use_is_allowed(use, matching)) {
                rcc_error(loc,
                          "nested alias template '%s' is inaccessible in class '%s'",
                          use->name ? use->name : "<unknown>",
                          use->owner && use->owner->name
                              ? use->owner->name : "<unknown>");
            }
        }
    }
}

static void cxx_restore_friend_function_signature_access(
    void* saved_context) {
    CxxFriendSignatureContext* context = saved_context;
    if (!context) return;
    active_friend_access_context = context->saved_access_context;
    active_friend_signature_candidates = context->saved_candidates;
    active_friend_signature_alias_uses = context->saved_alias_uses;
    active_friend_signature_collection = context->saved_is_collecting;
}

static void cxx_end_friend_function_signature_access(
    void* saved_context, const char* name, Type* return_type,
    DeclList* parameters, Type* function_type, SourceLoc loc,
    bool is_consteval, Expr* noexcept_expression) {
    cxx_validate_friend_function_signature_access(
        saved_context, name, return_type, parameters, function_type, loc,
        is_consteval, noexcept_expression);
    cxx_restore_friend_function_signature_access(saved_context);
}

void* rcc_parser_cxx_begin_function_signature_access(bool file_scope) {
    return cxx_begin_friend_function_signature_access(file_scope);
}

void rcc_parser_cxx_end_function_signature_access(
    void* saved_context, const char* name, Type* function_type,
    DeclList* parameters, SourceLoc loc) {
    Type* return_type = function_type && function_type->kind == TYPE_FUNC
        ? function_type->ret_type : NULL;
    cxx_end_friend_function_signature_access(
        saved_context, name, return_type, parameters,
        function_type && function_type->kind == TYPE_FUNC
            ? function_type : NULL,
        loc, false, NULL);
}

void* rcc_parser_cxx_mark_function_signature_alias_uses(void) {
    return active_friend_signature_collection
        ? active_friend_signature_alias_uses : NULL;
}

void rcc_parser_cxx_reset_function_signature_alias_uses(void* mark) {
    if (active_friend_signature_collection) {
        active_friend_signature_alias_uses =
            (CxxFriendSignatureAliasUse*)mark;
    }
}

void rcc_parser_cxx_validate_function_signature_access(
    void* saved_context, const char* name, Type* function_type,
    DeclList* parameters, SourceLoc loc) {
    Type* return_type = function_type && function_type->kind == TYPE_FUNC
        ? function_type->ret_type : NULL;
    cxx_validate_friend_function_signature_access(
        saved_context, name, return_type, parameters,
        function_type && function_type->kind == TYPE_FUNC
            ? function_type : NULL,
        loc, false, NULL);
}

void rcc_parser_cxx_restore_function_signature_access(void* saved_context) {
    cxx_restore_friend_function_signature_access(saved_context);
}

void* rcc_parser_cxx_begin_function_friend_access(
    const char* name, Type* function_type) {
    CxxFriendAccess* saved = active_friend_access_context;
    CxxNamespace* ns = active_namespace
        ? active_namespace : g_global_namespace;
    CxxFriendAccess* matching = cxx_find_friend_function_type_access(
        ns, name, function_type, saved);
    active_friend_access_context = matching;
    return saved;
}

void rcc_parser_cxx_end_function_friend_access(void* saved_context) {
    active_friend_access_context = (CxxFriendAccess*)saved_context;
}

static Decl* parse_cxx_function_declaration(bool parse_body,
                                            bool* is_constexpr,
                                            bool* is_noexcept,
                                            bool* is_consteval);
CxxTemplate* parse_cxx_template(void);
bool rcc_parse_cxx_deduction_guide(void);
static void add_cxx_declaration(AST* ast, Stmt* statement,
                                bool c_language_linkage,
                                CxxNamespace* ns);

static bool cxx_template_variable_starts(void) {
    Token* token = parser.cur;
    int parentheses = 0;
    int brackets = 0;

    /* A variable template definition has an initializer boundary or a
     * terminating semicolon before any top-level parameter list.  This is
     * intentionally structural: direct-initialized variables with (...)
     * remain outside the bounded profile and are diagnosed by the ordinary
     * function-template path instead of being misparsed as functions. */
    if (token && (token->type == TOK_CLASS || token->type == TOK_STRUCT)) {
        return false;
    }
    for (; token; token = token->next) {
        if (parentheses == 0 && brackets == 0) {
            if (token->type == TOK_ASSIGN || token->type == TOK_LBRACE ||
                token->type == TOK_SEMICOLON) {
                return true;
            }
            if (token->type == TOK_LPAREN) return false;
        }
        if (token->type == TOK_LPAREN) ++parentheses;
        else if (token->type == TOK_RPAREN && parentheses > 0) --parentheses;
        else if (token->type == TOK_LBRACKET) ++brackets;
        else if (token->type == TOK_RBRACKET && brackets > 0) --brackets;
    }
    return false;
}

/* `constexpr`/`consteval` can introduce either a function or a variable.  The dedicated
 * function parser is needed for C++ parameter/body handling, while ordinary
 * declaration parsing owns the variable initializer grammar.  Stop at the
 * first declaration-level initializer boundary so a call in a variable
 * initializer is not mistaken for a function declarator. */
static bool cxx_constexpr_starts_function(void) {
    Token* token = parser.cur;
    int parentheses = 0;
    int brackets = 0;

    if (!token || (token->type != TOK_CONSTEXPR &&
                   token->type != TOK_CONSTEVAL)) return false;
    token = token->next;
    for (; token; token = token->next) {
        if (parentheses == 0 && brackets == 0) {
            if (token->type == TOK_ASSIGN || token->type == TOK_LBRACE ||
                token->type == TOK_SEMICOLON) {
                return false;
            }
            if (token->type == TOK_LPAREN) return true;
        }
        if (token->type == TOK_LPAREN) ++parentheses;
        else if (token->type == TOK_RPAREN && parentheses > 0) --parentheses;
        else if (token->type == TOK_LBRACKET) ++brackets;
        else if (token->type == TOK_RBRACKET && brackets > 0) --brackets;
        if (token->type == TOK_EOF) break;
    }
    return false;
}

static bool cxx_decltype_auto_starts_function(void) {
    Token* token = parser.cur;
    if (!token || token->type != TOK_DECLTYPE) return false;
    token = token->next;
    if (!token || token->type != TOK_LPAREN) return false;
    token = token->next;
    if (!token || token->type != TOK_AUTO) return false;
    token = token->next;
    if (!token || token->type != TOK_RPAREN) return false;
    token = token->next;
    if (!token || token->type != TOK_IDENT) return false;
    token = token->next;
    return token && token->type == TOK_LPAREN;
}

/* `inline` is valid on both namespace-scope functions and variables.  The
 * dedicated C++ function parser must only claim the former; otherwise an
 * ordinary declaration such as `inline int value = 1;` is consumed as a
 * malformed function before the common declaration parser can preserve its
 * inline-variable linkage.  Keep this lexical probe conservative and stop at
 * declaration initializers, while accepting the standard parameter type
 * spellings used by the bounded profile. */
static bool cxx_inline_starts_function(void) {
    Token* token = parser.cur;
    Token* previous = NULL;
    int parentheses = 0;
    int brackets = 0;

    if (!token || (token->type != TOK_INLINE &&
                   token->type != TOK___INLINE__)) return false;
    token = token->next;
    while (token) {
        if (parentheses == 0 && brackets == 0) {
            if (token->type == TOK_ASSIGN || token->type == TOK_LBRACE ||
                token->type == TOK_SEMICOLON) return false;
            if (token->type == TOK_LPAREN) {
                Token* parameter = token->next;
                if (previous && previous->type == TOK_STAR) return false;
                if (!parameter || parameter->type == TOK_RPAREN ||
                    parameter->type == TOK_ELLIPSIS ||
                    parameter->type == TOK_CONST ||
                    parameter->type == TOK_VOLATILE ||
                    parameter->type == TOK_SIGNED ||
                    parameter->type == TOK_UNSIGNED ||
                    parameter->type == TOK_SHORT ||
                    parameter->type == TOK_LONG ||
                    parameter->type == TOK_VOID ||
                    parameter->type == TOK_BOOL ||
                    parameter->type == TOK_CHAR ||
                    parameter->type == TOK_CHAR8_T ||
                    parameter->type == TOK_INT ||
                    parameter->type == TOK_FLOAT ||
                    parameter->type == TOK_DOUBLE ||
                    parameter->type == TOK_AUTO ||
                    parameter->type == TOK_DECLTYPE ||
                    parameter->type == TOK_STRUCT ||
                    parameter->type == TOK_CLASS ||
                    parameter->type == TOK_TYPENAME ||
                    (parameter->type == TOK_IDENT && parameter->next &&
                     (parameter->next->type == TOK_IDENT ||
                      parameter->next->type == TOK_AMP ||
                      parameter->next->type == TOK_AND ||
                      parameter->next->type == TOK_STAR ||
                      parameter->next->type == TOK_RPAREN))) {
                    return true;
                }
                return false;
            }
        }
        if (token->type == TOK_LPAREN) ++parentheses;
        else if (token->type == TOK_RPAREN && parentheses > 0) --parentheses;
        else if (token->type == TOK_LBRACKET) ++brackets;
        else if (token->type == TOK_RBRACKET && brackets > 0) --brackets;
        previous = token;
        if (token->type == TOK_EOF) break;
        token = token->next;
    }
    return false;
}

/* ═══════════════════════════════════════
 * C++ Scope Resolution
 * ═══════════════════════════════════════ */

/* Parse qualified name: ns::ns::name */
static const char* parse_qualified_name(void) {
    char buffer[512] = "";

    /* Global scope? */
    if (match(TOK_SCOPE)) {
        strcat(buffer, "::");
    }

    if (!check(TOK_IDENT)) {
        rcc_error(peek()->loc, "expected identifier");
        return rcc_intern("");
    }

    Token* name = advance();
    strcat(buffer, name->value.str_val);

    while (match(TOK_SCOPE)) {
        strcat(buffer, "::");
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc, "expected identifier after ::");
            break;
        }
        name = advance();
        strcat(buffer, name->value.str_val);
    }

    return rcc_intern(buffer);
}

/* ═══════════════════════════════════════
 * C++ Class Parsing
 * ═══════════════════════════════════════ */

/* Parse access specifier */
static AccessSpec parse_access_spec(void) {
    if (match(TOK_PUBLIC)) {
        expect(TOK_COLON, ":");
        return ACCESS_PUBLIC;
    }
    if (match(TOK_PRIVATE)) {
        expect(TOK_COLON, ":");
        return ACCESS_PRIVATE;
    }
    if (match(TOK_PROTECTED)) {
        expect(TOK_COLON, ":");
        return ACCESS_PROTECTED;
    }
    return (AccessSpec)-1;
}

static bool check_next(TokenType type) {
    return parser.cur->next && parser.cur->next->type == type;
}

/* A dependent using-declaration such as `using Base<T>::Base` cannot be
 * resolved while its enclosing local class template is only a pattern.  Keep
 * the whole base type for the later class specialization, while leaving
 * ordinary non-template using-declarations on the established name parser. */
static bool cxx_using_starts_with_template_id_base(void) {
    Token* token = parser.cur;
    if (token && token->type == TOK_SCOPE) token = token->next;
    if (!token || token->type != TOK_IDENT) return false;
    while (token && token->type != TOK_EOF) {
        if (token->next && token->next->type == TOK_LT) {
            int depth = 0;
            token = token->next;
            while (token && token->type != TOK_EOF) {
                if (token->type == TOK_LT) {
                    ++depth;
                } else if (token->type == TOK_GT) {
                    if (--depth == 0) {
                        return token->next &&
                               token->next->type == TOK_SCOPE;
                    }
                } else if (token->type == TOK_RSHIFT) {
                    depth -= depth > 1 ? 2 : depth;
                    if (depth == 0) {
                        return token->next &&
                               token->next->type == TOK_SCOPE;
                    }
                }
                token = token->next;
            }
            return false;
        }
        if (!token->next || token->next->type != TOK_SCOPE ||
            !token->next->next || token->next->next->type != TOK_IDENT) {
            return false;
        }
        token = token->next->next;
    }
    return false;
}

static bool pending_cxx_nodiscard;
static bool pending_cxx_deprecated;
static bool pending_cxx_no_unique_address;
static bool pending_cxx_weak;
static const char* pending_cxx_deprecated_message;

/* C++ attributes are metadata at this stage.  Consume complete [[...]]
 * groups so they cannot be mistaken for array declarators. */
static void skip_cxx_attributes(void) {
    while (check(TOK_LBRACKET) && check_next(TOK_LBRACKET)) {
        SourceLoc loc = peek()->loc;
        int depth = 1;
        bool group_nodiscard = false;
        bool group_deprecated = false;
        bool group_maybe_unused = false;
        bool group_fallthrough = false;
        bool group_likely = false;
        bool group_unlikely = false;
        bool group_no_unique_address = false;
        bool group_weak = false;
        bool group_weak_arguments = false;
        const char* group_deprecated_message = NULL;
        advance();
        advance();
        while (depth > 0 && !at_end()) {
            if (check(TOK_LBRACKET) && check_next(TOK_LBRACKET)) {
                advance();
                advance();
                depth++;
            } else if (check(TOK_RBRACKET) && check_next(TOK_RBRACKET)) {
                advance();
                advance();
                depth--;
            } else {
                if (depth == 1 && check(TOK_IDENT)) {
                    if (strcmp(peek()->value.str_val, "nodiscard") == 0) {
                        group_nodiscard = true;
                    } else if (strcmp(peek()->value.str_val, "deprecated") == 0) {
                        group_deprecated = true;
                        if (parser.cur->next &&
                            parser.cur->next->type == TOK_LPAREN &&
                            parser.cur->next->next &&
                            parser.cur->next->next->type == TOK_STRING_LIT) {
                            group_deprecated_message =
                                parser.cur->next->next->value.str_val;
                        }
                    } else if (strcmp(peek()->value.str_val,
                                      "maybe_unused") == 0) {
                        group_maybe_unused = true;
                    } else if (strcmp(peek()->value.str_val,
                                      "fallthrough") == 0) {
                        group_fallthrough = true;
                    } else if (strcmp(peek()->value.str_val, "likely") == 0) {
                        group_likely = true;
                    } else if (strcmp(peek()->value.str_val,
                                      "unlikely") == 0) {
                        group_unlikely = true;
                    } else if (strcmp(peek()->value.str_val,
                                      "no_unique_address") == 0) {
                        group_no_unique_address = true;
                    } else if (strcmp(peek()->value.str_val, "gnu") == 0 &&
                               parser.cur->next &&
                               parser.cur->next->type == TOK_SCOPE &&
                               parser.cur->next->next &&
                               parser.cur->next->next->type == TOK_IDENT &&
                               strcmp(parser.cur->next->next->value.str_val,
                                      "weak") == 0) {
                        group_weak = true;
                        if (parser.cur->next->next->next &&
                            parser.cur->next->next->next->type == TOK_LPAREN) {
                            group_weak_arguments = true;
                        }
                    }
                }
                advance();
            }
        }
        if (depth != 0) {
            rcc_error(loc, "unterminated C++ attribute specifier");
            return;
        }
        if (group_nodiscard && !rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(loc, "[[nodiscard]] requires C++17 or newer");
        }
        if (group_deprecated && !rcc_parser_cxx_standard_at_least(14)) {
            rcc_error(loc, "[[deprecated]] requires C++14 or newer");
        }
        if ((group_maybe_unused || group_fallthrough) &&
            !rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(loc,
                      "[[maybe_unused]] and [[fallthrough]] require C++17 or newer");
        }
        if ((group_likely || group_unlikely) &&
            !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(loc,
                      "[[likely]] and [[unlikely]] require C++20 or newer");
        }
        if (group_no_unique_address &&
            !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(loc, "[[no_unique_address]] requires C++20 or newer");
        }
        if (group_weak_arguments) {
            rcc_error(loc, "[[gnu::weak]] does not accept arguments");
        }
        if (group_nodiscard) pending_cxx_nodiscard = true;
        if (group_weak) pending_cxx_weak = true;
        if (group_deprecated) {
            pending_cxx_deprecated = true;
            if (group_deprecated_message) {
                pending_cxx_deprecated_message = group_deprecated_message;
            }
        }
        if (group_no_unique_address) pending_cxx_no_unique_address = true;
    }
}

static bool take_cxx_nodiscard(void) {
    bool result = pending_cxx_nodiscard;
    pending_cxx_nodiscard = false;
    return result;
}

static bool take_cxx_weak(void) {
    bool result = pending_cxx_weak;
    pending_cxx_weak = false;
    return result;
}

static bool take_cxx_deprecated(const char** message) {
    bool result = pending_cxx_deprecated;
    if (message) *message = pending_cxx_deprecated_message;
    pending_cxx_deprecated = false;
    pending_cxx_deprecated_message = NULL;
    return result;
}

static bool take_cxx_no_unique_address(void) {
    bool result = pending_cxx_no_unique_address;
    pending_cxx_no_unique_address = false;
    return result;
}

/* Statement attributes are consumed by the shared C statement parser.  They
 * intentionally do not retain declaration metadata: this frontend has no
 * unused-variable or branch-probability diagnostics, but accepting these
 * standard annotations is still useful for portable C++ source. */
void rcc_parser_cxx_skip_statement_attributes(void) {
    bool saved_nodiscard = pending_cxx_nodiscard;
    bool saved_deprecated = pending_cxx_deprecated;
    bool saved_no_unique_address = pending_cxx_no_unique_address;
    bool saved_weak = pending_cxx_weak;
    const char* saved_deprecated_message = pending_cxx_deprecated_message;

    skip_cxx_attributes();
    /* A generic C++ function body can be parsed before its declaration
     * metadata is attached.  Preserve that outer metadata while discarding
     * attributes that appeared directly before a statement. */
    pending_cxx_nodiscard = saved_nodiscard;
    pending_cxx_deprecated = saved_deprecated;
    pending_cxx_no_unique_address = saved_no_unique_address;
    pending_cxx_weak = saved_weak;
    pending_cxx_deprecated_message = saved_deprecated_message;
}

/* C++20 permits an explicit-specifier to be a constant expression.  Keep the
 * bounded frontend honest: evaluate the condition before recording the
 * constructor/conversion metadata, and reject dependent or runtime forms
 * instead of treating them as an unconditional explicit declaration. */
static bool parse_cxx_explicit_specifier(SourceLoc loc) {
    Expr* condition;
    int64_t value = 0;

    expect(TOK_EXPLICIT, "explicit");
    if (!match(TOK_LPAREN)) return true;
    condition = parse_expression();
    expect(TOK_RPAREN, ")");
    if (!rcc_parser_cxx_standard_at_least(20)) {
        rcc_error(loc, "conditional explicit specifiers require C++20 or newer");
        return false;
    }
    if (!condition || !expr_eval_integer_constant(condition, &value)) {
        rcc_error(loc,
                  "conditional explicit specifier requires an integral constant expression");
        return false;
    }
    return value != 0;
}

static void skip_balanced(TokenType open, TokenType close) {
    int depth = 0;
    if (!match(open)) return;
    depth = 1;
    while (depth > 0 && !at_end()) {
        if (match(open)) depth++;
        else if (match(close)) depth--;
        else advance();
    }
}

static void cxx_requires_type_append(TypeList** list, Type* type,
                                     SourceLoc loc) {
    TypeList* item = ast_arena_alloc(sizeof(*item));
    TypeList** tail = list;
    item->type = type;
    item->loc = loc;
    item->next = NULL;
    while (*tail) tail = &(*tail)->next;
    *tail = item;
}

static void cxx_requires_compound_append(
    CxxCompoundRequirement** list, Expr* expr, bool is_noexcept,
    Type* return_type, bool return_type_convertible, SourceLoc loc) {
    CxxCompoundRequirement* item = ast_arena_alloc(sizeof(*item));
    CxxCompoundRequirement** tail = list;
    item->expr = expr;
    item->is_noexcept = is_noexcept;
    item->return_type = return_type;
    item->return_type_convertible = return_type_convertible;
    item->loc = loc;
    item->next = NULL;
    while (*tail) tail = &(*tail)->next;
    *tail = item;
}

static bool cxx_requires_return_constraint_has_arguments(void) {
    Token* token = parser.cur;

    if (!token) return false;
    if (token->type == TOK_SCOPE) token = token->next;
    if (!token || token->type != TOK_IDENT) return false;
    while (token->next && token->next->type == TOK_SCOPE) {
        token = token->next->next;
        if (!token || token->type != TOK_IDENT) return false;
    }
    return token->next && token->next->type == TOK_LT;
}

static bool cxx_name_has_suffix(const char* name, const char* suffix) {
    size_t name_length;
    size_t suffix_length;

    if (!name || !suffix) return false;
    name_length = strlen(name);
    suffix_length = strlen(suffix);
    return name_length >= suffix_length &&
           strcmp(name + name_length - suffix_length, suffix) == 0 &&
           (name_length == suffix_length ||
            name[name_length - suffix_length - 1u] == ':');
}

/* Recognize the standard-library type constraints that have a direct bounded
 * ABI interpretation.  Bare types remain accepted as a useful exact-type
 * extension for freestanding RinOS code; unknown concept names are diagnosed
 * instead of being lowered as an unconstrained requirement. */
static Type* parse_cxx_requires_return_constraint(bool* return_type_convertible) {
    const char* constraint_name;
    Type* return_type;

    *return_type_convertible = false;
    if (!cxx_requires_return_constraint_has_arguments()) {
        return parse_cxx_type_spec();
    }
    constraint_name = parse_qualified_name();
    expect(TOK_LT, "< after requires-expression return constraint");
    return_type = parse_cxx_type_spec();
    expect(TOK_GT, "> after requires-expression return constraint");
    if (cxx_name_has_suffix(constraint_name, "same_as")) return return_type;
    if (cxx_name_has_suffix(constraint_name, "convertible_to")) {
        *return_type_convertible = true;
        return return_type;
    }
    rcc_error(previous()->loc,
              "unsupported C++20 requires-expression return constraint '%s'",
              constraint_name);
    return NULL;
}

/* Parse a bounded type-requirement form: a public nested `using` alias, or a
 * dependent `T::Alias` that can be resolved after template substitution.
 * Unresolved names remain carriers so sema can make the surrounding
 * requires-expression false without rejecting the whole expression. */
static Type* parse_cxx_requires_type(SourceLoc loc) {
    const char* qualified;
    const char* separator;
    const char* owner_name;
    const char* member_name;
    size_t owner_length;
    char owner_buffer[512];
    CxxClass* owner;
    CxxTypeAlias* alias;
    Type* dependent;
    int parameter_index;

    if (!match(TOK_TYPENAME)) return NULL;
    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) {
        rcc_error(peek()->loc,
                  "requires-expression type requirement expects a qualified type");
        return NULL;
    }
    if (check(TOK_IDENT) && parser.cur->next &&
        parser.cur->next->type == TOK_LT) {
        Type* owner_type = parse_cxx_type_spec();
        const char* specialized_member = NULL;
        CxxClass* specialized_class = owner_type
            ? owner_type->cxx_class : NULL;
        CxxTypeAlias* specialized_alias = NULL;
        if (!match(TOK_SCOPE) || !check(TOK_IDENT)) {
            rcc_error(loc,
                      "requires-expression type requirement expects a nested type");
            return NULL;
        }
        specialized_member = rcc_intern(advance()->value.str_val);
        specialized_alias = specialized_class
            ? cxx_class_find_type_alias(specialized_class,
                                        specialized_member)
            : NULL;
        if (specialized_alias &&
            specialized_alias->access == ACCESS_PUBLIC) {
            return specialized_alias->type;
        }
        {
            Type* unresolved = ast_arena_alloc(sizeof(*unresolved));
            if (owner_type) *unresolved = *owner_type;
            else memset(unresolved, 0, sizeof(*unresolved));
            unresolved->cxx_dependent = true;
            unresolved->cxx_class = specialized_class;
            unresolved->cxx_template_param_index = -1;
            unresolved->cxx_dependent_member_name = specialized_member;
            return unresolved;
        }
    }
    qualified = parse_qualified_name();
    separator = qualified ? strrchr(qualified, ':') : NULL;
    if (!separator || separator == qualified || separator[-1] != ':') {
        rcc_error(loc,
                  "requires-expression type requirement expects a nested type");
        return NULL;
    }
    owner_length = (size_t)(separator - qualified - 1);
    if (owner_length == 0 || owner_length >= sizeof(owner_buffer)) {
        rcc_error(loc, "requires-expression type requirement owner is too long");
        return NULL;
    }
    memcpy(owner_buffer, qualified, owner_length);
    owner_buffer[owner_length] = '\0';
    owner_name = rcc_intern(owner_buffer);
    member_name = rcc_intern(separator + 1);
    owner = find_class(owner_name);
    parameter_index = active_template_type_index(owner_name);
    alias = owner ? cxx_class_find_type_alias(owner, member_name) : NULL;
    if (alias && alias->access == ACCESS_PUBLIC) return alias->type;

    dependent = type_struct(qualified);
    dependent->cxx_dependent = true;
    dependent->cxx_class = owner;
    dependent->cxx_template_param_index = parameter_index;
    dependent->cxx_dependent_member_name = member_name;
    return dependent;
}

Expr* rcc_parse_cxx_requires_expression(void) {
    SourceLoc loc;
    ExprList* requirements = NULL;
    ExprList* nested_requirements = NULL;
    DeclList* parameters = NULL;
    TypeList* type_requirements = NULL;
    CxxCompoundRequirement* compound_requirements = NULL;
    int parameter_index = 0;

    if (!match(TOK_REQUIRES)) return NULL;
    loc = previous()->loc;
    if (match(TOK_LPAREN)) {
        while (!check(TOK_RPAREN) && !at_end()) {
            SourceLoc parameter_loc = peek()->loc;
            Type* parameter_base = parse_cxx_type_spec();
            const char* parameter_name = NULL;
            Type* parameter_type;
            Decl* parameter;
            if (!parameter_base) {
                rcc_error(peek()->loc,
                          "requires-expression parameter requires a type");
                parameter_base = type_int;
            }
            parameter_type = rcc_parser_parse_cxx_declarator(
                parameter_base, &parameter_name, NULL);
            if (!parameter_name) {
                rcc_error(parameter_loc,
                          "requires-expression parameter requires a name");
                parameter_name = rcc_intern("__rcc_requires_parameter");
            }
            parameter = decl_param(parameter_name, parameter_type,
                                   parameter_index++, parameter_loc);
            decllist_append(&parameters, parameter);
            if (match(TOK_ASSIGN)) {
                rcc_error(previous()->loc,
                          "requires-expression parameters cannot have defaults");
                (void)parse_assignment_expression();
            }
            if (!match(TOK_COMMA)) break;
            if (check(TOK_RPAREN)) {
                rcc_error(peek()->loc,
                          "expected parameter declaration after ','");
                break;
            }
        }
        expect(TOK_RPAREN, ")");
    }
    if (!match(TOK_LBRACE)) {
        rcc_error(peek()->loc, "requires-expression expects a requirement body");
        return expr_cxx_requires(NULL, loc);
    }
    rcc_parser_cxx_begin_function_parameters(parameters);
    while (!check(TOK_RBRACE) && !at_end()) {
        if (match(TOK_LBRACE)) {
            SourceLoc requirement_loc = previous()->loc;
            Expr* compound = parse_expression();
            bool is_noexcept;
            bool return_type_convertible = false;
            Type* return_type = NULL;
            expect(TOK_RBRACE,
                   "'}' after requires-expression compound expression");
            is_noexcept = match(TOK_NOEXCEPT);
            if (match(TOK_ARROW)) {
                return_type = parse_cxx_requires_return_constraint(
                    &return_type_convertible);
            }
            if (compound) {
                cxx_requires_compound_append(&compound_requirements, compound,
                                             is_noexcept, return_type,
                                             return_type_convertible,
                                             requirement_loc);
            }
            expect(TOK_SEMICOLON,
                   "';' after requires-expression compound requirement");
            continue;
        }
        if (match(TOK_REQUIRES)) {
            SourceLoc requirement_loc = previous()->loc;
            Expr* nested = parse_assignment_expression();
            if (!nested) {
                rcc_error(requirement_loc,
                          "nested requires-expression requirement expects a constraint expression");
            } else {
                exprlist_append(&nested_requirements, nested);
            }
            expect(TOK_SEMICOLON,
                   "';' after nested requires-expression requirement");
            continue;
        }
        if (check(TOK_TYPENAME)) {
            SourceLoc requirement_loc = peek()->loc;
            cxx_requires_type_append(
                &type_requirements,
                parse_cxx_requires_type(requirement_loc), requirement_loc);
            expect(TOK_SEMICOLON,
                   "';' after requires-expression type requirement");
            continue;
        }
        Expr* requirement = parse_expression();
        if (requirement) exprlist_append(&requirements, requirement);
        expect(TOK_SEMICOLON, "';' after requires-expression requirement");
    }
    expect(TOK_RBRACE, "'}' after requires-expression requirements");
    rcc_parser_cxx_end_function_parameters();
    {
        Expr* result = expr_cxx_requires(requirements, loc);
        result->cxx_requires_params = parameters;
        result->cxx_requires_types = type_requirements;
        result->cxx_requires_nested = nested_requirements;
        result->cxx_requires_compound = compound_requirements;
        return result;
    }
}

static void skip_cxx_template_arguments(void) {
    SourceLoc loc = peek()->loc;
    int depth = 0;
    if (!match(TOK_LT)) return;
    depth = 1;
    while (depth > 0 && !at_end()) {
        if (match(TOK_LT)) {
            depth++;
        } else if (match(TOK_GT)) {
            depth--;
        } else if (match(TOK_RSHIFT)) {
            depth = depth > 1 ? depth - 2 : 0;
        } else {
            advance();
        }
    }
    if (depth != 0) {
        rcc_error(loc, "unterminated template argument list");
    }
}

static const char* parse_operator_name(void) {
    TokenType operation;
    if (check(TOK_STRING_LIT)) {
        Token* quote = advance();
        Token* suffix = expect(TOK_IDENT, "user-defined literal suffix");
        char name[512];
        int written;
        if (!quote || !quote->value.str_val ||
            strcmp(quote->value.str_val, "") != 0 || !suffix ||
            !suffix->value.str_val || suffix->value.str_val[0] != '_') {
            rcc_error(quote ? quote->loc : peek()->loc,
                      "user-defined literal operator requires an empty string and a suffix beginning with '_'");
            return rcc_intern("operator\"\"_invalid");
        }
        written = snprintf(name, sizeof(name), "operator\"\"%s",
                           suffix->value.str_val);
        if (written < 0 || (size_t)written >= sizeof(name)) {
            rcc_error(suffix->loc, "user-defined literal operator name is too long");
            return rcc_intern("operator\"\"_invalid");
        }
        return rcc_intern(name);
    }
    if (match(TOK_LPAREN)) {
        expect(TOK_RPAREN, ")");
        return rcc_intern("operator()");
    }
    if (match(TOK_LBRACKET)) {
        expect(TOK_RBRACKET, "]");
        return rcc_intern("operator[]");
    }
    operation = peek()->type;
    switch (operation) {
        case TOK_ASSIGN:
            advance();
            return rcc_intern("operator=");
        case TOK_PLUS_ASSIGN:
            advance();
            return rcc_intern("operator+=");
        case TOK_MINUS_ASSIGN:
            advance();
            return rcc_intern("operator-=");
        case TOK_STAR_ASSIGN:
            advance();
            return rcc_intern("operator*=");
        case TOK_SLASH_ASSIGN:
            advance();
            return rcc_intern("operator/=");
        case TOK_PERCENT_ASSIGN:
            advance();
            return rcc_intern("operator%=");
        case TOK_AMP_ASSIGN:
            advance();
            return rcc_intern("operator&=");
        case TOK_PIPE_ASSIGN:
            advance();
            return rcc_intern("operator|=");
        case TOK_CARET_ASSIGN:
            advance();
            return rcc_intern("operator^=");
        case TOK_LSHIFT_ASSIGN:
            advance();
            return rcc_intern("operator<<=");
        case TOK_RSHIFT_ASSIGN:
            advance();
            return rcc_intern("operator>>=");
        case TOK_PLUS:
            advance();
            return rcc_intern("operator+");
        case TOK_MINUS:
            advance();
            return rcc_intern("operator-");
        case TOK_STAR:
            advance();
            return rcc_intern("operator*");
        case TOK_SLASH:
            advance();
            return rcc_intern("operator/");
        case TOK_PERCENT:
            advance();
            return rcc_intern("operator%");
        case TOK_INC:
            advance();
            return rcc_intern("operator++");
        case TOK_DEC:
            advance();
            return rcc_intern("operator--");
        case TOK_EQ:
            advance();
            return rcc_intern("operator==");
        case TOK_NE:
            advance();
            return rcc_intern("operator!=");
        case TOK_LT:
            advance();
            return rcc_intern("operator<");
        case TOK_LE:
            advance();
            return rcc_intern("operator<=");
        case TOK_GT:
            advance();
            return rcc_intern("operator>");
        case TOK_GE:
            advance();
            return rcc_intern("operator>=");
        case TOK_SPACESHIP:
            advance();
            return rcc_intern("operator<=>");
        case TOK_AMP:
            advance();
            return rcc_intern("operator&");
        case TOK_PIPE:
            advance();
            return rcc_intern("operator|");
        case TOK_CARET:
            advance();
            return rcc_intern("operator^");
        case TOK_TILDE:
            advance();
            return rcc_intern("operator~");
        case TOK_NOT:
            advance();
            return rcc_intern("operator!");
        case TOK_AND:
            advance();
            return rcc_intern("operator&&");
        case TOK_OR:
            advance();
            return rcc_intern("operator||");
        case TOK_LSHIFT:
            advance();
            return rcc_intern("operator<<");
        case TOK_RSHIFT:
            advance();
            return rcc_intern("operator>>");
        case TOK_COMMA:
            advance();
            return rcc_intern("operator,");
        case TOK_ARROW:
            advance();
            return rcc_intern("operator->");
        case TOK_DOT_STAR:
            advance();
            return rcc_intern("operator.*");
        case TOK_ARROW_STAR:
            advance();
            return rcc_intern("operator->*");
        default:
            rcc_error(peek()->loc, "expected overloaded operator");
            return NULL;
    }
}

static Type* cxx_function_type_from_parameters(Type* return_type,
                                               DeclList* params) {
    TypeParam* type_params = NULL;
    TypeParam** tail = &type_params;
    for (DeclList* item = params; item; item = item->next) {
        TypeParam* parameter = ast_arena_alloc(sizeof(*parameter));
        parameter->name = item->decl ? item->decl->name : NULL;
        parameter->type = item->decl ? item->decl->type : NULL;
        parameter->is_bitfield = false;
        parameter->bit_width = 0u;
        parameter->is_static = false;
        parameter->initializer = item->decl ? item->decl->param_default : NULL;
        parameter->is_deprecated = false;
        parameter->deprecated_message = NULL;
        parameter->cxx_access = ACCESS_PUBLIC;
        parameter->next = NULL;
        *tail = parameter;
        tail = &parameter->next;
    }
    return type_func(return_type, type_params, false);
}

/* Parse a namespace-scope overloaded operator after the common parser has
 * consumed its return type.  The resulting declaration is an ordinary C++
 * function, so namespace lookup/ADL and the existing overload resolver remain
 * the single source of truth for calls. */
Stmt* rcc_parse_cxx_operator_declaration(Type* return_type, SourceLoc loc) {
    const char* name;
    DeclList* params;
    StmtList* statements = NULL;
    Stmt* body = NULL;
    Decl* declaration;

    if (!return_type || !match(TOK_OPERATOR)) return NULL;
    name = parse_operator_name();
    expect(TOK_LPAREN, "(");
    params = parse_cxx_parameter_declarations();
    expect(TOK_RPAREN, ")");
    if (name && strncmp(name, "operator\"\"", 10u) == 0) {
        Type* first = params && params->decl ? params->decl->type : NULL;
        Type* second = params && params->next && params->next->decl
            ? params->next->decl->type : NULL;
        bool integer_form = params && !params->next && first &&
            type_is_compatible(first, type_ullong);
        bool floating_form = params && !params->next && first &&
            type_is_floating(first) && first->size == type_double->size;
        bool character_form = params && !params->next && first &&
            first->kind == TYPE_CHAR;
        bool string_form = params && params->next && !params->next->next &&
            first && second && first->kind == TYPE_PTR && first->base &&
            first->base->kind == TYPE_CHAR && type_is_integer(second) &&
            second->is_unsigned && second->size ==
                (g_opts.target_arch == ARCH_X64 ? 8 : 4);
        if (!integer_form && !floating_form && !character_form &&
            !string_form) {
            rcc_error(loc,
                      "bounded RCC++ user-defined literal operators require one unsigned long long, double, char, or const char*/size_t parameter form");
        }
    }
    if (match(TOK_NOEXCEPT) && check(TOK_LPAREN)) {
        skip_balanced(TOK_LPAREN, TOK_RPAREN);
    }
    if (match(TOK_LBRACE)) {
        rcc_parser_cxx_begin_function_parameters(params);
        rcc_parser_function_scope_push(name);
        while (!check(TOK_RBRACE) && !at_end()) {
            Token* start = parser.cur;
            Stmt* statement = parse_cxx_statement();
            if (statement) stmtlist_append(&statements, statement);
            if (parser.cur == start && !at_end()) advance();
        }
        expect(TOK_RBRACE, "}");
        rcc_parser_function_scope_pop();
        rcc_parser_cxx_end_function_parameters();
        body = stmt_block(statements, loc);
    } else {
        expect(TOK_SEMICOLON, ";");
    }
    declaration = decl_func(name, cxx_function_type_from_parameters(
        return_type, params), params, body, loc);
    declaration->func_has_cxx_linkage = true;
    return stmt_decl(declaration, loc);
}

typedef struct ParsedConstructorInitializer {
    CxxConstructorInitializer* items;
    int count;
    bool is_supported;
} ParsedConstructorInitializer;

/* Convert the small, ABI-transparent constructor-body form into the same
 * field initializer representation used by a mem-initializer list.  The
 * conversion is deliberately structural: every statement must assign the
 * next data field, and each right-hand side must be either the corresponding
 * constructor parameter or an integer constant for a default constructor.
 * No arbitrary constructor statement is ever interpreted by codegen. */
static bool lowerable_constructor_body(CxxClass* cls,
                                       CxxConstructorInfo* constructor) {
    StmtList* statement_list;
    TypeParam* field;
    TypeParam* parameter;
    CxxConstructorInitializer* items = NULL;
    CxxConstructorInitializer** tail = &items;
    int count = 0;

    if (!cls || !constructor || constructor->initializers ||
        constructor->initializer_count != 0 ||
        !constructor->method || !constructor->method->decl ||
        !constructor->method->decl->func_body ||
        constructor->method->decl->func_body->kind != STMT_BLOCK) {
        return false;
    }
    statement_list = constructor->method->decl->func_body->block_stmts;
    field = cls->fields;
    parameter = constructor->parameters;
    while (statement_list && field) {
        Stmt* statement = statement_list->stmt;
        Expr* assignment;
        const char* field_name = NULL;
        Expr* value;
        CxxConstructorInitializer* item;

        if (!statement || statement->kind != STMT_EXPR ||
            !statement->expr || statement->expr->kind != EXPR_ASSIGN) {
            return false;
        }
        assignment = statement->expr;
        if (assignment->binary_lhs &&
            assignment->binary_lhs->kind == EXPR_IDENT) {
            field_name = assignment->binary_lhs->ident_name;
        } else if (assignment->binary_lhs &&
                   assignment->binary_lhs->kind == EXPR_PTR_MEMBER &&
                   assignment->binary_lhs->member_base &&
                   assignment->binary_lhs->member_base->kind == EXPR_IDENT &&
                   assignment->binary_lhs->member_base->ident_name &&
                   strcmp(assignment->binary_lhs->member_base->ident_name,
                          "this") == 0) {
            field_name = assignment->binary_lhs->member_name;
        }
        if (!field_name || !field->name ||
            strcmp(field_name, field->name) != 0) {
            return false;
        }
        value = assignment->binary_rhs;
        if (!value) return false;
        if (constructor->parameter_count == 0) {
            int64_t constant_value;
            if (!expr_eval_integer_constant(value, &constant_value)) {
                return false;
            }
        } else {
            if (!parameter || !parameter->name ||
                value->kind != EXPR_IDENT ||
                strcmp(value->ident_name, parameter->name) != 0) {
                return false;
            }
            parameter = parameter->next;
        }
        item = ast_arena_alloc(sizeof(*item));
        item->field = field->name;
        item->base_type_pattern = NULL;
        item->value = value;
        item->arguments = NULL;
        item->constructor = NULL;
        item->is_base_initializer = false;
        item->is_virtual_base_initializer = false;
        item->is_delegating_constructor = false;
        item->is_default_member_initializer = false;
        item->is_pack_expansion = false;
        item->next = NULL;
        *tail = item;
        tail = &item->next;
        ++count;
        field = field->next;
        statement_list = statement_list->next;
    }
    if (statement_list || field ||
        (constructor->parameter_count != 0 && parameter) || count == 0) {
        return false;
    }
    constructor->initializers = items;
    constructor->initializer_count = count;
    constructor->initializers_are_supported = true;
    constructor->body_is_empty = true;
    return true;
}

static TypeParam* cxx_constructor_field_parameter(CxxClass* cls,
                                                  const char* name) {
    for (TypeParam* field = cls ? cls->fields : NULL; field;
         field = field->next) {
        if (!field->is_static && field->name && name &&
            strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static const char* cxx_unqualified_name(const char* name) {
    const char* separator;
    if (!name) return NULL;
    separator = strrchr(name, ':');
    return separator && separator > name && separator[-1] == ':'
        ? separator + 1 : name;
}

static bool cxx_constructor_base_name_matches(CxxClass* cls, int index,
                                              const char* name) {
    CxxClass* base;
    const char* declared_name;
    const char* declared_tail;
    const char* name_tail;
    if (!cls || index < 0 || index >= cls->base_count || !name) return false;
    base = cls->bases[index].base;
    declared_name = cls->bases[index].base_name;
    declared_tail = cxx_unqualified_name(declared_name);
    name_tail = cxx_unqualified_name(name);
    if (declared_name && strcmp(declared_name, name) == 0) return true;
    if (base && base->name && strcmp(base->name, name) == 0) return true;
    return (declared_tail && name_tail &&
            strcmp(declared_tail, name_tail) == 0) ||
           (base && base->name && name_tail &&
            strcmp(cxx_unqualified_name(base->name), name_tail) == 0);
}

static int cxx_constructor_base_index(CxxClass* cls, const char* name) {
    if (!cls || !name) return -1;
    for (int index = 0; index < cls->base_count; ++index) {
        if (cxx_constructor_base_name_matches(cls, index, name)) return index;
    }
    return -1;
}

static int cxx_constructor_virtual_base_index(CxxClass* cls,
                                              const char* name) {
    const char* name_tail = cxx_unqualified_name(name);
    if (!cls || !name || !name_tail) return -1;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxClass* base = cls->virtual_bases[index].base;
        const char* base_tail = base ? cxx_unqualified_name(base->name) : NULL;
        if ((base && base->name && strcmp(base->name, name) == 0) ||
            (base_tail && strcmp(base_tail, name_tail) == 0)) {
            return index;
        }
    }
    return -1;
}

static bool cxx_constructor_base_layout_supported(CxxClass* cls, int index) {
    CxxClass* base;
    if (!cls || index < 0 || index >= cls->base_count ||
        !cls->base_offsets || cls->base_offsets[index] < 0 ||
        cls->bases[index].access != ACCESS_PUBLIC) {
        return false;
    }
    base = cls->bases[index].base;
    if (!base || !base->type || !type_is_complete(base->type)) {
        return false;
    }
    /* A class without a user constructor is only safe to zero here when it
     * is genuinely empty.  Zeroing a POD with fields would change the
     * semantics of default-initialization, while a non-trivial member needs
     * its own constructor path. */
    if (!base->constructors &&
        (base->fields || base->base_count || base->has_field_initializer ||
         base->type->cxx_nontrivial)) {
        return false;
    }
    return true;
}

static int cxx_constructor_argument_count(ExprList* arguments) {
    int count = 0;
    for (; arguments; arguments = arguments->next) ++count;
    return count;
}

static bool cxx_constructor_scalar_type(Type* type);
static bool cxx_constructor_scalar_constant(Expr* expression);
static bool cxx_constructor_dmi_expression_is_lowerable(
    CxxClass* cls, TypeParam* initialized_field, Expr* expression);

/* Constructor default arguments are stored on both the declaration
 * parameters and the function type parameters.  Keep the parser-side
 * arity checks independent from semantic analysis so direct initialization
 * can be recognized before the AST is walked. */
static unsigned cxx_constructor_required_parameter_count(
    CxxConstructorInfo* constructor) {
    unsigned required = 0u;
    TypeParam* parameter = constructor ? constructor->parameters : NULL;
    DeclList* declaration = constructor && constructor->method &&
        constructor->method->decl ? constructor->method->decl->func_params : NULL;
    for (; parameter; parameter = parameter->next) {
        if (!declaration || !declaration->decl ||
            !declaration->decl->param_default) {
            ++required;
        } else {
            break;
        }
        declaration = declaration->next;
    }
    return required;
}

static bool cxx_constructor_arity_has_defaults(
    CxxConstructorInfo* constructor, int supplied_count) {
    TypeParam* parameter;
    DeclList* declaration;
    int index;
    if (!constructor || supplied_count < 0 ||
        supplied_count > constructor->parameter_count) return false;
    parameter = constructor->parameters;
    declaration = constructor->method && constructor->method->decl
        ? constructor->method->decl->func_params : NULL;
    for (index = 0; index < supplied_count; ++index) {
        if (!parameter || !declaration) return false;
        parameter = parameter->next;
        declaration = declaration->next;
    }
    while (parameter && declaration) {
        if (!declaration->decl || !declaration->decl->param_default) {
            return false;
        }
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return !parameter && !declaration;
}

static bool cxx_append_constructor_default_arguments(
    ExprList** arguments, CxxConstructorInfo* constructor,
    int supplied_count) {
    TypeParam* parameter;
    DeclList* declaration;
    int index;
    if (!arguments || !constructor ||
        !cxx_constructor_arity_has_defaults(constructor, supplied_count)) {
        return false;
    }
    parameter = constructor->parameters;
    declaration = constructor->method && constructor->method->decl
        ? constructor->method->decl->func_params : NULL;
    for (index = 0; index < supplied_count; ++index) {
        parameter = parameter->next;
        declaration = declaration->next;
    }
    while (parameter && declaration) {
        exprlist_append(arguments, declaration->decl->param_default);
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return true;
}

static bool cxx_constructor_name_matches(CxxClass* cls, const char* name) {
    const char* class_name = cxx_unqualified_name(cls ? cls->name : NULL);
    const char* initializer_name = cxx_unqualified_name(name);
    return class_name && initializer_name &&
           strcmp(class_name, initializer_name) == 0;
}

static CxxConstructorInfo* cxx_find_delegating_constructor(
    CxxClass* cls, CxxConstructorInfo* current, int argument_count) {
    if (!cls || argument_count < 0) return NULL;
    for (CxxConstructorInfo* candidate = cls->constructors;
         candidate; candidate = candidate->next) {
        bool callable_body = candidate->method && candidate->method->decl &&
            candidate->method->decl->func_body;
        if (candidate == current || candidate->access != ACCESS_PUBLIC ||
            candidate->is_deleted || candidate->is_defaulted ||
            candidate->parameter_count < argument_count ||
            (candidate->parameter_count != argument_count &&
             !cxx_constructor_arity_has_defaults(candidate, argument_count)) ||
            !candidate->initializers_are_supported ||
            (!candidate->body_is_empty && !callable_body)) {
            continue;
        }
        return candidate;
    }
    return NULL;
}

static CxxConstructorInfo* cxx_find_base_constructor(CxxClass* base,
                                                      int argument_count) {
    if (!base) return NULL;
    for (CxxConstructorInfo* constructor = base->constructors;
         constructor; constructor = constructor->next) {
        bool callable_body = constructor->method && constructor->method->decl &&
            constructor->method->decl->func_body;
        if (constructor->access != ACCESS_PUBLIC || constructor->is_deleted ||
            constructor->is_defaulted ||
            constructor->parameter_count < argument_count ||
            (constructor->parameter_count != argument_count &&
             !cxx_constructor_arity_has_defaults(constructor, argument_count)) ||
            !constructor->initializers_are_supported ||
            (!constructor->body_is_empty && !callable_body)) {
            continue;
        }
        return constructor;
    }
    return NULL;
}

static CxxConstructorInitializer* cxx_find_base_initializer(
    CxxConstructorInfo* constructor, CxxClass* cls, int base_index,
    bool* duplicate) {
    CxxConstructorInitializer* result = NULL;
    if (duplicate) *duplicate = false;
    for (CxxConstructorInitializer* item = constructor
             ? constructor->initializers : NULL;
         item; item = item->next) {
        if (cxx_constructor_base_index(cls, item->field) != base_index) {
            continue;
        }
        if (result && duplicate) *duplicate = true;
        if (!result) result = item;
    }
    return result;
}

static CxxConstructorInitializer* cxx_find_virtual_base_initializer(
    CxxConstructorInfo* constructor, CxxClass* cls, int virtual_base_index,
    bool* duplicate) {
    CxxConstructorInitializer* result = NULL;
    if (duplicate) *duplicate = false;
    for (CxxConstructorInitializer* item = constructor
             ? constructor->initializers : NULL;
         item; item = item->next) {
        if (cxx_constructor_virtual_base_index(cls, item->field) !=
            virtual_base_index) {
            continue;
        }
        if (result && duplicate) *duplicate = true;
        if (!result) result = item;
    }
    return result;
}

static CxxConstructorInitializer* cxx_copy_constructor_initializer(
    CxxConstructorInitializer* source, const char* field, bool is_base,
    CxxConstructorInfo* base_constructor, bool is_default_member) {
    CxxConstructorInitializer* copy = ast_arena_alloc(sizeof(*copy));
    if (source) {
        *copy = *source;
    } else {
        copy->field = field;
        copy->base_type_pattern = NULL;
        copy->value = NULL;
        copy->arguments = NULL;
        copy->constructor = NULL;
        copy->is_delegating_constructor = false;
        copy->is_virtual_base_initializer = false;
        copy->is_pack_expansion = false;
    }
    copy->constructor = base_constructor;
    copy->is_base_initializer = is_base;
    copy->is_default_member_initializer = is_default_member;
    copy->next = NULL;
    return copy;
}

static bool cxx_using_name_matches_base(const char* using_base,
                                        const CxxClass* base) {
    const char* suffix;
    const char* template_name;
    if (!using_base || !base || !base->name) return false;
    if (strcmp(using_base, base->name) == 0) return true;
    template_name = base->templ ? base->templ->name : NULL;
    if (template_name && strcmp(using_base, template_name) == 0) return true;
    suffix = strstr(using_base, "::");
    while (suffix) {
        suffix += 2;
        if (strcmp(suffix, base->name) == 0 ||
            (template_name && strcmp(suffix, template_name) == 0)) {
            return true;
        }
        suffix = strstr(suffix, "::");
    }
    return false;
}

static bool cxx_constructor_parameter_lists_match(
    const CxxConstructorInfo* left, const CxxConstructorInfo* right) {
    TypeParam* left_parameter = left ? left->parameters : NULL;
    TypeParam* right_parameter = right ? right->parameters : NULL;
    if (!left || !right || left->parameter_count != right->parameter_count) {
        return false;
    }
    while (left_parameter && right_parameter) {
        if (!left_parameter->type || !right_parameter->type ||
            !type_is_compatible(left_parameter->type, right_parameter->type)) {
            return false;
        }
        left_parameter = left_parameter->next;
        right_parameter = right_parameter->next;
    }
    return !left_parameter && !right_parameter;
}

static uint32_t lowerable_constructor_arity_mask(CxxClass* cls);

/* The initializer parser represents C++ `{}` as a value-init compound with a
 * single synthetic zero element.  It is safe to lower for a class member
 * through a user-provided zero-argument constructor: value-initialization does
 * not pre-zero the object when that constructor is user-provided. */
static bool cxx_is_empty_class_value_initializer(const Expr* initializer) {
    const ExprList* item;
    if (!initializer || initializer->kind != EXPR_COMPOUND ||
        !initializer->compound_value_init) {
        return false;
    }
    item = initializer->compound_init;
    return item && !item->next && item->expr &&
           item->expr->kind == EXPR_INT_LIT && item->expr->int_val == 0;
}

/* Preserve a small direct-list initializer for a class member as constructor
 * arguments.  Requiring scalar constants keeps the synthesized inherited
 * constructor independent of the surrounding scope and makes both backends'
 * existing argument binding sufficient. */
static bool cxx_class_member_initializer_arguments(
    Expr* initializer, ExprList** arguments, unsigned* argument_count) {
    ExprList* item;
    unsigned count = 0u;
    if (!arguments || !argument_count) return false;
    *arguments = NULL;
    *argument_count = 0u;
    if (!initializer || initializer->kind != EXPR_COMPOUND) return false;
    if (initializer->compound_value_init) {
        return cxx_is_empty_class_value_initializer(initializer);
    }
    for (item = initializer->compound_init; item; item = item->next) {
        if (item->designator_kind != INIT_DESIGNATOR_NONE || !item->expr ||
            !cxx_constructor_scalar_constant(item->expr) || count >= 31u) {
            return false;
        }
        ++count;
    }
    if (count == 0u) return false;
    *arguments = initializer->compound_init;
    *argument_count = count;
    return true;
}

static bool cxx_inherited_constructor_member_supported(CxxClass* cls) {
    if (!cls) return false;
    for (TypeParam* field = cls->fields; field; field = field->next) {
        Type* type = field->type;
        if (field->is_static) continue;
        if (type && type->kind == TYPE_ARRAY) {
            Type* element_type = type->base;
            ExprList* initializer_arguments = NULL;
            unsigned initializer_argument_count = 0u;
            if ((field->initializer &&
                 (!cxx_class_member_initializer_arguments(
                      field->initializer, &initializer_arguments,
                      &initializer_argument_count) ||
                  initializer_argument_count != 0u)) ||
                !element_type ||
                type->array_len <= 0 || element_type->kind == TYPE_ARRAY) {
                return false;
            }
            if (element_type->cxx_class) {
                CxxClass* member_class = element_type->cxx_class;
                uint32_t constructor_mask =
                    lowerable_constructor_arity_mask(member_class);
                if (!member_class->type || member_class->destructor_method ||
                    member_class->type->cleanup_function ||
                    (constructor_mask & UINT32_C(1)) == 0u) {
                    return false;
                }
            }
            continue;
        }
        if (type && type->cxx_class) {
            CxxClass* member_class = type->cxx_class;
            ExprList* initializer_arguments = NULL;
            unsigned initializer_argument_count = 0u;
            uint32_t constructor_mask;
            if (field->initializer &&
                !cxx_class_member_initializer_arguments(
                    field->initializer, &initializer_arguments,
                    &initializer_argument_count)) {
                return false;
            }
            (void)initializer_arguments;
            constructor_mask = lowerable_constructor_arity_mask(member_class);
            if (!member_class->type ||
                member_class->destructor_method ||
                member_class->type->cleanup_function ||
                initializer_argument_count >= 32u ||
                (constructor_mask &
                 (UINT32_C(1) << initializer_argument_count)) == 0u) {
                return false;
            }
            continue;
        }
        if (field->initializer &&
            (!type || type->size <= 0 ||
             !(type_is_integer(type) || type->kind == TYPE_ENUM ||
               type->kind == TYPE_PTR || type->kind == TYPE_NULLPTR ||
               type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE) ||
             !cxx_constructor_scalar_constant(field->initializer) ||
             (g_opts.target_arch == ARCH_X86 && type->size > 8) ||
             (g_opts.target_arch == ARCH_X64 && type->size > 8))) {
            return false;
        }
    }
    return true;
}

static bool cxx_inherited_constructor_source_supported(
    const CxxConstructorInfo* source) {
    Type* function_type = source && source->method && source->method->decl
        ? source->method->decl->type : NULL;
    return source && source->method && source->method->decl &&
           function_type && function_type->kind == TYPE_FUNC &&
           function_type->has_prototype && !function_type->variadic &&
           !source->is_deleted && !source->is_defaulted &&
           source->access == ACCESS_PUBLIC &&
           source->initializers_are_supported;
}

static CxxConstructorInfo* cxx_make_inherited_constructor(
    CxxClass* cls, CxxClass* base, CxxConstructorInfo* source) {
    CxxMethod* method;
    CxxConstructorInfo* inherited;
    CxxConstructorInitializer* initializer;
    DeclList* parameter;
    if (!cls || !base || !source || !source->method ||
        !cxx_inherited_constructor_source_supported(source)) {
        return NULL;
    }
    method = cxx_method_new(cls->name, type_void,
                            source->method->decl->func_params, NULL,
                            source->method->decl->loc);
    method->decl->type->has_prototype =
        source->method->decl->type->has_prototype;
    method->decl->type->variadic = source->method->decl->type->variadic;
    method->source_name = cls->name;
    method->owner = cls;
    method->access = source->access;
    method->is_constructor = true;
    method->is_explicit = source->method->is_explicit;
    method->is_noexcept = source->method->is_noexcept;
    method->is_constexpr = source->method->is_constexpr;
    method->decl->func_is_cxx_method = true;
    method->decl->func_has_cxx_linkage = true;
    method->decl->func_is_cxx_constructor = true;
    method->decl->func_is_constexpr =
        source->method->decl->func_is_constexpr;
    method->decl->func_is_consteval =
        source->method->decl->func_is_consteval;
    method->decl->func_is_noreturn =
        source->method->decl->func_is_noreturn;
    method->decl->func_is_nodiscard =
        source->method->decl->func_is_nodiscard;
    method->decl->func_is_deprecated =
        source->method->decl->func_is_deprecated;
    method->decl->func_deprecated_message =
        source->method->decl->func_deprecated_message;
    method->decl->func_is_inline = source->method->decl->func_is_inline;
    method->decl->func_is_noexcept = method->is_noexcept;
    method->decl->func_noexcept_expr = source->method->decl->func_noexcept_expr;
    method->decl->func_method_owner = cls->type;

    inherited = rcc_alloc(sizeof(*inherited));
    inherited->method = method;
    inherited->parameter_count = source->parameter_count;
    inherited->parameters = source->parameters;
    inherited->initializers_are_supported = true;
    inherited->body_is_empty = true;
    inherited->is_deleted = false;
    inherited->is_defaulted = false;
    inherited->is_inherited = true;
    inherited->access = source->access;

    initializer = ast_arena_alloc(sizeof(*initializer));
    initializer->arguments = NULL;
    initializer->field = rcc_intern(base->name);
    initializer->base_type_pattern = NULL;
    initializer->constructor = source;
    initializer->is_base_initializer = true;
    initializer->is_virtual_base_initializer = false;
    initializer->is_delegating_constructor = false;
    initializer->is_default_member_initializer = false;
    initializer->is_pack_expansion = false;
    initializer->next = NULL;
    for (parameter = source->method->decl->func_params;
         parameter; parameter = parameter->next) {
        if (!parameter->decl || !parameter->decl->name) return NULL;
        exprlist_append(&initializer->arguments,
                        expr_ident(parameter->decl->name,
                                   parameter->decl->loc));
    }
    initializer->value = initializer->arguments
        ? initializer->arguments->expr : NULL;
    inherited->initializers = initializer;
    inherited->initializer_count = 1;
    return inherited;
}

/* Materialize only the bounded form of `using Base::Base`: public direct
 * non-virtual bases with scalar fields, safely lowerable class members, and
 * constant scalar or bounded scalar-list class default member initializers.
 * The synthesized constructor owns the derived object but delegates base
 * initialization to the original constructor, so no fake function body or
 * unresolved symbol is emitted. */
static void cxx_materialize_inherited_constructors(CxxClass* cls) {
    CxxConstructorInfo** tail;
    if (!cls || cls->base_count == 0) return;
    for (int using_index = 0;
         using_index < cls->using_base_member_count; ++using_index) {
        const char* member_name =
            cls->using_base_members[using_index].member_name;
        bool names_constructor = false;
        for (int base_index = 0; base_index < cls->base_count;
             ++base_index) {
            CxxClass* base = cls->bases[base_index].base;
            if (!base || !cxx_using_name_matches_base(
                             cls->using_base_members[using_index].base_name,
                             base) ||
                !cxx_using_name_matches_base(member_name, base)) {
                continue;
            }
            names_constructor = true;
            if (cls->bases[base_index].access != ACCESS_PUBLIC) {
                rcc_error(cls->using_base_members[using_index].loc,
                          "using-base constructor requires a public direct base");
            } else if (cls->bases[base_index].is_virtual) {
                rcc_error(cls->using_base_members[using_index].loc,
                          "using-base constructor cannot name a virtual base");
            } else if (!cxx_inherited_constructor_member_supported(cls)) {
                rcc_error(cls->using_base_members[using_index].loc,
                          "using-base constructors require safely lowerable derived "
                          "members and supported default member initializers in the "
                          "bounded RCC++ profile");
            } else {
                bool has_lowerable_source = false;
                for (CxxConstructorInfo* source = base->constructors;
                     source; source = source->next) {
                    if (!cxx_inherited_constructor_source_supported(source)) {
                        continue;
                    }
                    if (source->parameter_count == 1 && source->parameters &&
                        source->parameters->type &&
                        source->parameters->type->kind == TYPE_PTR &&
                        source->parameters->type->is_reference &&
                        source->parameters->type->base &&
                        type_is_compatible(source->parameters->type->base,
                                           base->type)) {
                        continue;
                    }
                    has_lowerable_source = true;
                    break;
                }
                if (!has_lowerable_source) {
                    rcc_error(cls->using_base_members[using_index].loc,
                              "using-base constructor has no safely lowerable "
                              "base overloads");
                }
            }
            break;
        }
        if (!names_constructor && member_name &&
            cls->using_base_members[using_index].base_name &&
            strcmp(cxx_unqualified_name(member_name),
                   cxx_unqualified_name(
                       cls->using_base_members[using_index].base_name)) == 0) {
            rcc_error(cls->using_base_members[using_index].loc,
                      "using-base constructor names an unknown direct base");
        }
    }
    if (!cxx_inherited_constructor_member_supported(cls)) return;
    tail = &cls->constructors;
    while (*tail) tail = &(*tail)->next;
    for (int base_index = 0; base_index < cls->base_count; ++base_index) {
        CxxClass* base = cls->bases[base_index].base;
        bool inherited = false;
        if (!base || cls->bases[base_index].access != ACCESS_PUBLIC ||
            cls->bases[base_index].is_virtual) continue;
        for (int index = 0; index < cls->using_base_member_count; ++index) {
            const char* member_name = cls->using_base_members[index].member_name;
            if (cxx_using_name_matches_base(
                    cls->using_base_members[index].base_name, base) &&
                cxx_using_name_matches_base(member_name, base)) {
                inherited = true;
                break;
            }
        }
        if (!inherited) continue;
        for (CxxConstructorInfo* source = base->constructors;
             source; source = source->next) {
            CxxConstructorInfo* duplicate = NULL;
            CxxConstructorInfo* copy;
            if (!cxx_inherited_constructor_source_supported(source) ||
                (source->parameter_count == 1 && source->parameters &&
                source->parameters->type &&
                source->parameters->type->kind == TYPE_PTR &&
                source->parameters->type->is_reference &&
                source->parameters->type->base &&
                type_is_compatible(source->parameters->type->base, base->type))) {
                continue;
            }
            for (CxxConstructorInfo* existing = cls->constructors;
                 existing; existing = existing->next) {
                if (cxx_constructor_parameter_lists_match(existing, source)) {
                    duplicate = existing;
                    break;
                }
            }
            if (duplicate) continue;
            copy = cxx_make_inherited_constructor(cls, base, source);
            if (!copy) {
                rcc_error((SourceLoc){"<class>", 0, 0},
                          "using-base constructor is not safely lowerable");
                continue;
            }
            *tail = copy;
            tail = &copy->next;
            cls->has_user_constructor = true;
        }
    }
}

static CxxConstructorInitializer* cxx_find_constructor_initializer(
    CxxConstructorInfo* constructor, const char* field, bool* duplicate) {
    CxxConstructorInitializer* result = NULL;
    if (duplicate) *duplicate = false;
    for (CxxConstructorInitializer* item = constructor
             ? constructor->initializers : NULL;
         item; item = item->next) {
        if (!item->field || !field || strcmp(item->field, field) != 0) {
            continue;
        }
        if (result && duplicate) *duplicate = true;
        if (!result) result = item;
    }
    return result;
}

/* Complete a constructor's effective base/member-initializer sequence in
 * declaration order.  Base initialization is limited to public bases whose
 * constructor overload and concrete subobject offset are known.  Virtual
 * bases use the most-derived fixed layout offset; their pointer conversions
 * use the runtime vbtable after construction. */
static void complete_cxx_default_member_initializers(CxxClass* cls) {
    if (!cls || (!cls->has_field_initializer && cls->base_count == 0 &&
                 !cls->constructors)) return;
    cxx_materialize_inherited_constructors(cls);
    for (CxxConstructorInfo* constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        CxxConstructorInitializer* ordered = NULL;
        CxxConstructorInitializer** tail = &ordered;
        CxxConstructorInitializer* delegation = NULL;
        bool valid = constructor->initializers_are_supported;
        int count = 0;

        if (!valid && constructor->initializer_count != 0) continue;
        for (CxxConstructorInitializer* item = constructor->initializers;
             item; item = item->next) {
            bool duplicate = false;
            if (cxx_constructor_name_matches(cls, item->field)) {
                int argument_count = cxx_constructor_argument_count(
                    item->arguments);
                CxxConstructorInfo* target =
                    cxx_find_delegating_constructor(
                        cls, constructor, argument_count);
                if (delegation || constructor->initializer_count != 1 ||
                    !target || target == constructor) {
                    valid = false;
                    continue;
                }
                if (target->parameter_count != argument_count &&
                    !cxx_append_constructor_default_arguments(
                        &item->arguments, target, argument_count)) {
                    valid = false;
                    continue;
                }
                item->value = item->arguments ? item->arguments->expr : NULL;
                item->constructor = target;
                item->is_base_initializer = false;
                item->is_delegating_constructor = true;
                item->is_default_member_initializer = false;
                delegation = item;
                continue;
            }
            int base_index = cxx_constructor_base_index(cls, item->field);
            if (base_index >= 0) {
                CxxClass* base = cls->bases[base_index].base;
                int argument_count = cxx_constructor_argument_count(
                    item->arguments);
                CxxConstructorInfo* base_constructor;
                if (cxx_find_base_initializer(constructor, cls, base_index,
                                              &duplicate) != item || duplicate ||
                    !cxx_constructor_base_layout_supported(cls, base_index)) {
                    valid = false;
                    continue;
                }
                base_constructor = constructor->is_inherited
                    ? item->constructor
                    : cxx_find_base_constructor(base, argument_count);
                if ((!base_constructor && argument_count != 0) ||
                    (!base_constructor && base && base->constructors)) {
                    valid = false;
                    continue;
                }
                if (base_constructor &&
                    argument_count != base_constructor->parameter_count &&
                    !cxx_append_constructor_default_arguments(
                        &item->arguments, base_constructor, argument_count)) {
                    valid = false;
                    continue;
                }
                item->value = item->arguments ? item->arguments->expr : NULL;
                item->constructor = base_constructor;
                item->is_base_initializer = true;
                item->is_virtual_base_initializer = cls->bases[base_index].is_virtual;
                item->is_default_member_initializer = false;
            } else {
                int virtual_base_index =
                    cxx_constructor_virtual_base_index(cls, item->field);
                if (virtual_base_index >= 0) {
                    CxxClass* virtual_base =
                        cls->virtual_bases[virtual_base_index].base;
                    int argument_count = cxx_constructor_argument_count(
                        item->arguments);
                    CxxConstructorInfo* virtual_constructor;
                    if (cxx_find_virtual_base_initializer(
                            constructor, cls, virtual_base_index,
                            &duplicate) != item || duplicate ||
                        !cls->virtual_bases[virtual_base_index].public_path) {
                        valid = false;
                        continue;
                    }
                    virtual_constructor = cxx_find_base_constructor(
                        virtual_base, argument_count);
                    if ((!virtual_constructor && argument_count != 0) ||
                        (!virtual_constructor && virtual_base &&
                         virtual_base->constructors)) {
                        valid = false;
                        continue;
                    }
                    if (virtual_constructor &&
                        argument_count != virtual_constructor->parameter_count &&
                        !cxx_append_constructor_default_arguments(
                            &item->arguments, virtual_constructor,
                            argument_count)) {
                        valid = false;
                        continue;
                    }
                    item->value = item->arguments ? item->arguments->expr : NULL;
                    item->constructor = virtual_constructor;
                    item->is_base_initializer = true;
                    item->is_virtual_base_initializer = true;
                    item->is_default_member_initializer = false;
                } else if (!cxx_constructor_field_parameter(cls, item->field) ||
                           cxx_find_constructor_initializer(
                               constructor, item->field, &duplicate) != item ||
                           duplicate) {
                    valid = false;
                } else {
                    item->is_base_initializer = false;
                }
            }
        }
        if (!valid) {
            constructor->initializers_are_supported = false;
            continue;
        }
        if (delegation) {
            constructor->initializers = cxx_copy_constructor_initializer(
                delegation, delegation->field, false, delegation->constructor,
                false);
            constructor->initializers->is_delegating_constructor = true;
            constructor->initializer_count = 1;
            constructor->initializers_are_supported = true;
            continue;
        }

        for (int base_index = 0; base_index < cls->base_count; ++base_index) {
            CxxClass* base = cls->bases[base_index].base;
            CxxConstructorInitializer* item = cxx_find_base_initializer(
                constructor, cls, base_index, NULL);
            CxxConstructorInfo* base_constructor;
            if (!cxx_constructor_base_layout_supported(cls, base_index)) {
                valid = false;
                break;
            }
            if (!item) {
                base_constructor = cxx_find_base_constructor(base, 0);
                if ((!base_constructor && base && base->constructors) ||
                    (!base_constructor && base &&
                     (base->fields || base->base_count ||
                      base->has_field_initializer ||
                      base->type->cxx_nontrivial))) {
                    valid = false;
                    break;
                }
                item = cxx_copy_constructor_initializer(
                    NULL, base && base->name ? base->name : NULL, true,
                    base_constructor, false);
                item->is_virtual_base_initializer =
                    cls->bases[base_index].is_virtual;
                if (base_constructor &&
                    !cxx_append_constructor_default_arguments(
                        &item->arguments, base_constructor, 0)) {
                    valid = false;
                    break;
                }
                item->value = item->arguments ? item->arguments->expr : NULL;
            } else {
                item = cxx_copy_constructor_initializer(
                    item, item->field, true, item->constructor, false);
            }
            *tail = item;
            tail = &item->next;
            ++count;
        }
        if (!valid) {
            constructor->initializers_are_supported = false;
            continue;
        }
        for (int virtual_base_index = 0;
             virtual_base_index < cls->virtual_base_count;
             ++virtual_base_index) {
            CxxClass* virtual_base =
                cls->virtual_bases[virtual_base_index].base;
            CxxConstructorInitializer* item = cxx_find_virtual_base_initializer(
                constructor, cls, virtual_base_index, NULL);
            CxxConstructorInfo* virtual_constructor;
            bool direct_virtual = false;
            for (int base_index = 0; base_index < cls->base_count;
                 ++base_index) {
                if (cls->bases[base_index].is_virtual &&
                    cls->bases[base_index].base == virtual_base) {
                    direct_virtual = true;
                    break;
                }
            }
            if (direct_virtual) continue;
            if (item) {
                item = cxx_copy_constructor_initializer(
                    item, item->field, true, item->constructor, false);
                item->is_virtual_base_initializer = true;
                *tail = item;
                tail = &item->next;
                ++count;
                continue;
            }
            if (!cls->virtual_bases[virtual_base_index].public_path) {
                valid = false;
                break;
            }
            virtual_constructor = cxx_find_base_constructor(virtual_base, 0);
            if ((!virtual_constructor && virtual_base &&
                 virtual_base->constructors) ||
                (!virtual_constructor && virtual_base &&
                 (virtual_base->fields || virtual_base->base_count ||
                  virtual_base->has_field_initializer ||
                  virtual_base->type->cxx_nontrivial))) {
                valid = false;
                break;
            }
            item = cxx_copy_constructor_initializer(
                NULL, virtual_base && virtual_base->name
                    ? virtual_base->name : NULL,
                true, virtual_constructor, false);
            item->is_virtual_base_initializer = true;
            if (virtual_constructor &&
                !cxx_append_constructor_default_arguments(
                    &item->arguments, virtual_constructor, 0)) {
                valid = false;
                break;
            }
            item->value = item->arguments ? item->arguments->expr : NULL;
            *tail = item;
            tail = &item->next;
            ++count;
        }
        if (!valid) {
            constructor->initializers_are_supported = false;
            continue;
        }
        for (TypeParam* field = cls->fields; field; field = field->next) {
            CxxConstructorInitializer* item;
            bool class_object_member = field->type &&
                (field->type->cxx_class ||
                 (field->type->kind == TYPE_ARRAY && field->type->base &&
                  field->type->base->cxx_class));
            if (field->is_static || !field->name) continue;
            item = cxx_find_constructor_initializer(
                constructor, field->name, NULL);
            if (!item && field->initializer &&
                !(class_object_member &&
                  cxx_is_empty_class_value_initializer(field->initializer))) {
                Type* type = field->type;
                if (!type || (!type->cxx_dependent && type->size <= 0) ||
                    (!cxx_constructor_scalar_type(type) &&
                     !type->cxx_dependent && type->kind != TYPE_ARRAY) ||
                    !cxx_constructor_dmi_value_is_lowerable(
                        cls, field, type, field->initializer) ||
                    (type->kind == TYPE_ARRAY && type->size > 512) ||
                    (type->kind != TYPE_ARRAY &&
                     ((g_opts.target_arch == ARCH_X86 && type->size > 8) ||
                      (g_opts.target_arch == ARCH_X64 && type->size > 8)))) {
                    valid = false;
                    break;
                }
                item = ast_arena_alloc(sizeof(*item));
                item->field = field->name;
                item->base_type_pattern = NULL;
                item->value = field->initializer;
                item->arguments = NULL;
                item->constructor = NULL;
                item->is_base_initializer = false;
                item->is_virtual_base_initializer = false;
                item->is_delegating_constructor = false;
                item->is_default_member_initializer = true;
                item->is_pack_expansion = false;
                item->next = NULL;
            } else if (!item && field->type &&
                       ((field->type->cxx_class != NULL) ||
                        (field->type->kind == TYPE_ARRAY &&
                         field->type->base &&
                         field->type->base->cxx_class))) {
                Type* member_type = field->type->kind == TYPE_ARRAY
                    ? field->type->base : field->type;
                CxxClass* member_class = member_type->cxx_class;
                ExprList* member_arguments = NULL;
                unsigned member_argument_count = 0u;
                CxxConstructorInfo* member_constructor =
                    NULL;
                if (field->initializer &&
                    !cxx_class_member_initializer_arguments(
                        field->initializer, &member_arguments,
                        &member_argument_count)) {
                    valid = false;
                    break;
                }
                member_constructor = cxx_find_base_constructor(
                    member_class, (int)member_argument_count);
                if (!member_constructor || !member_class->type ||
                    member_argument_count >= 32u ||
                    (lowerable_constructor_arity_mask(member_class) &
                     (UINT32_C(1) << member_argument_count)) == 0u) {
                    valid = false;
                    break;
                }
                item = ast_arena_alloc(sizeof(*item));
                item->field = field->name;
                item->base_type_pattern = NULL;
                item->value = NULL;
                item->arguments = member_arguments;
                item->constructor = member_constructor;
                item->is_base_initializer = false;
                item->is_virtual_base_initializer = false;
                item->is_delegating_constructor = false;
                item->is_default_member_initializer =
                    field->initializer != NULL;
                item->is_pack_expansion = false;
                item->next = NULL;
            }
            if (item) {
                item = cxx_copy_constructor_initializer(
                    item, item->field, false, item->constructor,
                    item->is_default_member_initializer);
                *tail = item;
                tail = &item->next;
                ++count;
            }
        }
        if (!valid) {
            constructor->initializers_are_supported = false;
            continue;
        }
        constructor->initializers = ordered;
        constructor->initializer_count = count;
        constructor->initializers_are_supported = true;
    }
}

/* Retain a parenthesized mem-initializer list.  It is lowered only when the
 * later verifier proves a one-to-one, declaration-order mapping from fields
 * to constructor parameters (or integer zeroes for a default constructor). */
static ParsedConstructorInitializer parse_ctor_initializer(void) {
    ParsedConstructorInitializer result = {0};
    bool supported = true;
    CxxConstructorInitializer** tail = &result.items;
    if (!match(TOK_COLON)) return result;
    do {
        const char* field = NULL;
        Type* base_type_pattern = NULL;
        Expr* value = NULL;
        ExprList* arguments = NULL;
        bool current_supported = true;
        bool is_pack_expansion = false;
        CxxConstructorInitializer* item;
        if (check(TOK_TYPENAME)) {
            base_type_pattern = parse_cxx_type_spec();
            field = base_type_pattern ? base_type_pattern->tag : NULL;
        } else if (check(TOK_IDENT) || check(TOK_SCOPE)) {
            Token* name_start = peek();
            Token* previous_before_name = parser.prev;
            field = parse_qualified_name();
            if (check(TOK_LT)) {
                parser.cur = name_start;
                parser.prev = previous_before_name;
                base_type_pattern = parse_cxx_type_spec();
                field = base_type_pattern ? base_type_pattern->tag : field;
            }
        } else {
            rcc_error(peek()->loc, "expected constructor initializer name");
            return result;
        }
        if (!field) {
            rcc_error(peek()->loc,
                      "constructor initializer type has no class name");
            return result;
        }
        if (base_type_pattern && base_type_pattern->kind != TYPE_STRUCT) {
            rcc_error(peek()->loc,
                      "constructor base initializer must name a class type");
            current_supported = false;
        }
        if (match(TOK_LPAREN)) {
            if (!check(TOK_RPAREN)) {
                do {
                    exprlist_append(&arguments,
                                    parse_assignment_expression());
                } while (match(TOK_COMMA));
                value = arguments ? arguments->expr : NULL;
            }
            expect(TOK_RPAREN, ")");
            is_pack_expansion = match(TOK_ELLIPSIS);
        } else if (check(TOK_LBRACE)) {
            skip_balanced(TOK_LBRACE, TOK_RBRACE);
            is_pack_expansion = match(TOK_ELLIPSIS);
            current_supported = false;
        } else {
            rcc_error(peek()->loc, "expected constructor initializer");
            return result;
        }
        item = ast_arena_alloc(sizeof(*item));
        item->field = field;
        item->base_type_pattern = base_type_pattern;
        item->value = value;
        item->arguments = arguments;
        item->constructor = NULL;
        item->is_base_initializer = false;
        item->is_virtual_base_initializer = false;
        item->is_delegating_constructor = false;
        item->is_default_member_initializer = false;
        item->is_pack_expansion = is_pack_expansion;
        item->next = NULL;
        *tail = item;
        tail = &item->next;
        ++result.count;
        if (!current_supported) supported = false;
    } while (match(TOK_COMMA));
    result.is_supported = result.count != 0 && supported;
    return result;
}

static bool class_has_virtual_member(CxxClass* cls) {
    struct CxxMember* member;
    for (member = cls->members; member; member = member->next) {
        if (member->is_virtual) return true;
    }
    return false;
}

static bool class_has_destructor(CxxClass* cls) {
    struct CxxMember* member;
    for (member = cls ? cls->members : NULL; member; member = member->next) {
        if (member->method && member->method->is_destructor) return true;
    }
    return false;
}

static int cxx_constructor_parameter_index(CxxConstructorInfo* constructor,
                                           const char* name) {
    int index = 0;
    if (!constructor || !name) return -1;
    for (TypeParam* parameter = constructor->parameters; parameter;
         parameter = parameter->next, ++index) {
        if (parameter->name && strcmp(parameter->name, name) == 0) {
            return index;
        }
    }
    return -1;
}

static Type* cxx_constructor_value_type(Type* type) {
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        return type->base;
    }
    return type;
}

static bool cxx_constructor_scalar_type(Type* type) {
    return type && (type_is_integer(type) || type->kind == TYPE_ENUM ||
                    type->kind == TYPE_PTR || type->kind == TYPE_NULLPTR ||
                    type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE);
}

static bool cxx_constructor_scalar_constant(Expr* expression) {
    int64_t integer_value;
    if (!expression) return false;
    if (expr_eval_integer_constant(expression, &integer_value)) return true;
    switch (expression->kind) {
        case EXPR_FLOAT_LIT:
            return true;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            return cxx_constructor_scalar_constant(
                expression->unary_operand);
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
            return cxx_constructor_scalar_constant(expression->binary_lhs) &&
                   cxx_constructor_scalar_constant(expression->binary_rhs);
        case EXPR_COND:
            return cxx_constructor_scalar_constant(expression->cond_test) &&
                   cxx_constructor_scalar_constant(expression->cond_then) &&
                   cxx_constructor_scalar_constant(expression->cond_else);
        case EXPR_CAST:
            return cxx_constructor_scalar_constant(expression->cast_expr);
        default:
            return false;
    }
}

static bool cxx_constructor_dmi_field_is_earlier(
    CxxClass* cls, TypeParam* initialized_field, const char* member_name) {
    if (!cls || !initialized_field || !member_name) return false;
    for (TypeParam* field = cls->fields;
         field && field != initialized_field; field = field->next) {
        if (!field->is_static && field->name &&
            strcmp(field->name, member_name) == 0) {
            return field->type &&
                (cxx_constructor_scalar_type(field->type) ||
                 field->type->cxx_dependent);
        }
    }
    return false;
}

static bool cxx_constructor_dmi_lvalue_is_lowerable(
    CxxClass* cls, TypeParam* initialized_field, Expr* expression) {
    if (!expression) return false;
    if (expression->kind == EXPR_IDENT && expression->ident_name) {
        for (TypeParam* field = cls ? cls->fields : NULL;
             field; field = field->next) {
            if (!field->is_static && field->name &&
                strcmp(field->name, expression->ident_name) == 0) {
                return field != initialized_field &&
                    cxx_constructor_dmi_field_is_earlier(
                        cls, initialized_field, expression->ident_name);
            }
        }
        /* Non-member names are checked by semantic analysis when the
         * specialized constructor is resolved. */
        return true;
    }
    if ((expression->kind == EXPR_MEMBER ||
         expression->kind == EXPR_PTR_MEMBER) &&
        expression->member_base &&
        expression->member_base->kind == EXPR_IDENT &&
        expression->member_base->ident_name &&
        strcmp(expression->member_base->ident_name, "this") == 0) {
        return cxx_constructor_dmi_field_is_earlier(
            cls, initialized_field, expression->member_name);
    }
    return false;
}

/* Dynamic scalar DMIs are evaluated by the constructor in declaration order.
 * Calls, assignments, comma expressions, and increments are admitted only
 * when their operands stay within scalar expressions and their writes target
 * a global scalar name or an already initialized field. */
static bool cxx_constructor_dmi_expression_is_lowerable(
    CxxClass* cls, TypeParam* initialized_field, Expr* expression) {
    if (!expression) return false;
    if (cxx_constructor_scalar_constant(expression)) return true;
    switch (expression->kind) {
        case EXPR_IDENT:
            return expression->ident_name &&
                cxx_constructor_dmi_field_is_earlier(
                    cls, initialized_field, expression->ident_name);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return expression->member_base &&
                expression->member_base->kind == EXPR_IDENT &&
                expression->member_base->ident_name &&
                strcmp(expression->member_base->ident_name, "this") == 0 &&
                cxx_constructor_dmi_field_is_earlier(
                    cls, initialized_field, expression->member_name);
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            return cxx_constructor_dmi_expression_is_lowerable(
                cls, initialized_field, expression->unary_operand);
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
            return cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->binary_lhs) &&
                   cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->binary_rhs);
        case EXPR_COND:
            return cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->cond_test) &&
                   cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->cond_then) &&
                   cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->cond_else);
        case EXPR_CAST:
            return cxx_constructor_scalar_type(expression->cast_type) &&
                   cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->cast_expr);
        case EXPR_CALL:
            if (!expression->call_func ||
                (expression->call_func->kind != EXPR_IDENT &&
                 expression->call_func->kind != EXPR_MEMBER &&
                 expression->call_func->kind != EXPR_PTR_MEMBER)) {
                return false;
            }
            for (ExprList* argument = expression->call_args;
                 argument; argument = argument->next) {
                if (!cxx_constructor_dmi_expression_is_lowerable(
                        cls, initialized_field, argument->expr)) {
                    return false;
                }
            }
            return true;
        case EXPR_COMPOUND:
            for (ExprList* item = expression->compound_init;
                 item; item = item->next) {
                if (item->designator_kind != INIT_DESIGNATOR_NONE ||
                    !cxx_constructor_dmi_expression_is_lowerable(
                        cls, initialized_field, item->expr)) {
                    return false;
                }
            }
            return true;
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
            return cxx_constructor_dmi_lvalue_is_lowerable(
                       cls, initialized_field, expression->binary_lhs) &&
                   cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->binary_rhs);
        case EXPR_COMMA:
            return cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->binary_lhs) &&
                   cxx_constructor_dmi_expression_is_lowerable(
                       cls, initialized_field, expression->binary_rhs);
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            return cxx_constructor_dmi_lvalue_is_lowerable(
                cls, initialized_field, expression->unary_operand);
        default:
            return false;
    }
}

static bool cxx_constructor_dmi_value_is_lowerable(
    CxxClass* cls, TypeParam* initialized_field, Type* type,
    Expr* expression) {
    if (!type || !expression) return false;
    if (cxx_constructor_scalar_type(type)) {
        return cxx_constructor_dmi_expression_is_lowerable(
            cls, initialized_field, expression);
    }
    if (type->kind == TYPE_ARRAY) {
        if (!type->base || type->array_len <= 0 || type->array_len > 64 ||
            expression->kind != EXPR_COMPOUND) {
            return false;
        }
        int element_count = 0;
        for (ExprList* item = expression->compound_init;
             item; item = item->next) {
            if (item->designator_kind != INIT_DESIGNATOR_NONE ||
                !item->expr || ++element_count > type->array_len) {
                return false;
            }
            if (type->base->kind == TYPE_ARRAY) {
                if (!cxx_constructor_dmi_value_is_lowerable(
                        cls, initialized_field, type->base, item->expr)) {
                    return false;
                }
            } else if ((cxx_constructor_scalar_type(type->base) ||
                        type->base->cxx_dependent) &&
                       cxx_constructor_dmi_expression_is_lowerable(
                           cls, initialized_field, item->expr)) {
                continue;
            } else {
                return false;
            }
        }
        return true;
    }
    /* A dependent class-template specialization is class-valued even though
     * its layout has not been substituted yet.  Keep it on the member
     * constructor path instead of treating a brace initializer as a scalar
     * aggregate expression.  A bare template type parameter still uses the
     * bounded dependent-scalar path below. */
    if (type->cxx_dependent && type->kind == TYPE_STRUCT &&
        (type->cxx_template || type->cxx_dependent_member_name)) {
        return false;
    }
    if (type->cxx_dependent) {
        return cxx_constructor_dmi_expression_is_lowerable(
            cls, initialized_field, expression);
    }
    return false;
}

/* A pointer member may be initialized from the address of an object with
 * static storage.  Keep this separate from integer constant evaluation: the
 * address must remain a relocatable symbol reference until the native object
 * or image emitter lays it out. */
static bool cxx_constructor_static_address(CxxConstructorInfo* constructor,
                                            Expr* expression) {
    Expr* operand;
    if (!expression) return false;
    if (expression->kind == EXPR_CAST) {
        return cxx_constructor_static_address(constructor,
                                              expression->cast_expr);
    }
    if (expression->kind != EXPR_ADDR) return false;
    operand = expression->unary_operand;
    return operand && operand->kind == EXPR_IDENT && operand->ident_name &&
        cxx_constructor_parameter_index(constructor, operand->ident_name) < 0;
}

static bool cxx_constructor_expression_is_lowerable(
    CxxConstructorInfo* constructor, Expr* expression, Type* target_type,
    bool* parameter_used, unsigned parameter_count) {
    if (!expression || (target_type &&
                        !cxx_constructor_scalar_type(target_type))) {
        return false;
    }
    if (expression->kind == EXPR_IDENT) {
        int index = cxx_constructor_parameter_index(
            constructor, expression->ident_name);
        TypeParam* parameter = constructor ? constructor->parameters : NULL;
        for (int step = 0; parameter && step < index; ++step) {
            parameter = parameter->next;
        }
        if (index < 0 || !parameter ||
            !cxx_constructor_scalar_type(parameter->type) ||
            (target_type &&
             !type_is_compatible(cxx_constructor_value_type(parameter->type),
                                 cxx_constructor_value_type(target_type)))) {
            return false;
        }
        if (parameter_used && index < (int)parameter_count) {
            parameter_used[index] = true;
        }
        return true;
    }
    if (expression->kind == EXPR_MEMBER && expression->member_base &&
        expression->member_base->kind == EXPR_IDENT &&
        expression->member_name) {
        int index = cxx_constructor_parameter_index(
            constructor, expression->member_base->ident_name);
        TypeParam* parameter = constructor ? constructor->parameters : NULL;
        Type* source_type;
        Type* canonical_source_type;
        TypeField* source_field;
        for (int step = 0; parameter && step < index; ++step) {
            parameter = parameter->next;
        }
        source_type = parameter
            ? cxx_constructor_value_type(parameter->type) : NULL;
        if (index < 0 || !parameter ||
            !(parameter->type && parameter->type->kind == TYPE_PTR &&
              parameter->type->is_reference) ||
            !source_type ||
            (source_type->kind != TYPE_STRUCT &&
             source_type->kind != TYPE_UNION) ||
            !constructor->method || !constructor->method->owner ||
            source_type->cxx_class != constructor->method->owner) {
            return false;
        }
        canonical_source_type = source_type->fields ? source_type
            : source_type->cxx_class->type;
        if (!canonical_source_type) return false;
        for (source_field = canonical_source_type->fields; source_field;
             source_field = source_field->next) {
            if (source_field->name &&
                strcmp(source_field->name, expression->member_name) == 0) {
                break;
            }
        }
        if (!source_field || !cxx_constructor_scalar_type(source_field->type) ||
            (target_type &&
             !type_is_compatible(
                 cxx_constructor_value_type(source_field->type),
                 cxx_constructor_value_type(target_type)))) {
            return false;
        }
        if (parameter_used && index < (int)parameter_count) {
            parameter_used[index] = true;
        }
        return true;
    }
    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_FLOAT_LIT:
            return !target_type || cxx_constructor_scalar_type(target_type);
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            return cxx_constructor_expression_is_lowerable(
                constructor, expression->unary_operand, NULL,
                parameter_used, parameter_count);
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
            return cxx_constructor_expression_is_lowerable(
                       constructor, expression->binary_lhs, NULL,
                       parameter_used, parameter_count) &&
                   cxx_constructor_expression_is_lowerable(
                       constructor, expression->binary_rhs, NULL,
                       parameter_used, parameter_count);
        case EXPR_COND:
            return cxx_constructor_expression_is_lowerable(
                       constructor, expression->cond_test, NULL,
                       parameter_used, parameter_count) &&
                   cxx_constructor_expression_is_lowerable(
                       constructor, expression->cond_then, NULL,
                       parameter_used, parameter_count) &&
                   cxx_constructor_expression_is_lowerable(
                       constructor, expression->cond_else, NULL,
                       parameter_used, parameter_count);
        case EXPR_CAST:
            return cxx_constructor_scalar_type(expression->cast_type) &&
                   cxx_constructor_expression_is_lowerable(
                       constructor, expression->cast_expr, NULL,
                       parameter_used, parameter_count);
        case EXPR_ADDR:
            return target_type && target_type->kind == TYPE_PTR &&
                   cxx_constructor_static_address(constructor, expression);
        default:
            return false;
    }
}

static bool cxx_base_initializer_is_lowerable(
    CxxConstructorInfo* derived_constructor,
    CxxConstructorInitializer* initializer, bool* parameter_used,
    unsigned parameter_count) {
    CxxConstructorInfo* base_constructor = initializer
        ? initializer->constructor : NULL;
    ExprList* argument = initializer ? initializer->arguments : NULL;
    TypeParam* parameter = base_constructor ? base_constructor->parameters : NULL;
    int argument_count = cxx_constructor_argument_count(argument);
    if (!base_constructor) return argument_count == 0;
    if (argument_count != base_constructor->parameter_count) return false;
    while (argument && parameter) {
        if (!cxx_constructor_expression_is_lowerable(
                derived_constructor, argument->expr, parameter->type,
                parameter_used, parameter_count)) {
            return false;
        }
        argument = argument->next;
        parameter = parameter->next;
    }
    return !argument && !parameter;
}

static bool cxx_class_has_polymorphic_layout(CxxClass* cls) {
    if (!cls) return false;
    if (cls->vtable_size > 0 || cls->secondary_vtable_count > 0 ||
        class_has_virtual_member(cls)) {
        return true;
    }
    for (int index = 0; index < cls->base_count; ++index) {
        if (cxx_class_has_polymorphic_layout(cls->bases[index].base)) {
            return true;
        }
    }
    return false;
}

/* Recognize constructors whose observable object representation is exactly
 * declaration-order initialization of their data fields.  This covers the
 * SDK status/outcome wrappers without executing arbitrary constructor code. */
static uint32_t lowerable_constructor_arity_mask(CxxClass* cls) {
    CxxConstructorInfo* constructor;
    bool polymorphic_layout = cxx_class_has_polymorphic_layout(cls);
    uint32_t mask = cls && cls->type && cls->type->move_constructor_method &&
            !polymorphic_layout
        ? UINT32_C(1) << 1 : 0u;
    if (!cls || !cls->type->is_complete ||
        cls->has_static_field ||
        (class_has_destructor(cls) && !cls->type->cleanup_function &&
         (!cls->destructor_method || !cls->destructor_method->decl ||
         !cls->destructor_method->decl->func_body))) {
        return 0u;
    }
    for (int base_index = 0; base_index < cls->base_count; ++base_index) {
        if (!cxx_constructor_base_layout_supported(cls, base_index)) {
            return 0u;
        }
    }
    if (!cls->constructors) return 0u;
    for (constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        unsigned arity;
        unsigned minimum_arity;
        bool supported = true;
        bool parameter_used[32] = {false};
        TypeParam* field = cls->fields;
        CxxConstructorInitializer* initializer = constructor->initializers;
        if (constructor->access != ACCESS_PUBLIC ||
            constructor->is_deleted || constructor->is_defaulted) {
            continue;
        }
        arity = (unsigned)constructor->parameter_count;
        minimum_arity = cxx_constructor_required_parameter_count(constructor);
        if (arity >= 32u || minimum_arity > arity || minimum_arity >= 32u) {
            continue;
        }
        if (!constructor->body_is_empty) {
            if (!polymorphic_layout &&
                (cls->base_count == 0 ||
                 constructor->initializers_are_supported) &&
                constructor->method && constructor->method->decl &&
                constructor->method->decl->func_is_cxx_method &&
                constructor->method->decl->func_body) {
                for (unsigned invocation_arity = minimum_arity;
                     invocation_arity <= arity; ++invocation_arity) {
                    mask |= UINT32_C(1) << invocation_arity;
                }
            }
            continue;
        }
        if (!constructor->initializers_are_supported) continue;
        if (initializer && initializer->is_delegating_constructor) {
            CxxConstructorInfo* target = initializer->constructor;
            TypeParam* parameter = target ? target->parameters : NULL;
            ExprList* argument = initializer->arguments;
            if (!target || target == constructor || initializer->next) {
                continue;
            }
            while (parameter && argument) {
                if (!cxx_constructor_expression_is_lowerable(
                        constructor, argument->expr, parameter->type,
                        parameter_used, arity)) {
                    supported = false;
                    break;
                }
                parameter = parameter->next;
                argument = argument->next;
            }
            if (!supported || parameter || argument) continue;
            if (arity != 0u) {
                bool all_parameters_used = true;
                for (unsigned index = 0; index < arity; ++index) {
                    if (!parameter_used[index]) {
                        all_parameters_used = false;
                        break;
                    }
                }
                if (!all_parameters_used) continue;
            }
            for (unsigned invocation_arity = minimum_arity;
                 invocation_arity <= arity; ++invocation_arity) {
                mask |= UINT32_C(1) << invocation_arity;
            }
            continue;
        }
        while (initializer && initializer->is_base_initializer) {
            if (!cxx_base_initializer_is_lowerable(
                    constructor, initializer, parameter_used, arity)) {
                supported = false;
                break;
            }
            initializer = initializer->next;
        }
        if (!supported) continue;
        while (field && initializer) {
            if (!initializer->field ||
                strcmp(initializer->field, field->name) != 0 ||
                (!initializer->value && !initializer->arguments &&
                 !(field->type &&
                   (field->type->cxx_class ||
                    (field->type->kind == TYPE_ARRAY &&
                     field->type->base &&
                     field->type->base->cxx_class))))) {
                supported = false;
                break;
            }
            /* A class-valued member initializer is resolved by semantic
             * overload selection after all class declarations are in the
             * symbol table.  Default member initializers keep their own
             * argument list; explicit constructor initializers are restricted
             * to the bounded parameter-forwarding form below. */
            if (field->type && field->type->kind == TYPE_ARRAY &&
                field->type->base &&
                field->type->base->cxx_class) {
                Type* element_type = field->type->base;
                CxxClass* member_class = element_type->cxx_class;
                if (field->type->array_len <= 0 ||
                    element_type->kind == TYPE_ARRAY ||
                    initializer->value || initializer->arguments ||
                    !initializer->constructor || !member_class ||
                    !member_class->type ||
                    member_class->destructor_method ||
                    member_class->type->cleanup_function ||
                    (lowerable_constructor_arity_mask(member_class) &
                     UINT32_C(1)) == 0u) {
                    supported = false;
                    break;
                }
                field = field->next;
                initializer = initializer->next;
                continue;
            }
            if (field->type && field->type->cxx_class) {
                if (initializer->is_default_member_initializer) {
                    CxxConstructorInfo* member_constructor =
                        initializer->constructor;
                    TypeParam* member_parameter = member_constructor
                        ? member_constructor->parameters : NULL;
                    ExprList* member_argument = initializer->arguments;
                    int member_argument_count =
                        cxx_constructor_argument_count(member_argument);
                    if (!member_constructor ||
                        member_argument_count < 0 ||
                        !cxx_constructor_arity_has_defaults(
                            member_constructor, member_argument_count) ||
                        (lowerable_constructor_arity_mask(
                             field->type->cxx_class) &
                         (UINT32_C(1) << (unsigned)member_argument_count)) ==
                            0u) {
                        supported = false;
                        break;
                    }
                    while (member_argument && member_parameter) {
                        if (!cxx_constructor_expression_is_lowerable(
                                constructor, member_argument->expr,
                                member_parameter->type, parameter_used,
                                arity)) {
                            supported = false;
                            break;
                        }
                        member_argument = member_argument->next;
                        member_parameter = member_parameter->next;
                    }
                    if (!supported || member_argument) {
                        supported = false;
                        break;
                    }
                    field = field->next;
                    initializer = initializer->next;
                    continue;
                }
                if (!initializer->arguments && initializer->constructor) {
                    if ((lowerable_constructor_arity_mask(
                             field->type->cxx_class) & 1u) == 0u) {
                        supported = false;
                        break;
                    }
                    field = field->next;
                    initializer = initializer->next;
                    continue;
                }
                if (arity != 0u) {
                    ExprList* argument = initializer->arguments;
                    int used_index = argument && argument->expr &&
                        argument->expr->kind == EXPR_IDENT
                        ? cxx_constructor_parameter_index(
                            constructor, argument->expr->ident_name) : -1;
                    if (used_index < 0 || !argument || argument->next ||
                        !argument->expr ||
                        used_index >= (int)arity) {
                        supported = false;
                        break;
                    }
                    parameter_used[used_index] = true;
                }
                field = field->next;
                initializer = initializer->next;
                continue;
            }
            if (initializer->is_default_member_initializer) {
                bool is_array = field->type &&
                    field->type->kind == TYPE_ARRAY;
                if (!cxx_constructor_dmi_value_is_lowerable(
                        cls, field, field->type, initializer->value) ||
                    !field->type || field->type->size <= 0 ||
                    (is_array && field->type->size > 512) ||
                    (!is_array && field->type->size > 8)) {
                    supported = false;
                    break;
                }
                field = field->next;
                initializer = initializer->next;
                continue;
            }
            if (arity == 0u) {
                if ((!cxx_constructor_scalar_constant(initializer->value) &&
                     !cxx_constructor_static_address(
                         constructor, initializer->value)) ||
                    !field->type ||
                    !(type_is_integer(field->type) ||
                      field->type->kind == TYPE_ENUM ||
                      field->type->kind == TYPE_PTR ||
                      field->type->kind == TYPE_NULLPTR ||
                      field->type->kind == TYPE_FLOAT ||
                      field->type->kind == TYPE_DOUBLE) ||
                    field->type->size <= 0 ||
                    (g_opts.target_arch == ARCH_X86 && field->type->size > 4) ||
                    (g_opts.target_arch == ARCH_X64 && field->type->size > 8)) {
                    supported = false;
                    break;
                }
            } else {
                if (!cxx_constructor_expression_is_lowerable(
                        constructor, initializer->value, field->type,
                        parameter_used, arity)) {
                    supported = false;
                    break;
                }
            }
            field = field->next;
            initializer = initializer->next;
        }
        if (!supported || field || initializer) {
            continue;
        }
        if (arity != 0u) {
            bool all_parameters_used = true;
            for (unsigned index = 0; index < arity; ++index) {
                if (!parameter_used[index]) {
                    all_parameters_used = false;
                    break;
                }
            }
            if (!all_parameters_used) continue;
        }
        for (unsigned invocation_arity = minimum_arity;
             invocation_arity <= arity; ++invocation_arity) {
            mask |= UINT32_C(1) << invocation_arity;
        }
    }
    return mask;
}

static TypeField* class_layout_field(CxxClass* cls, const char* name) {
    TypeField* field;
    for (field = cls && cls->type ? cls->type->fields : NULL;
         field; field = field->next) {
        if (field->name && name && strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static void register_inline_class_accessors(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    if (!cls || !cls->type || !cls->type->is_complete) return;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Expr* returned;
        Expr* field_expr = NULL;
        int64_t constant = 0;
        bool has_constant = false;
        TypeField* field;
        TypeMethodKind kind;
        TypeMethod* lowered;
        if (!method || method->is_static || method->is_virtual ||
            method->is_pure_virtual || method->is_deleted ||
            method->is_defaulted || method->is_constructor ||
            method->is_destructor || !method->is_const ||
            method->is_volatile ||
            method->decl->func_params || !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || statements->next ||
            !statements->stmt || statements->stmt->kind != STMT_RETURN ||
            !statements->stmt->return_val) {
            continue;
        }
        returned = statements->stmt->return_val;
        if (returned->kind == EXPR_IDENT) {
            field_expr = returned;
            kind = TYPE_METHOD_FIELD;
        } else if (returned->kind == EXPR_EQ || returned->kind == EXPR_NE) {
            if (returned->binary_lhs &&
                returned->binary_lhs->kind == EXPR_IDENT &&
                returned->binary_rhs &&
                expr_eval_integer_constant(returned->binary_rhs,
                                           &constant)) {
                field_expr = returned->binary_lhs;
                has_constant = true;
            } else if (returned->binary_rhs &&
                       returned->binary_rhs->kind == EXPR_IDENT &&
                       returned->binary_lhs &&
                       expr_eval_integer_constant(returned->binary_lhs,
                                                  &constant)) {
                field_expr = returned->binary_rhs;
                has_constant = true;
            } else {
                continue;
            }
            kind = returned->kind == EXPR_EQ
                ? TYPE_METHOD_FIELD_EQ_CONSTANT
                : TYPE_METHOD_FIELD_NE_CONSTANT;
        } else {
            continue;
        }
        field = class_layout_field(cls, field_expr->ident_name);
        if (!field || !field->type || field->type->size <= 0) {
            continue;
        }
        if (kind == TYPE_METHOD_FIELD) {
            Type* return_type = method->decl->type->ret_type;
            Type* return_value_type = return_type &&
                return_type->is_reference ? return_type->base : return_type;
            if (!type_is_compatible(return_value_type, field->type)) {
                continue;
            }
            if (return_type && return_type->is_reference &&
                !(type_is_integer(field->type) ||
                  field->type->kind == TYPE_ENUM ||
                  field->type->kind == TYPE_PTR ||
                  field->type->kind == TYPE_ARRAY ||
                  field->type->kind == TYPE_STRUCT ||
                  field->type->kind == TYPE_UNION)) {
                continue;
            }
            if ((!return_type || !return_type->is_reference) &&
                (field->type->size > 8 ||
                 !(type_is_integer(field->type) ||
                   field->type->kind == TYPE_ENUM ||
                   field->type->kind == TYPE_PTR))) {
                continue;
            }
        } else {
            if (field->type->size > 8 ||
                !(type_is_integer(field->type) ||
                  field->type->kind == TYPE_ENUM ||
                  field->type->kind == TYPE_PTR) ||
                !(type_is_integer(method->decl->type->ret_type) ||
                  method->decl->type->ret_type->kind == TYPE_ENUM)) {
                continue;
            }
        }
        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = method->source_name
            ? method->source_name : method->decl->name;
        lowered->return_type = method->decl->type->ret_type;
        lowered->field = field;
        lowered->function_decl = NULL;
        lowered->source_decl = method->decl;
        lowered->kind = kind;
        lowered->constant = has_constant ? constant : 0;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->is_noexcept = method->is_noexcept;
        lowered->this_owner = NULL;
        lowered->this_adjustment = 0;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

static TypeMethod* inline_bool_delegate_target(Type* aggregate,
                                               const char* name) {
    TypeMethod* method;
    for (method = aggregate ? aggregate->methods : NULL;
         method; method = method->next) {
        if (method->name && name && strcmp(method->name, name) == 0 &&
            method->return_type && method->return_type->kind == TYPE_BOOL &&
            method->field &&
            (method->kind == TYPE_METHOD_FIELD ||
             method->kind == TYPE_METHOD_FIELD_EQ_CONSTANT ||
             method->kind == TYPE_METHOD_FIELD_NE_CONSTANT)) {
            return method;
        }
    }
    return NULL;
}

/* Accept an operator-bool wrapper only when its complete body is:
 *
 *   return validated_zero_argument_bool_accessor();
 *
 * The target accessor has already been reduced to a field operation above,
 * so copying that operation cannot execute an arbitrary member body.  This
 * covers the SDK status/outcome wrappers while retaining explicit validation
 * for helper calls that have no executable lowering. */
static void register_inline_class_bool_delegates(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    if (!cls || !cls->type || !cls->type->is_complete) return;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Expr* returned;
        Expr* callee;
        TypeMethod* target;
        TypeMethod* lowered;
        if (!method || method->is_static || method->is_virtual ||
            method->is_pure_virtual || method->is_deleted ||
            method->is_defaulted || method->is_constructor ||
            method->is_destructor || !method->is_const ||
            method->is_volatile ||
            !method->decl || !method->decl->type ||
            !(method->source_name ? method->source_name : method->decl->name) ||
            strcmp(method->source_name ? method->source_name : method->decl->name,
                   "operator conversion") != 0 ||
            !method->decl->type->ret_type ||
            method->decl->type->ret_type->kind != TYPE_BOOL ||
            method->decl->func_params || !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || statements->next || !statements->stmt ||
            statements->stmt->kind != STMT_RETURN ||
            !statements->stmt->return_val) {
            continue;
        }
        returned = statements->stmt->return_val;
        if (returned->kind != EXPR_CALL || returned->call_args) continue;
        callee = returned->call_func;
        if (!callee || callee->kind != EXPR_IDENT || !callee->ident_name ||
            strcmp(callee->ident_name, "operator conversion") == 0) {
            continue;
        }
        target = inline_bool_delegate_target(cls->type,
                                             callee->ident_name);
        if (!target) continue;
        lowered = ast_arena_alloc(sizeof(*lowered));
        *lowered = *target;
        lowered->name = method->source_name
            ? method->source_name : method->decl->name;
        lowered->return_type = method->decl->type->ret_type;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->source_decl = method->decl;
        lowered->this_owner = target->this_owner;
        lowered->this_adjustment = target->this_adjustment;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

/* Accept the ownership-transfer primitive only in this exact form:
 *
 *   FieldType value = field;
 *   field = integer-invalid;
 *   return value;
 *
 * The backend can then return the old scalar and invalidate the object as one
 * validated operation.  No arbitrary method body is interpreted. */
static void register_inline_class_releases(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    if (!cls || !cls->type || !cls->type->is_complete) return;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Stmt* declaration_statement;
        Stmt* assignment_statement;
        Stmt* return_statement;
        Decl* local;
        Expr* assignment;
        TypeField* field;
        Type* return_type;
        int64_t invalid;
        TypeMethod* lowered;
        if (!method || method->is_static || method->is_virtual ||
            method->is_pure_virtual || method->is_deleted ||
            method->is_defaulted || method->is_constructor ||
            method->is_destructor || method->is_const ||
            method->is_volatile ||
            method->decl->func_params || !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || !statements->next ||
            !statements->next->next || statements->next->next->next) {
            continue;
        }
        declaration_statement = statements->stmt;
        assignment_statement = statements->next->stmt;
        return_statement = statements->next->next->stmt;
        if (!declaration_statement ||
            declaration_statement->kind != STMT_DECL ||
            !declaration_statement->decl ||
            declaration_statement->decl->kind != DECL_VAR ||
            !declaration_statement->decl->var_init ||
            declaration_statement->decl->var_init->kind != EXPR_IDENT ||
            !assignment_statement ||
            assignment_statement->kind != STMT_EXPR ||
            !assignment_statement->expr ||
            assignment_statement->expr->kind != EXPR_ASSIGN ||
            !return_statement || return_statement->kind != STMT_RETURN ||
            !return_statement->return_val ||
            return_statement->return_val->kind != EXPR_IDENT) {
            continue;
        }
        local = declaration_statement->decl;
        assignment = assignment_statement->expr;
        if (!local->name ||
            strcmp(return_statement->return_val->ident_name,
                   local->name) != 0 ||
            !assignment->binary_lhs ||
            assignment->binary_lhs->kind != EXPR_IDENT ||
            !expr_eval_integer_constant(assignment->binary_rhs, &invalid)) {
            continue;
        }
        field = class_layout_field(
            cls, declaration_statement->decl->var_init->ident_name);
        if (!field || !field->name ||
            strcmp(assignment->binary_lhs->ident_name, field->name) != 0 ||
            !field->type || field->type->size <= 0 ||
            field->type->size > 8 ||
            !(type_is_integer(field->type) ||
              field->type->kind == TYPE_ENUM ||
              field->type->kind == TYPE_PTR)) {
            continue;
        }
        if (class_has_destructor(cls) && !cls->destructor_method &&
            (!cls->type->cleanup_function ||
             cls->type->cleanup_field != field ||
             cls->type->cleanup_invalid != invalid)) {
            continue;
        }
        return_type = method->decl->type->ret_type;
        if (!type_is_compatible(return_type, field->type)) continue;
        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = method->source_name
            ? method->source_name : method->decl->name;
        lowered->return_type = return_type;
        lowered->field = field;
        lowered->function_decl = NULL;
        lowered->source_decl = method->decl;
        lowered->kind = TYPE_METHOD_FIELD_RELEASE;
        lowered->constant = invalid;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->is_noexcept = method->is_noexcept;
        lowered->this_owner = NULL;
        lowered->this_adjustment = 0;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

static bool class_reference_is_self(CxxClass* cls, Type* reference,
                                    bool require_rvalue) {
    Type* referred;
    const char* template_name;
    if (!cls || !reference || reference->kind != TYPE_PTR ||
        !reference->is_reference ||
        reference->is_rvalue_reference != require_rvalue ||
        !reference->base) {
        return false;
    }
    referred = reference->base;
    if (type_is_compatible(referred, cls->type)) return true;
    template_name = cls->templ && cls->templ->templated_class
        ? cls->templ->templated_class->name : NULL;
    return template_name && referred->kind == TYPE_STRUCT && referred->tag &&
           strcmp(referred->tag, template_name) == 0;
}

static bool move_parameter_is_self(CxxClass* cls, Type* parameter) {
    return class_reference_is_self(cls, parameter, true);
}

static TypeMethod* class_release_method(CxxClass* cls, const char* name,
                                        TypeField* field) {
    TypeMethod* method;
    for (method = cls && cls->type ? cls->type->methods : NULL;
         method; method = method->next) {
        if (method->kind == TYPE_METHOD_FIELD_RELEASE &&
            method->cxx_access == ACCESS_PUBLIC && method->name && name &&
            strcmp(method->name, name) == 0 && method->field == field &&
            method->return_type &&
            type_is_compatible(method->return_type, field->type)) {
            return method;
        }
    }
    return NULL;
}

/* Accept only the single-field ownership move used by the SDK:
 *
 *   Class(Class&& other) : field(other.release()) {}
 *
 * The release member must itself have passed the structural verifier above.
 * Constructor arguments can then be rewritten to that field operation without
 * interpreting arbitrary constructor code. */
static void register_inline_class_move_constructor(CxxClass* cls) {
    CxxConstructorInfo* constructor;
    TypeMethod* candidate = NULL;
    TypeField* only_field;
    if (!cls || !cls->type || !cls->type->is_complete ||
        cls->base_count != 0 || cls->has_static_field ||
        cls->has_field_initializer || class_has_virtual_member(cls)) {
        return;
    }
    only_field = cls->type->fields;
    if (!only_field || only_field->next) return;
    for (constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        TypeParam* parameter = constructor->parameters;
        CxxConstructorInitializer* initializer = constructor->initializers;
        Expr* value;
        Expr* callee;
        TypeMethod* release;
        if (constructor->access != ACCESS_PUBLIC ||
            constructor->parameter_count != 1 || !parameter ||
            parameter->next || !move_parameter_is_self(cls, parameter->type) ||
            constructor->is_deleted || constructor->is_defaulted ||
            !constructor->initializers_are_supported ||
            constructor->initializer_count != 1 || !initializer ||
            initializer->next || !initializer->field ||
            strcmp(initializer->field, only_field->name) != 0 ||
            !constructor->body_is_empty || !initializer->value) {
            continue;
        }
        value = initializer->value;
        if (value->kind != EXPR_CALL || value->call_args ||
            !value->call_func || value->call_func->kind != EXPR_MEMBER) {
            continue;
        }
        callee = value->call_func;
        if (!callee->member_name || !callee->member_base ||
            callee->member_base->kind != EXPR_IDENT ||
            !parameter->name ||
            strcmp(callee->member_base->ident_name, parameter->name) != 0) {
            continue;
        }
        release = class_release_method(cls, callee->member_name, only_field);
        if (!release) continue;
        if (candidate) {
            cls->type->move_constructor_method = NULL;
            return;
        }
        candidate = release;
    }
    cls->type->move_constructor_method = candidate;
}

static Expr* cleanup_unwrap_void_cast(Expr* expression) {
    if (expression && expression->kind == EXPR_CAST &&
        expression->cast_type == type_void) {
        return expression->cast_expr;
    }
    return expression;
}

static Stmt* cleanup_single_statement(Stmt* statement) {
    if (statement && statement->kind == STMT_BLOCK) {
        StmtList* items = statement->block_stmts;
        if (!items || items->next) return NULL;
        return items->stmt;
    }
    return statement;
}

/* Accept a destructor only when its complete observable behavior is:
 *
 *   if (field != integer-invalid)
 *       (void)cleanup_function((optional-cast)field);
 *
 * This is sufficient for SDK opaque-handle RAII without interpreting an
 * arbitrary C++ member-function body. */
static void register_inline_class_cleanup(CxxClass* cls) {
    struct CxxMember* member;
    if (!cls || !cls->type || !cls->type->is_complete ||
        cls->base_count != 0 || class_has_virtual_member(cls)) {
        return;
    }
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* body;
        Stmt* conditional;
        Stmt* action;
        Expr* condition;
        Expr* call;
        Expr* argument;
        ExprList* arguments;
        TypeField* field;
        int64_t invalid;
        if (!method || !method->is_destructor || method->is_deleted ||
            method->is_defaulted || method->decl->func_params ||
            !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        body = method->decl->func_body->block_stmts;
        if (!body || body->next) continue;
        conditional = body->stmt;
        if (!conditional || conditional->kind != STMT_IF ||
            conditional->if_else) {
            continue;
        }
        condition = conditional->if_cond;
        if (!condition || condition->kind != EXPR_NE ||
            !condition->binary_lhs ||
            condition->binary_lhs->kind != EXPR_IDENT ||
            !expr_eval_integer_constant(condition->binary_rhs, &invalid)) {
            continue;
        }
        field = class_layout_field(
            cls, condition->binary_lhs->ident_name);
        if (!field || !field->type || field->type->size <= 0 ||
            field->type->size > 8 ||
            !(type_is_integer(field->type) ||
              field->type->kind == TYPE_ENUM ||
              field->type->kind == TYPE_PTR)) {
            continue;
        }
        action = cleanup_single_statement(conditional->if_then);
        if (!action || action->kind != STMT_EXPR) continue;
        call = cleanup_unwrap_void_cast(action->expr);
        if (!call || call->kind != EXPR_CALL || !call->call_func ||
            call->call_func->kind != EXPR_IDENT ||
            !call->call_func->ident_name) {
            continue;
        }
        arguments = call->call_args;
        if (!arguments || arguments->next || !arguments->expr) continue;
        argument = arguments->expr;
        if (argument->kind == EXPR_CAST) argument = argument->cast_expr;
        if (!argument || argument->kind != EXPR_IDENT ||
            strcmp(argument->ident_name, field->name) != 0) {
            continue;
        }
        if (cls->type->cleanup_function) {
            cls->type->cleanup_function = NULL;
            cls->type->cleanup_field = NULL;
            return;
        }
        cls->type->cleanup_function = call->call_func->ident_name;
        cls->type->cleanup_field = field;
        cls->type->cleanup_invalid = invalid;
    }
}

static bool move_expression_is_identifier(Expr* expression,
                                          const char* name) {
    return expression && expression->kind == EXPR_IDENT &&
           expression->ident_name && name &&
           strcmp(expression->ident_name, name) == 0;
}

static bool expression_is_field_constant(Expr* expression, ExprKind kind,
                                         const char* field_name,
                                         int64_t expected) {
    int64_t value;
    if (!expression || expression->kind != kind) return false;
    if (move_expression_is_identifier(expression->binary_lhs, field_name) &&
        expr_eval_integer_constant(expression->binary_rhs, &value)) {
        return value == expected;
    }
    if (move_expression_is_identifier(expression->binary_rhs, field_name) &&
        expr_eval_integer_constant(expression->binary_lhs, &value)) {
        return value == expected;
    }
    return false;
}

static bool expression_is_empty_compound(Expr* expression, Type* type) {
    ExprList* item;
    int64_t value;
    if (!expression || expression->kind != EXPR_COMPOUND ||
        !expression->compound_value_init || !expression->compound_type ||
        !type_is_compatible(expression->compound_type, type)) {
        return false;
    }
    for (item = expression->compound_init; item; item = item->next) {
        if (item->designator_kind != INIT_DESIGNATOR_NONE || !item->expr ||
            !expr_eval_integer_constant(item->expr, &value) || value != 0) {
            return false;
        }
    }
    return true;
}

static bool expression_is_single_identifier_compound(Expr* expression,
                                                      const char* name,
                                                      Type* type) {
    ExprList* item;
    if (!expression || expression->kind != EXPR_COMPOUND ||
        !expression->compound_type ||
        !type_is_compatible(expression->compound_type, type)) {
        return false;
    }
    item = expression->compound_init;
    return item && !item->next &&
           item->designator_kind == INIT_DESIGNATOR_NONE &&
           move_expression_is_identifier(item->expr, name);
}

/* Prove the zero-argument close helper called by move assignment.  Its exact
 * observable form is the SDK sequence:
 *
 *   if (field == invalid) return Result{};
 *   Code result = cleanup_function((optional-cast)field);
 *   if (result == success) field = invalid;
 *   return Result{result};
 *
 * The result value is ignored by operator=, but constraining both returns and
 * the success branch prevents a seemingly harmless helper name from hiding
 * arbitrary side effects. */
static void register_inline_class_closes(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    TypeField* field;
    if (!cls || !cls->type || !cls->type->is_complete ||
        !cls->type->cleanup_function || !cls->type->cleanup_field) {
        return;
    }
    field = cls->type->cleanup_field;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Stmt* empty_guard;
        Stmt* call_declaration;
        Stmt* success_guard;
        Stmt* final_return;
        Stmt* action;
        Decl* result;
        Expr* call;
        Expr* argument;
        ExprList* arguments;
        Type* return_type;
        TypeField* result_field;
        int64_t success;
        int64_t assigned;
        TypeMethod* lowered;
        if (!method || !cxx_method_source_name(method) ||
            member->access != ACCESS_PUBLIC || method->is_static ||
            method->is_virtual || method->is_pure_virtual ||
            method->is_deleted || method->is_defaulted ||
            method->is_constructor || method->is_destructor ||
            method->is_const || method->is_volatile ||
            !method->decl->type ||
            method->decl->type->kind != TYPE_FUNC ||
            method->decl->func_params ||
            !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        return_type = method->decl->type->ret_type;
        if (!return_type || return_type->is_reference ||
            (return_type->kind != TYPE_STRUCT &&
             return_type->kind != TYPE_UNION) ||
            return_type->cleanup_function) {
            continue;
        }
        result_field = return_type->fields;
        if (!result_field || result_field->next || !result_field->type ||
            result_field->type->size <= 0 || result_field->type->size > 4 ||
            !(type_is_integer(result_field->type) ||
              result_field->type->kind == TYPE_ENUM)) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || !statements->next ||
            !statements->next->next || !statements->next->next->next ||
            statements->next->next->next->next) {
            continue;
        }
        empty_guard = statements->stmt;
        call_declaration = statements->next->stmt;
        success_guard = statements->next->next->stmt;
        final_return = statements->next->next->next->stmt;
        if (!empty_guard || empty_guard->kind != STMT_IF ||
            empty_guard->if_else ||
            !expression_is_field_constant(empty_guard->if_cond, EXPR_EQ,
                                          field->name,
                                          cls->type->cleanup_invalid)) {
            continue;
        }
        action = cleanup_single_statement(empty_guard->if_then);
        if (!action || action->kind != STMT_RETURN ||
            !expression_is_empty_compound(action->return_val, return_type)) {
            continue;
        }
        if (!call_declaration || call_declaration->kind != STMT_DECL ||
            !call_declaration->decl ||
            call_declaration->decl->kind != DECL_VAR ||
            !call_declaration->decl->name ||
            !call_declaration->decl->var_init) {
            continue;
        }
        result = call_declaration->decl;
        if (!result->type ||
            !type_is_compatible(result->type, result_field->type)) {
            continue;
        }
        call = result->var_init;
        if (call->kind != EXPR_CALL || !call->call_func ||
            call->call_func->kind != EXPR_IDENT ||
            !call->call_func->ident_name ||
            strcmp(call->call_func->ident_name,
                   cls->type->cleanup_function) != 0) {
            continue;
        }
        arguments = call->call_args;
        if (!arguments || arguments->next || !arguments->expr) continue;
        argument = arguments->expr;
        if (argument->kind == EXPR_CAST) argument = argument->cast_expr;
        if (!move_expression_is_identifier(argument, field->name)) continue;
        if (!success_guard || success_guard->kind != STMT_IF ||
            success_guard->if_else || !success_guard->if_cond ||
            success_guard->if_cond->kind != EXPR_EQ) {
            continue;
        }
        if (move_expression_is_identifier(
                success_guard->if_cond->binary_lhs, result->name) &&
            expr_eval_integer_constant(
                success_guard->if_cond->binary_rhs, &success)) {
            /* matched */
        } else if (move_expression_is_identifier(
                       success_guard->if_cond->binary_rhs, result->name) &&
                   expr_eval_integer_constant(
                       success_guard->if_cond->binary_lhs, &success)) {
            /* matched */
        } else {
            continue;
        }
        action = cleanup_single_statement(success_guard->if_then);
        if (!action || action->kind != STMT_EXPR || !action->expr ||
            action->expr->kind != EXPR_ASSIGN ||
            !move_expression_is_identifier(action->expr->binary_lhs,
                                           field->name) ||
            !expr_eval_integer_constant(action->expr->binary_rhs, &assigned) ||
            assigned != cls->type->cleanup_invalid) {
            continue;
        }
        if (!final_return || final_return->kind != STMT_RETURN ||
            !expression_is_single_identifier_compound(final_return->return_val,
                                                      result->name,
                                                      return_type)) {
            continue;
        }
        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = cxx_method_source_name(method);
        lowered->return_type = return_type;
        lowered->field = field;
        lowered->function_decl = NULL;
        lowered->source_decl = method->decl;
        lowered->kind = TYPE_METHOD_FIELD_CLOSE;
        lowered->constant = cls->type->cleanup_invalid;
        lowered->cleanup_function = cls->type->cleanup_function;
        lowered->result_field = result_field;
        lowered->success_constant = success;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->this_owner = NULL;
        lowered->this_adjustment = 0;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

static TypeMethod* class_close_method(CxxClass* cls, const char* name,
                                      TypeField* field) {
    TypeMethod* method;
    for (method = cls && cls->type ? cls->type->methods : NULL;
         method; method = method->next) {
        if (method->kind == TYPE_METHOD_FIELD_CLOSE &&
            method->cxx_access == ACCESS_PUBLIC && method->name && name &&
            strcmp(method->name, name) == 0 && method->field == field &&
            method->cleanup_function && method->result_field) {
            return method;
        }
    }
    return NULL;
}

/* Accept a public close alias only when its complete body is exactly:
 *
 *   return validated_zero_argument_close();
 *
 * The target close has already proved the handle, cleanup function, result
 * layout, success value, and retry semantics.  Cloning that metadata gives
 * SDK wrappers a conventional reset() spelling without interpreting an
 * arbitrary member body or allowing hidden side effects. */
static void register_inline_class_close_delegates(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    TypeField* field;
    if (!cls || !cls->type || !cls->type->is_complete ||
        !cls->type->cleanup_field) {
        return;
    }
    field = cls->type->cleanup_field;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Expr* returned;
        Expr* callee;
        TypeMethod* target;
        TypeMethod* lowered;
        if (!method || !cxx_method_source_name(method) ||
            member->access != ACCESS_PUBLIC || method->is_static ||
            method->is_virtual || method->is_pure_virtual ||
            method->is_deleted || method->is_defaulted ||
            method->is_constructor || method->is_destructor ||
            method->is_const || method->is_volatile ||
            !method->decl->type ||
            method->decl->type->kind != TYPE_FUNC ||
            method->decl->func_params || !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || statements->next || !statements->stmt ||
            statements->stmt->kind != STMT_RETURN ||
            !statements->stmt->return_val) {
            continue;
        }
        returned = statements->stmt->return_val;
        if (returned->kind != EXPR_CALL || returned->call_args ||
            !returned->call_func ||
            returned->call_func->kind != EXPR_IDENT) {
            continue;
        }
        callee = returned->call_func;
        if (!callee->ident_name ||
            strcmp(callee->ident_name, cxx_method_source_name(method)) == 0) {
            continue;
        }
        target = class_close_method(cls, callee->ident_name, field);
        if (!target || !target->return_type ||
            !type_is_compatible(method->decl->type->ret_type,
                                target->return_type)) {
            continue;
        }
        lowered = ast_arena_alloc(sizeof(*lowered));
        *lowered = *target;
        lowered->name = cxx_method_source_name(method);
        lowered->return_type = method->decl->type->ret_type;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->source_decl = method->decl;
        lowered->this_owner = target->this_owner;
        lowered->this_adjustment = target->this_adjustment;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

/* Accept only the SDK ownership assignment:
 *
 *   Class& operator=(Class&& other) {
 *     if (this != &other) { (void)close(); field = other.release(); }
 *     return *this;
 *   }
 *
 * close(), release(), the destructor cleanup, and the move constructor have
 * all independently passed structural verification before this metadata is
 * published to semantic analysis. */
static void register_inline_class_move_assignment(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod* candidate = NULL;
    TypeField* field;
    if (!cls || !cls->type || !cls->type->is_complete ||
        cls->base_count != 0 || cls->has_static_field ||
        cls->has_field_initializer || class_has_virtual_member(cls) ||
        !cls->type->cleanup_function || !cls->type->cleanup_field ||
        !cls->type->move_constructor_method) {
        return;
    }
    field = cls->type->fields;
    if (!field || field->next || cls->type->cleanup_field != field ||
        cls->type->move_constructor_method->field != field) {
        return;
    }
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        TypeParam* parameter;
        StmtList* statements;
        Stmt* guarded;
        Stmt* returned;
        StmtList* actions;
        Expr* condition;
        Expr* close_call;
        Expr* assignment;
        Expr* release_call;
        Expr* release_callee;
        TypeMethod* close;
        TypeMethod* release;
        const char* close_name;
        if (!method || !cxx_method_source_name(method) ||
            strcmp(cxx_method_source_name(method), "operator=") != 0 ||
            member->access != ACCESS_PUBLIC || method->is_static ||
            method->is_virtual || method->is_pure_virtual ||
            method->is_deleted || method->is_defaulted ||
            method->is_constructor || method->is_destructor ||
            method->is_const || method->is_volatile ||
            !method->decl->type ||
            !class_reference_is_self(cls, method->decl->type->ret_type,
                                     false) ||
            !method->decl->func_params ||
            method->decl->func_params->next ||
            !method->decl->func_params->decl ||
            !move_parameter_is_self(
                cls, method->decl->func_params->decl->type) ||
            !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        parameter = method->decl->type->params;
        if (!parameter || parameter->next || !parameter->name) continue;
        statements = method->decl->func_body->block_stmts;
        if (!statements || !statements->next || statements->next->next) {
            continue;
        }
        guarded = statements->stmt;
        returned = statements->next->stmt;
        if (!guarded || guarded->kind != STMT_IF || guarded->if_else ||
            !returned || returned->kind != STMT_RETURN ||
            !returned->return_val ||
            returned->return_val->kind != EXPR_DEREF ||
            !move_expression_is_identifier(
                returned->return_val->unary_operand, "this")) {
            continue;
        }
        condition = guarded->if_cond;
        if (!condition || condition->kind != EXPR_NE ||
            !move_expression_is_identifier(condition->binary_lhs, "this") ||
            !condition->binary_rhs ||
            condition->binary_rhs->kind != EXPR_ADDR ||
            !move_expression_is_identifier(
                condition->binary_rhs->unary_operand, parameter->name)) {
            continue;
        }
        if (!guarded->if_then || guarded->if_then->kind != STMT_BLOCK) {
            continue;
        }
        actions = guarded->if_then->block_stmts;
        if (!actions || !actions->next || actions->next->next ||
            !actions->stmt || actions->stmt->kind != STMT_EXPR ||
            !actions->next->stmt ||
            actions->next->stmt->kind != STMT_EXPR) {
            continue;
        }
        close_call = cleanup_unwrap_void_cast(actions->stmt->expr);
        if (!close_call || close_call->kind != EXPR_CALL ||
            close_call->call_args || !close_call->call_func ||
            close_call->call_func->kind != EXPR_IDENT ||
            !close_call->call_func->ident_name) {
            continue;
        }
        close_name = close_call->call_func->ident_name;
        close = class_close_method(cls, close_name, field);
        if (!close) continue;
        assignment = actions->next->stmt->expr;
        if (!assignment || assignment->kind != EXPR_ASSIGN ||
            !move_expression_is_identifier(assignment->binary_lhs,
                                           field->name)) {
            continue;
        }
        release_call = assignment->binary_rhs;
        if (!release_call || release_call->kind != EXPR_CALL ||
            release_call->call_args || !release_call->call_func ||
            release_call->call_func->kind != EXPR_MEMBER) {
            continue;
        }
        release_callee = release_call->call_func;
        if (!release_callee->member_name || !release_callee->member_base ||
            !move_expression_is_identifier(release_callee->member_base,
                                           parameter->name)) {
            continue;
        }
        release = class_release_method(cls, release_callee->member_name,
                                       field);
        if (!release || release != cls->type->move_constructor_method) {
            continue;
        }
        if (candidate) {
            cls->type->move_assignment_method = NULL;
            return;
        }
        candidate = release;
    }
    cls->type->move_assignment_method = candidate;
}

/* Build the source lookup spelling for a class member.  Qualified expressions
 * are retained as one identifier, so a static member needs a symbol-table key
 * such as api::Counter::add while its link name remains the Itanium spelling
 * produced by cxx_mangle_function(). */
static const char* cxx_class_method_source_name(CxxClass* cls,
                                                 const char* method_name,
                                                 SourceLoc loc) {
    CxxNamespace* stack[32];
    int count = 0;
    size_t length = 0u;
    char buffer[512] = "";

    if (!cls || !cls->name || !method_name) return rcc_intern("");
    for (CxxNamespace* ns = active_namespace;
         ns && ns->name;
         ns = ns->parent) {
        if (count == (int)(sizeof(stack) / sizeof(stack[0]))) {
            rcc_error(loc, "namespace nesting exceeds compiler limit");
            break;
        }
        stack[count++] = ns;
    }
    for (int index = count - 1; index >= 0; --index) {
        size_t part_length = strlen(stack[index]->name);
        if (part_length > sizeof(buffer) - 1u - length) {
            rcc_error(loc, "qualified member name exceeds compiler limit");
            return rcc_intern(buffer);
        }
        memcpy(buffer + length, stack[index]->name, part_length);
        length += part_length;
        if (length > sizeof(buffer) - 3u) {
            rcc_error(loc, "qualified member name exceeds compiler limit");
            return rcc_intern(buffer);
        }
        memcpy(buffer + length, "::", 2u);
        length += 2u;
    }
    if (strlen(cls->name) > sizeof(buffer) - 3u - length ||
        strlen(method_name) > sizeof(buffer) - 1u - length -
            strlen(cls->name) - 2u) {
        rcc_error(loc, "qualified member name exceeds compiler limit");
        return rcc_intern(buffer);
    }
    memcpy(buffer + length, cls->name, strlen(cls->name));
    length += strlen(cls->name);
    memcpy(buffer + length, "::", 2u);
    length += 2u;
    strcpy(buffer + length, method_name);
    return rcc_intern(buffer);
}

static const char* cxx_method_source_name(CxxMethod* method) {
    return method && method->source_name
        ? method->source_name
        : (method && method->decl ? method->decl->name : NULL);
}

static bool class_declares_method_name(CxxClass* cls, const char* name) {
    struct CxxMember* member;
    if (!cls || !name) return false;
    for (TypeParam* field = cls->fields; field; field = field->next) {
        if (field->name && strcmp(field->name, name) == 0) return true;
    }
    for (member = cls->members; member; member = member->next) {
        if (member->method && cxx_method_source_name(member->method) &&
            strcmp(cxx_method_source_name(member->method), name) == 0) {
            return true;
        }
    }
    return false;
}

static bool class_uses_base_member(CxxClass* cls, CxxClass* base,
                                   const char* name) {
    if (!cls || !base || !name) return false;
    for (int index = 0; index < cls->using_base_member_count; ++index) {
        const char* base_name = cls->using_base_members[index].base_name;
        const char* member_name = cls->using_base_members[index].member_name;
        const char* suffix;
        if (!base_name || !member_name || strcmp(member_name, name) != 0) {
            continue;
        }
        if (strcmp(base_name, base->name) == 0) return true;
        suffix = strstr(base_name, "::");
        while (suffix) {
            suffix += 2;
            if (strcmp(suffix, base->name) == 0) return true;
            suffix = strstr(suffix, "::");
        }
    }
    return false;
}

static bool class_has_using_base_member_name(CxxClass* cls,
                                             const char* name) {
    if (!cls || !name) return false;
    for (int index = 0; index < cls->using_base_member_count; ++index) {
        if (cls->using_base_members[index].member_name &&
            strcmp(cls->using_base_members[index].member_name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool class_has_method_declaration(CxxClass* cls, Decl* declaration) {
    if (!cls || !declaration) return false;
    for (TypeMethod* method = cls->type ? cls->type->methods : NULL;
         method; method = method->next) {
        if (method->function_decl == declaration) return true;
    }
    return false;
}

static bool cxx_method_parameter_lists_match(const CxxMethod* left,
                                             const CxxMethod* right) {
    DeclList* left_parameter;
    DeclList* right_parameter;
    if (!left || !right || !left->decl || !right->decl) return false;
    left_parameter = left->decl->func_params;
    right_parameter = right->decl->func_params;
    while (left_parameter && right_parameter) {
        if (!left_parameter->decl || !right_parameter->decl ||
            !type_is_compatible(left_parameter->decl->type,
                                right_parameter->decl->type)) {
            return false;
        }
        left_parameter = left_parameter->next;
        right_parameter = right_parameter->next;
    }
    return left_parameter == NULL && right_parameter == NULL;
}

static bool cxx_method_exception_spec_value(const CxxMethod* method,
                                            bool* value) {
    int64_t expression_value;
    if (!method || !method->decl || !value) return false;
    if (!method->decl->func_noexcept_expr) {
        *value = method->is_noexcept;
        return true;
    }
    if (!expr_eval_integer_constant(method->decl->func_noexcept_expr,
                                    &expression_value)) {
        return false;
    }
    *value = expression_value != 0;
    return true;
}

/* Diagnose invalid member declarations before treating them as overloads. */
static void diagnose_invalid_cxx_method_redeclaration(
    CxxClass* cls, CxxMethod* method) {
    const char* name = cxx_method_source_name(method);
    if (!cls || !method || !name) return;
    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        CxxMethod* previous = member->method;
        const char* previous_name;
        if (!previous) continue;
        previous_name = cxx_method_source_name(previous);
        if (!previous_name || strcmp(name, previous_name) != 0 ||
            !cxx_method_parameter_lists_match(method, previous)) {
            continue;
        }
        if (method->is_static == previous->is_static &&
            method->is_const == previous->is_const &&
            method->is_volatile == previous->is_volatile &&
            method->ref_qualifier == previous->ref_qualifier) {
            bool method_noexcept;
            bool previous_noexcept;
            if (cxx_method_exception_spec_value(method, &method_noexcept) &&
                cxx_method_exception_spec_value(previous,
                                                &previous_noexcept) &&
                method_noexcept != previous_noexcept) {
                rcc_error(method->decl->loc,
                          "declaration of member function '%s' has a different exception specification",
                          name);
                return;
            }
        }
        if ((method->ref_qualifier == CXX_REF_QUAL_NONE) !=
            (previous->ref_qualifier == CXX_REF_QUAL_NONE)) {
            rcc_error(method->decl->loc,
                      "a member function cannot be overloaded with the "
                      "same parameter types when only one declaration has "
                      "a ref-qualifier");
            return;
        }
    }
}

static bool class_method_declares_shared_virtual_base(
    CxxClass* cls, Decl* declaration) {
    CxxClass* owner;
    if (!cls || !declaration || !declaration->func_method_owner) return false;
    owner = declaration->func_method_owner->cxx_class;
    if (!owner) return false;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        if (cls->virtual_bases[index].base == owner) return true;
    }
    return false;
}

/* Publish methods of accessible bases on the derived class.  The layout pass
 * records the concrete offset used by this backend, so the alias can carry
 * the real base declaration and an explicit byte adjustment. */
static void register_inherited_class_methods(CxxClass* cls,
                                             TypeMethod*** tail) {
    if (!cls || !tail || !*tail || !cls->base_offsets) {
        return;
    }
    for (int base_index = 0; base_index < cls->base_count; ++base_index) {
        CxxClass* base = cls->bases[base_index].base;
        TypeMethod* method;
        if (!base || !base->type || !base->type->is_complete ||
            cls->base_offsets[base_index] < 0) {
            continue;
        }
        for (method = base->type->methods; method; method = method->next) {
            TypeMethod* inherited;
            if (method->kind != TYPE_METHOD_FUNCTION || !method->name ||
                !method->function_decl ||
                (class_has_method_declaration(cls, method->function_decl) &&
                 class_method_declares_shared_virtual_base(
                     cls, method->function_decl)) ||
                ((class_declares_method_name(cls, method->name) ||
                  class_has_using_base_member_name(cls, method->name)) &&
                 !class_uses_base_member(cls, base, method->name))) {
                continue;
            }
            inherited = ast_arena_alloc(sizeof(*inherited));
            *inherited = *method;
            if (cls->bases[base_index].access == ACCESS_PROTECTED &&
                inherited->cxx_access == ACCESS_PUBLIC) {
                inherited->cxx_access = ACCESS_PROTECTED;
                inherited->cxx_access_owner = cls;
            } else if (cls->bases[base_index].access == ACCESS_PRIVATE &&
                       inherited->cxx_access != ACCESS_PRIVATE) {
                inherited->cxx_access = ACCESS_PRIVATE;
                inherited->cxx_access_owner = cls;
            }
            if (method->this_owner) {
                int this_adjustment = cls->base_offsets[base_index];
                CxxClass* owner_class = method->this_owner->cxx_class;
                if (owner_class && cxx_class_virtual_base_offset(
                        cls, owner_class, &this_adjustment)) {
                    /* A method inherited through a virtual-base path must
                     * use the one shared subobject in the complete object;
                     * the intermediate branch offset is not sufficient. */
                } else {
                    this_adjustment = cls->base_offsets[base_index];
                    this_adjustment += method->this_adjustment;
                }
                inherited->this_owner = method->this_owner;
                inherited->this_adjustment = this_adjustment;
            } else if (method->function_decl->func_this_param) {
                int this_adjustment = cls->base_offsets[base_index];
                if (cls->bases[base_index].is_virtual) {
                    cxx_class_virtual_base_offset(cls, base, &this_adjustment);
                }
                inherited->this_owner = base->type;
                inherited->this_adjustment = this_adjustment;
            }
            inherited->next = NULL;
            **tail = inherited;
            *tail = &inherited->next;
        }
    }
}

static CxxConstructorInfo* constructor_info_for_method(CxxClass* cls,
                                                        CxxMethod* method) {
    for (CxxConstructorInfo* constructor = cls ? cls->constructors : NULL;
         constructor; constructor = constructor->next) {
        if (constructor->method == method) return constructor;
    }
    return NULL;
}

/* Publish ordinary non-virtual member definitions and static member
 * definitions as real functions.  Ordinary members receive the implicit
 * object parameter; static members deliberately do not, and use the normal C
 * call ABI after their qualified source lookup is resolved. */
static void register_ordinary_class_methods(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    if (!cls || !cls->type || !cls->type->is_complete || !active_ast ||
        active_template) return;

    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    register_inherited_class_methods(cls, &tail);
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        CxxConstructorInfo* constructor;
        Decl* declaration;
        TypeParam* this_type_parameter;
        Type* this_type;
        Type* qualified_owner;
        Decl* this_parameter;
        TypeMethod* lowered;
        const char* source_name;
        const char* link_name;

        constructor = method && method->is_constructor
            ? constructor_info_for_method(cls, method) : NULL;
        if (!method || !method->decl ||
            (!method->decl->func_body && !method->is_pure_virtual) ||
            method->is_deleted ||
            method->is_defaulted ||
            (method->is_constructor &&
             (!constructor || constructor->body_is_empty))) {
            continue;
        }

        declaration = method->decl;
        source_name = cxx_method_source_name(method);
        if (!method->is_destructor && source_name &&
            strcmp(source_name, "operator=") == 0 &&
            cls->type->move_assignment_method) {
            /* Validated field/ownership lowerings are complete executable
             * implementations.  Do not also publish their unvalidated body;
             * duplicate registration would make template instances route
             * through a call with no ABI function type. */
            continue;
        }
        link_name = rcc_intern(cxx_mangle_function(
            declaration, active_namespace, cls));

        /* Ordinary member names are not source-visible global symbols, so the
         * ABI spelling is also their semantic key.  Static members are
         * source-visible through Class::name and use that qualified key. */
        if (method->is_static) {
            declaration->name = cxx_class_method_source_name(
                cls, source_name, declaration->loc);
        } else {
            declaration->name = link_name;
        }
        declaration->link_name = link_name;
        declaration->func_has_cxx_linkage = true;
        declaration->func_has_local_linkage =
            cls->type->cxx_scope_identity != NULL;
        declaration->func_is_cxx_method = true;
        declaration->func_method_owner = cls->type;

        this_type = NULL;
        if (!method->is_static &&
            (method->is_const || method->is_volatile)) {
            qualified_owner = ast_arena_alloc(sizeof(*qualified_owner));
            *qualified_owner = *cls->type;
            qualified_owner->is_const = method->is_const;
            qualified_owner->is_volatile = method->is_volatile;
            this_type = type_ptr(qualified_owner);
        } else if (!method->is_static) {
            this_type = type_ptr(cls->type);
        }
        if (!method->is_static) {
            this_parameter = decl_param("this", this_type, -1,
                                        declaration->loc);
            declaration->func_this_param = this_parameter;

            this_type_parameter = ast_arena_alloc(sizeof(*this_type_parameter));
            this_type_parameter->name = "this";
            this_type_parameter->type = this_type;
            this_type_parameter->is_bitfield = false;
            this_type_parameter->bit_width = 0u;
            this_type_parameter->is_static = false;
            this_type_parameter->cxx_access = ACCESS_PUBLIC;
            this_type_parameter->next = declaration->type->params;
            declaration->type->params = this_type_parameter;
        }

        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = source_name;
        lowered->return_type = declaration->type->ret_type;
        lowered->field = NULL;
        lowered->function_decl = declaration;
        lowered->kind = TYPE_METHOD_FUNCTION;
        lowered->constant = 0;
        lowered->cleanup_function = NULL;
        lowered->result_field = NULL;
        lowered->success_constant = 0;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->is_explicit = method->is_explicit;
        lowered->ref_qualifier = method->ref_qualifier;
        lowered->this_owner = method->is_static ? NULL : cls->type;
        lowered->this_adjustment = 0;
        /* A method can override a secondary base slot without occupying a
         * slot in the class's primary table.  Calls through the complete
         * derived type can use the real body directly; calls through the
         * secondary base use that base's emitted adjusting thunk. */
        lowered->is_virtual = method->is_virtual &&
                              method->vtable_index >= 0 &&
                              cls->type->cxx_vtable_symbol != NULL;
        lowered->vtable_index = method->vtable_index;
        lowered->vtable_symbol = lowered->is_virtual
            ? cls->type->cxx_vtable_symbol : NULL;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;

        /* Pure virtual declarations must participate in member lookup and
         * virtual-call lowering, but they do not define an external function
         * symbol.  Their vtable slot is materialized by the target-local pure
         * virtual handler instead. */
        if (!method->is_pure_virtual) ast_add_decl(active_ast, declaration);
    }
}

static void diagnose_unlowered_destructors(CxxClass* cls) {
    struct CxxMember* member;
    if (!cls || !cls->type || cls->type->cleanup_function ||
        cls->destructor_method || active_template) {
        return;
    }
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        if (method && method->is_destructor && method->decl &&
            method->decl->func_body &&
            method->decl->func_body->block_stmts) {
            rcc_error(method->decl->loc,
                      "non-trivial C++ destructor body cannot be lowered "
                      "without object-lifetime support");
        }
    }
}

/* Publish in-class static data members as real global declarations.  They
 * retain a qualified source lookup name while their link name follows the
 * Itanium data-symbol spelling.  Static data members do not use the C
 * STORAGE_STATIC linkage rule: the `static` keyword belongs to the class
 * member, not to translation-unit visibility. */
static void register_class_static_fields(CxxClass* cls) {
    struct CxxMember* member;

    if (!cls || !active_ast || active_template) return;
    for (member = cls->members; member; member = member->next) {
        Decl* declaration = member->decl;
        const char* source_name;

        if (!member->is_static || member->method || !declaration ||
            declaration->kind != DECL_VAR || !declaration->name) {
            continue;
        }
        source_name = declaration->name;
        declaration->link_name = rcc_intern(cxx_mangle_name(
            source_name, active_namespace, cls));
        declaration->name = cxx_class_method_source_name(
            cls, source_name, declaration->loc);
        declaration->storage = STORAGE_NONE;
        ast_add_decl(active_ast, declaration);
    }
}

static const char* cxx_instance_static_source_name(
    CxxClass* cls, const char* field_name, SourceLoc loc) {
    char buffer[512];
    int written;
    if (!cls || !cls->name || !field_name) return rcc_intern("");
    written = snprintf(buffer, sizeof(buffer), "%s::%s",
                       cls->name, field_name);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        rcc_error(loc, "instantiated static member name exceeds compiler limit");
        return rcc_intern("");
    }
    return rcc_intern(buffer);
}

static void cxx_set_template_identity(
    CxxClass* cls, CxxTemplate* tmpl, Type** arguments,
    const int64_t* value_args, const bool* value_present, int argument_count) {
    bool expanded_pack = tmpl && tmpl->param_count == 1 &&
        tmpl->params[0].is_pack;
    if (!cls || !tmpl ||
        ((!expanded_pack && argument_count != tmpl->param_count) ||
         (argument_count > 0 && !arguments))) {
        return;
    }
    cls->template_identity_tmpl = tmpl;
    cls->template_identity_arg_count = argument_count;
    if (argument_count == 0) return;
    cls->template_identity_args = ast_arena_alloc(
        sizeof(Type*) * (size_t)argument_count);
    memcpy(cls->template_identity_args, arguments,
           sizeof(Type*) * (size_t)argument_count);
    if (value_args && value_present) {
        cls->template_identity_value_args = ast_arena_alloc(
            sizeof(int64_t) * (size_t)argument_count);
        cls->template_identity_value_present = ast_arena_alloc(
            sizeof(bool) * (size_t)argument_count);
        memcpy(cls->template_identity_value_args, value_args,
               sizeof(int64_t) * (size_t)argument_count);
        memcpy(cls->template_identity_value_present, value_present,
               sizeof(bool) * (size_t)argument_count);
    }
}

/* Static data members are declarations owned by the class template
 * definition, not methods, so the ordinary class publication pass skips
 * them while the template is still dependent.  Materialize one substituted
 * declaration for each cached class instance before its member bodies are
 * re-analyzed. */
static void register_instantiated_class_static_fields(
    CxxClass* instance, CxxClass* definition) {
    if (!instance || !definition || !active_ast) return;
    for (TypeParam* field = instance->fields; field; field = field->next) {
        Decl* source = NULL;
        Decl* declaration;
        if (!field->is_static || !field->name || !field->type) continue;
        for (struct CxxMember* member = definition->members;
             member; member = member->next) {
            const char* name;
            if (!member->is_static || member->method || !member->decl) continue;
            name = member->decl->name;
            name = name ? strrchr(name, ':') : NULL;
            name = name ? name + 1 : member->decl->name;
            if (name && strcmp(name, field->name) == 0) {
                source = member->decl;
                break;
            }
        }
        declaration = decl_var(
            cxx_instance_static_source_name(instance, field->name,
                                            source ? source->loc : (SourceLoc){"<template>", 0, 0}),
            field->type, field->initializer,
            source ? source->loc : (SourceLoc){"<template>", 0, 0});
        declaration->link_name = rcc_intern(cxx_mangle_name(
            field->name, instance->ns, instance));
        declaration->var_is_thread_local = source &&
                                           source->var_is_thread_local;
        declaration->var_is_inline = source && source->var_is_inline;
        declaration->var_is_constexpr = source && source->var_is_constexpr;
        declaration->var_is_constinit = source && source->var_is_constinit;
        declaration->var_is_deprecated = source && source->var_is_deprecated;
        declaration->var_deprecated_message = source
            ? source->var_deprecated_message : NULL;
        declaration->storage = STORAGE_NONE;
        cxx_class_add_member(instance, declaration,
                             (AccessSpec)field->cxx_access, true);
        ast_add_decl(active_ast, declaration);
    }
}

/* Parse class member (field or method) */
static void parse_class_member(CxxClass* cls, AccessSpec current_access) {
    SourceLoc loc = peek()->loc;
    int explicit_alignment = 0;
    bool has_explicit_alignment = false;

    while (check(TOK__ALIGNAS)) {
        int alignment = rcc_parser_parse_explicit_alignment();
        has_explicit_alignment = true;
        if (alignment > explicit_alignment) explicit_alignment = alignment;
    }

    skip_cxx_attributes();
    bool is_nodiscard = take_cxx_nodiscard();
    bool is_weak = take_cxx_weak();
    const char* deprecated_message = NULL;
    bool is_deprecated = take_cxx_deprecated(&deprecated_message);
    bool is_no_unique_address = take_cxx_no_unique_address();

    if (is_weak) {
        rcc_error(loc, "[[gnu::weak]] requires a file-scope declaration");
    }
    bool is_virtual = false;
    bool is_static = false;
    bool is_inline = false;
    bool is_constexpr = false;
    bool is_consteval = false;
    bool is_constinit = false;
    bool is_explicit = false;
    bool is_thread_local = false;
    bool is_friend = false;

    /* C++ declaration specifiers can be combined in either order. */
    for (;;) {
        if (match(TOK_VIRTUAL)) is_virtual = true;
        else if (match(TOK_STATIC)) is_static = true;
        else if (match(TOK_THREAD_LOCAL)) is_thread_local = true;
        else if (match(TOK_CONSTEXPR)) is_constexpr = true;
        else if (match(TOK_CONSTEVAL)) {
            is_constexpr = true;
            is_consteval = true;
        }
        else if (match(TOK_CONSTINIT)) is_constinit = true;
        else if (check(TOK_EXPLICIT)) {
            is_explicit = parse_cxx_explicit_specifier(peek()->loc);
        }
        else if (match(TOK_INLINE) || match(TOK___INLINE__)) is_inline = true;
        else if (match(TOK_FRIEND)) is_friend = true;
        else if (match(TOK_MUTABLE)) { }
        else if (check(TOK__ALIGNAS)) {
            int alignment = rcc_parser_parse_explicit_alignment();
            has_explicit_alignment = true;
            if (alignment > explicit_alignment) explicit_alignment = alignment;
        }
        else break;
    }

    if (is_no_unique_address && (is_static || is_friend)) {
        rcc_error(loc,
                  "[[no_unique_address]] requires a non-static class data member");
    }

    if (is_friend && (check(TOK_CLASS) || check(TOK_STRUCT))) {
        const char* friend_name;
        advance();
        friend_name = parse_qualified_name();
        if (!friend_name || !*friend_name || strcmp(friend_name, "::") == 0) {
            rcc_error(loc, "friend class declaration requires a class name");
        } else {
            cxx_class_add_friend_class(cls, friend_name);
        }
        expect(TOK_SEMICOLON, ";");
        return;
    }
    /* A friend function defined in a class is a namespace function, not a
     * member.  Treat it as static while parsing its body so no synthetic
     * `this` binding is introduced; it is published to the owning namespace
     * below instead of entering the class member/vtable set. */
    if (is_friend) is_static = true;

    /* Check for destructor */
    bool is_destructor = match(TOK_TILDE);
    bool is_constructor = false;

    /* Parse type (or constructor) */
    Type* type = NULL;
    const char* name = NULL;

    if (is_destructor) {
        /* Destructor: ~ClassName() */
        expect(TOK_IDENT, "class name");
        name = rcc_intern("~dtor");
        type = type_void;
    } else if (check(TOK_IDENT) && check_next(TOK_LPAREN) &&
               strcmp(peek()->value.str_val, cls->name) == 0) {
        /* Constructor */
        name = advance()->value.str_val;
        type = type_void;  /* Constructors have no return type */
        is_constructor = true;
    } else if (match(TOK_OPERATOR)) {
        /* Conversion function: operator bool(), operator T*(), ... */
        type = parse_cxx_type_spec();
        name = rcc_intern("operator conversion");
    } else {
        type = parse_cxx_type_spec();
        if (check(TOK_IDENT)) {
            name = advance()->value.str_val;
        } else if (match(TOK_OPERATOR)) {
            name = parse_operator_name();
        }
    }

    if (!name && !check(TOK_COLON)) {
        rcc_error(loc, "expected member name");
        return;
    }

    /* Is this a method or a field? */
    if (match(TOK_LPAREN)) {
        /* Method */
        if (has_explicit_alignment) {
            rcc_error(loc, "alignas cannot apply to a member function");
        }
        DeclList* params = NULL;
        int param_idx = 0;
        bool saw_default = false;
        ParsedConstructorInitializer constructor_initializer = {0};
        Expr* noexcept_expr = NULL;

        if (!check(TOK_RPAREN)) {
            if (check(TOK_VOID) && parser.cur->next && parser.cur->next->type == TOK_RPAREN) {
                advance();
            } else {
                do {
                    const char* pname = NULL;
                    Type* ptype = parse_cxx_type_spec();
                    ptype = rcc_parser_parse_cxx_declarator(ptype, &pname,
                                                            NULL);
                    Expr* default_argument = NULL;
                    if (match(TOK_ASSIGN)) {
                        default_argument = parse_assignment_expression();
                        saw_default = true;
                    } else if (saw_default) {
                        rcc_error(peek()->loc,
                                  "parameter without a default follows a default argument");
                    }
                    Decl* p = decl_param(pname, ptype, param_idx++, peek()->loc);
                    p->param_default = default_argument;
                    decllist_append(&params, p);
                } while (match(TOK_COMMA));
            }
        }
        expect(TOK_RPAREN, ")");

        bool is_const = false;
        bool is_volatile = false;
        CxxRefQualifier ref_qualifier = CXX_REF_QUAL_NONE;
        bool is_override = false;
        bool is_final = false;
        bool is_noexcept = false;
        for (;;) {
            if (match(TOK_CONST)) is_const = true;
            else if (match(TOK_VOLATILE)) is_volatile = true;
            else if (match(TOK_AMP)) {
                if (ref_qualifier != CXX_REF_QUAL_NONE) {
                    rcc_error(previous()->loc,
                              "a member function cannot have multiple "
                              "ref-qualifiers");
                } else {
                    ref_qualifier = CXX_REF_QUAL_LVALUE;
                }
            } else if (match(TOK_AND)) {
                if (ref_qualifier != CXX_REF_QUAL_NONE) {
                    rcc_error(previous()->loc,
                              "a member function cannot have multiple "
                              "ref-qualifiers");
                } else {
                    ref_qualifier = CXX_REF_QUAL_RVALUE;
                }
            } else if (match(TOK_OVERRIDE)) is_override = true;
            else if (match(TOK_FINAL)) is_final = true;
            else if (match(TOK_NOEXCEPT)) {
                if (check(TOK_LPAREN)) {
                    advance();
                    noexcept_expr = parse_expression();
                    expect(TOK_RPAREN, ")");
                } else {
                    is_noexcept = true;
                }
            } else break;
        }

        /* Pure virtual and explicitly defaulted/deleted functions. */
        bool is_pure = false;
        bool is_deleted = false;
        bool is_defaulted = false;
        if (match(TOK_ASSIGN)) {
            if (check(TOK_INT_LIT) && peek()->value.int_val == 0) {
                advance();
                is_pure = true;
            } else if (match(TOK_DELETE)) {
                is_deleted = true;
            } else if (match(TOK_DEFAULT)) {
                is_defaulted = true;
            } else {
                rcc_error(peek()->loc,
                          "expected 0, delete, or default after '='");
            }
        }

        if (is_constructor) {
            constructor_initializer = parse_ctor_initializer();
        }

        /* Method body or declaration */
        Stmt* body = NULL;
        if (match(TOK_LBRACE)) {
            /* Parse method body */
            StmtList* stmts = NULL;
            void* enum_scope = rcc_parser_enum_scope_mark();
            rcc_parser_cxx_begin_function_parameters(params);
            rcc_parser_function_scope_push(name);
            if (!is_static) {
                Type* this_type = type_ptr(cls->type);
                if (is_const || is_volatile) {
                    Type* qualified_owner =
                        ast_arena_alloc(sizeof(*qualified_owner));
                    *qualified_owner = *cls->type;
                    qualified_owner->is_const = is_const;
                    qualified_owner->is_volatile = is_volatile;
                    this_type = type_ptr(qualified_owner);
                }
                rcc_parser_cxx_add_value_binding("this", this_type);
            }
            while (!check(TOK_RBRACE) && !at_end()) {
                Token* statement_start = parser.cur;
                int errors_before = g_error_count;
                Stmt* s = parse_cxx_statement();
                if (s) stmtlist_append(&stmts, s);
                if (g_error_count > errors_before) {
                    while (!at_end() && !check(TOK_SEMICOLON) &&
                           !check(TOK_RBRACE)) {
                        advance();
                    }
                    if (check(TOK_SEMICOLON)) advance();
                } else if (parser.cur == statement_start && !at_end()) {
                    advance();
                }
            }
            expect(TOK_RBRACE, "}");
            rcc_parser_function_scope_pop();
            rcc_parser_enum_scope_restore(enum_scope);
            rcc_parser_cxx_end_function_parameters();
            body = stmt_block(stmts, loc);
        } else {
            expect(TOK_SEMICOLON, ";");
        }

        /* Create method */
        CxxMethod* method = cxx_method_new(name, type, params, body, loc);
        method->decl->type->is_const = is_const;
        method->decl->type->is_volatile = is_volatile;
        method->decl->type->function_ref_qualifier = ref_qualifier;
        method->decl->type->function_is_noexcept = is_noexcept;
        method->access = current_access;
        method->is_virtual = is_virtual;
        method->is_static = is_static;
        method->is_constexpr = is_constexpr;
        method->decl->func_is_constexpr = is_constexpr;
        method->decl->func_is_consteval = is_consteval;
        method->decl->func_is_nodiscard = is_nodiscard;
        method->decl->func_is_deprecated = is_deprecated;
        method->decl->func_deprecated_message = deprecated_message;
        method->is_explicit = is_explicit;
        method->is_const = is_const;
        method->is_volatile = is_volatile;
        method->ref_qualifier = ref_qualifier;
        method->is_override = is_override;
        method->is_final = is_final;
        method->is_noexcept = is_noexcept;
        method->decl->func_is_noexcept = is_noexcept;
        method->decl->func_noexcept_expr = noexcept_expr;
        method->is_pure_virtual = is_pure;
        method->is_deleted = is_deleted;
        method->is_defaulted = is_defaulted;
        method->is_constructor = is_constructor;
        method->is_destructor = is_destructor;
        if ((is_const || is_volatile ||
             ref_qualifier != CXX_REF_QUAL_NONE) && is_friend) {
            rcc_error(loc,
                      "a friend function cannot have member cv/ref qualifiers");
        }
        if (is_volatile && is_static) {
            rcc_error(loc,
                      "a static member function cannot have a volatile qualifier");
        }
        if (is_const && is_static) {
            rcc_error(loc,
                      "a static member function cannot have a const qualifier");
        }
        if (ref_qualifier != CXX_REF_QUAL_NONE && is_static) {
            rcc_error(loc,
                      "a static member function cannot have a ref-qualifier");
        }
        if ((is_const || is_volatile ||
             ref_qualifier != CXX_REF_QUAL_NONE) &&
            (is_constructor || is_destructor)) {
            rcc_error(loc,
                      "a constructor or destructor cannot have cv/ref qualifiers");
        }
        method->decl->func_is_cxx_constructor = is_constructor;
        method->decl->func_is_cxx_destructor = is_destructor;
        if (is_no_unique_address) {
            rcc_error(loc,
                      "[[no_unique_address]] requires a data member, not a method");
        }
        /* A function defined inside a class definition is implicitly inline
         * in C++, even without the `inline` keyword.  Preserve that linkage
         * property so identical in-class definitions from separate
         * translation units are weak/ODR definitions rather than strong
         * duplicate symbols. */
        method->decl->func_is_inline = body != NULL;

        if (is_friend) {
            method->owner = NULL;
            method->decl->func_is_hidden_friend = true;
            CxxFriendAccess* friend_access =
                ast_arena_alloc(sizeof(*friend_access));
            friend_access->owner = cls;
            friend_access->next = method->decl->func_friend_access;
            method->decl->func_friend_access = friend_access;
            if (active_ast) {
                add_namespace_declaration(active_ast,
                                          active_namespace ? active_namespace
                                                           : g_global_namespace,
                                          method->decl);
            }
            return;
        }
        method->owner = cls;
        diagnose_invalid_cxx_method_redeclaration(cls, method);

        if (is_constructor) cls->has_user_constructor = true;

        if (is_constructor) {
            CxxConstructorInfo* info = ast_arena_alloc(sizeof(*info));
            CxxConstructorInfo** tail = &cls->constructors;
            info->method = method;
            info->parameter_count = param_idx;
            info->parameters = method->decl->type->params;
            info->initializers = constructor_initializer.items;
            info->initializer_count = constructor_initializer.count;
            info->initializers_are_supported =
                constructor_initializer.count == 0 ||
                constructor_initializer.is_supported;
            info->body_is_empty = body && body->kind == STMT_BLOCK &&
                                  body->block_stmts == NULL;
            info->is_deleted = is_deleted;
            info->is_defaulted = is_defaulted;
            info->access = current_access;
            info->next = NULL;
            if (!info->body_is_empty) {
                (void)lowerable_constructor_body(cls, info);
            }
            while (*tail) tail = &(*tail)->next;
            *tail = info;
        }

        cxx_class_add_method(cls, method);
    } else {
        /* Field */
        bool is_bitfield = false;
        unsigned bit_width = 0u;
        int array_lengths[64];
        Expr* array_bounds[64];
        SourceLoc array_locs[64];
        int array_count = 0;
        /* Array suffixes bind from the member name outward. Keep the source
         * order and construct the nested array type inside-out after parsing
         * all dimensions (for example, `T matrix[2][3]` is [2] of [3] T). */
        while (match(TOK_LBRACKET)) {
            int len = -1;
            Expr* bound_expression = NULL;
            if (check(TOK_INT_LIT)) {
                len = (int)advance()->value.int_val;
            } else if (!check(TOK_RBRACKET)) {
                int64_t constant;
                bound_expression = parse_assignment_expression();
                if (expr_eval_integer_constant(bound_expression, &constant)) {
                    if (constant <= 0 || constant > INT_MAX) {
                        rcc_error(bound_expression->loc,
                                  "class member array bound must be a positive "
                                  "representable integer constant");
                    } else {
                        len = (int)constant;
                    }
                    bound_expression = NULL;
                } else if (!bound_expression) {
                    rcc_error(loc, "class member array bound is invalid");
                } else {
                    len = -2;
                }
            }
            expect(TOK_RBRACKET, "]");
            if (array_count >= (int)(sizeof(array_lengths) /
                                     sizeof(array_lengths[0]))) {
                rcc_error(previous()->loc,
                          "C++ member array declarator is too deep");
            } else {
                array_lengths[array_count] = len;
                array_bounds[array_count] = bound_expression;
                array_locs[array_count] = previous()->loc;
                ++array_count;
            }
        }
        while (array_count > 0) {
            int index = --array_count;
            int len = array_lengths[index];
            if (len > 0 && type->size > 0 &&
                len > INT_MAX / type->size) {
                rcc_error(array_locs[index],
                          "C++ member array bound is too large for its "
                          "complete element type");
                len = -1;
            }
            type = type_array(type, len);
            type->array_bound = array_bounds[index];
        }

        if (has_explicit_alignment) {
            type = rcc_parser_apply_explicit_alignment(
                type, explicit_alignment, loc);
        }

        if (match(TOK_COLON)) {
            Expr* width_expression = parse_assignment_expression();
            int64_t width_value = 0;
            is_bitfield = true;
            if (!type || (!type_is_integer(type) &&
                          type->kind != TYPE_ENUM)) {
                rcc_error(loc,
                          "C++ bit-field type must be an integer or enum type");
            } else if (type->size <= 0 || type->size > 4) {
                rcc_error(loc,
                          "C++ bit-field type width of %d bytes is not supported",
                          type->size);
            }
            if (!width_expression ||
                !expr_eval_integer_constant(width_expression, &width_value) ||
                width_value < 0 ||
                (type && type->size > 0 &&
                 (uint64_t)width_value > (uint64_t)type->size * 8u)) {
                rcc_error(width_expression ? width_expression->loc : loc,
                          "C++ bit-field width is not a valid storage-unit constant");
            } else {
                bit_width = (unsigned)width_value;
                if (bit_width == 0u && name) {
                    rcc_error(loc, "named C++ bit-field cannot have zero width");
                }
            }
        }

        if (is_no_unique_address && is_bitfield) {
            rcc_error(loc,
                      "[[no_unique_address]] cannot be applied to a bit-field");
        }

        /* Initializer? */
        Expr* init = NULL;
        if (match(TOK_ASSIGN)) {
            init = rcc_parser_parse_initializer();
        } else if (check(TOK_LBRACE)) {
            init = rcc_parser_parse_initializer();
        }
        if (init && !is_static) cls->has_field_initializer = true;
        if (is_thread_local && !is_static) {
            rcc_error(loc,
                      "thread-local storage is only valid on static C++ data members");
        }
        if (current_access != ACCESS_PUBLIC) {
            cls->has_nonpublic_field = true;
        }
        if (is_static) cls->has_static_field = true;
        if (is_static && is_inline &&
            !rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(loc, "inline variables require C++17 or newer");
        }

        expect(TOK_SEMICOLON, ";");

        /* Add field to class */
        cxx_class_add_field_initializer(cls, name, type, current_access, init,
                                         is_bitfield, bit_width, is_static,
                                         is_deprecated, deprecated_message,
                                         is_no_unique_address);
        if (is_static && name) {
            Decl* declaration = decl_var(name, type, init, loc);
            declaration->var_is_thread_local = is_thread_local;
            declaration->var_is_inline = is_inline;
            declaration->var_is_constexpr = is_constexpr;
            declaration->var_is_constinit = is_constinit;
            declaration->var_is_deprecated = is_deprecated;
            declaration->var_deprecated_message = deprecated_message;
            cxx_class_add_member(cls, declaration, current_access, true);
        }
    }
}

/* Class-scope member templates have a separate parser path.  Recognize the
 * friend-function-template form here so it is registered as a namespace
 * template rather than mistaken for a data member.  The balanced scan is
 * limited to the template parameter list; the full grammar is parsed below. */
static bool cxx_friend_function_template_starts(void) {
    Token* token;
    int depth = 0;
    int brace_depth = 0;
    if (!check(TOK_TEMPLATE)) return false;
    token = parser.cur->next;
    if (!token || token->type != TOK_LT) return false;
    for (; token; token = token->next) {
        if (token->type == TOK_LT) {
            ++depth;
        } else if (token->type == TOK_GT) {
            if (--depth == 0) {
                token = token->next;
                break;
            }
        } else if (token->type == TOK_RSHIFT) {
            depth -= 2;
            if (depth <= 0) {
                token = token->next;
                break;
            }
        }
    }
    if (!token) return false;
    if (token->type == TOK_FRIEND) return true;
    if (token->type != TOK_REQUIRES) return false;
    /* A requires-clause can itself contain a requires-expression with a
     * braced requirement body. Scan through that body, but stop at a function
     * body or declaration terminator so a later friend cannot claim this
     * member template. */
    while (token) {
        if (token->type == TOK_LBRACE) {
            ++brace_depth;
        } else if (token->type == TOK_RBRACE && brace_depth > 0) {
            --brace_depth;
        } else if (brace_depth == 0 && token->type == TOK_FRIEND) {
            return true;
        } else if (brace_depth == 0 &&
                   (token->type == TOK_SEMICOLON ||
                    token->type == TOK_LBRACE)) {
            return false;
        }
        token = token->next;
    }
    return false;
}

static bool cxx_class_alias_template_starts(void) {
    Token* token;
    int depth = 0;
    if (!check(TOK_TEMPLATE)) return false;
    token = parser.cur->next;
    if (!token || token->type != TOK_LT) return false;
    for (; token; token = token->next) {
        if (token->type == TOK_LT) {
            ++depth;
        } else if (token->type == TOK_GT) {
            if (--depth == 0) {
                token = token->next;
                break;
            }
        } else if (token->type == TOK_RSHIFT) {
            depth -= 2;
            if (depth <= 0) {
                token = token->next;
                break;
            }
        }
    }
    return token && token->type == TOK_USING;
}

/* Parse the body and ABI metadata of a class after its source name has
 * already been consumed.  Explicit template specializations use this same
 * path so their class body cannot be mistaken for a primary-template body. */
static CxxClass* cxx_find_class_declaration(const char* name) {
    CxxNamespace* ns = active_namespace
        ? active_namespace : cxx_namespace_global();
    if (!name || !ns) return NULL;
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* candidate = ns->classes[index];
        if (candidate && candidate->name &&
            strcmp(candidate->name, name) == 0 && candidate->type &&
            !candidate->templ) {
            return candidate;
        }
    }
    return NULL;
}

static CxxClass* parse_cxx_class_named(SourceLoc loc, bool is_struct,
                                       const char* class_name,
                                       const char* local_type_identity) {
    bool has_definition = false;
    bool is_final;

    /* A final class has the same object layout as an otherwise identical
     * class; the semantic restriction is enforced when bases are resolved. */
    is_final = match(TOK_FINAL);

    CxxClass* existing = (!active_template && !active_class &&
                          !local_type_identity)
        ? cxx_find_class_declaration(class_name) : NULL;
    CxxClass* cls = NULL;
    if (existing && existing->type && existing->type->is_complete &&
        !check(TOK_LBRACE) && !check(TOK_COLON)) {
        /* A repeated namespace-scope forward declaration denotes the same
         * class type; do not append a second incomplete class to the registry. */
        return existing;
    }
    if (existing && existing->type && !existing->type->is_complete) {
        cls = existing;
    }
    if (!cls) cls = cxx_class_new(class_name, loc);
    /* Friend-name matching is needed while this class body is parsed. Give
     * the class its namespace identity before member declarations are read;
     * namespace registration still happens after the complete definition. */
    cls->ns = active_namespace ? active_namespace : g_global_namespace;
    if (active_template && active_template->kind == TMPL_CLASS &&
        active_template->param_count > 0 &&
        !active_template->templated_class &&
        !active_template_class_definition) {
        active_template_class_definition = cls;
    }
    cls->is_struct = is_struct;
    cls->is_final = is_final;
    cls->type->cxx_scope_identity = local_type_identity;
    cls->pack_alignment = rcc_parser_pack_alignment();
    /* Make a local class's own type visible while parsing its members.
     * In particular, constructor parameters such as `Local&&` must refer to
     * this declaration so function-template substitution can retarget that
     * self-reference to the concrete local-class instance. */
    if (local_type_identity) {
        rcc_parser_define_type(cls->name, cls->type);
    }

    /* Inheritance */
    if (match(TOK_COLON)) {
        do {
            AccessSpec inherit_access = is_struct
                ? ACCESS_PUBLIC : ACCESS_PRIVATE;
            bool is_virtual = false;
            if (match(TOK_VIRTUAL)) is_virtual = true;
            if (match(TOK_PUBLIC)) inherit_access = ACCESS_PUBLIC;
            else if (match(TOK_PROTECTED)) inherit_access = ACCESS_PROTECTED;
            else if (match(TOK_PRIVATE)) inherit_access = ACCESS_PRIVATE;
            if (match(TOK_VIRTUAL)) is_virtual = true;

            if (active_template) {
                Type* base_type = parse_cxx_type_spec();
                bool is_pack_expansion = match(TOK_ELLIPSIS);
                if (!base_type || base_type->kind != TYPE_STRUCT) {
                    rcc_error(loc,
                              "template class base must name a class type");
                } else {
                    cxx_class_add_base_pattern(
                        cls, base_type, base_type->tag, inherit_access,
                        is_virtual, is_pack_expansion);
                }
            } else {
                const char* base_name = parse_qualified_name();
                cxx_class_add_base(cls, base_name, inherit_access);
                if (is_virtual) {
                    cls->bases[cls->base_count - 1].is_virtual = true;
                }
            }
        } while (match(TOK_COMMA));
    }

    /* Class body */
    if (match(TOK_LBRACE)) {
        CxxClass* enclosing_class = active_class;
        active_class = cls;
        has_definition = true;
        AccessSpec current_access = is_struct
            ? ACCESS_PUBLIC : ACCESS_PRIVATE;

        while (!check(TOK_RBRACE) && !at_end()) {
            /* Check for access specifier */
            AccessSpec new_access = parse_access_spec();
            if (new_access != (AccessSpec)-1) {
                current_access = new_access;
                continue;
            }

            if (cxx_friend_function_template_starts()) {
                CxxTemplate* tmpl;
                advance(); /* template */
                tmpl = parse_cxx_template();
                if (!tmpl || tmpl->kind != TMPL_FUNCTION ||
                    !tmpl->friend_access) {
                    rcc_error(peek()->loc,
                              "class friend template must declare a function template");
                } else {
                    cxx_namespace_add_template(
                        active_namespace ? active_namespace
                                         : g_global_namespace,
                        tmpl);
                }
                continue;
            }

            if (cxx_class_alias_template_starts()) {
                SourceLoc template_loc = peek()->loc;
                CxxTemplate* member_template;
                advance(); /* template */
                member_template = parse_cxx_template();
                if (!member_template || member_template->kind != TMPL_ALIAS) {
                    rcc_error(template_loc,
                              "class-scope alias template declaration is invalid");
                } else {
                    cxx_class_add_alias_template(
                        cls, member_template, current_access, template_loc);
                }
                continue;
            }

            if (match(TOK_USING)) {
                SourceLoc using_loc = previous()->loc;
                if (check(TOK_IDENT) && parser.cur->next &&
                    parser.cur->next->type == TOK_ASSIGN) {
                    const char* alias_name = advance()->value.str_val;
                    Type* alias_type;
                    advance();
                    alias_type = parse_cxx_type_spec();
                    alias_type = rcc_parser_parse_cxx_declarator(
                        alias_type, NULL, NULL);
                    if (!alias_type) {
                        rcc_error(using_loc,
                                  "nested type alias requires a type");
                    } else {
                        cxx_class_add_type_alias(cls, alias_name, alias_type,
                                                 current_access);
                    }
                    expect(TOK_SEMICOLON, ";");
                    continue;
                }
                if (active_template &&
                    cxx_using_starts_with_template_id_base()) {
                    const char* base_template_name = parse_qualified_name();
                    CxxTemplate* base_template = base_template_name
                        ? find_class_template(base_template_name) : NULL;
                    CxxTemplate* base_alias_template = base_template
                        ? NULL
                        : base_template_name
                            ? find_alias_template(base_template_name) : NULL;
                    Type* base_type = base_template
                        ? parse_class_template_specialization(
                              base_template, using_loc)
                        : base_alias_template
                            ? parse_alias_template_specialization(
                                  base_alias_template, using_loc, NULL)
                            : NULL;
                    Token* member_token;
                    if (!base_template && !base_alias_template) {
                        rcc_error(using_loc,
                                  "dependent base using-declaration requires a class or alias template specialization");
                        skip_cxx_template_arguments();
                    }
                    expect(TOK_SCOPE, ":: in dependent base using-declaration");
                    member_token = expect(TOK_IDENT,
                                          "member in dependent base using-declaration");
                    if (!base_type || base_type->kind != TYPE_STRUCT ||
                        !member_token) {
                        rcc_error(using_loc,
                                  "dependent base using-declaration requires a class type and member");
                    } else {
                        cxx_class_add_using_base_member(
                            cls, base_type->tag, member_token->value.str_val,
                            base_type, current_access, using_loc);
                    }
                    expect(TOK_SEMICOLON, ";");
                    continue;
                }
                const char* qualified = parse_qualified_name();
                const char* separator = qualified
                    ? strrchr(qualified, ':') : NULL;
                if (!separator || separator == qualified ||
                    separator[-1] != ':') {
                    rcc_error(using_loc,
                              "class using-declaration must name a base member");
                } else {
                    size_t base_length = (size_t)(separator - qualified - 1);
                    char base_name[512];
                    if (base_length == 0 || base_length >= sizeof(base_name)) {
                        rcc_error(using_loc,
                                  "class using-declaration base name is too long");
                    } else {
                        memcpy(base_name, qualified, base_length);
                        base_name[base_length] = '\0';
                        cxx_class_add_using_base_member(
                            cls, rcc_intern(base_name),
                            rcc_intern(separator + 1), NULL,
                            current_access, using_loc);
                    }
                }
                expect(TOK_SEMICOLON, ";");
                continue;
            }

            /* Parse member */
            Token* member_start = parser.cur;
            parse_class_member(cls, current_access);
            if (parser.cur == member_start && !at_end()) advance();
        }

        expect(TOK_RBRACE, "}");
        active_class = enclosing_class;
    }

    /* Optional semicolon */
    match(TOK_SEMICOLON);

    /* A forward declaration has no layout yet. */
    if (!has_definition) return cls;

    /* The namespace owner is normally published immediately after this
     * routine returns, but vtable names/layout metadata are built here. */
    cls->ns = active_namespace ? active_namespace : g_global_namespace;
    resolve_class_bases(cls, loc);
    if (!cxx_class_has_unresolved_dependent_base(cls)) {
        validate_class_virtual_specifiers(cls, loc);
    }
    cxx_class_compute_layout(cls);
    if ((!active_template || !active_template->is_local_class_template) &&
        !cxx_class_has_unresolved_dependent_base(cls)) {
        complete_cxx_default_member_initializers(cls);
    }
    cxx_class_build_vtable(cls);
    register_inline_class_accessors(cls);
    register_inline_class_bool_delegates(cls);
    register_inline_class_cleanup(cls);
    register_inline_class_releases(cls);
    register_inline_class_closes(cls);
    register_inline_class_close_delegates(cls);
    register_inline_class_move_constructor(cls);
    register_inline_class_move_assignment(cls);
    diagnose_unlowered_destructors(cls);
    register_class_static_fields(cls);
    register_ordinary_class_methods(cls);
    cxx_complete_pending_member_pointer_forms(cls);

    /* Aggregate classes and the validated one-field constructor subset can
     * reuse the common initializer/codegen backend.  Every complete class
     * still has to enter the parser's type-name table: later declarations
     * may use a non-aggregate class through a pointer or reference even when
     * its constructors, private members, or virtual members are not lowered
     * by the common backend. */
    if (local_type_identity && !cls->type->is_complete) {
        rcc_parser_define_type(cls->name, cls->type);
    }
    if ((!active_template || local_type_identity) &&
        cls->type->is_complete) {
        uint32_t constructor_mask = lowerable_constructor_arity_mask(cls);
        if (constructor_mask != 0u) {
            rcc_parser_define_cxx_constructor_type(
                cls->name, cls->type, constructor_mask);
        } else {
            rcc_parser_define_type(cls->name, cls->type);
        }
    }

    return cls;
}

/* Parse class definition. */
CxxClass* parse_cxx_class(void) {
    SourceLoc loc = previous()->loc;
    bool is_struct = previous()->type == TOK_STRUCT;
    Token* name_tok = expect(TOK_IDENT, "class name");
    const char* class_name = name_tok ? name_tok->value.str_val : "anonymous";
    return parse_cxx_class_named(loc, is_struct, class_name, NULL);
}

/* ═══════════════════════════════════════
 * C++ Namespace Parsing
 * ═══════════════════════════════════════ */

static const char* namespace_qualified_decl_name(CxxNamespace* ns,
                                                 const char* name,
                                                 SourceLoc loc) {
    CxxNamespace* stack[32];
    int count = 0;
    char buffer[512] = "";
    size_t length = 0u;

    for (CxxNamespace* current = ns;
         current && current->name;
         current = current->parent) {
        if (count == (int)(sizeof(stack) / sizeof(stack[0]))) {
            rcc_error(loc, "namespace nesting exceeds compiler limit");
            break;
        }
        stack[count++] = current;
    }
    for (int index = count - 1; index >= 0; --index) {
        size_t part_length = strlen(stack[index]->name);
        if (part_length > sizeof(buffer) - 1u - length) {
            rcc_error(loc, "qualified declaration name exceeds compiler limit");
            return rcc_intern(buffer);
        }
        memcpy(buffer + length, stack[index]->name, part_length);
        length += part_length;
        if (length > sizeof(buffer) - 3u) {
            rcc_error(loc, "qualified declaration name exceeds compiler limit");
            return rcc_intern(buffer);
        }
        memcpy(buffer + length, "::", 2u);
        length += 2u;
    }
    if (strlen(name) > sizeof(buffer) - 1u - length) {
        rcc_error(loc, "qualified declaration name exceeds compiler limit");
        return rcc_intern(buffer);
    }
    strcpy(buffer + length, name);
    return rcc_intern(buffer);
}

static void set_cxx_link_name(Decl* declaration, CxxNamespace* ns,
                              bool c_language_linkage) {
    if (!declaration) return;
    if (declaration->kind == DECL_FUNC) {
        declaration->func_has_cxx_linkage = !c_language_linkage;
    }
    if (c_language_linkage) return;
    if (!ns && declaration->name &&
        strcmp(declaration->name, "main") == 0) {
        /* The hosted entry point is never mangled. */
        return;
    }
    if (declaration->kind == DECL_FUNC) {
        declaration->link_name = rcc_intern(
            cxx_mangle_function(declaration, ns, NULL));
    } else if (declaration->kind == DECL_VAR) {
        declaration->link_name = rcc_intern(
            cxx_mangle_name(declaration->name, ns, NULL));
    }
}

static void add_namespace_declaration(AST* ast, CxxNamespace* ns,
                                      Decl* declaration) {
    const char* qualified_name;

    if (!declaration) {
        (void)take_cxx_nodiscard();
        (void)take_cxx_weak();
        (void)take_cxx_deprecated(NULL);
        if (take_cxx_no_unique_address()) {
            rcc_error((SourceLoc){"<declaration>", 0, 0},
                      "[[no_unique_address]] requires a class data member");
        }
        return;
    }
    if (take_cxx_no_unique_address()) {
        rcc_error(declaration->loc,
                  "[[no_unique_address]] requires a class data member");
    }
    if (declaration->kind == DECL_FUNC && take_cxx_nodiscard()) {
        declaration->func_is_nodiscard = true;
    } else if (declaration->kind != DECL_FUNC) {
        (void)take_cxx_nodiscard();
    }
    if (declaration->kind != DECL_FUNC) {
        declaration->is_weak = take_cxx_weak();
    }
    {
        const char* deprecated_message = NULL;
        bool is_deprecated = take_cxx_deprecated(&deprecated_message);
        if (is_deprecated && declaration->kind == DECL_FUNC) {
            declaration->func_is_deprecated = true;
            declaration->func_deprecated_message = deprecated_message;
        } else if (is_deprecated && declaration->kind == DECL_VAR) {
            declaration->var_is_deprecated = true;
            declaration->var_deprecated_message = deprecated_message;
        }
    }
    set_cxx_link_name(declaration, ns, false);
    if (declaration->kind == DECL_FUNC && ns) {
        declaration->func_cxx_namespace = cxx_namespace_qualified_name(ns);
        declaration->func_cxx_namespace_scope = ns;
    }
    qualified_name = namespace_qualified_decl_name(
        ns, declaration->name, declaration->loc);
    declaration->name = qualified_name;
    cxx_namespace_add_decl(ns, declaration);
    ast_add_decl(ast, declaration);
}

static void add_namespace_statement(AST* ast, CxxNamespace* ns,
                                     Stmt* statement) {
    if (!statement || statement->kind != STMT_DECL || !statement->decl) {
        return;
    }
    if (statement->decl->kind == DECL_STATIC_ASSERT) {
        /* Assertions have no namespace-owned symbol.  Keep them in the
         * translation-unit stream so sema evaluates them at their source
         * position without attempting to qualify a NULL declaration name. */
        ast_add_decl(ast, statement->decl);
        return;
    }
    add_namespace_declaration(ast, ns, statement->decl);
}

static const char* cxx_using_qualified_name(const char* name,
                                            SourceLoc loc) {
    char buffer[512];
    const char* namespace_name;
    size_t namespace_length;
    CxxNamespace* ns;

    if (!name) return rcc_intern("");
    while (name[0] == ':' && name[1] == ':') name += 2;
    if (active_namespace && active_namespace->name) {
        namespace_name = cxx_namespace_qualified_name(active_namespace);
        namespace_length = namespace_name ? strlen(namespace_name) : 0u;
        if (namespace_length != 0u &&
            namespace_length + 2u + strlen(name) < sizeof(buffer)) {
            memcpy(buffer, namespace_name, namespace_length);
            memcpy(buffer + namespace_length, "::", 2u);
            strcpy(buffer + namespace_length + 2u, name);
            /* Prefer the innermost namespace spelling when it names a real
             * declaration owner; this gives `using detail::f` inside `api`
             * the expected `api::detail::f` target. */
            {
                char* separator = strrchr(buffer, ':');
                if (separator && separator > buffer && separator[-1] == ':') {
                    separator[-1] = '\0';
                    ns = cxx_namespace_find(g_global_namespace, buffer);
                    separator[-1] = ':';
                    if (ns) return rcc_intern(buffer);
                }
            }
        }
    }
    (void)loc;
    return rcc_intern(name);
}

static CxxNamespace* resolve_cxx_namespace_reference(
    CxxNamespace* scope, const char* name) {
    CxxNamespace* candidate;

    if (!name || !*name || !g_global_namespace) return NULL;
    if (name[0] == ':' && name[1] == ':') {
        return cxx_namespace_find(g_global_namespace, name + 2);
    }
    for (candidate = scope; candidate; candidate = candidate->parent) {
        CxxNamespace* target = cxx_namespace_find(candidate, name);
        if (target) return target;
    }
    return NULL;
}

typedef struct {
    CxxNamespace* ns;
    int matches;
    bool alias_found;
} CxxNamespaceDefinitionLookup;

/* Namespace definitions find an existing namespace in the enclosing
 * namespace and its inline-namespace set.  This is deliberately separate
 * from cxx_namespace_lookup(): namespace aliases are lookup results, but a
 * definition cannot extend an alias target. */
static void collect_namespace_definition_lookup(
    CxxNamespace* scope, const char* name,
    CxxNamespaceDefinitionLookup* lookup) {
    if (!scope || !name || !lookup) return;
    for (CxxNamespace* child = scope->children; child; child = child->next) {
        if (child->name && strcmp(child->name, name) == 0) {
            if (!lookup->ns) lookup->ns = child;
            ++lookup->matches;
        }
    }
    for (int index = 0; index < scope->namespace_alias_count; ++index) {
        if (scope->namespace_alias_names[index] &&
            strcmp(scope->namespace_alias_names[index], name) == 0) {
            lookup->alias_found = true;
            ++lookup->matches;
        }
    }
    for (CxxNamespace* child = scope->children; child; child = child->next) {
        if (child->is_inline_namespace) {
            collect_namespace_definition_lookup(child, name, lookup);
        }
    }
}

static CxxNamespace* get_or_create_namespace_definition(
    CxxNamespace* parent, const char* name, SourceLoc loc, bool* created) {
    CxxNamespaceDefinitionLookup lookup = { 0 };
    CxxNamespace* ns;
    if (created) *created = false;
    collect_namespace_definition_lookup(parent, name, &lookup);
    if (lookup.matches == 1 && !lookup.alias_found) return lookup.ns;
    if (lookup.matches > 1) {
        rcc_error(loc,
                  "namespace definition name '%s' is ambiguous in the inline namespace set",
                  name);
        /* Keep error recovery detached so an invalid redeclaration cannot
         * add declarations to one of the existing namespace owners. */
        ns = cxx_namespace_new(name, loc);
        if (created) *created = true;
        return ns;
    }
    if (lookup.alias_found) {
        rcc_error(loc,
                  "namespace definition conflicts with namespace alias '%s'",
                  name);
        ns = cxx_namespace_new(name, loc);
        if (created) *created = true;
        return ns;
    }
    ns = cxx_namespace_new(name, loc);
    if (parent) cxx_namespace_add_namespace(parent, ns);
    if (created) *created = true;
    return ns;
}

static void parse_cxx_using(CxxNamespace* ns) {
    SourceLoc loc = previous()->loc;
    const char* name;
    Type* enum_type;
    Token* local_name;

    if (match(TOK_NAMESPACE)) {
        name = parse_qualified_name();
        CxxNamespace* target = cxx_namespace_find(g_global_namespace, name);
        if (!target) {
            rcc_error(loc, "unknown namespace in using-directive '%s'", name);
        } else {
            cxx_namespace_add_using_namespace(ns, target);
        }
        expect(TOK_SEMICOLON, ";");
        return;
    }

    if (match(TOK_ENUM)) {
        const char* enum_name = parse_qualified_name();
        const char* final_name = enum_name ? strrchr(enum_name, ':') : NULL;
        enum_type = enum_name ? rcc_parser_lookup_type(enum_name) : NULL;
        if (!enum_type && final_name) {
            enum_type = rcc_parser_lookup_type(final_name + 1);
        }
        if (!enum_type || enum_type->kind != TYPE_ENUM) {
            rcc_error(loc,
                      "using enum requires a declared enumeration type '%s'",
                      enum_name ? enum_name : "");
        } else {
            rcc_parser_import_enum_constants(enum_type, loc);
        }
        expect(TOK_SEMICOLON, ";");
        return;
    }

    local_name = expect(TOK_IDENT, "name in using-declaration");
    if (!local_name) {
        while (!at_end() && !match(TOK_SEMICOLON)) advance();
        return;
    }
    if (match(TOK_ASSIGN)) {
        Type* alias_type = parse_cxx_type_spec();
        alias_type = rcc_parser_parse_cxx_declarator(
            alias_type, NULL, NULL);
        if (!alias_type) {
            rcc_error(loc, "using-alias requires a type");
        } else {
            rcc_parser_define_type(local_name->value.str_val, alias_type);
        }
    } else {
        char target[512];
        const char* suffix = NULL;
        /* The first identifier was consumed as the local spelling.  A using
         * declaration has no separate local identifier, so it is the first
         * component of the target and the remaining `::` chain follows. */
        if (strlen(local_name->value.str_val) >= sizeof(target)) {
            rcc_error(loc, "using-declaration name is too long");
            target[0] = '\0';
        } else {
            strcpy(target, local_name->value.str_val);
        }
        while (match(TOK_SCOPE)) {
            if (!check(TOK_IDENT)) {
                rcc_error(peek()->loc, "expected identifier after ::");
                break;
            }
            if (strlen(target) + 2u +
                    strlen(peek()->value.str_val) >= sizeof(target)) {
                rcc_error(loc, "using-declaration name is too long");
                break;
            }
            strcat(target, "::");
            suffix = advance()->value.str_val;
            strcat(target, suffix);
        }
        cxx_namespace_add_using_decl(
            ns, cxx_using_qualified_name(target, loc));
    }
    expect(TOK_SEMICOLON, ";");
}

static void parse_cxx_local_using(void) {
    SourceLoc loc = previous()->loc;
    const char* target;
    const char* final_component;
    CxxNamespace* target_namespace;

    if (match(TOK_NAMESPACE)) {
        target = parse_qualified_name();
        target_namespace = cxx_namespace_find(g_global_namespace, target);
        if (!target_namespace) {
            rcc_error(loc, "unknown namespace in local using-directive '%s'",
                      target ? target : "");
        } else {
            cxx_parser_add_local_using(
                NULL, cxx_namespace_qualified_name(target_namespace), true,
                loc);
        }
        expect(TOK_SEMICOLON, ";");
        return;
    }

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) {
        rcc_error(loc, "expected qualified name in local using-declaration");
        while (!at_end() && !match(TOK_SEMICOLON)) advance();
        return;
    }
    target = parse_qualified_name();
    final_component = target ? strrchr(target, ':') : NULL;
    if (!final_component || final_component == target ||
        final_component[-1] != ':') {
        rcc_error(loc,
                  "local using-declaration requires a qualified target");
    } else {
        ++final_component;
        cxx_parser_add_local_using(
            final_component,
            cxx_using_qualified_name(target, loc), false, loc);
    }
    expect(TOK_SEMICOLON, ";");
}

static CxxNamespace* parse_cxx_namespace(AST* ast, CxxNamespace* parent,
                                         bool is_inline_namespace) {
    SourceLoc loc = previous()->loc;
    CxxNamespace* outer_namespace = active_namespace;

    /* Namespace name (can be anonymous) */
    const char* ns_name = NULL;
    if (check(TOK_IDENT)) {
        ns_name = advance()->value.str_val;
    }

    if (match(TOK_ASSIGN)) {
        const char* target_name = parse_qualified_name();
        CxxNamespace* target = resolve_cxx_namespace_reference(
            parent, target_name);
        if (is_inline_namespace) {
            rcc_error(loc, "inline namespace alias is not valid");
        } else if (!ns_name) {
            rcc_error(loc, "namespace alias requires a name");
        } else if (!target) {
            rcc_error(loc, "unknown namespace alias target '%s'",
                      target_name ? target_name : "");
        } else if (!cxx_namespace_add_alias(parent, ns_name, target)) {
            rcc_error(loc, "namespace alias '%s' conflicts with an existing declaration",
                      ns_name);
        }
        expect(TOK_SEMICOLON, ";");
        return NULL;
    }

    CxxNamespace* ns;
    if (ns_name) {
        /* Namespace definitions with the same name extend one namespace.
         * Keeping a fresh node for every definition splits its declarations
         * from classes declared in an earlier definition, breaking lookup
         * (including the associated namespaces used by ADL). */
        bool created = false;
        ns = get_or_create_namespace_definition(parent, ns_name, loc,
                                                &created);
        if (is_inline_namespace) {
            if (created) {
                ns->is_inline_namespace = true;
            } else if (!ns->is_inline_namespace) {
                rcc_error(loc,
                          "namespace '%s' cannot become inline after its first definition",
                          ns_name);
            }
        }
    } else {
        const char* translation_unit = g_opts.input_file[0]
            ? g_opts.input_file
            : (loc.filename ? loc.filename : "<translation-unit>");
        ns = NULL;
        for (CxxNamespace* child = parent ? parent->children : NULL;
             child; child = child->next) {
            if (child->is_anonymous_namespace &&
                child->anonymous_typeinfo_identity &&
                strcmp(child->anonymous_typeinfo_identity,
                       translation_unit) == 0) {
                ns = child;
                break;
            }
        }
        if (!ns) {
            ns = cxx_namespace_new(NULL, loc);
            ns->is_anonymous_namespace = true;
            ns->anonymous_typeinfo_identity = rcc_intern(translation_unit);
            if (parent) cxx_namespace_add_namespace(parent, ns);
        }
    }

    /* C++17 permits a nested namespace definition to spell the namespace
     * chain in one declaration (`namespace api::v2 { ... }`).  Keep each
     * component as a real namespace node so qualified lookup and Itanium
     * names retain the same structure as the equivalent nested declarations. */
    while (match(TOK_SCOPE)) {
        if (!rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(peek()->loc,
                      "nested namespace definitions require C++17 or newer");
        }
        const char* nested_name;
        CxxNamespace* nested;
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc,
                      "expected namespace identifier after ::");
            break;
        }
        nested_name = advance()->value.str_val;
        nested = get_or_create_namespace_definition(ns, nested_name, loc,
                                                    NULL);
        ns = nested;
    }
    active_namespace = ns;

    expect(TOK_LBRACE, "{");

    /* Parse namespace contents */
    while (!check(TOK_RBRACE) && !at_end()) {
        Token* declaration_start = parser.cur;
        int errors_before = g_error_count;
        skip_cxx_attributes();
        if (cxx_leading_alignas_class_starts()) {
            SourceLoc alignment_loc = peek()->loc;
            int explicit_alignment = 0;
            while (check(TOK__ALIGNAS)) {
                int alignment = rcc_parser_parse_explicit_alignment();
                if (alignment > explicit_alignment) {
                    explicit_alignment = alignment;
                }
            }
            if (match(TOK_CLASS) || match(TOK_STRUCT)) {
                CxxClass* cls = parse_cxx_class();
                cxx_class_apply_explicit_alignment(
                    cls, explicit_alignment, alignment_loc);
                (void)take_cxx_nodiscard();
                if (take_cxx_weak()) {
                    rcc_error(loc, "[[gnu::weak]] requires a file-scope declaration");
                }
                (void)take_cxx_deprecated(NULL);
                if (take_cxx_no_unique_address()) {
                    rcc_error(loc,
                              "[[no_unique_address]] requires a class data member");
                }
                cxx_namespace_add_class(ns, cls);
            }
        } else if (match(TOK_PRAGMA_PACK)) {
            rcc_parser_apply_pragma_pack(previous());
        } else if (match(TOK_CLASS) || match(TOK_STRUCT)) {
            CxxClass* cls = parse_cxx_class();
            (void)take_cxx_nodiscard();
            if (take_cxx_weak()) {
                rcc_error(loc, "[[gnu::weak]] requires a file-scope declaration");
            }
            (void)take_cxx_deprecated(NULL);
            if (take_cxx_no_unique_address()) {
                rcc_error(loc,
                          "[[no_unique_address]] requires a class data member");
            }
            cxx_namespace_add_class(ns, cls);
        } else if (match(TOK_TEMPLATE)) {
            CxxTemplate* tmpl = parse_cxx_template();
            cxx_namespace_add_template(ns, tmpl);
        } else if (check(TOK_INLINE) && check_next(TOK_NAMESPACE)) {
            advance();
            advance();
            (void)parse_cxx_namespace(ast, ns, true);
        } else if (match(TOK_NAMESPACE)) {
            (void)parse_cxx_namespace(ast, ns, false);
        } else if (match(TOK_USING)) {
            parse_cxx_using(ns);
        } else if (check(TOK_EXTERN) && parser.cur->next &&
                   parser.cur->next->type == TOK_STRING_LIT) {
            parse_cxx_language_linkage(ast, ns);
        } else if ((check(TOK_CONSTEXPR) || check(TOK_CONSTEVAL)) &&
                   !cxx_constexpr_starts_function()) {
            Stmt* statement = parse_cxx_statement();
            add_namespace_statement(ast, ns, statement);
        } else if ((check(TOK_AUTO) && parser.cur->next &&
                   parser.cur->next->type == TOK_IDENT &&
                   parser.cur->next->next &&
                   parser.cur->next->next->type == TOK_LPAREN) ||
                   cxx_decltype_auto_starts_function()) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            if (declaration) {
                add_namespace_declaration(ast, ns, declaration);
            }
        } else if (check(TOK_CONSTEXPR) || check(TOK_CONSTEVAL) ||
                   cxx_inline_starts_function()) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            add_namespace_declaration(ast, ns, declaration);
        } else {
            Stmt* statement = parse_cxx_statement();
            add_namespace_statement(ast, ns, statement);
        }
        if (g_error_count > errors_before) {
            while (!at_end() && !check(TOK_SEMICOLON) &&
                   !check(TOK_RBRACE)) {
                advance();
            }
            if (check(TOK_SEMICOLON)) advance();
        } else if (parser.cur == declaration_start && !at_end()) {
            advance();
        }
    }

    expect(TOK_RBRACE, "}");
    active_namespace = outer_namespace;

    return ns;
}

/* A constrained placeholder starts with a named concept and ends with the
 * auto placeholder. Keep the look-ahead structural so a normal named type
 * is still parsed by the ordinary parameter declarator. */
static bool cxx_constrained_auto_parameter_starts(void) {
    Token* token = parser.cur;

    if (token && token->type == TOK_CONST) token = token->next;
    if (!token || (token->type != TOK_IDENT && token->type != TOK_SCOPE)) {
        return false;
    }
    if (token->type == TOK_SCOPE) token = token->next;
    if (!token || token->type != TOK_IDENT) return false;
    while (token->next && token->next->type == TOK_SCOPE) {
        token = token->next->next;
        if (!token || token->type != TOK_IDENT) return false;
    }
    return token->next && token->next->type == TOK_AUTO;
}

/* A constrained type template parameter has the spelling
 * `Concept T` (or `namespace::Concept T`).  Keep this look-ahead purely
 * structural so an ordinary expression or a non-type parameter is not
 * consumed while deciding which template-parameter form is present. */
static bool cxx_constrained_type_parameter_starts(void) {
    Token* token = parser.cur;

    if (!token || (token->type != TOK_IDENT && token->type != TOK_SCOPE)) {
        return false;
    }
    if (token->type == TOK_SCOPE) token = token->next;
    if (!token || token->type != TOK_IDENT) return false;
    while (token->next && token->next->type == TOK_SCOPE) {
        token = token->next->next;
        if (!token || token->type != TOK_IDENT) return false;
    }
    return token->next && (token->next->type == TOK_IDENT ||
                           token->next->type == TOK_ELLIPSIS);
}

static DeclList* parse_cxx_parameter_declarations(void) {
    DeclList* params = NULL;
    int param_idx = 0;

    if (check(TOK_VOID) && check_next(TOK_RPAREN)) {
        advance();
        return NULL;
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        const char* name = NULL;
        Type* type;
        bool parameter_pack = false;
        Expr* default_argument = NULL;
        Decl* parameter;
        if (check(TOK_AUTO) || (check(TOK_CONST) && check_next(TOK_AUTO)) ||
            cxx_constrained_auto_parameter_starts()) {
            SourceLoc loc = peek()->loc;
            bool is_const = match(TOK_CONST);
            bool is_pointer = false;
            bool is_reference = false;
            bool is_rvalue_reference = false;
            const char* concept_name = NULL;
            CxxTemplate* concept = NULL;
            if (!rcc_parser_cxx_standard_at_least(20)) {
                rcc_error(loc,
                          "abbreviated function templates require C++20 or newer");
            }
            if (!check(TOK_AUTO)) {
                concept_name = parse_qualified_name();
                concept = find_concept(concept_name);
                if (!concept) {
                    rcc_error(loc,
                              "constrained abbreviated function parameter "
                              "requires a known named concept");
                } else if (concept->param_count != 1 ||
                           concept->params[0].kind != TPARAM_TYPE ||
                           concept->params[0].is_pack ||
                           concept->params[0].has_default) {
                    rcc_error(loc,
                              "constrained abbreviated function parameter "
                              "requires a single type-parameter concept");
                    concept = NULL;
                }
            }
            expect(TOK_AUTO, "abbreviated function parameter");
            if (match(TOK_STAR)) {
                is_pointer = true;
            } else if (match(TOK_AMP)) {
                is_reference = true;
            } else if (match(TOK_AND)) {
                is_reference = true;
                is_rvalue_reference = true;
            }
            type = parse_cxx_lambda_auto_type(
                active_template,
                active_template ? active_template->param_count : 0,
                is_const, is_pointer, is_reference, is_rvalue_reference, loc);
            if (concept) {
                Type* concept_type = type;
                Expr* concept_argument;
                Expr* concept_call;
                ExprList* concept_arguments = NULL;
                while (concept_type && concept_type->kind == TYPE_PTR) {
                    concept_type = concept_type->base;
                }
                concept_argument = expr_int(0, loc);
                concept_argument->type = concept_type;
                exprlist_append(&concept_arguments, concept_argument);
                concept_call = expr_call(
                    expr_ident(concept_name, loc), concept_arguments, loc);
                concept_call->cxx_concept_template = concept;
                if (active_template->constraint) {
                    active_template->constraint = expr_binary(
                        EXPR_AND, active_template->constraint,
                        concept_call, loc);
                } else {
                    active_template->constraint = concept_call;
                }
            }
            parameter_pack = match(TOK_ELLIPSIS);
            if (check(TOK_IDENT)) {
                name = advance()->value.str_val;
            } else {
                rcc_error(peek()->loc,
                          "abbreviated function parameter requires a name");
            }
        } else {
            type = parse_cxx_type_spec();
            parameter_pack = match(TOK_ELLIPSIS);
            type = rcc_parser_parse_cxx_declarator(type, &name, NULL);
            parameter_pack = parameter_pack ||
                rcc_parser_last_cxx_declarator_was_pack();
        }
        if (match(TOK_ASSIGN)) {
            default_argument = parse_assignment_expression();
        }
        parameter = decl_param(name, type, param_idx++, peek()->loc);
        parameter->param_is_pack = parameter_pack;
        if (parameter_pack &&
            (!active_template ||
             (active_template->kind != TMPL_FUNCTION &&
              !active_template->is_local_class_template))) {
            rcc_error(parameter->loc,
                      "function parameter packs require a function template");
        }
        parameter->param_default = default_argument;
        decllist_append(&params, parameter);
        if (!match(TOK_COMMA)) break;
    }
    return params;
}

static Type* cxx_lambda_function_type(Type* return_type, DeclList* params) {
    TypeParam* type_params = NULL;
    TypeParam** tail = &type_params;
    for (DeclList* item = params; item; item = item->next) {
        TypeParam* parameter = ast_arena_alloc(sizeof(*parameter));
        parameter->name = item->decl ? item->decl->name : NULL;
        parameter->type = item->decl ? item->decl->type : NULL;
        parameter->is_bitfield = false;
        parameter->bit_width = 0u;
        parameter->is_static = false;
        parameter->initializer = item->decl ? item->decl->param_default : NULL;
        parameter->is_deprecated = false;
        parameter->deprecated_message = NULL;
        parameter->next = NULL;
        *tail = parameter;
        tail = &parameter->next;
    }
    return type_func(return_type, type_params, false);
}

/* Generic lambda parameters are function-template type parameters in the
 * closure's call operator.  Keep the placeholder inside the parameter type
 * (including pointer/reference layers) so the ordinary template substitution
 * and target ABI lowering paths can be reused after an invocation supplies a
 * concrete argument. */
static Type* parse_cxx_lambda_auto_type(CxxTemplate* tmpl, int parameter_index,
                                        bool is_const, bool is_pointer,
                                        bool is_reference,
                                        bool is_rvalue_reference,
                                        SourceLoc loc) {
    char name[64];
    int written;
    const char* parameter_name;
    Type* placeholder;

    if (!tmpl) {
        rcc_error(loc, "generic lambda parameter has no template context");
        return type_int;
    }
    written = snprintf(name, sizeof(name), "__rcc_lambda_T%d",
                       parameter_index);
    if (written < 0 || (size_t)written >= sizeof(name)) {
        rcc_error(loc, "generic lambda type parameter name is too long");
        return type_int;
    }
    parameter_name = rcc_intern(name);
    cxx_template_add_type_param(tmpl, parameter_name);
    placeholder = type_struct(parameter_name);
    placeholder->cxx_dependent = true;
    placeholder->is_const = is_const;
    if (is_pointer) placeholder = type_ptr(placeholder);
    if (is_reference)
        placeholder = type_reference(placeholder, is_rvalue_reference);
    return placeholder;
}

static int cxx_lambda_template_type_index(CxxTemplate* tmpl, Type* type) {
    if (!tmpl || !type) return -1;
    if (type->kind == TYPE_PTR) {
        return cxx_lambda_template_type_index(tmpl, type->base);
    }
    if (type->kind == TYPE_ARRAY) {
        if (type->array_bound && type->array_bound->kind == EXPR_IDENT &&
            type->array_bound->ident_name) {
            for (int index = 0; index < tmpl->param_count; ++index) {
                TemplateParam* parameter = &tmpl->params[index];
                if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                    strcmp(parameter->name,
                           type->array_bound->ident_name) == 0) {
                    return index;
                }
            }
        }
        return cxx_lambda_template_type_index(tmpl, type->base);
    }
    if (type->kind != TYPE_STRUCT || !type->tag) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind == TPARAM_TYPE && parameter->name &&
            strcmp(parameter->name, type->tag) == 0) {
            return index;
        }
    }
    return -1;
}

/* C++20 explicit lambda templates share the ordinary function-template
 * substitution path.  Template-template parameters remain outside the
 * bounded lambda ABI, but type and integral non-type parameters use the same
 * deduction and substitution data as ordinary function templates. */
static bool parse_cxx_lambda_template_parameters(CxxTemplate* tmpl) {
    if (!tmpl || !match(TOK_LT)) return false;
    if (!check(TOK_GT)) {
        do {
            bool parameter_pack = false;
            if (match(TOK_TYPENAME) || match(TOK_CLASS)) {
                const char* name = NULL;
                parameter_pack = match(TOK_ELLIPSIS);
                if (check(TOK_IDENT)) {
                    name = advance()->value.str_val;
                } else {
                    rcc_error(peek()->loc,
                              "lambda template type parameter requires a name");
                }
                cxx_template_add_type_param(tmpl, name);
                tmpl->params[tmpl->param_count - 1].is_pack = parameter_pack;
            } else if (match(TOK_AUTO)) {
                const char* name = NULL;
                parameter_pack = match(TOK_ELLIPSIS);
                if (check(TOK_IDENT)) {
                    name = advance()->value.str_val;
                } else {
                    rcc_error(peek()->loc,
                              "lambda non-type template parameter requires a name");
                }
                cxx_template_add_value_param(tmpl, name, type_int);
                tmpl->params[tmpl->param_count - 1].is_pack = parameter_pack;
            } else {
                Type* type = parse_cxx_type_spec();
                const char* name = NULL;
                parameter_pack = match(TOK_ELLIPSIS);
                type = rcc_parser_parse_cxx_declarator(type, &name, NULL);
                if (!name) {
                    rcc_error(peek()->loc,
                              "lambda non-type template parameter requires a name");
                }
                cxx_template_add_value_param(tmpl, name, type);
                tmpl->params[tmpl->param_count - 1].is_pack = parameter_pack;
            }
        } while (match(TOK_COMMA));
    }
    expect(TOK_GT, "lambda template parameter list");
    return true;
}

static DeclList* parse_cxx_lambda_parameters(CxxTemplate* tmpl) {
    DeclList* params = NULL;
    int param_idx = 0;

    if (check(TOK_VOID) && check_next(TOK_RPAREN)) {
        advance();
        return NULL;
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        const char* name = NULL;
        Type* type;
        Expr* default_argument = NULL;
        bool is_auto = check(TOK_AUTO) ||
            (check(TOK_CONST) && check_next(TOK_AUTO));
        bool parameter_pack = false;

        if (is_auto) {
            bool is_const = match(TOK_CONST);
            bool is_pointer = false;
            bool is_reference = false;
            bool is_rvalue_reference = false;
            SourceLoc loc = peek()->loc;
            if (!rcc_parser_cxx_standard_at_least(14)) {
                rcc_error(loc, "generic lambda parameters require C++14 or newer");
            }
            expect(TOK_AUTO, "auto lambda parameter");
            if (match(TOK_STAR)) {
                is_pointer = true;
            } else if (match(TOK_AMP)) {
                is_reference = true;
            } else if (match(TOK_AND)) {
                is_reference = true;
                is_rvalue_reference = true;
            }
            parameter_pack = match(TOK_ELLIPSIS);
            name = expect(TOK_IDENT, "lambda parameter name")
                ? parser.prev->value.str_val : NULL;
            type = parse_cxx_lambda_auto_type(
                tmpl, tmpl ? tmpl->param_count : 0, is_const,
                is_pointer, is_reference, is_rvalue_reference, loc);
        } else {
            type = parse_cxx_type_spec();
            parameter_pack = match(TOK_ELLIPSIS);
            type = rcc_parser_parse_cxx_declarator(type, &name, NULL);
            parameter_pack = parameter_pack ||
                rcc_parser_last_cxx_declarator_was_pack();
            if (parameter_pack) {
                int template_index = cxx_lambda_template_type_index(tmpl, type);
                if (template_index < 0) {
                    rcc_error(peek()->loc,
                              "typed lambda parameter pack requires a template type parameter");
                } else {
                    tmpl->params[template_index].is_pack = true;
                }
            }
        }
        if (match(TOK_ASSIGN)) default_argument = parse_assignment_expression();
        {
            Decl* parameter = decl_param(name, type, param_idx++, peek()->loc);
            parameter->param_is_pack = parameter_pack;
            parameter->param_default = default_argument;
            if (parameter_pack && tmpl) {
                int template_index = cxx_lambda_template_type_index(tmpl, type);
                if (template_index >= 0) {
                    tmpl->params[template_index].is_pack = true;
                }
            }
            decllist_append(&params, parameter);
        }
        if (!match(TOK_COMMA)) break;
    }
    return params;
}

/* Lower an immediately-invoked lambda to a real internal function
 * declaration. Captures are explicit leading parameters, so this lowering
 * never drops captured state. */
Expr* rcc_parse_cxx_lambda(void) {
    SourceLoc loc = peek()->loc;
    DeclList* capture_params = NULL;
    DeclList* params = NULL;
    DeclList* all_params = NULL;
    ExprList* captures = NULL;
    StmtList* statements = NULL;
    Type* return_type = NULL;
    Stmt* body;
    Decl* function;
    char name[64];
    int written;
    int capture_count = 0;
    bool lambda_mutable = false;
    bool lambda_constexpr = false;
    bool lambda_consteval = false;
    CxxReferenceCapture* lambda_reference_captures = NULL;
    CxxLambdaCaptureSpec* explicit_captures = NULL;
    CxxLambdaCaptureSpec* explicit_capture_tail = NULL;
    bool default_capture = false;
    bool default_reference = false;
    bool explicit_template_parameters = false;
    CxxTemplate* lambda_outer_template = active_template;
    CxxTemplate* lambda_template = cxx_template_new(loc);

    expect(TOK_LBRACKET, "[");
    if ((check(TOK_ASSIGN) || check(TOK_AMP)) &&
        (check_next(TOK_RBRACKET) || check_next(TOK_COMMA))) {
        bool reference_default = check(TOK_AMP);
        advance();
        default_capture = true;
        default_reference = reference_default;
    }
    if (default_capture && !check(TOK_RBRACKET)) {
        expect(TOK_COMMA, "',' after lambda default capture");
    }
    if (!match(TOK_RBRACKET)) {
        do {
            Token* capture;
            bool reference_capture = match(TOK_AMP) || match(TOK_AND);
            if (check(TOK_THIS)) {
                capture = advance();
                if (reference_capture) {
                    rcc_error(capture->loc,
                              "lambda cannot capture this by reference");
                    reference_capture = false;
                }
            } else {
                capture = expect(TOK_IDENT, "lambda capture name");
            }
            if (!capture) break;
            {
                CxxLambdaCaptureSpec* spec =
                    ast_arena_alloc(sizeof(*spec));
                spec->name = capture->type == TOK_THIS
                    ? rcc_intern("this") : capture->value.str_val;
                spec->loc = capture->loc;
                spec->reference = reference_capture;
                spec->initializer = NULL;
                if (match(TOK_ASSIGN)) {
                    if (!rcc_parser_cxx_standard_at_least(14)) {
                        rcc_error(capture->loc,
                                  "lambda init-captures require C++14 or newer");
                    }
                    if (reference_capture || capture->type == TOK_THIS) {
                        rcc_error(capture->loc,
                                  "lambda init-capture cannot initialize a reference or this capture");
                    }
                    spec->initializer = parse_cxx_expression();
                    if (!spec->initializer) {
                        rcc_error(capture->loc,
                                  "lambda init-capture requires an initializer expression");
                    }
                }
                spec->next = NULL;
                if (explicit_capture_tail) {
                    explicit_capture_tail->next = spec;
                } else {
                    explicit_captures = spec;
                }
                explicit_capture_tail = spec;
            }
        } while (match(TOK_COMMA));
        expect(TOK_RBRACKET, "]");
    }
    for (CxxLambdaCaptureSpec* spec = explicit_captures; spec;
         spec = spec->next) {
        Type* capture_type = spec->initializer
            ? cxx_lambda_capture_expression_type(spec->initializer)
            : cxx_parser_value_type(spec->name);
        if (spec->initializer && !capture_type) {
            rcc_error(spec->loc,
                      "lambda init-capture expression has no statically inferable type");
            continue;
        }
        if (!capture_type) {
            rcc_error(spec->loc, "lambda capture '%s' is not a local value",
                      spec->name);
            continue;
        }
        if (spec->initializer) {
            exprlist_append(&captures, spec->initializer);
        } else if (spec->reference) {
            CxxReferenceCapture* reference =
                ast_arena_alloc(sizeof(*reference));
            reference->name = spec->name;
            reference->next = lambda_reference_captures;
            lambda_reference_captures = reference;
            capture_type = type_ptr(capture_type);
            exprlist_append(&captures, expr_unary(
                EXPR_ADDR, expr_ident(spec->name, spec->loc), spec->loc));
        } else {
            exprlist_append(&captures,
                            expr_ident(spec->name, spec->loc));
        }
        decllist_append(&capture_params,
                        decl_param(spec->name, capture_type,
                                   capture_count++, spec->loc));
    }
    if (default_capture) {
        for (CxxParserValueBinding* binding = active_value_bindings;
             binding; binding = binding->next) {
            bool explicitly_captured = false;
            for (CxxLambdaCaptureSpec* spec = explicit_captures; spec;
                 spec = spec->next) {
                if (strcmp(spec->name, binding->name) == 0) {
                    explicitly_captured = true;
                    break;
                }
            }
            if (explicitly_captured) continue;
            Type* capture_type = binding->type;
            if (!capture_type) {
                rcc_error(loc, "lambda capture has no semantic type");
                continue;
            }
            if (default_reference && strcmp(binding->name, "this") != 0) {
                CxxReferenceCapture* reference =
                    ast_arena_alloc(sizeof(*reference));
                reference->name = binding->name;
                reference->next = lambda_reference_captures;
                lambda_reference_captures = reference;
                capture_type = type_ptr(capture_type);
                exprlist_append(&captures, expr_unary(
                    EXPR_ADDR, expr_ident(binding->name, loc), loc));
            } else {
                exprlist_append(&captures,
                                expr_ident(binding->name, loc));
            }
            decllist_append(&capture_params,
                            decl_param(binding->name, capture_type,
                                       capture_count++, loc));
        }
    }
    if (check(TOK_LT)) {
        if (!rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(peek()->loc,
                      "lambda template parameters require C++20 or newer");
        }
        active_template = lambda_template;
        explicit_template_parameters =
            parse_cxx_lambda_template_parameters(lambda_template);
    }
    if (match(TOK_LPAREN)) {
        params = parse_cxx_lambda_parameters(lambda_template);
        expect(TOK_RPAREN, ")");
    }
    for (DeclList* item = capture_params; item; item = item->next) {
        decllist_append(&all_params, item->decl);
    }
    for (DeclList* item = params; item; item = item->next) {
        if (item->decl) {
            item->decl->param_index =
                capture_count + item->decl->param_index;
        }
        decllist_append(&all_params, item->decl);
    }
    lambda_mutable = match(TOK_MUTABLE);
    if (match(TOK_CONSTEXPR)) {
        if (!rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(previous()->loc,
                      "constexpr lambda specifiers require C++17 or newer");
        }
        lambda_constexpr = true;
    } else if (match(TOK_CONSTEVAL)) {
        if (!rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(previous()->loc,
                      "consteval lambda specifiers require C++20 or newer");
        }
        lambda_constexpr = true;
        lambda_consteval = true;
    }
    if (!lambda_mutable) {
        /* A non-mutable lambda has a const call operator.  Model each
         * by-value capture as a top-level const parameter so assignments to
         * the captured object are diagnosed while pointer/reference captures
         * retain their standard pointee mutability.  `this` is a pointer to
         * the original object and is likewise intentionally not qualified. */
        for (DeclList* item = capture_params; item; item = item->next) {
            Type* capture_type = item->decl ? item->decl->type : NULL;
            if (!item->decl || !capture_type ||
                (item->decl->name &&
                 strcmp(item->decl->name, "this") == 0) ||
                capture_type->is_reference || capture_type->is_const) {
                continue;
            }
            capture_type = ast_arena_alloc(sizeof(*capture_type));
            *capture_type = *item->decl->type;
            capture_type->is_const = true;
            item->decl->type = capture_type;
        }
    }
    if (match(TOK_NOEXCEPT)) {
        if (check(TOK_LPAREN)) skip_balanced(TOK_LPAREN, TOK_RPAREN);
    }
    if (match(TOK_ARROW)) {
        return_type = parse_cxx_type_spec();
        return_type = rcc_parser_parse_cxx_declarator(
            return_type, NULL, NULL);
    }
    expect(TOK_LBRACE, "{");
    if (saved_reference_capture_depth >=
        (int)(sizeof(saved_reference_captures) /
              sizeof(saved_reference_captures[0]))) {
        rcc_fatal("C++ lambda nesting is too deep");
    }
    saved_reference_captures[saved_reference_capture_depth++] =
        active_reference_captures;
    active_reference_captures = lambda_reference_captures;
    void* enum_scope = rcc_parser_enum_scope_mark();
    rcc_parser_cxx_begin_function_parameters(all_params);
    rcc_parser_function_scope_push("operator()");
    while (!check(TOK_RBRACE) && !at_end()) {
        Token* start = parser.cur;
        Stmt* statement = parse_cxx_statement();
        if (statement) stmtlist_append(&statements, statement);
        if (parser.cur == start && !at_end()) advance();
    }
    expect(TOK_RBRACE, "}");
    rcc_parser_function_scope_pop();
    rcc_parser_enum_scope_restore(enum_scope);
    rcc_parser_cxx_end_function_parameters();
    active_reference_captures = saved_reference_captures[
        --saved_reference_capture_depth];
    if (explicit_template_parameters) {
        active_template = lambda_outer_template;
    }
    body = stmt_block(statements, loc);
    written = snprintf(name, sizeof(name), "__rcc_lambda_%u",
                       ++cxx_lambda_counter);
    if (written < 0 || (size_t)written >= sizeof(name)) {
        rcc_fatal("C++ lambda symbol name exceeds compiler limits");
    }
    function = decl_func(rcc_intern(name),
                         cxx_lambda_function_type(
                             return_type ? return_type : type_int,
                             all_params),
                         all_params, body, loc);
    function->storage = STORAGE_STATIC;
    function->func_is_inline = true;
    function->func_is_constexpr = lambda_constexpr;
    function->func_is_consteval = lambda_consteval;
    /* An omitted lambda trailing return type follows the ordinary C++
     * placeholder-return rules.  Keep a concrete type in the pre-sema
     * function signature so parsing and call construction remain well typed,
     * then let sema deduce the exact return type from every return statement.
     * This is important for `double`, pointer, aggregate, and void lambdas;
     * treating every non-empty lambda as `int` silently changed the ABI and
     * truncated otherwise valid results. */
    function->func_is_auto_return = return_type == NULL;
    function->link_name = function->name;
    if (lambda_template->param_count > 0) {
        lambda_template->kind = TMPL_FUNCTION;
        lambda_template->name = function->name;
        lambda_template->func_def = function;
        lambda_template->ns = active_namespace;
    } else if (active_ast) {
        ast_add_decl(active_ast, function);
    }
    {
        Expr* result = expr_ident(function->name, loc);
        result->ident_decl = function;
        result->type = function->type;
        result->cxx_lambda_captures = captures;
        result->cxx_lambda_template = lambda_template->param_count > 0
            ? lambda_template : NULL;
        return result;
    }
}

/* Header-only SDK functions are emitted eagerly today.  Parse only bodies
 * made from the common C/C++ expression subset; template definitions and
 * exception/allocation constructs stay deferred instead of being assigned a
 * guessed meaning.  Local auto declarations are accepted because their
 * initializer must provide a concrete type before parsing can continue. */
static bool inline_body_is_lowerable(void) {
    Token* cursor = parser.cur;
    int depth = 0;
    if (!cursor || cursor->type != TOK_LBRACE) return false;
    do {
        switch (cursor->type) {
            case TOK_TEMPLATE:
            case TOK_TRY:
            case TOK_THROW:
            case TOK_NEW:
            case TOK_DELETE:
                return false;
            case TOK_LBRACE:
                ++depth;
                break;
            case TOK_RBRACE:
                --depth;
                break;
            default:
                break;
        }
        cursor = cursor->next;
    } while (cursor && depth > 0);
    return depth == 0;
}

static Decl* parse_cxx_function_declaration(bool parse_body,
                                            bool* is_constexpr,
                                            bool* is_noexcept,
                                            bool* is_consteval) {
    SourceLoc loc;
    Type* return_type;
    const char* function_name;
    DeclList* params;
    Stmt* body = NULL;
    bool is_inline = false;
    bool is_auto_return = false;
    bool is_decltype_auto_return = false;
    Expr* noexcept_expr = NULL;
    void* friend_signature_context = NULL;
    CxxFriendAccess* saved_friend_access_context =
        active_friend_access_context;

    *is_constexpr = false;
    *is_noexcept = false;
    *is_consteval = false;
    skip_cxx_attributes();
    bool is_nodiscard = take_cxx_nodiscard();
    bool is_weak = take_cxx_weak();
    const char* deprecated_message = NULL;
    bool is_deprecated = take_cxx_deprecated(&deprecated_message);
    bool is_no_unique_address = take_cxx_no_unique_address();
    if (is_no_unique_address) {
        rcc_error(peek()->loc,
                  "[[no_unique_address]] requires a data member, not a function");
    }
    loc = peek()->loc;
    friend_signature_context =
        cxx_begin_friend_function_signature_access(!active_class);
    for (;;) {
        if (match(TOK_CONSTEXPR)) *is_constexpr = true;
        else if (match(TOK_CONSTEVAL)) {
            *is_constexpr = true;
            *is_consteval = true;
        }
        else if (match(TOK_INLINE) || match(TOK___INLINE__)) is_inline = true;
        else break;
    }
    if (match(TOK_AUTO)) {
        is_auto_return = true;
        /* The final return type is resolved after the body has been parsed
         * and its expressions have entered the function scope. */
        return_type = type_int;
    } else if (check(TOK_DECLTYPE) && parser.cur->next &&
               parser.cur->next->type == TOK_LPAREN &&
               parser.cur->next->next &&
               parser.cur->next->next->type == TOK_AUTO &&
               parser.cur->next->next->next &&
               parser.cur->next->next->next->type == TOK_RPAREN) {
        advance();
        advance();
        advance();
        advance();
        is_auto_return = true;
        is_decltype_auto_return = true;
        /* decltype(auto) is deduced after the body has been semantically
         * analyzed.  The placeholder is never emitted as a real type. */
        return_type = type_int;
    } else {
        return_type = parse_cxx_type_spec();
    }
    if (match(TOK_OPERATOR)) {
        function_name = parse_operator_name();
    } else {
        Token* name = expect(TOK_IDENT, "function name");
        if (!name) {
            cxx_end_friend_function_signature_access(
                friend_signature_context, NULL, NULL, NULL, NULL, loc,
                *is_consteval, noexcept_expr);
            return NULL;
        }
        function_name = name->value.str_val;
    }
    expect(TOK_LPAREN, "(");
    params = parse_cxx_parameter_declarations();
    expect(TOK_RPAREN, ")");
    if (is_auto_return && match(TOK_ARROW)) {
        return_type = parse_cxx_type_spec();
        is_auto_return = false;
        is_decltype_auto_return = false;
    }
    if (match(TOK_NOEXCEPT)) {
        if (check(TOK_LPAREN)) {
            advance();
            noexcept_expr = parse_expression();
            expect(TOK_RPAREN, ")");
        } else {
            *is_noexcept = true;
        }
    }
    {
        Expr* trailing_constraint = parse_cxx_trailing_requires_clause(loc);
        if (trailing_constraint) {
            if (active_template->constraint) {
                active_template->constraint = expr_binary(
                    EXPR_AND, active_template->constraint,
                    trailing_constraint, trailing_constraint->loc);
            } else {
                active_template->constraint = trailing_constraint;
            }
        }
    }
    cxx_end_friend_function_signature_access(
        friend_signature_context, function_name, return_type, params, NULL,
        loc, *is_consteval, noexcept_expr);
    /* Emit only the verified non-dependent header subset.  Incomplete class
     * and template bodies remain deferred until their object model exists. */
    if (!parse_body && is_inline && type_is_complete(return_type) &&
        check(TOK_LBRACE) && inline_body_is_lowerable()) {
        parse_body = true;
    }
    if (parse_body && check(TOK_LBRACE)) {
        active_friend_access_context = cxx_find_friend_function_access(
            function_name, return_type, params,
            active_friend_access_context, *is_consteval, noexcept_expr);
        if (active_template && active_template->kind == TMPL_FUNCTION) {
            active_friend_access_context = cxx_merge_friend_access(
                active_friend_access_context, active_template->friend_access);
        }
    }
    if (!parse_body && check(TOK_LBRACE)) {
        skip_balanced(TOK_LBRACE, TOK_RBRACE);
    } else if (match(TOK_LBRACE)) {
        StmtList* statements = NULL;
        void* enum_scope = rcc_parser_enum_scope_mark();
        rcc_parser_cxx_begin_function_parameters(params);
        rcc_parser_function_scope_push(function_name);
        while (!check(TOK_RBRACE) && !at_end()) {
            Token* start = parser.cur;
            Stmt* statement = parse_cxx_statement();
            if (statement) stmtlist_append(&statements, statement);
            if (parser.cur == start && !at_end()) advance();
        }
        expect(TOK_RBRACE, "}");
        rcc_parser_function_scope_pop();
        rcc_parser_enum_scope_restore(enum_scope);
        rcc_parser_cxx_end_function_parameters();
        body = stmt_block(statements, loc);
    } else {
        expect(TOK_SEMICOLON, ";");
    }
    active_friend_access_context = saved_friend_access_context;

    CxxMethod* function = cxx_method_new(function_name, return_type,
                                         params, body, loc);
    for (DeclList* parameter = params; parameter; parameter = parameter->next) {
        if (parameter->decl && parameter->decl->param_is_pack) {
            function->decl->type->variadic = true;
            break;
        }
    }
    function->decl->func_is_inline = is_inline;
    function->decl->func_is_constexpr = *is_constexpr;
    function->decl->func_is_consteval = *is_consteval;
    function->decl->func_is_nodiscard = is_nodiscard;
    function->decl->is_weak = is_weak;
    function->decl->func_is_deprecated = is_deprecated;
    function->decl->func_deprecated_message = deprecated_message;
    function->decl->func_is_noexcept = *is_noexcept;
    function->decl->func_noexcept_expr = noexcept_expr;
    function->decl->func_is_auto_return = is_auto_return;
    function->decl->func_is_decltype_auto_return = is_decltype_auto_return;
    return function->decl;
}

/* ═══════════════════════════════════════
 * C++ Template Parsing
 * ═══════════════════════════════════════ */

static bool template_type_parameter_matches(CxxTemplate* tmpl, Type* type,
                                            int parameter_index) {
    TemplateParam* parameter;
    if (!tmpl || !type || type->kind != TYPE_STRUCT || !type->tag ||
        parameter_index < 0 || parameter_index >= tmpl->param_count) {
        return false;
    }
    parameter = &tmpl->params[parameter_index];
    return parameter->kind == TPARAM_TYPE && parameter->name &&
           strcmp(parameter->name, type->tag) == 0;
}

static bool expression_is_identifier(Expr* expression, const char* name) {
    return expression && expression->kind == EXPR_IDENT && name &&
           expression->ident_name &&
           strcmp(expression->ident_name, name) == 0;
}

static bool expression_is_member_of(Expr* expression, const char* object,
                                    const char* member) {
    return expression && expression->kind == EXPR_MEMBER && member &&
           expression->member_name &&
           strcmp(expression->member_name, member) == 0 &&
           expression_is_identifier(expression->member_base, object);
}

static bool expression_is_template_sizeof(CxxTemplate* tmpl,
                                           Expr* expression,
                                           int parameter_index) {
    TemplateParam* parameter;
    if (!tmpl || !expression || expression->kind != EXPR_SIZEOF ||
        parameter_index < 0 || parameter_index >= tmpl->param_count) {
        return false;
    }
    parameter = &tmpl->params[parameter_index];
    if (expression->sizeof_type) {
        return template_type_parameter_matches(tmpl, expression->sizeof_type,
                                               parameter_index);
    }
    return parameter->name &&
           expression_is_identifier(expression->unary_operand,
                                    parameter->name);
}

/* Recognize only the ABI initializer idiom used by RinSDK.  Keeping this
 * structural, rather than interpreting arbitrary function-template bodies,
 * makes an accepted specialization equivalent to a designated value
 * initializer in the common C AST:
 *
 *   T value{};
 *   value.struct_size = sizeof(T);
 *   value.version = <integer constant>;
 *   return value;
 */
static void recognize_versioned_function_template(CxxTemplate* tmpl) {
    Decl* function;
    StmtList* statements;
    Stmt* declaration;
    Stmt* size_assignment;
    Stmt* version_assignment;
    Stmt* result;
    const char* variable;
    int64_t version;

    if (!tmpl || tmpl->kind != TMPL_FUNCTION || tmpl->param_count != 1 ||
        tmpl->params[0].kind != TPARAM_TYPE) {
        return;
    }
    function = tmpl->func_def;
    if (!function || function->kind != DECL_FUNC || !function->type ||
        function->type->kind != TYPE_FUNC || function->func_params ||
        !template_type_parameter_matches(tmpl, function->type->ret_type, 0) ||
        !function->func_body || function->func_body->kind != STMT_BLOCK) {
        return;
    }
    statements = function->func_body->block_stmts;
    if (!statements || !statements->next || !statements->next->next ||
        !statements->next->next->next ||
        statements->next->next->next->next) {
        return;
    }
    declaration = statements->stmt;
    size_assignment = statements->next->stmt;
    version_assignment = statements->next->next->stmt;
    result = statements->next->next->next->stmt;
    if (!declaration || declaration->kind != STMT_DECL ||
        !declaration->decl || declaration->decl->kind != DECL_VAR ||
        !declaration->decl->name ||
        !template_type_parameter_matches(tmpl, declaration->decl->type, 0) ||
        !declaration->decl->var_init ||
        declaration->decl->var_init->kind != EXPR_COMPOUND ||
        !declaration->decl->var_init->compound_value_init ||
        declaration->decl->var_init->compound_init) {
        return;
    }
    variable = declaration->decl->name;
    if (!size_assignment || size_assignment->kind != STMT_EXPR ||
        !size_assignment->expr || size_assignment->expr->kind != EXPR_ASSIGN ||
        !expression_is_member_of(size_assignment->expr->binary_lhs, variable,
                                 "struct_size") ||
        !expression_is_template_sizeof(tmpl,
                                       size_assignment->expr->binary_rhs, 0)) {
        return;
    }
    if (!version_assignment || version_assignment->kind != STMT_EXPR ||
        !version_assignment->expr ||
        version_assignment->expr->kind != EXPR_ASSIGN ||
        !expression_is_member_of(version_assignment->expr->binary_lhs,
                                 variable, "version") ||
        !expr_eval_integer_constant(version_assignment->expr->binary_rhs,
                                    &version) ||
        version < 0 || (uint64_t)version > UINT32_MAX) {
        return;
    }
    if (!result || result->kind != STMT_RETURN ||
        !expression_is_identifier(result->return_val, variable)) {
        return;
    }
    tmpl->function_lowering = TMPL_FUNCTION_VERSIONED_STRUCT;
    tmpl->function_constant = version;
}

static CxxTemplate* cxx_template_snapshot_parameter_scope(
    const CxxTemplate* tmpl) {
    CxxTemplate* snapshot;
    if (!tmpl) return NULL;
    snapshot = ast_arena_alloc(sizeof(*snapshot));
    *snapshot = *tmpl;
    if (tmpl->param_count > 0) {
        snapshot->params = ast_arena_alloc(
            sizeof(*snapshot->params) * (size_t)tmpl->param_count);
        memcpy(snapshot->params, tmpl->params,
               sizeof(*snapshot->params) * (size_t)tmpl->param_count);
    } else {
        snapshot->params = NULL;
    }
    return snapshot;
}

static bool cxx_template_declaration_is_alias(void) {
    Token* token = parser.cur;
    int angle_depth = 0;
    int parentheses = 0;
    int brackets = 0;
    bool has_requires_clause = false;
    int requires_braces = 0;

    if (!token || token->type != TOK_LT) return false;
    for (; token && token->type != TOK_EOF; token = token->next) {
        if (token->type == TOK_LPAREN) {
            ++parentheses;
        } else if (token->type == TOK_RPAREN && parentheses > 0) {
            --parentheses;
        } else if (token->type == TOK_LBRACKET) {
            ++brackets;
        } else if (token->type == TOK_RBRACKET && brackets > 0) {
            --brackets;
        } else if (!parentheses && !brackets && token->type == TOK_LT) {
            ++angle_depth;
        } else if (!parentheses && !brackets && token->type == TOK_GT) {
            if (--angle_depth == 0) {
                token = token->next;
                break;
            }
        } else if (!parentheses && !brackets &&
                   token->type == TOK_RSHIFT) {
            angle_depth -= 2;
            if (angle_depth <= 0) {
                token = token->next;
                break;
            }
        }
    }
    for (; token && token->type != TOK_EOF; token = token->next) {
        if (token->type == TOK_REQUIRES) {
            has_requires_clause = true;
        } else if (token->type == TOK_USING && !parentheses && !brackets &&
                   requires_braces == 0) {
            return true;
        } else if (token->type == TOK_SEMICOLON && !parentheses &&
                   !brackets && requires_braces == 0) {
            return false;
        } else if ((token->type == TOK_CLASS || token->type == TOK_STRUCT ||
                    token->type == TOK_ENUM) && !parentheses && !brackets &&
                   requires_braces == 0) {
            return false;
        } else if (token->type == TOK_LPAREN) {
            ++parentheses;
        } else if (token->type == TOK_RPAREN && parentheses > 0) {
            --parentheses;
        } else if (token->type == TOK_LBRACKET) {
            ++brackets;
        } else if (token->type == TOK_RBRACKET && brackets > 0) {
            --brackets;
        } else if (token->type == TOK_LBRACE && has_requires_clause) {
            ++requires_braces;
        } else if (token->type == TOK_RBRACE && requires_braces > 0) {
            --requires_braces;
        } else if (token->type == TOK_LBRACE && !has_requires_clause) {
            return false;
        }
    }
    return false;
}

CxxTemplate* parse_cxx_template(void) {
    SourceLoc loc = previous()->loc;
    CxxTemplate* parameter_outer_template = active_template;
    int explicit_class_alignment = 0;
    bool is_alias_template = active_class && parameter_outer_template &&
        parameter_outer_template->kind == TMPL_CLASS &&
        cxx_template_declaration_is_alias();

    expect(TOK_LT, "<");

    CxxTemplate* tmpl = cxx_template_new(loc);
    if (is_alias_template) {
        tmpl->enclosing_template = parameter_outer_template;
    }
    active_template = tmpl;

    /* Parse template parameters */
    if (!check(TOK_GT)) {
        do {
            bool type_parameter = false;
            bool parameter_pack = false;
            bool template_parameter = false;
            if (cxx_constrained_type_parameter_starts()) {
                type_parameter = true;
                const char* concept_name = parse_qualified_name();
                CxxTemplate* concept = find_concept(concept_name);
                const char* param_name = NULL;
                Type* dependent_type;
                Expr* concept_argument;
                Expr* concept_call;
                ExprList* concept_arguments = NULL;
                if (!rcc_parser_cxx_standard_at_least(20)) {
                    rcc_error(peek()->loc,
                              "constrained type template parameters require "
                              "C++20 or newer");
                }
                parameter_pack = match(TOK_ELLIPSIS);
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                } else {
                    rcc_error(peek()->loc,
                              "constrained type template parameter requires a "
                              "name");
                }
                cxx_template_add_type_param(tmpl, param_name);
                dependent_type = type_struct(param_name ? param_name : "<type>");
                dependent_type->cxx_dependent = true;
                dependent_type->cxx_template_param_index = tmpl->param_count - 1;
                concept_argument = expr_ident(
                    param_name ? param_name : "<type>", loc);
                concept_argument->type = dependent_type;
                exprlist_append(&concept_arguments, concept_argument);
                if (!concept) {
                    rcc_error(loc,
                              "constrained type template parameter requires a "
                              "known named concept");
                } else if (concept->param_count != 1 ||
                           concept->params[0].kind != TPARAM_TYPE ||
                           concept->params[0].is_pack ||
                           concept->params[0].has_default) {
                    rcc_error(loc,
                              "constrained type template parameter requires a "
                              "single type-parameter concept");
                } else {
                    concept_call = expr_call(
                        expr_ident(concept_name, loc), concept_arguments, loc);
                    concept_call->cxx_concept_template = concept;
                    if (tmpl->constraint) {
                        tmpl->constraint = expr_binary(
                            EXPR_AND, tmpl->constraint, concept_call, loc);
                    } else {
                        tmpl->constraint = concept_call;
                    }
                }
            } else if (match(TOK_TYPENAME) || match(TOK_CLASS)) {
                /* Type parameter */
                type_parameter = true;
                parameter_pack = match(TOK_ELLIPSIS);
                const char* param_name = NULL;
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                }
                cxx_template_add_type_param(tmpl, param_name);
                tmpl->params[tmpl->param_count - 1].is_pack = parameter_pack;
            } else if (match(TOK_TEMPLATE)) {
                /* Keep the accepted template-template profile explicit.  The
                 * nested signature is retained so an argument cannot be
                 * accepted merely because it happens to name a class. */
                CxxTemplate* signature = cxx_template_new(peek()->loc);
                const char* param_name = NULL;
                expect(TOK_LT, "template parameter list");
                if (!check(TOK_GT)) {
                    do {
                        const char* nested_name = NULL;
                        bool nested_parameter_pack = false;
                        if (match(TOK_TYPENAME) || match(TOK_CLASS)) {
                            nested_parameter_pack = match(TOK_ELLIPSIS);
                            if (check(TOK_IDENT)) {
                                nested_name = advance()->value.str_val;
                            }
                            cxx_template_add_type_param(signature, nested_name);
                            signature->params[signature->param_count - 1].is_pack =
                                nested_parameter_pack;
                        } else if (match(TOK_AUTO)) {
                            if (!rcc_parser_cxx_standard_at_least(17)) {
                                rcc_error(peek()->loc,
                                          "template<auto> parameters require C++17 or newer");
                            }
                            nested_parameter_pack = match(TOK_ELLIPSIS);
                            if (check(TOK_IDENT)) {
                                nested_name = advance()->value.str_val;
                            } else {
                                rcc_error(peek()->loc,
                                          "template-template auto parameter "
                                          "requires a name");
                            }
                            cxx_template_add_value_param(signature, nested_name,
                                                         type_int);
                            signature->params[signature->param_count - 1].is_pack =
                                nested_parameter_pack;
                        } else {
                            Type* nested_type = parse_cxx_type_spec();
                            nested_parameter_pack = match(TOK_ELLIPSIS);
                            if (!nested_type || !type_is_integer(nested_type)) {
                                rcc_error(peek()->loc,
                                          "template-template non-type "
                                          "parameter requires an integral type");
                                nested_type = type_int;
                            }
                            if (check(TOK_IDENT)) {
                                nested_name = advance()->value.str_val;
                            }
                            cxx_template_add_value_param(signature, nested_name,
                                                         nested_type);
                            signature->params[signature->param_count - 1].is_pack =
                                nested_parameter_pack;
                        }
                    } while (match(TOK_COMMA));
                }
                expect(TOK_GT, "template parameter list");
                if (!match(TOK_CLASS) && !match(TOK_TYPENAME)) {
                    rcc_error(peek()->loc,
                              "template-template parameter requires class "
                              "or typename");
                }
                if (check(TOK_IDENT)) param_name = advance()->value.str_val;
                tmpl->params = ast_arena_grow(
                    tmpl->params, sizeof(TemplateParam) * (size_t)tmpl->param_count,
                    sizeof(TemplateParam) * (size_t)(tmpl->param_count + 1));
                {
                    TemplateParam* parameter =
                        &tmpl->params[tmpl->param_count++];
                    memset(parameter, 0, sizeof(*parameter));
                    parameter->kind = TPARAM_TEMPLATE;
                    parameter->name = param_name ? rcc_strdup(param_name) : NULL;
                    parameter->template_signature = signature;
                }
                template_parameter = true;
            } else if (match(TOK_AUTO)) {
                /* C++17 `template<auto N>` is represented by the existing
                 * integral non-type path.  The bounded RinOS profile accepts
                 * only values that evaluate as target-independent integers;
                 * keeping the parameter's ABI type as int preserves the
                 * existing substitution, constraint, and mangling rules. */
                const char* param_name = NULL;
                if (!rcc_parser_cxx_standard_at_least(17)) {
                    rcc_error(peek()->loc,
                              "template<auto> parameters require C++17 or newer");
                }
                parameter_pack = match(TOK_ELLIPSIS);
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                } else {
                    rcc_error(peek()->loc,
                              "auto non-type template parameter requires a name");
                }
                cxx_template_add_value_param(tmpl, param_name, type_int);
                parameter_pack = parameter_pack || match(TOK_ELLIPSIS);
            } else {
                /* Non-type parameter */
                Type* param_type = parse_cxx_type_spec();
                const char* param_name = NULL;
                parameter_pack = match(TOK_ELLIPSIS);
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                }
                cxx_template_add_value_param(tmpl, param_name, param_type);
            }

            if (parameter_pack) {
                tmpl->params[tmpl->param_count - 1].is_pack = true;
            }

            /* Default value? */
            if (match(TOK_ASSIGN)) {
                int parameter_index = tmpl->param_count - 1;
                if (parameter_index >= 0) {
                    if (template_parameter) {
                        tmpl->params[parameter_index].has_default = true;
                        tmpl->params[parameter_index].default_type =
                            parse_template_template_default(peek()->loc);
                    } else {
                        tmpl->params[parameter_index].has_default = true;
                    }
                    if (type_parameter) {
                        tmpl->params[parameter_index].default_type =
                            parse_cxx_type_spec();
                    } else if (!template_parameter) {
                        rcc_parser_set_cxx_template_default_mode(true);
                        tmpl->params[parameter_index].default_value =
                            parse_assignment_expression();
                        tmpl->params[parameter_index].default_context =
                            cxx_template_snapshot_parameter_scope(tmpl);
                        rcc_parser_set_cxx_template_default_mode(false);
                    }
                }
            }
        } while (match(TOK_COMMA));
    }

    expect(TOK_GT, ">");

    /* Keep named concepts bounded, but preserve real type parameters so a
     * requires-expression can be substituted and semantically checked.  Packs
     * and defaults remain outside this compiler's finite template ABI. */
    if (match(TOK_CONCEPT)) {
        Token* concept_name = expect(TOK_IDENT, "concept name");
        tmpl->kind = TMPL_FUNCTION;
        tmpl->is_concept = true;
        tmpl->name = concept_name ? concept_name->value.str_val : "<concept>";
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (!parameter->name) {
                rcc_error(loc,
                          "RCC++ named concepts require named parameters");
            }
            if (parameter->is_pack || parameter->has_default) {
                rcc_error(loc,
                          "RCC++ named concepts do not support parameter packs "
                          "or defaults");
            }
            if (parameter->kind == TPARAM_TEMPLATE) {
                rcc_error(loc,
                          "RCC++ named concepts do not support "
                          "template-template parameters");
            } else if (parameter->kind == TPARAM_NONTYPE &&
                       (!parameter->type || !type_is_integer(parameter->type))) {
                rcc_error(loc,
                          "RCC++ named concepts require integral non-type "
                          "parameters");
            } else if (parameter->kind != TPARAM_TYPE &&
                       parameter->kind != TPARAM_NONTYPE) {
                rcc_error(loc,
                          "RCC++ named concepts support only type and integral "
                          "non-type parameters");
            }
        }
        expect(TOK_ASSIGN, "= after concept name");
        tmpl->constraint = parse_assignment_expression();
        if (!tmpl->constraint) {
            rcc_error(loc, "concept definition requires a constraint expression");
        }
        expect(TOK_SEMICOLON, "; after concept definition");
        active_template = parameter_outer_template;
        return tmpl;
    }

    /* C++20 permits a requires-clause between the template parameter list
     * and the declaration.  It is evaluated after template arguments are
     * substituted; unsupported constraint forms remain explicit diagnostics
     * instead of being treated as an always-true annotation. */
    if (match(TOK_REQUIRES)) {
        bool parenthesized = match(TOK_LPAREN);
        tmpl->constraint = parse_assignment_expression();
        if (parenthesized) expect(TOK_RPAREN, ")");
        if (!tmpl->constraint) {
            rcc_error(loc, "requires-clause requires a constraint expression");
        } else {
            tmpl->constraint_context =
                cxx_template_snapshot_parameter_scope(tmpl);
        }
    }

    /* A friend function template declared in a class is a namespace
     * template.  Preserve the granting class on the template so every
     * specialization receives precisely this access during semantic
     * analysis; do not manufacture a class member or a placeholder body. */
    if (match(TOK_FRIEND)) {
        bool is_consteval = false;
        CxxTemplate* outer_template = parameter_outer_template;
        CxxFriendAccess* friend_access;
        if (!active_class) {
            rcc_error(loc,
                      "friend function template declaration requires class scope");
        }
        tmpl->kind = TMPL_FUNCTION;
        if (active_class) {
            friend_access = ast_arena_alloc(sizeof(*friend_access));
            friend_access->owner = active_class;
            friend_access->next = tmpl->friend_access;
            tmpl->friend_access = friend_access;
        }
        tmpl->ns = active_namespace ? active_namespace : g_global_namespace;
        active_template = tmpl;
        tmpl->func_def = parse_cxx_function_declaration(
            true, &tmpl->is_constexpr, &tmpl->is_noexcept, &is_consteval);
        active_template = outer_template;
        if (tmpl->func_def) {
            tmpl->name = ast_arena_strdup(tmpl->func_def->name);
            if (tmpl->func_def->func_body) {
                tmpl->func_def->func_is_inline = true;
            }
        }
        recognize_versioned_function_template(tmpl);
        return tmpl;
    }

    /* A templated user-defined deduction guide has the same template
     * parameter scope as an ordinary function template, but it is metadata
     * consumed by class-template deduction and must not become a callable
     * namespace function. */
    if (rcc_parse_cxx_deduction_guide()) {
        active_template = parameter_outer_template;
        return tmpl;
    }
    active_template = parameter_outer_template;

    /* C++ alias templates are lowered by substituting their bounded type
     * expression before ordinary declaration parsing.  They have no runtime
     * entity, so accepting one as a function or class template would produce
     * an invalid ABI artifact. */
    if (match(TOK_USING)) {
        Token* alias_name = expect(TOK_IDENT, "alias template name");
        Type* alias_type;
        tmpl->kind = TMPL_ALIAS;
        if (active_class && parameter_outer_template &&
            parameter_outer_template->kind == TMPL_CLASS) {
            tmpl->enclosing_template = parameter_outer_template;
        }
        tmpl->name = ast_arena_strdup(
            alias_name ? alias_name->value.str_val : "<alias>");
        if (tmpl->param_count == 0) {
            rcc_error(loc, "alias template requires at least one parameter");
        }
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack) {
                rcc_error(loc,
                          "RCC++ alias template parameter packs are not "
                          "supported by the bounded type ABI");
            }
            if (parameter->kind == TPARAM_TEMPLATE) {
                rcc_error(loc,
                          "RCC++ alias template template-parameters are not "
                          "supported by the bounded type ABI");
            }
        }
        expect(TOK_ASSIGN, "= after alias template name");
        active_template = tmpl;
        alias_type = parse_cxx_type_spec();
        /* A type-id may carry array or function declarator suffixes after the
         * base specifier. Reuse the shared declarator parser so aliases such
         * as `int[N]` retain their dependent bound for substitution instead
         * of silently becoming the element type. */
        alias_type = rcc_parser_parse_cxx_declarator(alias_type, NULL, NULL);
        active_template = parameter_outer_template;
        if (!alias_type) {
            rcc_error(loc, "alias template requires a type-id");
            alias_type = type_int;
        }
        tmpl->alias_type = alias_type;
        expect(TOK_SEMICOLON, "; after alias template");
        return tmpl;
    }

    if (cxx_template_variable_starts()) {
        Stmt* statement;
        if (!rcc_parser_cxx_standard_at_least(14)) {
            rcc_error(loc,
                      "variable templates require C++14 or newer");
        }
        active_template = tmpl;
        statement = parse_declaration();
        active_template = parameter_outer_template;
        if (!statement || statement->kind != STMT_DECL ||
            !statement->decl || statement->decl->kind != DECL_VAR) {
            rcc_error(loc,
                      "RCC++ variable template requires a variable definition");
            tmpl->kind = TMPL_VARIABLE;
            tmpl->name = ast_arena_strdup("<invalid-variable-template>");
            return tmpl;
        }
        tmpl->kind = TMPL_VARIABLE;
        tmpl->var_def = statement->decl;
        tmpl->name = ast_arena_strdup(statement->decl->name);
        tmpl->is_constexpr = statement->decl->var_is_constexpr;
        return tmpl;
    }

    if (cxx_leading_alignas_class_starts()) {
        while (check(TOK__ALIGNAS)) {
            int alignment = rcc_parser_parse_explicit_alignment();
            if (alignment > explicit_class_alignment) {
                explicit_class_alignment = alignment;
            }
        }
    }

    /* Template body */
    if ((check(TOK_CLASS) || check(TOK_STRUCT)) &&
        parser.cur->next && parser.cur->next->type == TOK_IDENT &&
        parser.cur->next->next &&
        parser.cur->next->next->type == TOK_LT) {
        bool is_struct = match(TOK_STRUCT);
        Token* name_token;
        CxxTemplate* primary;
        Type* arguments[32] = { NULL };
        Expr* value_arguments[32] = { NULL };
        int argument_count = 0;
        CxxClass* specialized_class;
        CxxClass* outer_template_class_definition =
            active_template_class_definition;
        CxxTemplate* outer_template = active_template;

        if (!is_struct) expect(TOK_CLASS, "class or struct");
        name_token = expect(TOK_IDENT, "specialized class name");
        primary = name_token ? find_class_template(name_token->value.str_val)
                             : NULL;
        active_template = tmpl;
        expect(TOK_LT, "<");
        if (!check(TOK_GT)) {
            do {
                if (argument_count == (int)(sizeof(arguments) /
                                            sizeof(arguments[0]))) {
                    rcc_error(loc, "class specialization argument limit exceeded");
                    while (!check(TOK_GT) && !at_end()) advance();
                    break;
                }
                if (primary && argument_count < primary->param_count &&
                    primary->params[argument_count].kind == TPARAM_NONTYPE) {
                    rcc_parser_set_cxx_template_default_mode(true);
                    value_arguments[argument_count] =
                        parse_assignment_expression();
                    rcc_parser_set_cxx_template_default_mode(false);
                } else {
                    arguments[argument_count] = parse_cxx_type_spec();
                }
                ++argument_count;
            } while (match(TOK_COMMA));
        }
        expect(TOK_GT, ">");
        if (!primary || primary->kind != TMPL_CLASS ||
            argument_count != primary->param_count) {
            rcc_error(loc, "explicit specialization has no matching class template");
        }
        tmpl->name = ast_arena_strdup(name_token ? name_token->value.str_val
                                                  : "specialization");
        tmpl->kind = TMPL_CLASS;
        tmpl->primary_template = primary;
        tmpl->templated_class = NULL;
        active_template_class_definition = NULL;
        specialized_class = parse_cxx_class_named(
            loc, is_struct,
            name_token ? name_token->value.str_val : "specialization",
            NULL);
        active_template_class_definition = outer_template_class_definition;
        if (specialized_class && explicit_class_alignment > 0) {
            cxx_class_apply_explicit_alignment(
                specialized_class, explicit_class_alignment, loc);
        }
        active_template = outer_template;
        tmpl->templated_class = specialized_class;
        if (specialized_class) {
            int64_t identity_values[32] = { 0 };
            bool identity_present[32] = { false };
            if (primary && argument_count == primary->param_count) {
                for (int argument_index = 0;
                     argument_index < argument_count; ++argument_index) {
                    if (primary->params[argument_index].kind ==
                            TPARAM_NONTYPE &&
                        value_arguments[argument_index] &&
                        expr_eval_integer_constant(
                            value_arguments[argument_index],
                            &identity_values[argument_index])) {
                        identity_present[argument_index] = true;
                    }
                }
                cxx_set_template_identity(
                    specialized_class, primary, arguments,
                    identity_values, identity_present, argument_count);
            }
            if (tmpl->param_count == 0) {
                register_class_static_fields(specialized_class);
                register_ordinary_class_methods(specialized_class);
            }
            specialized_class->templ = tmpl;
            if (primary && argument_count == primary->param_count) {
                tmpl->specialization_args = ast_arena_alloc(
                    sizeof(Type*) * (size_t)argument_count);
                memcpy(tmpl->specialization_args, arguments,
                       sizeof(Type*) * (size_t)argument_count);
                tmpl->specialization_value_args = ast_arena_alloc(
                    sizeof(Expr*) * (size_t)argument_count);
                memcpy(tmpl->specialization_value_args, value_arguments,
                       sizeof(Expr*) * (size_t)argument_count);
                tmpl->specialization_arg_count = argument_count;
                primary->specializations = ast_arena_grow(
                    primary->specializations,
                    sizeof(CxxTemplate*) * (size_t)primary->specialization_count,
                    sizeof(CxxTemplate*) *
                        (size_t)(primary->specialization_count + 1));
                primary->specializations[primary->specialization_count++] = tmpl;
            }
        }
    } else if (match(TOK_CLASS) || match(TOK_STRUCT)) {
        CxxTemplate* outer_template = active_template;
        CxxClass* outer_template_class_definition =
            active_template_class_definition;
        active_template = tmpl;
        tmpl->kind = TMPL_CLASS;
        active_template_class_definition = NULL;
        tmpl->templated_class = parse_cxx_class();
        active_template_class_definition = outer_template_class_definition;
        if (tmpl->templated_class && explicit_class_alignment > 0) {
            cxx_class_apply_explicit_alignment(
                tmpl->templated_class, explicit_class_alignment, loc);
        }
        active_template = outer_template;
        tmpl->kind = TMPL_CLASS;
        tmpl->class_def = tmpl->templated_class;
        if (tmpl->templated_class) {
            tmpl->name = ast_arena_strdup(tmpl->templated_class->name);
        }
    } else {
        tmpl->kind = TMPL_FUNCTION;
        CxxTemplate* outer_template = active_template;
        bool is_consteval = false;
        active_template = tmpl;
        tmpl->func_def = parse_cxx_function_declaration(
            true, &tmpl->is_constexpr, &tmpl->is_noexcept, &is_consteval);
        active_template = outer_template;
        if (tmpl->func_def) {
            tmpl->name = ast_arena_strdup(tmpl->func_def->name);
        }
        recognize_versioned_function_template(tmpl);
    }

    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].is_pack &&
            tmpl->kind != TMPL_FUNCTION &&
            cxx_class_pack_index(tmpl) < 0) {
            rcc_error(loc,
                      "only a single class-template parameter pack is supported");
        }
    }

    return tmpl;
}

/* ═══════════════════════════════════════
 * C++ Type Parsing
 * ═══════════════════════════════════════ */

static CxxTemplate* namespace_template(CxxNamespace* ns,
                                        const char* name) {
    int index;
    if (!ns || !name) return NULL;
    for (index = 0; index < ns->template_count; ++index) {
        CxxTemplate* candidate = ns->templates[index];
        if (candidate && candidate->name &&
            strcmp(candidate->name, name) == 0) {
            return candidate;
        }
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        if (child->is_inline_namespace) {
            CxxTemplate* candidate = namespace_template(child, name);
            if (candidate) return candidate;
        }
    }
    return NULL;
}

static CxxTemplate* find_template(const char* qualified_name,
                                  int kind) {
    char buffer[512];
    char* component;
    char* next;
    CxxNamespace* ns;
    CxxTemplate* result;
    const char* name = qualified_name;

    if (!name) return NULL;
    while (name[0] == ':' && name[1] == ':') name += 2;
    if (!strstr(name, "::")) {
        for (ns = active_namespace; ns; ns = ns->parent) {
            result = namespace_template(ns, name);
            if (result && (int)result->kind == kind) return result;
        }
        result = namespace_template(g_global_namespace, name);
        return result && (int)result->kind == kind ? result : NULL;
    }

    if (strlen(name) >= sizeof(buffer)) return NULL;
    strcpy(buffer, name);
    ns = g_global_namespace;
    component = buffer;
    for (;;) {
        next = strstr(component, "::");
        if (!next) break;
        *next = '\0';
        ns = cxx_namespace_lookup(ns, component);
        if (!ns) return NULL;
        component = next + 2;
    }
    result = namespace_template(ns, component);
    return result && (int)result->kind == kind ? result : NULL;
}

static CxxTemplate* find_class_template(const char* qualified_name) {
    return find_template(qualified_name, TMPL_CLASS);
}

static CxxTemplate* find_alias_template(const char* qualified_name) {
    return find_template(qualified_name, TMPL_ALIAS);
}

static CxxTemplate* find_variable_template(const char* qualified_name) {
    return find_template(qualified_name, TMPL_VARIABLE);
}

static CxxTemplate* find_concept(const char* qualified_name) {
    CxxTemplate* candidate = find_template(qualified_name, TMPL_FUNCTION);
    return candidate && candidate->is_concept ? candidate : NULL;
}

static int namespace_function_templates(CxxNamespace* ns, const char* name,
                                        CxxTemplate** results, int capacity) {
    int count = 0;
    if (!ns || !name || !results || capacity <= 0) return 0;
    for (int index = 0; index < ns->template_count; ++index) {
        CxxTemplate* candidate = ns->templates[index];
        if (!candidate || candidate->kind != TMPL_FUNCTION ||
            candidate->is_concept ||
            !candidate->name || strcmp(candidate->name, name) != 0) {
            continue;
        }
        if (count < capacity) results[count] = candidate;
        ++count;
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        if (child->is_inline_namespace) {
            CxxTemplate* nested[32] = { NULL };
            int nested_count = namespace_function_templates(
                child, name, nested, (int)(sizeof(nested) / sizeof(nested[0])));
            for (int nested_index = 0; nested_index < nested_count;
                 ++nested_index) {
                if (nested_index < (int)(sizeof(nested) / sizeof(nested[0])) &&
                    count < capacity) {
                    results[count] = nested[nested_index];
                }
                ++count;
            }
        }
    }
    return count;
}

/* Unlike ordinary name lookup, a function-template name denotes a candidate
 * set.  Stop at the first namespace in the lexical search that contributes a
 * match, then let the call-site deduction/partial-ordering pass select one. */
static int find_function_template_candidates(const char* qualified_name,
                                             CxxTemplate** results,
                                             int capacity) {
    char buffer[512];
    char* component;
    char* next;
    CxxNamespace* ns;
    const char* name = qualified_name;
    int count;

    if (!name || !results || capacity <= 0) return 0;
    while (name[0] == ':' && name[1] == ':') name += 2;
    if (!strstr(name, "::")) {
        for (ns = active_namespace; ns; ns = ns->parent) {
            count = namespace_function_templates(ns, name, results, capacity);
            if (count > 0) return count;
        }
        return namespace_function_templates(g_global_namespace, name,
                                            results, capacity);
    }
    if (strlen(name) >= sizeof(buffer)) return 0;
    strcpy(buffer, name);
    ns = g_global_namespace;
    component = buffer;
    for (;;) {
        next = strstr(component, "::");
        if (!next) break;
        *next = '\0';
        ns = cxx_namespace_lookup(ns, component);
        if (!ns) return 0;
        component = next + 2;
    }
    return namespace_function_templates(ns, component, results, capacity);
}

static bool cxx_class_is_associated_with(CxxClass* actual,
                                         CxxClass* granting_class) {
    if (!actual || !granting_class) return false;
    if (actual == granting_class) return true;
    for (int index = 0; index < actual->base_count; ++index) {
        if (cxx_class_is_associated_with(actual->bases[index].base,
                                         granting_class)) {
            return true;
        }
    }
    return false;
}

static bool cxx_type_is_associated_with(Type* type,
                                        CxxClass* granting_class) {
    if (!type || !granting_class) return false;
    if (type->cxx_is_member_pointer &&
        type->cxx_member_pointer_owner &&
        cxx_class_is_associated_with(
            type->cxx_member_pointer_owner->cxx_class, granting_class)) {
        return true;
    }
    if ((type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) &&
        !type->cxx_is_member_pointer) {
        return cxx_type_is_associated_with(type->base, granting_class);
    }
    if (type->cxx_class &&
        cxx_class_is_associated_with(type->cxx_class, granting_class)) {
        return true;
    }
    for (int index = 0; index < type->cxx_template_arg_count; ++index) {
        if (cxx_type_is_associated_with(type->cxx_template_args[index],
                                        granting_class)) {
            return true;
        }
    }
    if (type->cxx_class) {
        CxxClass* cls = type->cxx_class;
        Type** arguments = cls->template_identity_tmpl
            ? cls->template_identity_args : cls->template_args;
        int argument_count = cls->template_identity_tmpl
            ? cls->template_identity_arg_count : cls->template_arg_count;
        for (int index = 0; arguments && index < argument_count; ++index) {
            if (cxx_type_is_associated_with(arguments[index], granting_class)) {
                return true;
            }
        }
    }
    return false;
}

/* Hidden friend templates are admitted only when an argument contributes the
 * class that declared the friend, one of its associated base classes, or a
 * class carried by a class-template type argument. A later namespace-scope
 * redeclaration clears is_hidden_friend in the AST registry and returns the
 * template to ordinary lookup. */
static bool cxx_hidden_friend_template_matches_adl(
    CxxTemplate* tmpl, ExprList* arguments) {
    if (!tmpl || !tmpl->is_hidden_friend) return true;
    for (CxxFriendAccess* grant = tmpl->friend_access; grant;
         grant = grant->next) {
        for (ExprList* argument = arguments; argument;
             argument = argument->next) {
            Type* type = cxx_parser_expression_type(argument->expr);
            if (cxx_type_is_associated_with(type, grant->owner)) {
                return true;
            }
        }
    }
    return false;
}

Expr* rcc_parse_cxx_concept_expression(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name;
    CxxTemplate* concept;
    ExprList* arguments = NULL;
    int argument_count = 0;

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    name = parse_qualified_name();
    concept = find_concept(name);
    if (!concept) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    if (!match(TOK_LT)) {
        rcc_error(loc, "named concept '%s' requires template arguments", name);
        return expr_int(0, loc);
    }
    if (!check(TOK_GT)) {
        do {
            Expr* argument;
            if (argument_count >= 32) {
                rcc_error(peek()->loc,
                          "named concept argument limit exceeded");
                while (!check(TOK_GT) && !at_end()) advance();
                break;
            }
            if (argument_count < concept->param_count &&
                concept->params[argument_count].kind == TPARAM_TYPE) {
                Type* argument_type = parse_cxx_type_spec();
                if (!argument_type) {
                    rcc_error(peek()->loc,
                              "named concept type argument requires a type");
                    argument_type = type_int;
                }
                argument_type = rcc_parser_parse_cxx_declarator(
                    argument_type, NULL, NULL);
                argument = expr_int(0, loc);
                argument->type = argument_type;
            } else {
                rcc_parser_set_cxx_template_default_mode(true);
                argument = parse_assignment_expression();
                rcc_parser_set_cxx_template_default_mode(false);
            }
            exprlist_append(&arguments, argument);
            ++argument_count;
        } while (match(TOK_COMMA));
    }
    expect(TOK_GT, ">");
    if (argument_count != concept->param_count) {
        rcc_error(loc,
                  "named concept '%s' expects %d argument(s), got %d",
                  name, concept->param_count, argument_count);
    }
    {
        Expr* call = expr_call(expr_ident(name, loc), arguments, loc);
        call->cxx_concept_template = concept;
        return call;
    }
}

/* Parse a trailing function requires-clause and attach it to the active
 * function template.  Keeping this separate from the body parser makes the
 * same constrained-template path serve abbreviated and explicit templates. */
static Expr* parse_cxx_trailing_requires_clause(SourceLoc loc) {
    bool parenthesized;
    Expr* constraint;

    if (!match(TOK_REQUIRES)) return NULL;
    if (!rcc_parser_cxx_standard_at_least(20)) {
        rcc_error(loc, "requires-clauses require C++20 or newer");
    }
    parenthesized = match(TOK_LPAREN);
    constraint = parse_assignment_expression();
    if (parenthesized) expect(TOK_RPAREN, ") after trailing requires-clause");
    if (!constraint) {
        rcc_error(loc,
                  "trailing requires-clause requires a constraint expression");
        return NULL;
    }
    if (!active_template || active_template->kind != TMPL_FUNCTION) {
        rcc_error(loc,
                  "trailing requires-clause requires a function template");
        return NULL;
    }
    return constraint;
}

static CxxClass* namespace_class(CxxNamespace* ns, const char* name) {
    if (!ns || !name) return NULL;
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* candidate = ns->classes[index];
        if (candidate && candidate->name &&
            strcmp(candidate->name, name) == 0) {
            return candidate;
        }
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        if (child->is_inline_namespace) {
            CxxClass* candidate = namespace_class(child, name);
            if (candidate) return candidate;
        }
    }
    return NULL;
}

static CxxClass* find_class(const char* qualified_name) {
    char buffer[512];
    char* component;
    char* next;
    CxxNamespace* ns;
    const char* name = qualified_name;
    bool explicitly_global;

    if (!name) return NULL;
    explicitly_global = name[0] == ':' && name[1] == ':';
    while (name[0] == ':' && name[1] == ':') name += 2;
    if (!strstr(name, "::")) {
        if (!explicitly_global) {
            for (ns = active_namespace; ns; ns = ns->parent) {
                CxxClass* result = namespace_class(ns, name);
                if (result) return result;
            }
        }
        return namespace_class(g_global_namespace, name);
    }
    if (strlen(name) >= sizeof(buffer)) return NULL;
    strcpy(buffer, name);
    ns = g_global_namespace;
    component = buffer;
    for (;;) {
        next = strstr(component, "::");
        if (!next) break;
        *next = '\0';
        ns = cxx_namespace_lookup(ns, component);
        if (!ns) return NULL;
        component = next + 2;
    }
    return namespace_class(ns, component);
}

/* Resolve a non-dependent qualified nested type-id such as
 * `Namespace::Owner::value_type`.  The parser's ordinary type table does not
 * contain class-scope aliases, so qualified names must consult the class
 * alias registry and preserve its access/ambiguity result. */
static CxxTypeAlias* find_qualified_class_type_alias(
    const char* qualified_name, CxxClass** owner_out,
    bool* ambiguous, bool* accessible) {
    const char* separator;
    size_t owner_length;
    char owner_name[512];
    CxxClass* owner;
    Type* owner_type;

    if (owner_out) *owner_out = NULL;
    if (ambiguous) *ambiguous = false;
    if (accessible) *accessible = false;
    if (!qualified_name) return NULL;
    separator = strrchr(qualified_name, ':');
    if (!separator || separator <= qualified_name || separator[-1] != ':') {
        return NULL;
    }
    owner_length = (size_t)(separator - qualified_name - 1);
    if (owner_length == 0u || owner_length >= sizeof(owner_name)) return NULL;
    memcpy(owner_name, qualified_name, owner_length);
    owner_name[owner_length] = '\0';
    owner = find_class(owner_name);
    if (!owner) {
        owner_type = rcc_parser_lookup_type(owner_name);
        if (owner_type && (owner_type->kind == TYPE_STRUCT ||
                           owner_type->kind == TYPE_UNION)) {
            owner = owner_type->cxx_class;
        }
    }
    if (!owner) return NULL;
    if (owner_out) *owner_out = owner;
    return cxx_class_find_inherited_type_alias(
        owner, separator + 1, active_class, ambiguous, accessible);
}

/* A template-id can itself be the owner of a nested type-id, for example
 * `Box<int>::value_type` or an alias-template specialization naming a class.
 * Balance the tokenized argument list only to distinguish that form from a
 * standalone template-id. */
static bool cxx_template_id_followed_by_scope(const char** name_out) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    const char* template_name;
    Token* token;
    int depth = 0;
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;
    bool followed_by_scope = false;

    if (name_out) *name_out = NULL;
    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return false;
    template_name = parse_qualified_name();
    if (!check(TOK_LT) ||
        (!find_class_template(template_name) &&
         !find_alias_template(template_name))) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return false;
    }
    for (token = parser.cur; token && token->type != TOK_EOF;
         token = token->next) {
        if (token->type == TOK_LPAREN) {
            ++parentheses;
            continue;
        }
        if (token->type == TOK_RPAREN && parentheses > 0) {
            --parentheses;
            continue;
        }
        if (token->type == TOK_LBRACKET) {
            ++brackets;
            continue;
        }
        if (token->type == TOK_RBRACKET && brackets > 0) {
            --brackets;
            continue;
        }
        if (token->type == TOK_LBRACE) {
            ++braces;
            continue;
        }
        if (token->type == TOK_RBRACE && braces > 0) {
            --braces;
            continue;
        }
        if (parentheses || brackets || braces) continue;
        if (token->type == TOK_LT) {
            ++depth;
        } else if (token->type == TOK_GT) {
            if (depth > 0) --depth;
            if (depth == 0) {
                followed_by_scope = token->next &&
                                    token->next->type == TOK_SCOPE;
                break;
            }
        } else if (token->type == TOK_RSHIFT) {
            /* If this candidate starts inside another template argument,
             * the second character in `>>` closes that surrounding argument;
             * it is not followed immediately by `::` for this template-id. */
            if (depth < 2) break;
            depth -= 2;
            if (depth == 0) {
                followed_by_scope = token->next &&
                                    token->next->type == TOK_SCOPE;
                break;
            }
        }
    }
    parser.cur = saved_cur;
    parser.prev = saved_prev;
    if (followed_by_scope && name_out) *name_out = template_name;
    return followed_by_scope;
}

/* Semantic analysis needs the declaring class for qualified static-member
 * expressions so protected/private access is checked after ordinary symbol
 * lookup.  Keep the class registry private to this frontend while exposing a
 * read-only lookup boundary to the shared C semantic pass. */
CxxClass* rcc_parser_cxx_find_class(const char* qualified_name) {
    return find_class(qualified_name);
}

Type* rcc_parser_cxx_find_class_type(const char* qualified_name) {
    CxxClass* cls = find_class(qualified_name);
    return cls ? cls->type : NULL;
}

/* Complete a previously declared static data member outside its class.  The
 * class parser has already published the declaration and its ABI spelling;
 * this hook only consumes the qualified definition and updates that same
 * declaration, avoiding duplicate data symbols in one translation unit. */
Stmt* rcc_parse_cxx_qualified_data_definition(
    Type* base_type, int storage, bool is_inline, bool is_constexpr,
    bool is_constinit, bool is_thread_local, SourceLoc loc) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    const char* qualified;
    const char* separator;
    const char* member_name;
    size_t owner_length;
    char owner_name[512];
    CxxClass* cls;
    Decl* declaration = NULL;
    TypeParam* field;
    Expr* initializer = NULL;

    (void)is_inline;
    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    /* Do not consume a data-member pointer declarator such as `int C::*p`
     * as a qualified static data-member definition.  The shared declarator
     * parser owns this spelling and retains the class on the pointer type. */
    {
        Token* cursor = parser.cur;
        while (cursor && cursor->type == TOK_IDENT && cursor->next &&
               cursor->next->type == TOK_SCOPE && cursor->next->next) {
            if (cursor->next->next->type == TOK_STAR) return NULL;
            if (cursor->next->next->type != TOK_IDENT) break;
            cursor = cursor->next->next;
        }
    }
    qualified = parse_qualified_name();
    separator = qualified ? strrchr(qualified, ':') : NULL;
    if (!separator || separator <= qualified || separator[-1] != ':') {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    owner_length = (size_t)(separator - qualified - 1);
    if (owner_length == 0u || owner_length >= sizeof(owner_name)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    memcpy(owner_name, qualified, owner_length);
    owner_name[owner_length] = '\0';
    member_name = separator + 1;
    if (!*member_name) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    cls = find_class(owner_name);
    if (!cls) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        const char* final_name;
        if (!member->is_static || member->method || !member->decl ||
            member->decl->kind != DECL_VAR || !member->decl->name) {
            continue;
        }
        final_name = strrchr(member->decl->name, ':');
        final_name = final_name ? final_name + 1 : member->decl->name;
        if (strcmp(final_name, member_name) == 0) {
            declaration = member->decl;
            break;
        }
    }
    if (!declaration) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    if (is_inline && !rcc_parser_cxx_standard_at_least(17)) {
        rcc_error(loc, "inline variables require C++17 or newer");
    }
    if (is_thread_local) declaration->var_is_thread_local = true;
    if (!base_type || !declaration->type ||
        !type_is_compatible(base_type, declaration->type)) {
        rcc_error(loc, "static data member definition type does not match '%s'",
                  declaration->name);
    }
    if (match(TOK_ASSIGN)) {
        initializer = rcc_parser_parse_initializer();
    } else if (check(TOK_LBRACE)) {
        initializer = rcc_parser_parse_initializer();
    }
    rcc_parser_validate_cxx_constructor_initializer(
        declaration->type, initializer);
    expect(TOK_SEMICOLON, ";");

    if (declaration->var_init && initializer) {
        rcc_error(loc, "redefinition of static data member '%s'",
                  declaration->name);
    } else if (initializer) {
        declaration->var_init = initializer;
        declaration->var_is_constexpr = is_constexpr;
        declaration->var_is_constinit = is_constinit;
        for (field = cls->fields; field; field = field->next) {
            if (field->is_static && field->name &&
                strcmp(field->name, member_name) == 0) {
                field->initializer = initializer;
                break;
            }
        }
    }
    if (storage == STORAGE_EXTERN) declaration->storage = STORAGE_EXTERN;
    if (is_inline) declaration->var_is_inline = true;
    return stmt_null(loc);
}

static void resolve_class_bases(CxxClass* cls, SourceLoc loc) {
    if (!cls) return;
    for (int index = 0; index < cls->base_count; ++index) {
        const char* base_name = cls->bases[index].base_name;
        CxxClass* base = cls->bases[index].base;
        if (!base && !base_name) continue;
        if (!base && cls->bases[index].type_pattern &&
            cls->bases[index].type_pattern->cxx_dependent) {
            continue;
        }
        if (!base) base = find_class(base_name);
        if (!base) {
            rcc_error(loc, "unknown base class '%s'", base_name);
            continue;
        }
        if (base == cls) {
            rcc_error(loc, "a class cannot derive from itself");
            continue;
        }
        if (base->is_final) {
            rcc_error(loc, "cannot derive from final class '%s'", base_name);
        }
        cls->bases[index].base = base;
    }
}

static bool cxx_class_has_unresolved_dependent_base(const CxxClass* cls) {
    if (!cls) return false;
    for (int index = 0; index < cls->base_count; ++index) {
        if (!cls->bases[index].base && cls->bases[index].type_pattern &&
            cls->bases[index].type_pattern->cxx_dependent) {
            return true;
        }
    }
    return false;
}

/* Return the virtual base declaration whose source signature is inherited by
 * a derived method.  The vtable entries retain the original CxxMethod even
 * when a base inherited that slot from one of its own bases, so scanning the
 * primary and secondary tables covers all supported inheritance paths. */
static CxxMethod* find_base_virtual_method(CxxClass* base,
                                           CxxMethod* derived) {
    if (!base || !derived) return NULL;
    for (int index = 0; index < base->vtable_size; ++index) {
        CxxMethod* candidate = base->vtable[index].method;
        if (candidate && candidate->is_virtual &&
            cxx_method_virtual_signature_matches(derived, candidate)) {
            return candidate;
        }
    }
    for (int table_index = 0; table_index < base->secondary_vtable_count;
         ++table_index) {
        CxxSecondaryVtable* table = &base->secondary_vtables[table_index];
        for (int index = 0; index < table->size; ++index) {
            CxxMethod* candidate = table->entries[index].method;
            if (candidate && candidate->is_virtual &&
                cxx_method_virtual_signature_matches(derived, candidate)) {
                return candidate;
            }
        }
    }
    return NULL;
}

static void validate_class_virtual_specifiers(CxxClass* cls, SourceLoc loc) {
    if (!cls) return;
    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        CxxMethod* method = member->method;
        CxxMethod* base_method = NULL;
        const char* name;
        if (!method || !method->decl) continue;
        name = cxx_method_source_name(method);
        if ((method->is_override || method->is_final) && method->is_static) {
            rcc_error(method->decl->loc,
                      "static member function '%s' cannot use override or final",
                      name ? name : "<unnamed>");
            continue;
        }
        for (int base_index = 0; base_index < cls->base_count;
             ++base_index) {
            CxxClass* base = cls->bases[base_index].base;
            base_method = find_base_virtual_method(base, method);
            if (base_method) break;
        }

        if (base_method &&
            !cxx_method_override_signature_matches(method, base_method)) {
            rcc_error(method->decl->loc,
                      "return type of '%s' is incompatible with the overridden "
                      "virtual method",
                      name ? name : "<unnamed>");
            continue;
        }
        if (method->is_override && !base_method) {
            rcc_error(method->decl->loc,
                      "method '%s' is marked override but does not override a "
                      "base class method",
                      name ? name : "<unnamed>");
            continue;
        }
        if (base_method && base_method->is_final) {
            rcc_error(method->decl->loc,
                      "cannot override final method '%s'",
                      name ? name : "<unnamed>");
            continue;
        }
        if (method->is_final && !base_method && !method->is_virtual) {
            rcc_error(method->decl->loc,
                      "method '%s' is marked final but is not virtual",
                      name ? name : "<unnamed>");
        }
    }
    (void)loc;
}

/* Tell the shared C declaration parser when an identifier begins a C++ type
 * declaration.  This is deliberately a query: parsing an expression such as
 * `value < limit` must not consume tokens merely to decide whether it is a
 * declaration. */
bool rcc_parse_cxx_type_start(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    const char* name;
    bool result = false;

    if (check(TOK_CLASS) || check(TOK_STRUCT)) {
        Token* next = parser.cur->next;
        result = next && next->type == TOK_IDENT;
        return result;
    }
    if (check(TOK_BOOL) || check(TOK_CHAR8_T)) return true;
    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return false;
    if (cxx_qualified_class_alias_template_starts()) return true;

    name = parse_qualified_name();
    result = is_active_template_type(name) || find_class(name) != NULL ||
             find_class_template(name) != NULL ||
             rcc_parser_lookup_type(name) != NULL;
    if (!result && strstr(name, "::")) {
        bool ambiguous = false;
        bool accessible = false;
        CxxTypeAlias* alias = find_qualified_class_type_alias(
            name, NULL, &ambiguous, &accessible);
        result = alias != NULL || ambiguous;
        (void)accessible;
    }
    if (check(TOK_LT) &&
        (find_class_template(name) || find_alias_template(name) ||
         active_template_template_parameter_index(name) >= 0)) {
        result = true;
    }
    if (!result && check(TOK_LT) && active_class &&
        !strstr(name, "::")) {
        CxxClass* declaring_class = NULL;
        bool ambiguous = false;
        bool accessible = false;
        cxx_resolve_known_class_bases(active_class);
        result = cxx_parser_find_inherited_alias_template(
                     active_class, name, active_class, &declaring_class,
                     &ambiguous, &accessible) != NULL || ambiguous;
        (void)declaring_class;
        (void)accessible;
    }

    parser.cur = saved_cur;
    parser.prev = saved_prev;
    return result;
}

static int template_parameter_index(CxxTemplate* tmpl, Type* type) {
    int index;
    if (!tmpl || !type || type->kind != TYPE_STRUCT || !type->tag) {
        return -1;
    }
    for (index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_TYPE &&
            tmpl->params[index].name &&
            strcmp(tmpl->params[index].name, type->tag) == 0) {
            return index;
        }
    }
    if (tmpl->enclosing_template) {
        CxxTemplate* enclosing = tmpl->enclosing_template;
        for (index = 0; index < enclosing->param_count; ++index) {
            if (enclosing->params[index].kind == TPARAM_TYPE &&
                enclosing->params[index].name &&
                strcmp(enclosing->params[index].name, type->tag) == 0) {
                return tmpl->param_count + index;
            }
        }
    }
    return -1;
}

static Type* instantiate_class_template(CxxTemplate* tmpl, Type** arguments,
                                        const int64_t* value_args,
                                        const bool* value_present,
                                        int argument_count, SourceLoc loc);

static int active_template_template_parameter_index(const char* name) {
    if (!active_template || !name) return -1;
    for (int index = 0; index < active_template->param_count; ++index) {
        TemplateParam* parameter = &active_template->params[index];
        if (parameter->kind == TPARAM_TEMPLATE && parameter->name &&
            strcmp(parameter->name, name) == 0) {
            return index;
        }
    }
    if (active_template->enclosing_template) {
        CxxTemplate* enclosing = active_template->enclosing_template;
        for (int index = 0; index < enclosing->param_count; ++index) {
            TemplateParam* parameter = &enclosing->params[index];
            if (parameter->kind == TPARAM_TEMPLATE && parameter->name &&
                strcmp(parameter->name, name) == 0) {
                return active_template->param_count + index;
            }
        }
    }
    return -1;
}

static bool template_template_signature_matches(
    const TemplateParam* parameter, const CxxTemplate* actual) {
    const CxxTemplate* signature = parameter ? parameter->template_signature : NULL;
    if (!parameter || parameter->kind != TPARAM_TEMPLATE || !signature ||
        !actual || actual->kind != TMPL_CLASS ||
        signature->param_count != actual->param_count) {
        return false;
    }
    for (int index = 0; index < signature->param_count; ++index) {
        if (signature->params[index].kind != actual->params[index].kind ||
            signature->params[index].is_pack != actual->params[index].is_pack) {
            return false;
        }
        if (signature->params[index].kind == TPARAM_NONTYPE &&
            (!signature->params[index].type || !actual->params[index].type ||
             !type_is_compatible(signature->params[index].type,
                                 actual->params[index].type))) {
            return false;
        }
    }
    return true;
}

static Type* parse_template_template_default(SourceLoc loc) {
    const char* name = NULL;
    CxxTemplate* actual = NULL;
    Type* carrier;

    if (check(TOK_IDENT) || check(TOK_SCOPE)) {
        name = parse_qualified_name();
    } else {
        rcc_error(peek()->loc,
                  "template-template default requires a class template name");
    }
    if (name) actual = find_class_template(name);
    if (!actual) {
        rcc_error(loc,
                  "template-template default must name a class template");
        return type_int;
    }
    carrier = type_struct(name);
    carrier->cxx_template = actual;
    return carrier;
}

static Type* substitute_template_type(CxxTemplate* tmpl, Type* type,
                                      Type** arguments, int argument_count,
                                      const int64_t* value_args,
                                      const bool* value_present) {
    Type* substituted;
    Type* base;
    int array_len;
    Expr* array_bound;
    int parameter_index;
    if (!type) return NULL;
    if (tmpl && tmpl->is_local_class_template &&
        tmpl->local_class_pattern && tmpl->local_class_instance &&
        tmpl->local_class_pattern->type &&
        tmpl->local_class_instance->type &&
        type->kind == TYPE_STRUCT &&
        (type->cxx_class == tmpl->local_class_pattern ||
         (!type->cxx_class && type->tag &&
          tmpl->local_class_pattern->type->tag &&
          strcmp(type->tag,
                 tmpl->local_class_pattern->type->tag) == 0 &&
          (!type->cxx_scope_identity ||
           (tmpl->local_class_pattern->type->cxx_scope_identity &&
            strcmp(type->cxx_scope_identity,
                   tmpl->local_class_pattern->type->cxx_scope_identity) ==
                0)) &&
          ((!type->cxx_namespace &&
            !tmpl->local_class_pattern->type->cxx_namespace) ||
           (type->cxx_namespace &&
            tmpl->local_class_pattern->type->cxx_namespace &&
            strcmp(type->cxx_namespace,
                   tmpl->local_class_pattern->type->cxx_namespace) ==
                0))))) {
        Type* instance_type = tmpl->local_class_instance->type;
        if (type == tmpl->local_class_pattern->type) {
            return instance_type;
        }
        substituted = ast_arena_alloc(sizeof(*substituted));
        *substituted = *instance_type;
        substituted->is_const = type->is_const;
        substituted->is_volatile = type->is_volatile;
        substituted->is_atomic = type->is_atomic;
        substituted->is_restrict = type->is_restrict;
        return substituted;
    }
    if (type->cxx_dependent && type->cxx_dependent_member_name &&
        type->cxx_template_param_index >= 0 &&
        type->cxx_template_param_index < argument_count &&
        arguments[type->cxx_template_param_index]) {
        Type* owner = arguments[type->cxx_template_param_index];
        if (owner->cxx_dependent) {
            Type* unresolved = ast_arena_alloc(sizeof(*unresolved));
            *unresolved = *type;
            unresolved->cxx_class = owner->cxx_class;
            if (owner->cxx_template_param_index >= 0) {
                unresolved->cxx_template_param_index =
                    owner->cxx_template_param_index;
            }
            return unresolved;
        }
        bool inherited_alias_ambiguous = false;
        bool inherited_alias_accessible = false;
        CxxClass* access_context = tmpl->active_class_instance
            ? tmpl->active_class_instance : tmpl->templated_class;
        CxxTypeAlias* alias = owner->cxx_class
            ? cxx_class_find_inherited_type_alias(
                  owner->cxx_class, type->cxx_dependent_member_name,
                  access_context, &inherited_alias_ambiguous,
                  &inherited_alias_accessible)
            : NULL;
        if (alias) {
            if (!inherited_alias_accessible) {
                rcc_error(parser.cur ? parser.cur->loc
                                     : (SourceLoc){"<template>", 0, 0},
                          "dependent nested type '%s' is inaccessible in class '%s'",
                          type->cxx_dependent_member_name,
                          owner->cxx_class && owner->cxx_class->name
                              ? owner->cxx_class->name : "<unnamed>");
                return type_int;
            }
            return substitute_template_type(
                tmpl, alias->type, arguments, argument_count,
                value_args, value_present);
        }
        rcc_error(parser.cur ? parser.cur->loc
                             : (SourceLoc){"<template>", 0, 0},
                  inherited_alias_ambiguous
                      ? "nested type '%s' is ambiguous in class '%s'"
                      : "class '%s' has no unique accessible nested type '%s'",
                  inherited_alias_ambiguous
                      ? type->cxx_dependent_member_name
                      : (owner->cxx_class && owner->cxx_class->name
                             ? owner->cxx_class->name
                             : (owner->tag ? owner->tag : "<non-class>")),
                  inherited_alias_ambiguous
                      ? (owner->cxx_class && owner->cxx_class->name
                             ? owner->cxx_class->name : "<unnamed>")
                      : type->cxx_dependent_member_name);
        return type_int;
    }
    if (type->cxx_dependent && type->cxx_template &&
        type->cxx_template_param_index < 0 &&
        type->cxx_template_arg_count > 0) {
        Type* nested_arguments[32] = { NULL };
        int64_t nested_values[32] = { 0 };
        bool nested_value_present[32] = { false };
        Expr* nested_value_expressions[32] = { NULL };
        int pack_index = cxx_class_pack_index(type->cxx_template);
        bool still_dependent = false;
        if (type->cxx_template_arg_count >
            (int)(sizeof(nested_arguments) / sizeof(nested_arguments[0]))) {
            rcc_error((SourceLoc){"<template>", 0, 0},
                      "dependent class template argument limit exceeded");
            return NULL;
        }
        for (int nested_index = 0;
             nested_index < type->cxx_template_arg_count; ++nested_index) {
            int parameter_index = pack_index >= 0 &&
                    nested_index >= pack_index
                ? pack_index : nested_index;
            TemplateParam* parameter = parameter_index <
                    type->cxx_template->param_count
                ? &type->cxx_template->params[parameter_index] : NULL;
            if (parameter && parameter->kind == TPARAM_NONTYPE) {
                Expr* value_expression = type->cxx_template_value_args
                    ? type->cxx_template_value_args[nested_index] : NULL;
                nested_arguments[nested_index] = parameter->type
                    ? parameter->type : type_int;
                nested_value_expressions[nested_index] = value_expression;
                if (!value_expression) {
                    rcc_error((SourceLoc){"<template>", 0, 0},
                              "dependent class-template non-type argument "
                              "is missing");
                    return NULL;
                }
                if (eval_template_integer_expression(
                        value_expression, tmpl, value_args, value_present,
                        &nested_values[nested_index])) {
                    nested_value_present[nested_index] = true;
                } else if (cxx_expression_references_template_non_type_parameter(
                               value_expression, tmpl)) {
                    still_dependent = true;
                } else {
                    rcc_error(value_expression->loc,
                              "dependent class-template non-type argument "
                              "must be an integer constant expression");
                    return NULL;
                }
            } else {
                nested_arguments[nested_index] = substitute_template_type(
                    tmpl, type->cxx_template_args[nested_index], arguments,
                    argument_count, value_args, value_present);
                if (!nested_arguments[nested_index]) return NULL;
                if (nested_arguments[nested_index]->cxx_dependent) {
                    still_dependent = true;
                }
            }
        }
        if (still_dependent) {
            substituted = ast_arena_alloc(sizeof(*substituted));
            *substituted = *type;
            substituted->cxx_template_args = ast_arena_alloc(
                sizeof(Type*) * (size_t)type->cxx_template_arg_count);
            memcpy(substituted->cxx_template_args, nested_arguments,
                   sizeof(Type*) * (size_t)type->cxx_template_arg_count);
            substituted->cxx_template_value_args = ast_arena_alloc(
                sizeof(Expr*) * (size_t)type->cxx_template_arg_count);
            for (int nested_index = 0;
                 nested_index < type->cxx_template_arg_count; ++nested_index) {
                Expr* value_expression = nested_value_expressions[nested_index];
                substituted->cxx_template_value_args[nested_index] =
                    nested_value_present[nested_index]
                        ? expr_int(nested_values[nested_index],
                                   value_expression
                                       ? value_expression->loc
                                       : (SourceLoc){"<template>", 0, 0})
                        : value_expression;
            }
            return substituted;
        }
        return instantiate_class_template(
            type->cxx_template, nested_arguments, nested_values,
            nested_value_present,
            type->cxx_template_arg_count, (SourceLoc){"<template>", 0, 0});
    }
    if (type->cxx_dependent && type->cxx_template_param_index >= 0 &&
        type->cxx_template_arg_count > 0) {
        Type* template_argument;
        CxxTemplate* actual_template;
        Type* nested_arguments[32] = { NULL };
        int64_t nested_values[32] = { 0 };
        bool nested_value_present[32] = { false };
        Type* instantiated;
        if (type->cxx_template_param_index >= argument_count ||
            !arguments ||
            !(template_argument = arguments[type->cxx_template_param_index]) ||
            !(actual_template = template_argument->cxx_template) ||
            type->cxx_template_arg_count >
                (int)(sizeof(nested_arguments) / sizeof(nested_arguments[0])) ||
            !template_template_signature_matches(
                tmpl && type->cxx_template_param_index < tmpl->param_count
                    ? &tmpl->params[type->cxx_template_param_index] : NULL,
                actual_template) ||
            actual_template->param_count != type->cxx_template_arg_count) {
            rcc_error(type->cxx_class ? (SourceLoc){"<template>", 0, 0}
                                      : (SourceLoc){"<template>", 0, 0},
                      "template-template argument cannot be instantiated");
            return NULL;
        }
        for (int nested_index = 0;
             nested_index < type->cxx_template_arg_count; ++nested_index) {
            TemplateParam* nested_parameter =
                &actual_template->params[nested_index];
            if (nested_parameter->kind == TPARAM_NONTYPE) {
                Expr* value_expression = type->cxx_template_value_args
                    ? type->cxx_template_value_args[nested_index] : NULL;
                if (!value_expression ||
                    !eval_template_integer_expression(
                        value_expression, tmpl, value_args, value_present,
                        &nested_values[nested_index])) {
                    rcc_error(value_expression ? value_expression->loc
                                                : (SourceLoc){"<template>", 0, 0},
                              "dependent template-template non-type argument "
                              "must be an integer constant expression");
                    return NULL;
                }
                nested_arguments[nested_index] = nested_parameter->type
                    ? nested_parameter->type : type_int;
                nested_value_present[nested_index] = true;
            } else {
                nested_arguments[nested_index] = substitute_template_type(
                    tmpl, type->cxx_template_args[nested_index], arguments,
                    argument_count, value_args, value_present);
                if (!nested_arguments[nested_index]) return NULL;
            }
        }
        instantiated = instantiate_class_template(
            actual_template, nested_arguments, nested_values,
            nested_value_present,
            type->cxx_template_arg_count, (SourceLoc){"<template>", 0, 0});
        if (!instantiated) return NULL;
        return instantiated;
    }
    parameter_index = template_parameter_index(tmpl, type);
    if (parameter_index >= 0 && parameter_index < argument_count) {
        substituted = arguments[parameter_index];
        if (substituted != type && substituted->cxx_dependent) {
            Type* resolved = substitute_template_type(
                tmpl, substituted, arguments, argument_count, value_args,
                value_present);
            if (resolved) substituted = resolved;
        }
        if ((type->is_const && !substituted->is_const) ||
            (type->is_volatile && !substituted->is_volatile)) {
            Type* qualified = ast_arena_alloc(sizeof(*qualified));
            *qualified = *substituted;
            qualified->is_const = qualified->is_const || type->is_const;
            qualified->is_volatile = qualified->is_volatile ||
                                     type->is_volatile;
            substituted = qualified;
        }
        return substituted;
    }
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) {
        base = substitute_template_type(
            tmpl, type->base, arguments, argument_count,
            value_args, value_present);
        if (type->kind == TYPE_PTR && type->is_reference && base &&
            base->kind == TYPE_PTR && base->is_reference) {
            return type_reference(
                base, type->is_rvalue_reference &&
                          base->is_rvalue_reference);
        }
        array_len = type->array_len;
        array_bound = type->array_bound;
        if (type->kind == TYPE_ARRAY && type->array_bound) {
            int64_t value;
            if (eval_template_integer_expression(
                    type->array_bound, tmpl, value_args, value_present,
                    &value)) {
                if (value <= 0 || value > INT_MAX) {
                    rcc_error(type->array_bound->loc,
                              "template array bound is out of range");
                    return NULL;
                }
                array_len = (int)value;
                array_bound = NULL;
            }
        }
        if (base != type->base || array_len != type->array_len ||
            array_bound != type->array_bound) {
            substituted = ast_arena_alloc(sizeof(*substituted));
            *substituted = *type;
            substituted->base = base;
            if (substituted->kind == TYPE_ARRAY) {
                substituted->array_len = array_len;
                substituted->array_bound = array_bound;
                substituted->size = substituted->array_len > 0
                    ? base->size * substituted->array_len : 0;
                substituted->align = base->align;
            }
            return substituted;
        }
    } else if (type->kind == TYPE_FUNC) {
        Type* return_type = substitute_template_type(
            tmpl, type->ret_type, arguments, argument_count, value_args,
            value_present);
        TypeParam* parameters = NULL;
        TypeParam** tail = &parameters;
        bool changed = return_type != type->ret_type;
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            TypeParam* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *parameter;
            copy->type = substitute_template_type(
                tmpl, parameter->type, arguments, argument_count, value_args,
                value_present);
            copy->next = NULL;
            if (copy->type != parameter->type) changed = true;
            *tail = copy;
            tail = &copy->next;
        }
        if (changed) {
            substituted = ast_arena_alloc(sizeof(*substituted));
            *substituted = *type;
            substituted->ret_type = return_type;
            substituted->params = parameters;
            return substituted;
        }
    }
    return type;
}

static TypeParam* substitute_template_parameters(CxxTemplate* tmpl,
                                                  TypeParam* parameters,
                                                  Type** arguments,
                                                  int argument_count,
                                                  const int64_t* value_args,
                                                  const bool* value_present) {
    TypeParam* result = NULL;
    TypeParam** tail = &result;
    for (; parameters; parameters = parameters->next) {
        TypeParam* copy = ast_arena_alloc(sizeof(*copy));
        *copy = *parameters;
        copy->type = substitute_template_type(
            tmpl, parameters->type, arguments, argument_count,
            value_args, value_present);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return result;
}

static int cxx_template_pack_index_in_type(CxxTemplate* tmpl, Type* type);

static DeclList* substitute_template_decl_parameters(
    CxxTemplate* tmpl, DeclList* parameters, Type** arguments,
    int argument_count, const int64_t* value_args,
    const bool* value_present) {
    DeclList* result = NULL;
    int next_parameter_index = 0;
    for (; parameters; parameters = parameters->next) {
        Decl* parameter = parameters->decl;
        if (!parameter) continue;
        if (parameter->param_is_pack) {
            int pack_index = cxx_template_pack_index_in_type(
                tmpl, parameter->type);
            if (pack_index < 0 || !tmpl || tmpl->pending_pack_count < 0 ||
                tmpl->params[pack_index].kind != TPARAM_TYPE ||
                (tmpl->pending_pack_count > 0 && !tmpl->pending_pack_args)) {
                rcc_error(parameter->loc,
                          "local member parameter pack has no matching type pack");
                continue;
            }
            for (int pack_value = 0;
                 pack_value < tmpl->pending_pack_count; ++pack_value) {
                Type* expanded_arguments[32] = { NULL };
                int64_t expanded_values[32] = { 0 };
                bool expanded_value_present[32] = { false };
                Type* expanded_type;
                Decl* copy;
                char generated_name[64];
                int written;
                if (argument_count > (int)(sizeof(expanded_arguments) /
                                           sizeof(expanded_arguments[0]))) {
                    rcc_error(parameter->loc,
                              "local member parameter pack exceeds compiler limits");
                    break;
                }
                memcpy(expanded_arguments, arguments,
                       sizeof(Type*) * (size_t)argument_count);
                if (value_args && value_present) {
                    memcpy(expanded_values, value_args,
                           sizeof(int64_t) * (size_t)argument_count);
                    memcpy(expanded_value_present, value_present,
                           sizeof(bool) * (size_t)argument_count);
                }
                expanded_arguments[pack_index] =
                    tmpl->pending_pack_args[pack_value];
                expanded_type = substitute_template_type(
                    tmpl, parameter->type, expanded_arguments,
                    argument_count, expanded_values,
                    expanded_value_present);
                if (!expanded_type) {
                    rcc_error(parameter->loc,
                              "local member parameter pack substitution failed");
                    break;
                }
                written = snprintf(generated_name, sizeof(generated_name),
                                   "__rcc_pack_arg_%d", pack_value);
                if (written < 0 || (size_t)written >= sizeof(generated_name)) {
                    rcc_error(parameter->loc,
                              "local member parameter name exceeds compiler limits");
                    break;
                }
                copy = decl_param(parameter->name
                                      ? rcc_intern(generated_name) : NULL,
                                  expanded_type, next_parameter_index++,
                                  parameter->loc);
                decllist_append(&result, copy);
            }
            continue;
        }
        {
            Type* parameter_type = substitute_template_type(
                tmpl, parameter->type, arguments, argument_count,
                value_args, value_present);
            Decl* copy = decl_param(parameter->name, parameter_type,
                                    next_parameter_index++, parameter->loc);
            copy->param_array_type = substitute_template_type(
                tmpl, parameter->param_array_type, arguments,
                argument_count, value_args, value_present);
            copy->param_default = cxx_template_clone_expr_with_values(
                tmpl, parameter->param_default, arguments, argument_count,
                value_args, value_present);
            decllist_append(&result, copy);
        }
    }
    return result;
}

static CxxMethod* substitute_template_method(CxxTemplate* tmpl,
                                             CxxMethod* method,
                                             Type** arguments,
                                             int argument_count,
                                             const int64_t* value_args,
                                             const bool* value_present) {
    CxxMethod* copy;
    DeclList* parameters;
    DeclList* saved_pack_parameters;
    Type* return_type;
    if (!method || !method->decl || !method->decl->type) return NULL;
    parameters = substitute_template_decl_parameters(
        tmpl, method->decl->func_params, arguments, argument_count,
        value_args, value_present);
    return_type = substitute_template_type(
        tmpl, method->decl->type->ret_type, arguments, argument_count,
        value_args, value_present);
    copy = cxx_method_new(cxx_method_source_name(method), return_type, parameters,
                          method->decl->func_body, method->decl->loc);
    copy->decl->type->variadic = method->decl->type->variadic;
    copy->decl->type->has_prototype = method->decl->type->has_prototype;
    copy->decl->is_weak = method->decl->is_weak;
    copy->decl->func_is_inline = method->decl->func_is_inline;
    copy->decl->func_is_defined = method->decl->func_is_defined;
    copy->decl->func_is_template_instance =
        method->decl->func_is_template_instance;
    copy->decl->func_has_cxx_linkage =
        method->decl->func_has_cxx_linkage;
    copy->decl->func_has_local_linkage =
        method->decl->func_has_local_linkage;
    copy->decl->func_is_cxx_method = method->decl->func_is_cxx_method;
    copy->decl->func_friend_access = method->decl->func_friend_access;
    copy->decl->func_is_constexpr = method->decl->func_is_constexpr;
    copy->decl->func_is_consteval = method->decl->func_is_consteval;
    copy->decl->func_is_noreturn = method->decl->func_is_noreturn;
    copy->decl->func_is_nodiscard = method->decl->func_is_nodiscard;
    copy->decl->func_is_deprecated = method->decl->func_is_deprecated;
    copy->decl->func_deprecated_message =
        method->decl->func_deprecated_message;
    copy->decl->func_cxx_namespace = method->decl->func_cxx_namespace;
    copy->decl->func_cxx_namespace_scope =
        method->decl->func_cxx_namespace_scope;
    copy->access = method->access;
    copy->is_static = method->is_static;
    copy->is_virtual = method->is_virtual;
    copy->is_pure_virtual = method->is_pure_virtual;
    copy->is_override = method->is_override;
    copy->is_final = method->is_final;
    copy->is_const = method->is_const;
    copy->is_volatile = method->is_volatile;
    copy->ref_qualifier = method->ref_qualifier;
    copy->is_constexpr = method->is_constexpr;
    copy->is_explicit = method->is_explicit;
    copy->is_noexcept = method->is_noexcept;
    copy->decl->func_is_noexcept = copy->is_noexcept;
    copy->is_deleted = method->is_deleted;
    copy->is_defaulted = method->is_defaulted;
    copy->is_constructor = method->is_constructor;
    copy->is_destructor = method->is_destructor;
    copy->decl->func_is_cxx_constructor = copy->is_constructor;
    copy->decl->func_is_cxx_destructor = copy->is_destructor;
    copy->decl->func_is_auto_return = method->decl->func_is_auto_return;
    copy->decl->func_is_decltype_auto_return =
        method->decl->func_is_decltype_auto_return;
    copy->decl->func_noexcept_expr = cxx_template_clone_expr_with_values(
        tmpl, method->decl->func_noexcept_expr, arguments, argument_count,
        value_args, value_present);
    copy->vtable_index = method->vtable_index;
    saved_pack_parameters = tmpl->active_pack_parameters;
    tmpl->active_pack_parameters = method->decl->func_params;
    copy->decl->func_body = cxx_template_clone_stmt_with_values(
        tmpl, method->decl->func_body, arguments, argument_count,
        value_args, value_present);
    tmpl->active_pack_parameters = saved_pack_parameters;
    return copy;
}

static CxxMethod* instantiated_constructor_method(CxxClass* instance,
                                                   int ordinal) {
    for (struct CxxMember* member = instance ? instance->members : NULL;
         member; member = member->next) {
        if (!member->method || !member->method->is_constructor) continue;
        if (ordinal == 0) return member->method;
        --ordinal;
    }
    return NULL;
}

static TypeParam* constructor_parameters_from_method(DeclList* parameters,
                                                     int* parameter_count) {
    TypeParam* result = NULL;
    TypeParam** tail = &result;
    int count = 0;
    for (; parameters; parameters = parameters->next) {
        Decl* declaration = parameters->decl;
        TypeParam* copy;
        if (!declaration) continue;
        copy = ast_arena_alloc(sizeof(*copy));
        memset(copy, 0, sizeof(*copy));
        copy->name = declaration->name;
        copy->type = declaration->type;
        copy->initializer = declaration->param_default;
        copy->cxx_access = ACCESS_PUBLIC;
        *tail = copy;
        tail = &copy->next;
        ++count;
    }
    if (parameter_count) *parameter_count = count;
    return result;
}

/* Expand one trailing class-template parameter pack while preserving any
 * fixed parameters declared before it.  A pack in any other position remains
 * diagnosed by the parser because its explicit argument boundary is not
 * recoverable in this bounded lowering path. */
static int cxx_class_pack_index(CxxTemplate* tmpl) {
    int pack_index = -1;
    if (!tmpl || tmpl->kind != TMPL_CLASS) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (!tmpl->params[index].is_pack) continue;
        if (pack_index >= 0 ||
            (!tmpl->is_local_class_template &&
             index != tmpl->param_count - 1)) {
            return -1;
        }
        pack_index = index;
    }
    return pack_index;
}

static int cxx_template_pack_index_in_type(CxxTemplate* tmpl, Type* type) {
    if (!tmpl || !type) return -1;
    if (type->kind == TYPE_STRUCT && type->tag) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->is_pack &&
                parameter->name && strcmp(parameter->name, type->tag) == 0) {
                return index;
            }
        }
    }
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) {
        return cxx_template_pack_index_in_type(tmpl, type->base);
    }
    if (type->cxx_is_member_pointer) {
        return cxx_template_pack_index_in_type(
            tmpl, type->cxx_member_pointer_owner);
    }
    if (type->kind == TYPE_FUNC) {
        int index = cxx_template_pack_index_in_type(tmpl, type->ret_type);
        if (index >= 0) return index;
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            index = cxx_template_pack_index_in_type(tmpl, parameter->type);
            if (index >= 0) return index;
        }
    }
    for (int index = 0; index < type->cxx_template_arg_count; ++index) {
        int pack_index = cxx_template_pack_index_in_type(
            tmpl, type->cxx_template_args ? type->cxx_template_args[index]
                                         : NULL);
        if (pack_index >= 0) return pack_index;
    }
    return -1;
}

static bool class_template_instance_matches(
    CxxTemplate* tmpl, int instance_index, Type** arguments,
    const int64_t* value_args, const bool* value_present, int argument_count,
    int pack_index) {
    int expected_pack_count;
    if (!tmpl || instance_index < 0 || instance_index >= tmpl->instance_count ||
        tmpl->instances[instance_index].arg_count != argument_count) {
        return false;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->is_pack) continue;
        if (parameter->kind == TPARAM_NONTYPE) {
            if (!value_args || !value_present || !value_present[index] ||
                !tmpl->instances[instance_index].value_args ||
                !tmpl->instances[instance_index].value_present ||
                !tmpl->instances[instance_index].value_present[index] ||
                tmpl->instances[instance_index].value_args[index] !=
                    value_args[index]) {
                return false;
            }
        } else if (parameter->kind == TPARAM_TEMPLATE) {
            if (!arguments || !arguments[index] ||
                !tmpl->instances[instance_index].args ||
                !tmpl->instances[instance_index].args[index] ||
                tmpl->instances[instance_index].args[index]->cxx_template !=
                    arguments[index]->cxx_template) {
                return false;
            }
        } else if (!arguments || !arguments[index] ||
                   !tmpl->instances[instance_index].args ||
                   !tmpl->instances[instance_index].args[index] ||
                   !type_is_compatible(
                       tmpl->instances[instance_index].args[index],
                       arguments[index])) {
            return false;
        }
    }
    if (pack_index < 0) return true;
    expected_pack_count = tmpl->is_local_class_template &&
            tmpl->pending_pack_count >= 0
        ? tmpl->pending_pack_count : argument_count - pack_index;
    if (tmpl->instances[instance_index].pack_count != expected_pack_count) {
        return false;
    }
    if (tmpl->params[pack_index].kind == TPARAM_TYPE) {
        for (int index = 0; index < expected_pack_count; ++index) {
            Type* expected = tmpl->is_local_class_template &&
                    tmpl->pending_pack_count >= 0
                ? tmpl->pending_pack_args[index]
                : arguments[pack_index + index];
            if (!expected || !tmpl->instances[instance_index].pack_args ||
                !type_is_compatible(
                    tmpl->instances[instance_index].pack_args[index],
                    expected)) {
                return false;
            }
        }
    } else {
        for (int index = 0; index < expected_pack_count; ++index) {
            int64_t expected = tmpl->is_local_class_template &&
                    tmpl->pending_pack_count >= 0
                ? tmpl->pending_pack_values[index]
                : value_args[pack_index + index];
            bool expected_present = tmpl->is_local_class_template &&
                    tmpl->pending_pack_count >= 0
                ? tmpl->pending_pack_value_present[index]
                : value_present[pack_index + index];
            if (!expected_present ||
                !tmpl->instances[instance_index].pack_values ||
                !tmpl->instances[instance_index].pack_value_present ||
                !tmpl->instances[instance_index].pack_value_present[index] ||
                tmpl->instances[instance_index].pack_values[index] != expected) {
                return false;
            }
        }
    }
    return true;
}

static int cxx_template_base_pack_pattern(CxxTemplate* tmpl,
                                         CxxClass* definition,
                                         const char* initializer_name,
                                         int* pack_index) {
    int matched_base = -1;
    int matched_pack = -1;
    if (pack_index) *pack_index = -1;
    for (int index = 0; definition && index < definition->base_count;
         ++index) {
        int candidate;
        if (!definition->bases[index].is_pack_expansion ||
            !definition->bases[index].type_pattern ||
            !cxx_constructor_base_name_matches(definition, index,
                                               initializer_name)) {
            continue;
        }
        candidate = cxx_template_pack_index_in_type(
            tmpl, definition->bases[index].type_pattern);
        if (candidate < 0 || matched_base >= 0) {
            return -1;
        }
        matched_base = index;
        matched_pack = candidate;
    }
    if (pack_index) *pack_index = matched_pack;
    return matched_base;
}

static void cxx_template_class_add_resolved_base(
    CxxClass* instance, Type* resolved, const char* base_name,
    AccessSpec access, bool is_virtual, SourceLoc loc) {
    CxxClass* base = resolved ? resolved->cxx_class : NULL;
    if (!resolved || resolved->kind != TYPE_STRUCT ||
        resolved->cxx_dependent || !base) {
        rcc_error(loc,
                  "dependent class-template base did not resolve to a class type");
        return;
    }
    if (base == instance || base->is_final) {
        rcc_error(loc, base == instance
                           ? "a class cannot derive from itself"
                           : "cannot derive from a final class");
        return;
    }
    for (int index = 0; instance && index < instance->base_count; ++index) {
        if (instance->bases[index].base == base) {
            rcc_error(loc,
                      "a class cannot name the same direct base twice");
            return;
        }
    }
    cxx_class_add_base_pattern(instance, resolved, base_name, access,
                               is_virtual, false);
}

static const char* cxx_method_pack_name(DeclList* parameters,
                                        CxxTemplate* tmpl,
                                        int pack_index) {
    for (; parameters; parameters = parameters->next) {
        Decl* parameter = parameters->decl;
        if (parameter && parameter->param_is_pack && parameter->name &&
            cxx_template_pack_index_in_type(tmpl, parameter->type) ==
                pack_index) {
            return parameter->name;
        }
    }
    return NULL;
}

static Type* instantiate_class_template(CxxTemplate* tmpl, Type** arguments,
                                        const int64_t* value_args,
                                        const bool* value_present,
                                        int argument_count, SourceLoc loc) {
    CxxClass* definition;
    CxxClass* instance;
    char tag[320];
    int index;
    uint32_t constructor_mask;
    bool has_value_parameters = false;
    CxxClass* saved_local_class_instance;
    CxxClass* saved_active_class_instance;
    int pack_index = cxx_class_pack_index(tmpl);

    if (!tmpl || tmpl->kind != TMPL_CLASS || !tmpl->templated_class ||
        (tmpl->is_local_class_template &&
         argument_count != tmpl->param_count) ||
        (pack_index < 0 && argument_count != tmpl->param_count) ||
        (pack_index >= 0 &&
         (argument_count < pack_index || argument_count > 32))) {
        rcc_error(loc, "class template argument count mismatch");
        return type_struct(tmpl && tmpl->name ? tmpl->name : "template");
    }
    for (index = 0; index < argument_count; ++index) {
        int parameter_index = !tmpl->is_local_class_template &&
                pack_index >= 0 && index >= pack_index
            ? pack_index : index;
        TemplateParam* parameter = &tmpl->params[parameter_index];
        if (tmpl->is_local_class_template && parameter->is_pack) {
            if (tmpl->pending_pack_count < 0 ||
                (parameter->kind == TPARAM_TYPE &&
                 tmpl->pending_pack_count > 0 && !tmpl->pending_pack_args) ||
                (parameter->kind == TPARAM_NONTYPE &&
                 tmpl->pending_pack_count > 0 &&
                 (!tmpl->pending_pack_values ||
                  !tmpl->pending_pack_value_present))) {
                rcc_error(loc,
                          "local class template pack arguments are missing");
                return NULL;
            }
            if (parameter->kind == TPARAM_NONTYPE) {
                has_value_parameters = true;
            }
            continue;
        }
        if (parameter->kind == TPARAM_NONTYPE) {
            has_value_parameters = true;
        }
        if (parameter->kind == TPARAM_TYPE &&
            (!arguments || !arguments[index])) {
            rcc_error(loc,
                      "class template type argument %d is missing", index + 1);
            return NULL;
        }
        if (parameter->kind == TPARAM_NONTYPE &&
            (!value_args || !value_present || !value_present[index] ||
             !parameter->type || !type_is_integer(parameter->type))) {
            rcc_error(loc,
                      "class template non-type argument %d requires an "
                      "integer constant", index + 1);
            return NULL;
        }
        if (parameter->kind == TPARAM_TEMPLATE) {
            if (!arguments || !arguments[index] ||
                !arguments[index]->cxx_template ||
                !template_template_signature_matches(
                    parameter, arguments[index]->cxx_template)) {
                rcc_error(loc, "class template template argument is invalid");
                return NULL;
            }
        }
    }
    if (!cxx_template_constraint_satisfied(
            tmpl, arguments, value_args, value_present, loc, true, NULL)) {
        return type_int;
    }
    for (index = 0; index < tmpl->instance_count; ++index) {
        if (class_template_instance_matches(
                tmpl, index, arguments, value_args, value_present,
                argument_count, pack_index)) {
            return ((CxxClass*)tmpl->instances[index].instantiated)->type;
        }
    }

    definition = tmpl->templated_class;
    if (tmpl->specialization_arg_count > 0) {
        char specialization_suffix[256] = "";
        size_t suffix_length = 0;
        for (int argument_index = 0;
             argument_index < tmpl->specialization_arg_count;
             ++argument_index) {
            const char* mangled = tmpl->specialization_value_args &&
                    tmpl->specialization_value_args[argument_index]
                ? "v"
                : cxx_mangle_type(tmpl->specialization_args[argument_index]);
            int written = snprintf(
                specialization_suffix + suffix_length,
                sizeof(specialization_suffix) - suffix_length,
                "%s%s", argument_index == 0 ? "" : "_",
                mangled ? mangled : "?");
            if (written < 0 || (size_t)written >=
                                   sizeof(specialization_suffix) -
                                       suffix_length) {
                rcc_error(loc,
                          "class template specialization pattern is too long");
                return NULL;
            }
            suffix_length += (size_t)written;
        }
        if (snprintf(tag, sizeof(tag), "%s.__instance%d.%s",
                     tmpl->name ? tmpl->name : "template",
                     tmpl->instance_count, specialization_suffix) >=
            (int)sizeof(tag)) {
            rcc_error(loc, "class template specialization name is too long");
            return NULL;
        }
    } else if (snprintf(tag, sizeof(tag), "%s.__instance%d",
                        tmpl->name ? tmpl->name : "template",
                        tmpl->instance_count) >= (int)sizeof(tag)) {
        rcc_error(loc, "class template specialization name is too long");
        return NULL;
    }
    instance = cxx_class_new(ast_arena_strdup(tag), loc);
    instance->is_struct = definition->is_struct;
    instance->is_final = definition->is_final;
    instance->ns = definition->ns;
    instance->has_user_constructor = definition->has_user_constructor;
    instance->has_nonpublic_field = definition->has_nonpublic_field;
    instance->has_static_field = definition->has_static_field;
    instance->has_field_initializer = definition->has_field_initializer;
    instance->explicit_alignment = definition->explicit_alignment;
    instance->pack_alignment = definition->pack_alignment;
    instance->using_base_member_count = definition->using_base_member_count;
    if (definition->using_base_member_count != 0) {
        instance->using_base_members = ast_arena_alloc(
            sizeof(instance->using_base_members[0]) *
            (size_t)definition->using_base_member_count);
        memcpy(instance->using_base_members, definition->using_base_members,
               sizeof(instance->using_base_members[0]) *
               (size_t)definition->using_base_member_count);
    }
    instance->friend_class_count = definition->friend_class_count;
    if (definition->friend_class_count != 0) {
        instance->friend_class_names = ast_arena_alloc(
            sizeof(instance->friend_class_names[0]) *
            (size_t)definition->friend_class_count);
        memcpy(instance->friend_class_names, definition->friend_class_names,
               sizeof(instance->friend_class_names[0]) *
               (size_t)definition->friend_class_count);
    }
    instance->templ = tmpl;
    instance->template_arg_count = argument_count;
    if (argument_count > 0) {
        instance->template_args = ast_arena_alloc(
            sizeof(Type*) * (size_t)argument_count);
        memcpy(instance->template_args, arguments,
               sizeof(Type*) * (size_t)argument_count);
    }
    if (has_value_parameters) {
        if (argument_count > 0) {
            instance->template_value_args = ast_arena_alloc(
                sizeof(int64_t) * (size_t)argument_count);
            instance->template_value_present = ast_arena_alloc(
                sizeof(bool) * (size_t)argument_count);
            memcpy(instance->template_value_args, value_args,
                   sizeof(int64_t) * (size_t)argument_count);
            memcpy(instance->template_value_present, value_present,
                   sizeof(bool) * (size_t)argument_count);
        }
    }
    if (pack_index >= 0) {
        int actual_pack_count = tmpl->is_local_class_template &&
                tmpl->pending_pack_count >= 0
            ? tmpl->pending_pack_count : argument_count - pack_index;
        instance->template_pack_count = actual_pack_count;
        if (tmpl->params[pack_index].kind == TPARAM_TYPE) {
            if (!tmpl->is_local_class_template && argument_count > 0) {
                instance->template_args = ast_arena_alloc(
                    sizeof(Type*) * (size_t)argument_count);
                memcpy(instance->template_args, arguments,
                       sizeof(Type*) * (size_t)argument_count);
            }
        } else if (actual_pack_count > 0) {
            instance->template_pack_values = ast_arena_alloc(
                sizeof(int64_t) * (size_t)actual_pack_count);
            instance->template_pack_value_present = ast_arena_alloc(
                sizeof(bool) * (size_t)actual_pack_count);
            if (tmpl->is_local_class_template &&
                tmpl->pending_pack_count >= 0) {
                memcpy(instance->template_pack_values,
                       tmpl->pending_pack_values,
                       sizeof(int64_t) * (size_t)actual_pack_count);
                memcpy(instance->template_pack_value_present,
                       tmpl->pending_pack_value_present,
                       sizeof(bool) * (size_t)actual_pack_count);
            } else {
                memcpy(instance->template_pack_values,
                       value_args + pack_index,
                       sizeof(int64_t) * (size_t)actual_pack_count);
                memcpy(instance->template_pack_value_present,
                       value_present + pack_index,
                       sizeof(bool) * (size_t)actual_pack_count);
            }
        }
    }
    if (tmpl->primary_template &&
        tmpl->specialization_arg_count ==
            tmpl->primary_template->param_count) {
        Type* identity_arguments[32] = { NULL };
        int64_t identity_values[32] = { 0 };
        bool identity_present[32] = { false };
        for (int identity_index = 0;
             identity_index < tmpl->primary_template->param_count;
             ++identity_index) {
            TemplateParam* parameter =
                &tmpl->primary_template->params[identity_index];
            if (parameter->kind == TPARAM_TYPE) {
                identity_arguments[identity_index] = substitute_template_type(
                    tmpl, tmpl->specialization_args[identity_index],
                    arguments, argument_count, value_args, value_present);
            } else if (parameter->kind == TPARAM_NONTYPE &&
                       tmpl->specialization_value_args &&
                       tmpl->specialization_value_args[identity_index]) {
                if (eval_template_integer_expression(
                        tmpl->specialization_value_args[identity_index],
                        tmpl,
                        value_args, value_present,
                        &identity_values[identity_index])) {
                    identity_present[identity_index] = true;
                } else {
                    rcc_error(loc,
                              "class template specialization value is not "
                              "an integer constant expression");
                }
            }
        }
        cxx_set_template_identity(
            instance, tmpl->primary_template, identity_arguments,
            identity_values, identity_present,
            tmpl->primary_template->param_count);
    } else {
        cxx_set_template_identity(instance, tmpl, arguments, value_args,
                                  value_present, argument_count);
    }

    if (tmpl->is_local_class_template && tmpl->local_class_pattern) {
        char identity[1024];
        size_t identity_length;
        const char* source_identity =
            tmpl->local_class_pattern->type->cxx_scope_identity;
        int written = snprintf(identity, sizeof(identity), "%s",
                               source_identity ? source_identity : tmpl->name);
        if (written < 0 || (size_t)written >= sizeof(identity)) {
            rcc_error(loc, "local class specialization identity is too long");
            return NULL;
        }
        identity_length = (size_t)written;
        for (int identity_index = 0;
             identity_index < tmpl->param_count; ++identity_index) {
            char argument_identity[384];
            TemplateParam* parameter = &tmpl->params[identity_index];
            if (parameter->is_pack && tmpl->pending_pack_count >= 0) {
                written = snprintf(argument_identity,
                                   sizeof(argument_identity), "_P%d",
                                   tmpl->pending_pack_count);
                if (written < 0 ||
                    (size_t)written >= sizeof(argument_identity) ||
                    (size_t)written >= sizeof(identity) - identity_length) {
                    rcc_error(loc,
                              "local class specialization identity exceeds compiler limits");
                    return NULL;
                }
                memcpy(identity + identity_length, argument_identity,
                       (size_t)written + 1u);
                identity_length += (size_t)written;
                for (int pack_value = 0;
                     pack_value < tmpl->pending_pack_count; ++pack_value) {
                    if (parameter->kind == TPARAM_NONTYPE) {
                        written = snprintf(argument_identity,
                                           sizeof(argument_identity), "_V%lld",
                                           (long long)tmpl->pending_pack_values[
                                               pack_value]);
                    } else {
                        char* mangled = cxx_mangle_type(
                            tmpl->pending_pack_args[pack_value]);
                        written = snprintf(argument_identity,
                                           sizeof(argument_identity), "_T%s",
                                           mangled ? mangled : "unknown");
                    }
                    if (written < 0 ||
                        (size_t)written >= sizeof(argument_identity) ||
                        (size_t)written >= sizeof(identity) - identity_length) {
                        rcc_error(loc,
                                  "local class specialization identity exceeds compiler limits");
                        return NULL;
                    }
                    memcpy(identity + identity_length, argument_identity,
                           (size_t)written + 1u);
                    identity_length += (size_t)written;
                }
                continue;
            }
            if (parameter->kind == TPARAM_NONTYPE) {
                written = snprintf(argument_identity,
                                   sizeof(argument_identity), "_V%lld",
                                   (long long)value_args[identity_index]);
            } else {
                char* mangled = cxx_mangle_type(arguments[identity_index]);
                written = snprintf(argument_identity,
                                   sizeof(argument_identity), "_T%s",
                                   mangled ? mangled : "unknown");
            }
            if (written < 0 || (size_t)written >= sizeof(argument_identity) ||
                (size_t)written >= sizeof(identity) - identity_length) {
                rcc_error(loc,
                          "local class specialization identity exceeds compiler limits");
                return NULL;
            }
            memcpy(identity + identity_length, argument_identity,
                   (size_t)written + 1u);
            identity_length += (size_t)written;
        }
        instance->type->cxx_scope_identity = rcc_intern(identity);
    }

    int saved_pending_pack_count = tmpl->pending_pack_count;
    Type** saved_pending_pack_args = tmpl->pending_pack_args;
    int64_t* saved_pending_pack_values = tmpl->pending_pack_values;
    bool* saved_pending_pack_value_present = tmpl->pending_pack_value_present;
    saved_active_class_instance = tmpl->active_class_instance;
    tmpl->active_class_instance = instance;
    saved_local_class_instance = tmpl->local_class_instance;
    if (tmpl->is_local_class_template) {
        tmpl->local_class_instance = instance;
    }
    if (pack_index >= 0 &&
        !(tmpl->is_local_class_template &&
          tmpl->pending_pack_count >= 0)) {
        tmpl->pending_pack_args = tmpl->params[pack_index].kind == TPARAM_TYPE
            ? arguments + pack_index : NULL;
        tmpl->pending_pack_values = tmpl->params[pack_index].kind == TPARAM_NONTYPE
            ? (int64_t*)value_args + pack_index : NULL;
        tmpl->pending_pack_value_present =
            tmpl->params[pack_index].kind == TPARAM_NONTYPE
                ? (bool*)value_present + pack_index : NULL;
        tmpl->pending_pack_count = argument_count - pack_index;
    }
    for (int base_index = 0; base_index < definition->base_count;
         ++base_index) {
        Type* pattern = definition->bases[base_index].type_pattern;
        if (!pattern) {
            CxxClass* base = definition->bases[base_index].base;
            if (!base) {
                rcc_error(loc,
                          "local class base was not resolved before specialization");
                continue;
            }
            cxx_template_class_add_resolved_base(
                instance, base->type, definition->bases[base_index].base_name,
                definition->bases[base_index].access,
                definition->bases[base_index].is_virtual, loc);
            continue;
        }
        if (definition->bases[base_index].is_pack_expansion) {
            int base_pack_index = cxx_template_pack_index_in_type(tmpl,
                                                                  pattern);
            if (base_pack_index < 0 ||
                tmpl->params[base_pack_index].kind != TPARAM_TYPE ||
                tmpl->pending_pack_count < 0 ||
                (tmpl->pending_pack_count > 0 &&
                 !tmpl->pending_pack_args)) {
                rcc_error(loc,
                          "local base pack expansion requires one type parameter pack");
                continue;
            }
            for (int pack_value = 0;
                 pack_value < tmpl->pending_pack_count; ++pack_value) {
                Type* expanded_arguments[32] = { NULL };
                int64_t expanded_values[32] = { 0 };
                bool expanded_value_present[32] = { false };
                Type* resolved;
                if (argument_count >
                    (int)(sizeof(expanded_arguments) /
                          sizeof(expanded_arguments[0]))) {
                    rcc_error(loc,
                              "local base pack substitution exceeds compiler limits");
                    break;
                }
                memcpy(expanded_arguments, arguments,
                       sizeof(Type*) * (size_t)argument_count);
                if (value_args && value_present) {
                    memcpy(expanded_values, value_args,
                           sizeof(int64_t) * (size_t)argument_count);
                    memcpy(expanded_value_present, value_present,
                           sizeof(bool) * (size_t)argument_count);
                }
                expanded_arguments[base_pack_index] =
                    tmpl->pending_pack_args[pack_value];
                resolved = substitute_template_type(
                    tmpl, pattern, expanded_arguments, argument_count,
                    expanded_values, expanded_value_present);
                cxx_template_class_add_resolved_base(
                    instance, resolved,
                    definition->bases[base_index].base_name,
                    definition->bases[base_index].access,
                    definition->bases[base_index].is_virtual, loc);
            }
            continue;
        }
        if (cxx_template_pack_index_in_type(tmpl, pattern) >= 0) {
            rcc_error(loc,
                      "a base type parameter pack requires an ellipsis");
            continue;
        }
        {
            Type* resolved = substitute_template_type(
                tmpl, pattern, arguments, argument_count,
                value_args, value_present);
            cxx_template_class_add_resolved_base(
                instance, resolved,
                definition->bases[base_index].base_name,
                definition->bases[base_index].access,
                definition->bases[base_index].is_virtual, loc);
        }
    }
    for (int using_index = 0;
         using_index < instance->using_base_member_count; ++using_index) {
        Type* pattern = instance->using_base_members[using_index]
                            .base_type_pattern;
        Type* resolved;
        CxxClass* used_base;
        bool is_direct_base = false;
        if (!pattern) continue;
        resolved = substitute_template_type(
            tmpl, pattern, arguments, argument_count, value_args,
            value_present);
        used_base = resolved ? resolved->cxx_class : NULL;
        if (!resolved || resolved->kind != TYPE_STRUCT ||
            resolved->cxx_dependent || !used_base) {
            rcc_error(loc,
                      "dependent base using-declaration did not resolve to a class type");
            continue;
        }
        for (int base_index = 0; base_index < instance->base_count;
             ++base_index) {
            if (instance->bases[base_index].base == used_base) {
                is_direct_base = true;
                break;
            }
        }
        if (!is_direct_base) {
            rcc_error(loc,
                      "dependent base using-declaration must name a direct base");
            continue;
        }
        {
            const char* member_name = instance->using_base_members[using_index]
                                          .member_name;
            const char* template_name = used_base->templ
                ? used_base->templ->name : NULL;
            const char* member_tail = cxx_unqualified_name(member_name);
            const char* base_tail = cxx_unqualified_name(used_base->name);
            const char* template_tail = cxx_unqualified_name(template_name);
            if (member_tail &&
                ((base_tail && strcmp(member_tail, base_tail) == 0) ||
                 (template_tail && strcmp(member_tail, template_tail) == 0))) {
                instance->using_base_members[using_index].member_name =
                    used_base->name;
            }
        }
        instance->using_base_members[using_index].base_name = used_base->name;
        instance->using_base_members[using_index].base_type_pattern = NULL;
    }
    for (CxxTypeAlias* alias = definition->type_aliases; alias;
         alias = alias->next) {
        cxx_class_add_type_alias(
            instance, alias->name,
            substitute_template_type(tmpl, alias->type, arguments,
                                     argument_count, value_args, value_present),
            alias->access);
    }
    for (TypeParam* field = definition->fields; field; field = field->next) {
    cxx_class_add_field_initializer(
        instance, field->name,
            substitute_template_type(
                tmpl, field->type, arguments, argument_count,
                value_args, value_present),
            (AccessSpec)field->cxx_access,
            cxx_template_clone_expr_with_values(
                tmpl, field->initializer, arguments, argument_count,
                value_args, value_present),
            field->is_bitfield, field->bit_width, field->is_static,
            field->is_deprecated, field->deprecated_message,
            field->cxx_no_unique_address);
    }
    register_instantiated_class_static_fields(instance, definition);
    for (struct CxxMember* member = definition->members; member;
         member = member->next) {
        CxxMethod* method = substitute_template_method(
            tmpl, member->method, arguments, argument_count,
            value_args, value_present);
        if (method) {
            method->owner = instance;
            method->decl->func_method_owner = instance->type;
            cxx_class_add_method(instance, method);
        }
    }
    {
        int constructor_ordinal = 0;
        for (CxxConstructorInfo* constructor = definition->constructors;
             constructor; constructor = constructor->next) {
        CxxConstructorInfo* copy = ast_arena_alloc(sizeof(*copy));
        CxxConstructorInfo** tail = &instance->constructors;
        CxxConstructorInitializer** initializer_tail;
        *copy = *constructor;
        copy->method = instantiated_constructor_method(instance,
                                                       constructor_ordinal++);
        if (copy->method && copy->method->decl) {
            copy->parameters = constructor_parameters_from_method(
                copy->method->decl->func_params, &copy->parameter_count);
        } else {
            copy->parameters = substitute_template_parameters(
                tmpl, constructor->parameters, arguments, argument_count,
                value_args, value_present);
        }
        copy->initializers = NULL;
        copy->initializer_count = 0;
        initializer_tail = &copy->initializers;
        for (CxxConstructorInitializer* initializer = constructor->initializers;
             initializer; initializer = initializer->next) {
            int base_pack_index = -1;
            int base_pattern_index = initializer->is_pack_expansion
                ? cxx_template_base_pack_pattern(
                      tmpl, definition, initializer->field,
                      &base_pack_index)
                : -1;
            int expansion_count = initializer->is_pack_expansion
                ? tmpl->pending_pack_count : 1;
            const char* parameter_pack_name = NULL;
            if (initializer->is_pack_expansion) {
                if (base_pattern_index < 0 || base_pack_index < 0 ||
                    base_pack_index >= tmpl->param_count ||
                    tmpl->params[base_pack_index].kind != TPARAM_TYPE ||
                    expansion_count < 0 ||
                    (expansion_count > 0 && !tmpl->pending_pack_args)) {
                    rcc_error(loc,
                              "constructor initializer pack must expand one class base type pack");
                    copy->initializers_are_supported = false;
                    continue;
                }
                parameter_pack_name = cxx_method_pack_name(
                    constructor->method && constructor->method->decl
                        ? constructor->method->decl->func_params : NULL,
                    tmpl, base_pack_index);
            }
            for (int pack_value = 0; pack_value < expansion_count;
                 ++pack_value) {
                CxxConstructorInitializer* initializer_copy =
                    ast_arena_alloc(sizeof(*initializer_copy));
                *initializer_copy = *initializer;
                initializer_copy->arguments = NULL;
                initializer_copy->is_pack_expansion = false;
                if (initializer->is_pack_expansion) {
                    Type* expanded_arguments[32] = { NULL };
                    int64_t expanded_values[32] = { 0 };
                    bool expanded_value_present[32] = { false };
                    Type* resolved_base;
                    Type** substitution_arguments = arguments;
                    DeclList* saved_pack_parameters =
                        tmpl->active_pack_parameters;
                    if (argument_count >
                        (int)(sizeof(expanded_arguments) /
                              sizeof(expanded_arguments[0]))) {
                        rcc_error(loc,
                                  "constructor base pack exceeds compiler limits");
                        copy->initializers_are_supported = false;
                        break;
                    }
                    if (argument_count > 0) {
                        memcpy(expanded_arguments, arguments,
                               sizeof(Type*) * (size_t)argument_count);
                        if (value_args && value_present) {
                            memcpy(expanded_values, value_args,
                                   sizeof(int64_t) * (size_t)argument_count);
                            memcpy(expanded_value_present, value_present,
                                   sizeof(bool) * (size_t)argument_count);
                        }
                        expanded_arguments[base_pack_index] =
                            tmpl->pending_pack_args[pack_value];
                        substitution_arguments = expanded_arguments;
                    }
                    resolved_base = substitute_template_type(
                        tmpl,
                        definition->bases[base_pattern_index].type_pattern,
                        substitution_arguments, argument_count,
                        value_args && value_present
                            ? expanded_values : value_args,
                        value_args && value_present
                            ? expanded_value_present : value_present);
                    if (!resolved_base || resolved_base->cxx_dependent ||
                        !resolved_base->cxx_class) {
                        rcc_error(loc,
                                  "constructor base pack did not resolve to a class type");
                        copy->initializers_are_supported = false;
                        continue;
                    }
                    initializer_copy->field =
                        rcc_intern(resolved_base->cxx_class->name);
                    if (parameter_pack_name) {
                        tmpl->active_pack_parameters =
                            constructor->method->decl->func_params;
                    }
                    for (ExprList* argument = initializer->arguments;
                         argument; argument = argument->next) {
                        Expr* cloned_argument = parameter_pack_name
                            ? cxx_template_clone_pack_expansion(
                                  tmpl, argument->expr, parameter_pack_name,
                                  pack_value, substitution_arguments,
                                  argument_count,
                                  value_args && value_present
                                      ? expanded_values : value_args,
                                  value_args && value_present
                                      ? expanded_value_present : value_present)
                            : cxx_template_clone_expr_with_values(
                                  tmpl, argument->expr,
                                  substitution_arguments, argument_count,
                                  value_args, value_present);
                        exprlist_append(&initializer_copy->arguments,
                                        cloned_argument);
                    }
                    if (parameter_pack_name) {
                        tmpl->active_pack_parameters = saved_pack_parameters;
                    }
                    initializer_copy->value = initializer_copy->arguments
                        ? initializer_copy->arguments->expr : NULL;
                } else {
                    if (initializer->base_type_pattern) {
                        Type* resolved_base = substitute_template_type(
                            tmpl, initializer->base_type_pattern, arguments,
                            argument_count, value_args, value_present);
                        if (!resolved_base ||
                            resolved_base->kind != TYPE_STRUCT ||
                            resolved_base->cxx_dependent ||
                            !resolved_base->cxx_class) {
                            rcc_error(loc,
                                      "constructor base initializer type did not resolve to a class");
                            copy->initializers_are_supported = false;
                            continue;
                        }
                        initializer_copy->field =
                            rcc_intern(resolved_base->cxx_class->name);
                    }
                    initializer_copy->value =
                        cxx_template_clone_expr_with_values(
                            tmpl, initializer->value, arguments,
                            argument_count, value_args, value_present);
                    for (ExprList* argument = initializer->arguments;
                         argument; argument = argument->next) {
                        exprlist_append(
                            &initializer_copy->arguments,
                            cxx_template_clone_expr_with_values(
                                tmpl, argument->expr, arguments,
                                argument_count, value_args, value_present));
                    }
                }
                /* The cloned initializer is re-resolved against the concrete
                 * bases and constructors after the specialized class layout
                 * is complete; the pattern's constructor belongs to the
                 * unspecialized class. */
                initializer_copy->base_type_pattern = NULL;
                initializer_copy->constructor = NULL;
                initializer_copy->next = NULL;
                *initializer_tail = initializer_copy;
                initializer_tail = &initializer_copy->next;
                ++copy->initializer_count;
            }
        }
        if (copy->method && !copy->body_is_empty && !copy->initializers) {
            (void)lowerable_constructor_body(instance, copy);
        }
        copy->next = NULL;
        while (*tail) tail = &(*tail)->next;
        *tail = copy;
        }
    }
    tmpl->local_class_instance = saved_local_class_instance;
    tmpl->active_class_instance = saved_active_class_instance;
    tmpl->pending_pack_args = saved_pending_pack_args;
    tmpl->pending_pack_values = saved_pending_pack_values;
    tmpl->pending_pack_value_present = saved_pending_pack_value_present;
    tmpl->pending_pack_count = saved_pending_pack_count;

    validate_class_virtual_specifiers(instance, loc);
    cxx_class_compute_layout(instance);
    if (instance->explicit_alignment > 0) {
        cxx_class_apply_explicit_alignment(
            instance, instance->explicit_alignment, loc);
    }
    complete_cxx_default_member_initializers(instance);
    cxx_class_build_vtable(instance);
    /* Template instances are normally kept only on their CxxTemplate.  The
     * object-image backend walks namespace classes when emitting vtables and
     * virtual-base offset tables, so publish instances that own ABI tables
     * into the same namespace registry as non-template classes. */
    if (instance->ns &&
        (instance->virtual_base_count > 0 || instance->vtable_size > 0 ||
         instance->secondary_vtable_count > 0)) {
        cxx_namespace_add_class(instance->ns, instance);
    }
    diagnose_unlowered_destructors(instance);
    register_inline_class_accessors(instance);
    register_inline_class_bool_delegates(instance);
    register_inline_class_cleanup(instance);
    register_inline_class_releases(instance);
    register_inline_class_closes(instance);
    register_inline_class_close_delegates(instance);
    register_inline_class_move_constructor(instance);
    register_inline_class_move_assignment(instance);
    register_ordinary_class_methods(instance);
    constructor_mask = lowerable_constructor_arity_mask(instance);
    if (constructor_mask != 0u) {
        rcc_parser_define_cxx_constructor_type(instance->name,
                                               instance->type,
                                               constructor_mask);
    }

    tmpl->instances = ast_arena_grow(
        tmpl->instances,
        sizeof(tmpl->instances[0]) * (size_t)tmpl->instance_count,
        sizeof(tmpl->instances[0]) * (size_t)(tmpl->instance_count + 1));
    tmpl->instances[tmpl->instance_count].args = instance->template_args;
    tmpl->instances[tmpl->instance_count].value_args = NULL;
    tmpl->instances[tmpl->instance_count].value_present = NULL;
    tmpl->instances[tmpl->instance_count].pack_args = NULL;
    tmpl->instances[tmpl->instance_count].pack_values = NULL;
    tmpl->instances[tmpl->instance_count].pack_value_present = NULL;
    tmpl->instances[tmpl->instance_count].pack_count = 0;
    if (has_value_parameters) {
        tmpl->instances[tmpl->instance_count].value_args = ast_arena_alloc(
            sizeof(int64_t) * (size_t)argument_count);
        tmpl->instances[tmpl->instance_count].value_present = ast_arena_alloc(
            sizeof(bool) * (size_t)argument_count);
        memcpy(tmpl->instances[tmpl->instance_count].value_args, value_args,
               sizeof(int64_t) * (size_t)argument_count);
        memcpy(tmpl->instances[tmpl->instance_count].value_present,
               value_present, sizeof(bool) * (size_t)argument_count);
    }
    if (pack_index >= 0) {
        int actual_pack_count = tmpl->is_local_class_template &&
                tmpl->pending_pack_count >= 0
            ? tmpl->pending_pack_count : argument_count - pack_index;
        tmpl->instances[tmpl->instance_count].pack_count = actual_pack_count;
        if (actual_pack_count > 0 &&
            tmpl->params[pack_index].kind == TPARAM_TYPE) {
            Type** pack_args = tmpl->is_local_class_template &&
                    tmpl->pending_pack_count >= 0
                ? tmpl->pending_pack_args : arguments + pack_index;
            tmpl->instances[tmpl->instance_count].pack_args =
                ast_arena_alloc(sizeof(Type*) * (size_t)actual_pack_count);
            memcpy(tmpl->instances[tmpl->instance_count].pack_args,
                   pack_args, sizeof(Type*) * (size_t)actual_pack_count);
        } else if (actual_pack_count > 0) {
            int64_t* pack_values = tmpl->is_local_class_template &&
                    tmpl->pending_pack_count >= 0
                ? tmpl->pending_pack_values : (int64_t*)value_args + pack_index;
            bool* pack_value_present = tmpl->is_local_class_template &&
                    tmpl->pending_pack_count >= 0
                ? tmpl->pending_pack_value_present
                : (bool*)value_present + pack_index;
            tmpl->instances[tmpl->instance_count].pack_values = ast_arena_alloc(
                sizeof(int64_t) * (size_t)actual_pack_count);
            tmpl->instances[tmpl->instance_count].pack_value_present =
                ast_arena_alloc(sizeof(bool) * (size_t)actual_pack_count);
            memcpy(tmpl->instances[tmpl->instance_count].pack_values,
                   pack_values, sizeof(int64_t) * (size_t)actual_pack_count);
            memcpy(tmpl->instances[tmpl->instance_count].pack_value_present,
                   pack_value_present,
                   sizeof(bool) * (size_t)actual_pack_count);
        }
    }
    tmpl->instances[tmpl->instance_count].arg_count = argument_count;
    tmpl->instances[tmpl->instance_count].instantiated = instance;
    ++tmpl->instance_count;
    return instance->type;
}

static bool deduce_class_specialization_value(
    CxxTemplate* tmpl, Expr* pattern, int64_t actual, int64_t* values,
    bool* value_present, int* specificity) {
    int parameter_index;
    int64_t constant;
    if (!tmpl || !pattern || !values || !value_present) return false;
    parameter_index = -1;
    if (pattern->kind == EXPR_IDENT && pattern->ident_name) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                strcmp(parameter->name, pattern->ident_name) == 0) {
                parameter_index = index;
                break;
            }
        }
    }
    if (parameter_index >= 0) {
        if (value_present[parameter_index]) {
            return values[parameter_index] == actual;
        }
        values[parameter_index] = actual;
        value_present[parameter_index] = true;
        return true;
    }
    if (!expr_eval_integer_constant(pattern, &constant)) return false;
    if (specificity) *specificity += 16;
    return constant == actual;
}

static bool cxx_class_specialization_type_equal(const Type* pattern,
                                                const Type* actual) {
    if (!pattern || !actual || pattern->kind != actual->kind ||
        pattern->is_const != actual->is_const ||
        pattern->is_volatile != actual->is_volatile ||
        pattern->is_reference != actual->is_reference ||
        pattern->is_rvalue_reference != actual->is_rvalue_reference) {
        return false;
    }
    if (pattern->kind == TYPE_PTR) {
        return cxx_class_specialization_type_equal(pattern->base,
                                                   actual->base);
    }
    if (pattern->kind == TYPE_ARRAY) {
        return (pattern->array_len < 0 || actual->array_len < 0 ||
                pattern->array_len == actual->array_len) &&
            cxx_class_specialization_type_equal(pattern->base,
                                                 actual->base);
    }
    return type_is_compatible((Type*)pattern, (Type*)actual);
}

/* This is a bounded partial-ordering rank, not a claim of full standard
 * ordering.  It is deliberately structural: a nested pointer/array pattern
 * is more constrained than a bare parameter, and a concrete type or value is
 * more constrained than its dependent counterpart.  Keeping one rank per
 * template argument lets orthogonal patterns such as `X<T, 4>` and
 * `X<int, N>` remain ambiguous instead of letting an arbitrary scalar score
 * choose one. */
static int cxx_class_specialization_type_rank(const Type* pattern) {
    int rank = 0;
    if (!pattern) return 0;
    if (pattern->is_const) ++rank;
    if (pattern->is_volatile) ++rank;
    if (pattern->kind == TYPE_PTR || pattern->kind == TYPE_ARRAY) {
        return rank + 1 + cxx_class_specialization_type_rank(pattern->base);
    }
    if (pattern->cxx_dependent) return rank;
    return rank + 3;
}

static int cxx_class_specialization_value_rank(const CxxTemplate* tmpl,
                                               const Expr* pattern) {
    if (tmpl && pattern && pattern->kind == EXPR_IDENT &&
        pattern->ident_name) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            const TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                strcmp(parameter->name, pattern->ident_name) == 0) {
                return 0;
            }
        }
    }
    return pattern ? 3 : 0;
}

static bool cxx_class_specialization_rank_dominates(
    const int* left, const int* right, int count) {
    bool strictly_greater = false;
    if (!left || !right || count < 0) return false;
    for (int index = 0; index < count; ++index) {
        if (left[index] < right[index]) return false;
        if (left[index] > right[index]) strictly_greater = true;
    }
    return strictly_greater;
}

static bool deduce_class_specialization_type(CxxTemplate* tmpl,
                                              Type* pattern, Type* actual,
                                              Type** arguments,
                                              int* specificity) {
    int nested_specificity = 0;
    if (!tmpl || !pattern || !actual || !arguments) return false;
    /* type_is_compatible intentionally ignores top-level cv qualifiers for
     * ordinary expression conversions.  Partial-specialization deduction is
     * different: a concrete `const int` pattern must not match `int`, and a
     * dependent `const T` pattern must prove that the corresponding actual is
     * const-qualified before binding T.  Keep the check local to deduction so
     * normal C/C++ compatibility rules are unchanged. */
    if (pattern->cxx_dependent) {
        if ((pattern->is_const && !actual->is_const) ||
            (pattern->is_volatile && !actual->is_volatile)) {
            return false;
        }
        if (specificity) {
            if (pattern->is_const) *specificity += 2;
            if (pattern->is_volatile) *specificity += 2;
        }
    } else if (pattern->is_const != actual->is_const ||
               pattern->is_volatile != actual->is_volatile) {
        return false;
    }
    if (pattern->kind == TYPE_STRUCT && pattern->tag) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->name &&
                strcmp(parameter->name, pattern->tag) == 0) {
                if (!arguments[index]) {
                    arguments[index] = actual;
                    if (specificity) *specificity += 0;
                    return true;
                }
                return type_is_compatible(arguments[index], actual);
            }
        }
    }
    if (pattern->kind == TYPE_PTR && actual->kind == TYPE_PTR &&
        pattern->is_reference == actual->is_reference &&
        pattern->is_rvalue_reference == actual->is_rvalue_reference) {
        return deduce_class_specialization_type(
            tmpl, pattern->base, actual->base, arguments,
            specificity ? &nested_specificity : NULL) &&
            (!specificity || (*specificity += nested_specificity + 1, true));
    }
    if (pattern->kind == TYPE_ARRAY && actual->kind == TYPE_ARRAY &&
        (pattern->array_len < 0 ||
         pattern->array_len == actual->array_len)) {
        if (specificity && pattern->array_len >= 0) *specificity += 4;
        return deduce_class_specialization_type(
            tmpl, pattern->base, actual->base, arguments, specificity);
    }
    if (!type_is_compatible(pattern, actual)) return false;
    if (specificity) *specificity += 8;
    return true;
}

CxxClass* rcc_cxx_instantiate_class_template(CxxTemplate* tmpl,
                                              Type** arguments,
                                              const int64_t* value_args,
                                              const bool* value_present,
                                              int argument_count,
                                              SourceLoc loc) {
    Type* type = instantiate_class_template(tmpl, arguments, value_args,
                                             value_present, argument_count,
                                             loc);
    return type ? type->cxx_class : NULL;
}

static bool cxx_expression_references_template_non_type_parameter(
    const Expr* expression, const CxxTemplate* tmpl) {
    if (!expression || !tmpl) return false;
    if (expression->kind == EXPR_IDENT && expression->ident_name) {
        for (const CxxTemplate* scope = tmpl; scope;
             scope = scope->enclosing_template) {
            for (int index = 0; index < scope->param_count; ++index) {
                const TemplateParam* parameter = &scope->params[index];
                if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                    strcmp(parameter->name, expression->ident_name) == 0) {
                    return true;
                }
            }
        }
        return false;
    }
    switch (expression->kind) {
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            return cxx_expression_references_template_non_type_parameter(
                expression->unary_operand, tmpl);
        case EXPR_CAST:
            return cxx_expression_references_template_non_type_parameter(
                expression->cast_expr, tmpl);
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
            return cxx_expression_references_template_non_type_parameter(
                       expression->binary_lhs, tmpl) ||
                   cxx_expression_references_template_non_type_parameter(
                       expression->binary_rhs, tmpl);
        case EXPR_COND:
            return cxx_expression_references_template_non_type_parameter(
                       expression->cond_test, tmpl) ||
                   cxx_expression_references_template_non_type_parameter(
                       expression->cond_then, tmpl) ||
                   cxx_expression_references_template_non_type_parameter(
                       expression->cond_else, tmpl);
        default:
            return false;
    }
}

static Type* parse_class_template_specialization(CxxTemplate* tmpl,
                                                 SourceLoc loc) {
    Type* arguments[32] = { NULL };
    int64_t values[32] = { 0 };
    bool value_present[32] = { false };
    Expr* value_expressions[32] = { NULL };
    int argument_count = 0;
    int pack_index = cxx_class_pack_index(tmpl);
    expect(TOK_LT, "<");
    if (!check(TOK_GT)) {
        do {
            if (argument_count == (int)(sizeof(arguments) /
                                        sizeof(arguments[0]))) {
                rcc_error(loc, "class template argument limit exceeded");
                break;
            }
            if (argument_count >= tmpl->param_count && pack_index < 0) {
                rcc_error(peek()->loc, "too many class template arguments");
                while (!check(TOK_COMMA) && !check(TOK_GT) && !at_end()) {
                    advance();
                }
                arguments[argument_count++] = type_int;
                continue;
            }
            TemplateParam* parameter = &tmpl->params[
                pack_index >= 0 && argument_count >= pack_index
                    ? pack_index : argument_count];
            if (parameter->kind == TPARAM_TYPE) {
                arguments[argument_count++] = parse_cxx_type_spec();
            } else if (parameter->kind == TPARAM_NONTYPE) {
                Expr* value_expression;
                int64_t value;
                rcc_parser_set_cxx_template_default_mode(true);
                value_expression = parse_assignment_expression();
                rcc_parser_set_cxx_template_default_mode(false);
                value_expressions[argument_count] = value_expression;
                if (expr_eval_integer_constant(value_expression, &value)) {
                    values[argument_count] = value;
                    value_present[argument_count] = true;
                } else if (!cxx_expression_references_template_non_type_parameter(
                               value_expression, active_template)) {
                    SourceLoc value_loc;
                    cxx_parser_expr_loc(&value_loc, value_expression, &loc);
                    rcc_error(value_loc,
                              "class template non-type argument must be an "
                              "integer constant expression");
                    value = 0;
                    values[argument_count] = value;
                    value_present[argument_count] = true;
                }
                arguments[argument_count] = parameter->type;
                ++argument_count;
            } else {
                const char* argument_name = NULL;
                CxxTemplate* argument_template;
                Type* template_carrier;
                if (check(TOK_IDENT) || check(TOK_SCOPE)) {
                    argument_name = parse_qualified_name();
                } else {
                    rcc_error(peek()->loc,
                              "template-template argument requires a class "
                              "template name");
                }
                argument_template = argument_name
                    ? find_class_template(argument_name) : NULL;
                if (!argument_template ||
                    !template_template_signature_matches(
                        parameter, argument_template)) {
                    rcc_error(loc,
                              "template-template argument does not match its "
                              "parameter list");
                    template_carrier = type_int;
                } else {
                    template_carrier = type_struct(argument_name);
                    template_carrier->cxx_template = argument_template;
                }
                arguments[argument_count++] = template_carrier;
            }
        } while (match(TOK_COMMA));
    }
    while (pack_index < 0 && argument_count < tmpl->param_count &&
           tmpl->params[argument_count].has_default) {
        TemplateParam* parameter = &tmpl->params[argument_count];
        if (parameter->kind == TPARAM_TYPE && parameter->default_type) {
            arguments[argument_count] = substitute_template_type(
                tmpl, parameter->default_type, arguments, tmpl->param_count,
                values, value_present);
        } else if (parameter->kind == TPARAM_TEMPLATE &&
                   parameter->default_type &&
                   parameter->default_type->cxx_template) {
            arguments[argument_count] = parameter->default_type;
        } else if (parameter->kind == TPARAM_NONTYPE &&
                   parameter->default_value) {
            int64_t value;
            if (parameter->default_context) {
                parameter->default_context->pending_pack_count =
                    tmpl->pending_pack_count;
            }
            if (!eval_template_integer_expression(
                    parameter->default_value,
                    parameter->default_context
                        ? parameter->default_context : tmpl,
                    values, value_present,
                    &value)) {
                if (!cxx_expression_references_template_non_type_parameter(
                        parameter->default_value,
                        parameter->default_context
                            ? parameter->default_context : tmpl)) {
                    rcc_error(loc,
                              "class template non-type default must be an "
                              "integer constant expression");
                    value = 0;
                    values[argument_count] = value;
                    value_present[argument_count] = true;
                }
            }
            value_expressions[argument_count] = parameter->default_value;
            arguments[argument_count] = parameter->type;
            if (eval_template_integer_expression(
                    parameter->default_value,
                    parameter->default_context
                        ? parameter->default_context : tmpl,
                    values, value_present, &value)) {
                values[argument_count] = value;
                value_present[argument_count] = true;
            }
        } else {
            break;
        }
        ++argument_count;
    }
    expect(TOK_GT, ">");
    for (int argument_index = 0; argument_index < argument_count;
         ++argument_index) {
        if (arguments[argument_index] &&
            arguments[argument_index]->cxx_dependent) {
            Type* dependent = type_struct(tmpl->name ? tmpl->name :
                                          "dependent-template");
            dependent->cxx_dependent = true;
            dependent->cxx_template = tmpl;
            dependent->cxx_template_param_index = -1;
            dependent->cxx_template_arg_count = argument_count;
            if (argument_count > 0) {
                dependent->cxx_template_args = ast_arena_alloc(
                    sizeof(Type*) * (size_t)argument_count);
                memcpy(dependent->cxx_template_args, arguments,
                       sizeof(Type*) * (size_t)argument_count);
                dependent->cxx_template_value_args = ast_arena_alloc(
                    sizeof(Expr*) * (size_t)argument_count);
                memcpy(dependent->cxx_template_value_args, value_expressions,
                       sizeof(Expr*) * (size_t)argument_count);
            }
            return dependent;
        }
        if (value_expressions[argument_index] &&
            !value_present[argument_index]) {
            Type* dependent = type_struct(tmpl->name ? tmpl->name :
                                          "dependent-template");
            dependent->cxx_dependent = true;
            dependent->cxx_template = tmpl;
            dependent->cxx_template_param_index = -1;
            dependent->cxx_template_arg_count = argument_count;
            if (argument_count > 0) {
                dependent->cxx_template_args = ast_arena_alloc(
                    sizeof(Type*) * (size_t)argument_count);
                memcpy(dependent->cxx_template_args, arguments,
                       sizeof(Type*) * (size_t)argument_count);
                dependent->cxx_template_value_args = ast_arena_alloc(
                    sizeof(Expr*) * (size_t)argument_count);
                memcpy(dependent->cxx_template_value_args, value_expressions,
                       sizeof(Expr*) * (size_t)argument_count);
            }
            return dependent;
        }
    }
    /* A constrained primary class template remains a viability condition for
     * all concrete specializations.  Partial-specialization constraints are
     * checked when their selected definition is instantiated below. */
    if (tmpl->specialization_count > 0 &&
        !cxx_template_constraint_satisfied(
            tmpl, arguments, values, value_present, loc, true, NULL)) {
        return type_int;
    }
    CxxTemplate* selected = NULL;
    Type* selected_arguments[32] = { NULL };
    int64_t selected_values[32] = { 0 };
    bool selected_value_present[32] = { false };
    int selected_ranks[32] = { 0 };
    for (int index = 0; index < tmpl->specialization_count; ++index) {
        CxxTemplate* specialization = tmpl->specializations[index];
        bool matches = specialization &&
            specialization->specialization_arg_count == argument_count;
        Type* specialization_arguments[32] = { NULL };
        int64_t specialization_values[32] = { 0 };
        bool specialization_value_present[32] = { false };
        int specialization_ranks[32] = { 0 };
        int specificity = specialization && specialization->param_count == 0
            ? 100000 : 0;
        for (int argument_index = 0; matches &&
             argument_index < argument_count; ++argument_index) {
            if (tmpl->params[argument_index].kind == TPARAM_NONTYPE) {
                specialization_ranks[argument_index] =
                    specialization->param_count == 0 ? 100000 :
                    cxx_class_specialization_value_rank(
                        specialization,
                        specialization->specialization_value_args
                            ? specialization->specialization_value_args[
                                argument_index]
                            : NULL);
                matches = specialization->specialization_value_args &&
                    specialization->specialization_value_args[argument_index] &&
                    deduce_class_specialization_value(
                        specialization,
                        specialization->specialization_value_args[
                            argument_index],
                        values[argument_index],
                        specialization_values,
                        specialization_value_present, &specificity);
            } else if (specialization->param_count == 0) {
                specialization_ranks[argument_index] = 100000;
                matches = specialization->specialization_args &&
                    cxx_class_specialization_type_equal(
                        specialization->specialization_args[argument_index],
                        arguments[argument_index]);
            } else if (specialization->param_count <=
                       (int)(sizeof(specialization_arguments) /
                             sizeof(specialization_arguments[0]))) {
                specialization_ranks[argument_index] =
                    cxx_class_specialization_type_rank(
                        specialization->specialization_args[argument_index]);
                matches = deduce_class_specialization_type(
                    specialization,
                    specialization->specialization_args[argument_index],
                    arguments[argument_index], specialization_arguments,
                    &specificity);
            } else {
                matches = false;
            }
        }
        if (matches && specialization->templated_class) {
            bool constraint_unsupported = false;
            for (int parameter_index = 0;
                 parameter_index < specialization->param_count;
                 ++parameter_index) {
                if (specialization->params[parameter_index].kind ==
                        TPARAM_NONTYPE
                    ? !specialization_value_present[parameter_index]
                    : !specialization_arguments[parameter_index]) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                if (specialization->constraint &&
                    !cxx_template_constraint_satisfied(
                        specialization, specialization_arguments,
                        specialization_values, specialization_value_present,
                        loc, false, &constraint_unsupported)) {
                    if (constraint_unsupported) {
                        rcc_error(loc,
                                  "class template partial specialization "
                                  "constraint could not be evaluated");
                        return NULL;
                    }
                    matches = false;
                }
            }
            if (matches) {
                bool current_dominates = selected &&
                    cxx_class_specialization_rank_dominates(
                        specialization_ranks, selected_ranks,
                        argument_count);
                bool selected_dominates = selected &&
                    cxx_class_specialization_rank_dominates(
                        selected_ranks, specialization_ranks,
                        argument_count);
                if (!selected || current_dominates) {
                    selected = specialization;
                    memcpy(selected_ranks, specialization_ranks,
                           sizeof(selected_ranks));
                    memcpy(selected_arguments, specialization_arguments,
                           sizeof(selected_arguments));
                    memcpy(selected_values, specialization_values,
                           sizeof(selected_values));
                    memcpy(selected_value_present, specialization_value_present,
                           sizeof(selected_value_present));
                } else if (!selected_dominates) {
                    rcc_error(loc,
                              "ambiguous class template partial specialization "
                              "for '%s'",
                              tmpl->name ? tmpl->name : "template");
                    return NULL;
                }
            }
        }
    }
    if (selected) {
        if (selected->param_count == 0) {
            CxxClass* specialized = selected->templated_class;
            int64_t identity_values[32] = { 0 };
            bool identity_present[32] = { false };
            for (int argument_index = 0; argument_index < argument_count;
                 ++argument_index) {
                if (tmpl->params[argument_index].kind == TPARAM_NONTYPE &&
                    selected->specialization_value_args &&
                    selected->specialization_value_args[argument_index] &&
                    expr_eval_integer_constant(
                        selected->specialization_value_args[argument_index],
                        &identity_values[argument_index])) {
                    identity_present[argument_index] = true;
                }
            }
            cxx_set_template_identity(
                specialized, tmpl, arguments, identity_values,
                identity_present, argument_count);
            return specialized->type;
        }
        Type* instance_type = instantiate_class_template(
            selected, selected_arguments, selected_values,
            selected_value_present,
            selected->param_count, loc);
        if (instance_type && instance_type->cxx_class) {
            cxx_set_template_identity(
                instance_type->cxx_class, tmpl, arguments, values,
                value_present, argument_count);
        }
        return instance_type;
    }
    return instantiate_class_template(tmpl, arguments, values, value_present,
                                      argument_count, loc);
}

static bool cxx_deduction_guide_starts(void) {
    Token* token = parser.cur;
    int depth = 0;
    if (!token) return false;
    if (token->type == TOK_SCOPE) token = token->next;
    if (!token || token->type != TOK_IDENT) return false;
    token = token->next;
    while (token && token->type == TOK_SCOPE) {
        token = token->next;
        if (!token || token->type != TOK_IDENT) return false;
        token = token->next;
    }
    if (!token || token->type != TOK_LPAREN) return false;
    for (; token; token = token->next) {
        if (token->type == TOK_LPAREN) {
            ++depth;
        } else if (token->type == TOK_RPAREN) {
            --depth;
            if (depth == 0) {
                return token->next && token->next->type == TOK_ARROW;
            }
        }
    }
    return false;
}

/* Parse a user-defined C++17 deduction guide.  The hook is transactional
 * when the source is an ordinary declaration, allowing the shared parser to
 * retain all non-guide declarations. */
bool rcc_parse_cxx_deduction_guide(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    CxxTemplate* guide_template = active_template;
    const char* target_name;
    const char* return_name;
    CxxTemplate* target;
    CxxTemplate* return_template;
    DeclList* parameters = NULL;
    Type* return_type = NULL;
    CxxDeductionGuide* guide;
    int parameter_index = 0;

    if (!cxx_deduction_guide_starts()) return false;
    if (!rcc_parser_cxx_standard_at_least(17)) {
        rcc_error(loc, "deduction guides require C++17 or newer");
    }

    target_name = parse_qualified_name();
    target = target_name ? find_class_template(target_name) : NULL;
    if (!target) {
        rcc_error(loc, "deduction guide target must name a class template");
    }
    expect(TOK_LPAREN, "deduction guide parameter list");
    if (!check(TOK_RPAREN)) {
        do {
            const char* parameter_name = NULL;
            Type* parameter_type = parse_cxx_type_spec();
            parameter_type = rcc_parser_parse_cxx_declarator(
                parameter_type, &parameter_name, NULL);
            if (match(TOK_ELLIPSIS)) {
                rcc_error(previous()->loc,
                          "deduction guide parameter packs are not supported");
            }
            if (match(TOK_ASSIGN)) {
                rcc_error(previous()->loc,
                          "deduction guide default arguments are not supported");
                (void)parse_assignment_expression();
            }
            {
                Decl* parameter = decl_param(
                    parameter_name, parameter_type, parameter_index++,
                    peek()->loc);
                decllist_append(&parameters, parameter);
            }
        } while (match(TOK_COMMA));
    }
    expect(TOK_RPAREN, ")");
    expect(TOK_ARROW, "-> in deduction guide");

    if (check(TOK_IDENT) || check(TOK_SCOPE)) {
        return_name = parse_qualified_name();
    } else {
        return_name = NULL;
        rcc_error(peek()->loc,
                  "deduction guide must return a class-template specialization");
    }
    return_template = return_name ? find_class_template(return_name) : NULL;
    if (!return_template || return_template != target) {
        rcc_error(loc,
                  "deduction guide return type must name its target class "
                  "template");
    } else if (!check(TOK_LT)) {
        rcc_error(loc,
                  "deduction guide return type requires explicit template "
                  "arguments");
    } else {
        return_type = parse_class_template_specialization(return_template, loc);
    }
    if (!return_type) return_type = type_int;
    expect(TOK_SEMICOLON, "; after deduction guide");

    if (target) {
        guide = ast_arena_alloc(sizeof(*guide));
        guide->template_owner = guide_template;
        guide->parameters = parameters;
        guide->return_type = return_type;
        guide->next = NULL;
        {
            CxxDeductionGuide** tail = &target->deduction_guides;
            while (*tail) tail = &(*tail)->next;
            *tail = guide;
        }
    }
    if (guide_template) {
        guide_template->kind = TMPL_DEDUCTION_GUIDE;
        guide_template->name = ast_arena_strdup("<deduction-guide>");
    }
    (void)saved_cur;
    (void)saved_prev;
    return true;
}

static bool deduce_function_template_type(CxxTemplate* tmpl, Type* pattern,
                                          Type* actual, Type** arguments,
                                          int64_t* values,
                                          bool* value_present,
                                          int* specificity) {
    TypeParam* pattern_parameter;
    TypeParam* actual_parameter;
    if (!tmpl || !pattern || !actual || !arguments) return false;
    if (pattern->kind == TYPE_STRUCT && pattern->tag) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->name &&
                strcmp(parameter->name, pattern->tag) == 0) {
                if (pattern->is_const || pattern->is_volatile) {
                    Type* unqualified = ast_arena_alloc(sizeof(*unqualified));
                    *unqualified = *actual;
                    unqualified->is_const = false;
                    unqualified->is_volatile = false;
                    actual = unqualified;
                }
                if (!arguments[index]) {
                    arguments[index] = actual;
                    return true;
                }
                return type_is_compatible(arguments[index], actual);
            }
        }
    }
    if (pattern->kind == TYPE_PTR && pattern->is_reference) {
        bool preserve_lvalue_reference =
            pattern->is_rvalue_reference && actual->kind == TYPE_PTR &&
            actual->is_reference && pattern->base &&
            !pattern->base->is_const && !pattern->base->is_volatile;
        if (preserve_lvalue_reference && pattern->base &&
            pattern->base->kind == TYPE_STRUCT && pattern->base->tag) {
            for (int index = 0; index < tmpl->param_count; ++index) {
                TemplateParam* parameter = &tmpl->params[index];
                if (parameter->kind != TPARAM_TYPE || !parameter->name ||
                    strcmp(parameter->name, pattern->base->tag) != 0) {
                    continue;
                }
                if (!arguments[index]) {
                    arguments[index] = actual;
                    return true;
                }
                return type_is_compatible(arguments[index], actual);
            }
        }
        if (actual->kind == TYPE_PTR && actual->is_reference) {
            actual = actual->base;
        }
        if (specificity) *specificity += 8;
        return deduce_function_template_type(tmpl, pattern->base, actual,
                                             arguments, values,
                                             value_present, specificity);
    }
    if (pattern->kind == TYPE_PTR && !pattern->is_reference &&
        actual->kind == TYPE_FUNC && pattern->base &&
        pattern->base->kind == TYPE_FUNC) {
        /* A function designator undergoes the standard function-to-pointer
         * conversion when it is passed to a function-pointer parameter. */
        if (specificity) *specificity += 4;
        return deduce_function_template_type(
            tmpl, pattern->base, actual, arguments, values, value_present,
            specificity);
    }
    if (pattern->kind == TYPE_PTR && actual->kind == TYPE_PTR) {
        if (specificity) *specificity += 8;
        return deduce_function_template_type(tmpl, pattern->base,
                                             actual->base, arguments, values,
                                             value_present, specificity);
    }
    if (pattern->kind == TYPE_FUNC && actual->kind == TYPE_FUNC) {
        if (pattern->variadic != actual->variadic ||
            pattern->has_prototype != actual->has_prototype) {
            return false;
        }
        if (!deduce_function_template_type(
                tmpl, pattern->ret_type, actual->ret_type, arguments, values,
                value_present, specificity)) {
            return false;
        }
        pattern_parameter = pattern->params;
        actual_parameter = actual->params;
        while (pattern_parameter && actual_parameter) {
            if (!deduce_function_template_type(
                    tmpl, pattern_parameter->type, actual_parameter->type,
                    arguments, values, value_present, specificity)) {
                return false;
            }
            pattern_parameter = pattern_parameter->next;
            actual_parameter = actual_parameter->next;
        }
        if (pattern_parameter || actual_parameter) return false;
        if (specificity) *specificity += 16;
        return true;
    }
    if (pattern->kind == TYPE_ARRAY && actual->kind == TYPE_ARRAY) {
        if (specificity) *specificity += 8;
        if (pattern->array_bound &&
            pattern->array_bound->kind == EXPR_IDENT && values &&
            value_present) {
            for (int index = 0; index < tmpl->param_count; ++index) {
                TemplateParam* parameter = &tmpl->params[index];
                if (parameter->kind != TPARAM_NONTYPE ||
                    !parameter->name ||
                    strcmp(parameter->name,
                           pattern->array_bound->ident_name) != 0) {
                    continue;
                }
                if (actual->array_len <= 0) return false;
                if (value_present[index] && values[index] != actual->array_len) {
                    return false;
                }
                values[index] = actual->array_len;
                value_present[index] = true;
                break;
            }
        } else if (pattern->array_len >= 0 &&
                   pattern->array_len != actual->array_len) {
            return false;
        }
        return deduce_function_template_type(tmpl, pattern->base,
                                             actual->base, arguments, values,
                                             value_present, specificity);
    }
    if (specificity) *specificity += 16;
    return type_is_compatible(pattern, actual);
}

/* Function-call template deduction applies the by-value parameter
 * adjustments before matching the pattern.  In particular, an array or
 * function argument decays at the call boundary, while an array bound behind
 * a reference pattern remains available for non-type deduction. */
static Type* cxx_parser_template_deduction_argument(Type* pattern,
                                                    Type* actual) {
    Type* adjusted;
    if (!pattern || !actual || pattern->is_reference ||
        pattern->is_rvalue_reference) {
        return actual;
    }
    if (actual->kind == TYPE_ARRAY) {
        return type_ptr(actual->base);
    }
    if (actual->kind == TYPE_FUNC) return type_ptr(actual);
    if (!actual->is_const && !actual->is_volatile) return actual;
    adjusted = ast_arena_alloc(sizeof(*adjusted));
    *adjusted = *actual;
    adjusted->is_const = false;
    adjusted->is_volatile = false;
    return adjusted;
}

static bool cxx_function_template_type_contains_parameter(
    CxxTemplate* tmpl, Type* type) {
    if (!tmpl || !type) return false;
    if (type->cxx_dependent) {
        return true;
    }
    if (type->kind == TYPE_STRUCT && type->tag) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            if (tmpl->params[index].kind == TPARAM_TYPE &&
                tmpl->params[index].name &&
                strcmp(tmpl->params[index].name, type->tag) == 0) {
                return true;
            }
        }
    }
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) {
        return cxx_function_template_type_contains_parameter(tmpl,
                                                              type->base);
    }
    if (type->kind == TYPE_FUNC) {
        if (cxx_function_template_type_contains_parameter(tmpl,
                                                           type->ret_type)) {
            return true;
        }
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            if (cxx_function_template_type_contains_parameter(
                    tmpl, parameter->type)) return true;
        }
    }
    return false;
}

static int cxx_parser_template_conversion_rank(Expr* argument,
                                                Type* target);
static int cxx_parser_function_template_overload_conversion_rank(
    Expr* argument, Type* target);

/* C++17 adds an aggregate deduction candidate when no user-declared
 * constructor is available.  Keep this candidate deliberately ABI-bounded:
 * only a struct with public, named, non-static, non-bit-field members and
 * ordinary type template parameters is materialized.  Unsupported aggregate
 * shapes are diagnosed instead of being guessed as constructor calls. */
static Type* deduce_class_template_from_aggregate(
    CxxTemplate* tmpl, ExprList* arguments, SourceLoc loc, bool brace_form,
    bool* recognized) {
    CxxClass* definition = tmpl ? tmpl->templated_class : NULL;
    TypeParam* field;
    ExprList* argument;
    Type* template_arguments[32] = { NULL };
    int64_t template_values[32] = { 0 };
    bool template_value_present[32] = { false };
    int field_count = 0;
    int argument_count = cxx_constructor_argument_count(arguments);
    int specificity = 0;

    if (recognized) *recognized = false;
    if (!definition || !definition->is_struct || definition->constructors ||
        definition->has_user_constructor || definition->has_nonpublic_field ||
        definition->has_field_initializer || definition->base_count != 0 ||
        definition->vtable_size != 0) {
        return NULL;
    }
    for (field = definition->fields; field; field = field->next) {
        if (field->is_static) continue;
        if (!field->name || field->is_bitfield ||
            field->cxx_access != ACCESS_PUBLIC || !field->type) {
            return NULL;
        }
        if (field_count == 32) {
            rcc_error(loc, "aggregate class template has too many fields");
            return NULL;
        }
        ++field_count;
    }
    if (recognized) *recognized = true;
    if (!brace_form && !rcc_parser_cxx_standard_at_least(20)) {
        rcc_error(loc,
                  "aggregate class template argument deduction requires "
                  "braced initialization before C++20");
        return NULL;
    }
    if (tmpl->param_count <= 0 || tmpl->param_count > 32) {
        rcc_error(loc,
                  "aggregate class template argument deduction exceeds "
                  "compiler limits");
        return NULL;
    }
    if (argument_count != field_count) {
        rcc_error(loc,
                  "aggregate class template argument deduction requires "
                  "one initializer for each aggregate field");
        return NULL;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind != TPARAM_TYPE || parameter->is_pack) {
            rcc_error(loc,
                      "RCC++ aggregate CTAD requires non-pack type template "
                      "parameters");
            return NULL;
        }
    }

    field = definition->fields;
    argument = arguments;
    while (field && argument) {
        Type* actual;
        if (field->is_static) {
            field = field->next;
            continue;
        }
        actual = cxx_parser_expression_type(argument->expr);
        if (!actual || !deduce_function_template_type(
                tmpl, field->type,
                cxx_parser_template_deduction_argument(field->type, actual),
                template_arguments, template_values, template_value_present,
                &specificity)) {
            rcc_error(loc,
                      "aggregate class template argument deduction could not "
                      "deduce a field type");
            return NULL;
        }
        field = field->next;
        argument = argument->next;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (template_arguments[index]) continue;
        if (!parameter->has_default || !parameter->default_type) {
            rcc_error(loc,
                      "aggregate class template argument deduction could not "
                      "deduce all template arguments");
            return NULL;
        }
        template_arguments[index] = substitute_template_type(
            tmpl, parameter->default_type, template_arguments,
            tmpl->param_count, template_values, template_value_present);
        if (!template_arguments[index] ||
            template_arguments[index]->cxx_dependent) {
            rcc_error(loc,
                      "aggregate class template default argument is "
                      "dependent");
            return NULL;
        }
    }

    field = definition->fields;
    argument = arguments;
    while (field && argument) {
        Type* target;
        int rank;
        if (field->is_static) {
            field = field->next;
            continue;
        }
        target = substitute_template_type(
            tmpl, field->type, template_arguments, tmpl->param_count,
            template_values, template_value_present);
        rank = cxx_parser_template_conversion_rank(argument->expr, target);
        if (rank < 0) {
            rcc_error(loc,
                      "aggregate class template argument deduction has an "
                      "invalid field conversion");
            return NULL;
        }
        field = field->next;
        argument = argument->next;
    }
    return instantiate_class_template(
        tmpl, template_arguments, template_values, template_value_present,
        tmpl->param_count, loc);
}

static Type* deduce_class_template_from_guides(
    CxxTemplate* tmpl, ExprList* arguments, SourceLoc loc) {
    Type* best_type = NULL;
    int best_total = INT_MAX;
    int best_worst = INT_MAX;
    int best_specificity = -1;
    int viable_count = 0;

    for (CxxDeductionGuide* guide = tmpl ? tmpl->deduction_guides : NULL;
         guide; guide = guide->next) {
        CxxTemplate* owner = guide->template_owner;
        Type* template_arguments[32] = { NULL };
        int64_t template_values[32] = { 0 };
        bool template_value_present[32] = { false };
        DeclList* parameter = guide->parameters;
        ExprList* argument = arguments;
        int specificity = 0;
        int total = 0;
        int worst = 0;
        bool viable = true;

        if (owner && (owner->param_count <= 0 || owner->param_count > 32)) {
            rcc_error(loc, "deduction guide template parameter limit exceeded");
            continue;
        }
        if (owner) {
            for (int index = 0; index < owner->param_count; ++index) {
                TemplateParam* template_parameter = &owner->params[index];
                if (template_parameter->kind != TPARAM_TYPE ||
                    template_parameter->is_pack) {
                    rcc_error(loc,
                              "RCC++ deduction guides require non-pack type "
                              "template parameters");
                    viable = false;
                    break;
                }
            }
        }
        if (!viable) continue;

        while (argument && parameter) {
            Type* actual = cxx_parser_expression_type(argument->expr);
            Type* pattern = parameter->decl ? parameter->decl->type : NULL;
            if (!actual || !pattern) {
                viable = false;
                break;
            }
            if (owner && !deduce_function_template_type(
                    owner, pattern,
                    cxx_parser_template_deduction_argument(pattern, actual),
                    template_arguments, template_values,
                    template_value_present, &specificity)) {
                viable = false;
                break;
            }
            argument = argument->next;
            parameter = parameter->next;
        }
        if (argument || parameter) viable = false;
        if (!viable) continue;

        if (owner) {
            for (int index = 0; index < owner->param_count; ++index) {
                TemplateParam* template_parameter = &owner->params[index];
                if (template_arguments[index]) continue;
                if (!template_parameter->has_default ||
                    !template_parameter->default_type) {
                    viable = false;
                    break;
                }
                template_arguments[index] = substitute_template_type(
                    owner, template_parameter->default_type,
                    template_arguments, owner->param_count, template_values,
                    template_value_present);
                if (!template_arguments[index] ||
                    template_arguments[index]->cxx_dependent) {
                    viable = false;
                    break;
                }
            }
        }
        if (!viable) continue;

        parameter = guide->parameters;
        argument = arguments;
        while (argument && parameter) {
            Type* pattern = parameter->decl ? parameter->decl->type : NULL;
            Type* target_type = owner
                ? substitute_template_type(
                    owner, pattern, template_arguments,
                    owner->param_count, template_values,
                    template_value_present)
                : pattern;
            int rank = cxx_parser_template_conversion_rank(
                argument->expr, target_type);
            if (rank < 0) {
                viable = false;
                break;
            }
            total += rank;
            if (rank > worst) worst = rank;
            argument = argument->next;
            parameter = parameter->next;
        }
        if (!viable) continue;

        Type* result = owner
            ? substitute_template_type(
                owner, guide->return_type, template_arguments,
                owner->param_count, template_values, template_value_present)
            : guide->return_type;
        if (!result || result->cxx_dependent || !result->cxx_class) continue;
        if (total < best_total ||
            (total == best_total && worst < best_worst) ||
            (total == best_total && worst == best_worst &&
             specificity > best_specificity)) {
            best_type = result;
            best_total = total;
            best_worst = worst;
            best_specificity = specificity;
            viable_count = 1;
        } else if (total == best_total && worst == best_worst &&
                   specificity == best_specificity) {
            ++viable_count;
        }
    }

    if (viable_count > 1) {
        rcc_error(loc,
                  "ambiguous user-defined deduction guides for '%s'",
                  tmpl && tmpl->name ? tmpl->name : "template");
        return NULL;
    }
    return viable_count == 1 ? best_type : NULL;
}

/* Bounded C++17 class-template argument deduction.  The RinOS ABI can lower
 * class instances only after a concrete template argument list exists, so
 * CTAD is resolved from public constructors before object initialization is
 * validated.  This deliberately covers ordinary type parameters and
 * constructor deduction; deduction guides, non-type/template parameters,
 * and packs remain explicit diagnostics instead of guessed types. */
static Type* deduce_class_template_from_constructor(
    CxxTemplate* tmpl, ExprList* arguments, SourceLoc loc, bool brace_form) {
    Type* best_arguments[32] = { NULL };
    int64_t best_values[32] = { 0 };
    bool best_value_present[32] = { false };
    int best_total = INT_MAX;
    int best_worst = INT_MAX;
    int best_specificity = -1;
    int viable_count = 0;
    int argument_count = cxx_constructor_argument_count(arguments);
    bool aggregate_candidate = false;

    if (!tmpl || tmpl->kind != TMPL_CLASS || tmpl->param_count <= 0 ||
        tmpl->param_count > (int)(sizeof(best_arguments) /
                                  sizeof(best_arguments[0]))) {
        rcc_error(loc, "class template argument deduction exceeds compiler "
                       "limits");
        return NULL;
    }
    if (tmpl->deduction_guides) {
        Type* guided = deduce_class_template_from_guides(tmpl, arguments, loc);
        if (guided) return guided;
    }
    {
        Type* aggregate = deduce_class_template_from_aggregate(
            tmpl, arguments, loc, brace_form, &aggregate_candidate);
        if (aggregate) return aggregate;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind != TPARAM_TYPE || parameter->is_pack) {
            rcc_error(loc,
                      "RCC++ CTAD currently requires non-pack type template "
                      "parameters");
            return NULL;
        }
    }

    for (CxxConstructorInfo* constructor = tmpl->templated_class->constructors;
         constructor; constructor = constructor->next) {
        Type* candidate_arguments[32] = { NULL };
        int64_t candidate_values[32] = { 0 };
        bool candidate_value_present[32] = { false };
        TypeParam* parameter = constructor->parameters;
        DeclList* declaration = constructor->method && constructor->method->decl
            ? constructor->method->decl->func_params : NULL;
        ExprList* argument = arguments;
        int specificity = 0;
        int total = 0;
        int worst = 0;
        bool viable = true;

        if (constructor->access != ACCESS_PUBLIC || constructor->is_deleted ||
            constructor->is_defaulted ||
            argument_count < (int)cxx_constructor_required_parameter_count(
                constructor) ||
            argument_count > constructor->parameter_count ||
            !cxx_constructor_arity_has_defaults(constructor, argument_count)) {
            continue;
        }
        while (argument && parameter && declaration) {
            Type* actual = cxx_parser_expression_type(argument->expr);
            Type* pattern = parameter->type;
            if (!actual || !pattern ||
                !deduce_function_template_type(
                    tmpl, pattern,
                    cxx_parser_template_deduction_argument(pattern, actual),
                    candidate_arguments, candidate_values,
                    candidate_value_present, &specificity)) {
                viable = false;
                break;
            }
            argument = argument->next;
            parameter = parameter->next;
            declaration = declaration->next;
        }
        if (argument || parameter || declaration) viable = false;
        if (!viable) continue;

        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* template_parameter = &tmpl->params[index];
            if (candidate_arguments[index]) continue;
            if (!template_parameter->has_default ||
                !template_parameter->default_type) {
                viable = false;
                break;
            }
            candidate_arguments[index] = substitute_template_type(
                tmpl, template_parameter->default_type, candidate_arguments,
                tmpl->param_count, candidate_values,
                candidate_value_present);
            if (!candidate_arguments[index] ||
                candidate_arguments[index]->cxx_dependent) {
                viable = false;
                break;
            }
        }
        if (!viable) continue;

        parameter = constructor->parameters;
        argument = arguments;
        while (argument && parameter) {
            Type* target = substitute_template_type(
                tmpl, parameter->type, candidate_arguments, tmpl->param_count,
                candidate_values, candidate_value_present);
            int rank = cxx_parser_template_conversion_rank(
                argument->expr, target);
            if (rank < 0) {
                viable = false;
                break;
            }
            total += rank;
            if (rank > worst) worst = rank;
            argument = argument->next;
            parameter = parameter->next;
        }
        if (!viable) continue;
        if (total < best_total ||
            (total == best_total && worst < best_worst) ||
            (total == best_total && worst == best_worst &&
             specificity > best_specificity)) {
            memcpy(best_arguments, candidate_arguments,
                   sizeof(best_arguments));
            memcpy(best_values, candidate_values, sizeof(best_values));
            memcpy(best_value_present, candidate_value_present,
                   sizeof(best_value_present));
            best_total = total;
            best_worst = worst;
            best_specificity = specificity;
            viable_count = 1;
        } else if (total == best_total && worst == best_worst &&
                   specificity == best_specificity) {
            ++viable_count;
        }
    }

    if (viable_count == 0) {
        if (aggregate_candidate) return NULL;
        rcc_error(loc,
                  "no viable public constructor for class template argument "
                  "deduction of '%s'",
                  tmpl->name ? tmpl->name : "template");
        return NULL;
    }
    if (viable_count > 1) {
        rcc_error(loc,
                  "ambiguous class template argument deduction for '%s'",
                  tmpl->name ? tmpl->name : "template");
        return NULL;
    }
    return instantiate_class_template(
        tmpl, best_arguments, best_values, best_value_present,
        tmpl->param_count, loc);
}

static int cxx_parser_type_pack_index(CxxTemplate* tmpl, Type* pattern);

static bool deduce_function_template_arguments(CxxTemplate* tmpl,
                                               ExprList* call_arguments,
                                               Type** template_arguments,
                                               int64_t* template_values,
                                               bool* template_value_present,
                                               int* specificity,
                                               bool report_errors,
                                               Type** pack_arguments,
                                               int* pack_count) {
    DeclList* parameter;
    ExprList* argument;
    if (!tmpl || !tmpl->func_def || !template_arguments) return false;
    parameter = tmpl->func_def->func_params;
    argument = call_arguments;
    while (parameter && argument) {
        if (parameter->decl && parameter->decl->param_is_pack) {
            int pack_index = cxx_parser_type_pack_index(
                tmpl, parameter->decl->type);
            (void)pack_index;
            if (!pack_arguments || !pack_count || pack_index < 0) {
                if (report_errors) {
                    rcc_error(parameter->decl->loc,
                              "function parameter pack is not a type pack");
                }
                return false;
            }
            while (argument) {
                Type* actual = cxx_parser_expression_type(argument->expr);
                if (!actual) {
                    SourceLoc location;
                    cxx_parser_expr_loc(&location, argument->expr,
                                        &tmpl->func_def->loc);
                    if (report_errors) {
                        rcc_error(location,
                                  "cannot deduce function template pack type from an expression without a parser-known type");
                    }
                    return false;
                }
                if (*pack_count >= 32) {
                    if (report_errors) {
                        rcc_error(argument->expr->loc,
                                  "function template parameter pack exceeds compiler limits");
                    }
                    return false;
                }
                pack_arguments[(*pack_count)++] =
                    cxx_parser_template_deduction_argument(
                        parameter->decl->type, actual);
                argument = argument->next;
            }
            parameter = parameter->next;
            break;
        }
        Type* actual = cxx_parser_expression_type(argument->expr);
        if (!actual) {
            SourceLoc location;
            cxx_parser_expr_loc(&location, argument->expr,
                                &tmpl->func_def->loc);
            if (report_errors) {
                rcc_error(location,
                          "cannot deduce function template type from an expression "
                          "without a parser-known type");
            }
            return false;
        }
        Type* deduction_actual = actual;
        if (parameter->decl && parameter->decl->type &&
            parameter->decl->type->kind == TYPE_PTR &&
            parameter->decl->type->is_reference &&
            parameter->decl->type->is_rvalue_reference &&
            cxx_parser_expression_is_lvalue(argument->expr) &&
            !actual->is_reference) {
            deduction_actual = type_reference(actual, false);
        }
        if (!deduce_function_template_type(
                tmpl, parameter->decl->type,
                cxx_parser_template_deduction_argument(
                    parameter->decl->type, deduction_actual), template_arguments,
                template_values, template_value_present, specificity) &&
            (cxx_function_template_type_contains_parameter(
                 tmpl, parameter->decl->type) ||
             cxx_parser_function_template_overload_conversion_rank(
                 argument->expr, parameter->decl->type) < 0)) {
            if (report_errors) {
                rcc_error(argument->expr->loc,
                          "function template argument type does not match its "
                          "parameter pattern");
            }
            return false;
        }
        parameter = parameter->next;
        argument = argument->next;
    }
    if (argument) {
        SourceLoc location;
        cxx_parser_expr_loc(&location, argument->expr,
                            &tmpl->func_def->loc);
        if (report_errors) {
            rcc_error(location,
                      "too many arguments for function template deduction");
        }
        return false;
    }
    for (; parameter; parameter = parameter->next) {
        if (parameter->decl && parameter->decl->param_is_pack) continue;
        if (!parameter->decl->param_default) {
            if (report_errors) {
                rcc_error(tmpl->func_def->loc,
                          "too few arguments for function template deduction");
            }
            return false;
        }
    }
    return true;
}

static bool eval_template_integer_expression(Expr* expression,
                                              CxxTemplate* tmpl,
                                              const int64_t* values,
                                              const bool* value_present,
                                              int64_t* result) {
    int64_t left;
    int64_t right;
    if (!expression || !result) return false;
    if (expr_eval_integer_constant(expression, result)) return true;
    if (expression->kind == EXPR_SIZEOF && expression->sizeof_pack_name &&
        tmpl && tmpl->pending_pack_count >= 0) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack && parameter->name &&
                strcmp(parameter->name, expression->sizeof_pack_name) == 0) {
                *result = tmpl->pending_pack_count;
                return true;
            }
        }
    }
    if (expression->kind == EXPR_IDENT && tmpl && values && value_present) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                value_present[index] &&
                strcmp(parameter->name, expression->ident_name) == 0) {
                *result = values[index];
                return true;
            }
        }
        if (tmpl->enclosing_template) {
            CxxTemplate* enclosing = tmpl->enclosing_template;
            for (int index = 0; index < enclosing->param_count; ++index) {
                TemplateParam* parameter = &enclosing->params[index];
                int substitution_index;
                if (index > INT_MAX - tmpl->param_count) continue;
                substitution_index = tmpl->param_count + index;
                if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                    value_present[substitution_index] &&
                    strcmp(parameter->name, expression->ident_name) == 0) {
                    *result = values[substitution_index];
                    return true;
                }
            }
        }
        return false;
    }
    if (expression->kind == EXPR_CALL && expression->cxx_concept_template) {
        CxxTemplate* concept = expression->cxx_concept_template;
        Type* concept_types[32] = { NULL };
        int64_t concept_values[32] = { 0 };
        bool concept_value_present[32] = { false };
        ExprList* argument = expression->call_args;
        if (!concept->is_concept || concept->param_count > 32 ||
            !concept->constraint) return false;
        for (int index = 0; index < concept->param_count; ++index) {
            TemplateParam* parameter = &concept->params[index];
            if (!argument) return false;
            if (parameter->kind == TPARAM_TYPE) {
                if (!argument->expr || !argument->expr->type) return false;
                concept_types[index] = argument->expr->type;
            } else if (parameter->kind == TPARAM_NONTYPE) {
                if (!eval_template_integer_expression(
                        argument->expr, tmpl, values, value_present,
                        &concept_values[index])) {
                    return false;
                }
                concept_value_present[index] = true;
            } else {
                return false;
            }
            argument = argument->next;
        }
        if (argument || !concept->constraint) {
            return false;
        }
        if (concept->constraint->kind == EXPR_CXX_REQUIRES) {
            Expr* instantiated = cxx_template_clone_expr_with_values(
                concept, concept->constraint, concept_types,
                concept->param_count, concept_values,
                concept_value_present);
            if (!instantiated ||
                !rcc_sema_cxx_requires_satisfied(instantiated)) {
                *result = 0;
                return true;
            }
            *result = 1;
            return true;
        }
        Expr* instantiated = cxx_template_clone_expr_with_values(
            concept, concept->constraint, concept_types,
            concept->param_count, concept_values, concept_value_present);
        if (!instantiated || !eval_template_integer_expression(
                instantiated, concept, concept_values,
                concept_value_present, result)) {
            return false;
        }
        *result = *result != 0;
        return true;
    }
    switch (expression->kind) {
        case EXPR_NEG:
            if (!eval_template_integer_expression(
                    expression->unary_operand, tmpl, values, value_present,
                    &left)) return false;
            *result = -left;
            return true;
        case EXPR_NOT:
            if (!eval_template_integer_expression(
                    expression->unary_operand, tmpl, values, value_present,
                    &left)) return false;
            *result = !left;
            return true;
        case EXPR_BITNOT:
            if (!eval_template_integer_expression(
                    expression->unary_operand, tmpl, values, value_present,
                    &left)) return false;
            *result = ~left;
            return true;
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
            if (!eval_template_integer_expression(
                    expression->binary_lhs, tmpl, values, value_present,
                    &left) ||
                !eval_template_integer_expression(
                    expression->binary_rhs, tmpl, values, value_present,
                    &right)) return false;
            switch (expression->kind) {
                case EXPR_ADD: *result = left + right; break;
                case EXPR_SUB: *result = left - right; break;
                case EXPR_MUL: *result = left * right; break;
                case EXPR_DIV:
                    if (right == 0) return false;
                    *result = left / right;
                    break;
                case EXPR_MOD:
                    if (right == 0) return false;
                    *result = left % right;
                    break;
                case EXPR_BITAND: *result = left & right; break;
                case EXPR_BITOR: *result = left | right; break;
                case EXPR_BITXOR: *result = left ^ right; break;
                case EXPR_LSHIFT:
                    if (right < 0 || right >= 64) return false;
                    *result = (int64_t)((uint64_t)left << (unsigned)right);
                    break;
                case EXPR_RSHIFT:
                    if (right < 0 || right >= 64) return false;
                    *result = (int64_t)((uint64_t)left >> (unsigned)right);
                    break;
                case EXPR_EQ: *result = left == right; break;
                case EXPR_NE: *result = left != right; break;
                case EXPR_LT: *result = left < right; break;
                case EXPR_GT: *result = left > right; break;
                case EXPR_LE: *result = left <= right; break;
                case EXPR_GE: *result = left >= right; break;
                case EXPR_AND: *result = left && right; break;
                case EXPR_OR: *result = left || right; break;
                default: return false;
            }
            return true;
        case EXPR_COND:
            if (!eval_template_integer_expression(
                    expression->cond_test, tmpl, values, value_present,
                    &left)) return false;
            return eval_template_integer_expression(
                left ? expression->cond_then : expression->cond_else,
                tmpl, values, value_present, result);
        case EXPR_CXX_REQUIRES:
            *result = rcc_sema_cxx_requires_satisfied(expression) ? 1 : 0;
            return true;
        case EXPR_CAST:
            return eval_template_integer_expression(
                expression->cast_expr, tmpl, values, value_present, result);
        default:
            return false;
    }
}

static bool cxx_template_constraint_satisfied(CxxTemplate* tmpl,
                                               Type** arguments,
                                               const int64_t* values,
                                               const bool* value_present,
                                               SourceLoc loc,
                                               bool report_errors,
                                               bool* unsupported) {
    int64_t result;
    Expr* constraint;
    CxxTemplate* constraint_context;
    Type** saved_pending_pack_args;
    int64_t* saved_pending_pack_values;
    bool* saved_pending_pack_value_present;
    int saved_pending_pack_count;
    if (unsupported) *unsupported = false;
    if (!tmpl || !tmpl->constraint) return true;
    constraint_context = tmpl->constraint_context
        ? tmpl->constraint_context : tmpl;
    saved_pending_pack_args = constraint_context->pending_pack_args;
    saved_pending_pack_values = constraint_context->pending_pack_values;
    saved_pending_pack_value_present =
        constraint_context->pending_pack_value_present;
    saved_pending_pack_count = constraint_context->pending_pack_count;
    constraint_context->pending_pack_args = tmpl->pending_pack_args;
    constraint_context->pending_pack_values = tmpl->pending_pack_values;
    constraint_context->pending_pack_value_present =
        tmpl->pending_pack_value_present;
    constraint_context->pending_pack_count = tmpl->pending_pack_count;
    constraint = cxx_template_clone_expr_with_values(
        constraint_context, tmpl->constraint, arguments, tmpl->param_count, values,
        value_present);
    constraint_context->pending_pack_args = saved_pending_pack_args;
    constraint_context->pending_pack_values = saved_pending_pack_values;
    constraint_context->pending_pack_value_present =
        saved_pending_pack_value_present;
    constraint_context->pending_pack_count = saved_pending_pack_count;
    if (!constraint) {
        if (report_errors) {
            rcc_error(loc, "template constraint could not be instantiated");
        }
        if (unsupported) *unsupported = true;
        return false;
    }
    if (constraint->kind == EXPR_CXX_REQUIRES) {
        result = rcc_sema_cxx_requires_satisfied(constraint) ? 1 : 0;
    } else if (!eval_template_integer_expression(
                   constraint, constraint_context, values, value_present,
                   &result)) {
        if (report_errors) {
            rcc_error(loc,
                      "requires-clause must be a supported constant constraint "
                      "over the template parameters");
        }
        if (unsupported) *unsupported = true;
        return false;
    }
    if (!result) {
        if (report_errors) {
            rcc_error(loc, "template constraints are not satisfied");
        }
        return false;
    }
    return true;
}

typedef struct CxxAliasOwnerValueSubstitution {
    CxxTemplate* alias_template;
    Expr** alias_value_expressions;
    int64_t* alias_values;
    bool* alias_value_present;
    CxxTemplate* owner_template;
    Type* owner_type;
} CxxAliasOwnerValueSubstitution;

static Expr* cxx_substitute_class_owner_value_expression(
    Expr* expression, const CxxAliasOwnerValueSubstitution* substitution) {
    Expr* copy;
    Expr* left;
    Expr* right;
    CxxTemplate* owner_template;
    Type* owner_type;
    if (!expression || !substitution) return expression;
    owner_template = substitution->owner_template;
    owner_type = substitution->owner_type;
    if (expression->kind == EXPR_IDENT && expression->ident_name) {
        CxxTemplate* alias_template = substitution->alias_template;
        if (alias_template) {
            for (int index = 0; index < alias_template->param_count; ++index) {
                TemplateParam* parameter = &alias_template->params[index];
                if (parameter->kind != TPARAM_NONTYPE || !parameter->name ||
                    strcmp(parameter->name, expression->ident_name) != 0) {
                    continue;
                }
                if (substitution->alias_value_present &&
                    substitution->alias_value_present[index]) {
                    return expr_int(substitution->alias_values[index],
                                    expression->loc);
                }
                if (substitution->alias_value_expressions &&
                    substitution->alias_value_expressions[index]) {
                    return substitution->alias_value_expressions[index];
                }
                break;
            }
        }
        if (!owner_template || !owner_type) return expression;
        for (int index = 0; index < owner_template->param_count; ++index) {
            TemplateParam* parameter = &owner_template->params[index];
            Expr* owner_argument;
            if (parameter->kind != TPARAM_NONTYPE || !parameter->name ||
                strcmp(parameter->name, expression->ident_name) != 0) {
                continue;
            }
            if (owner_type->cxx_template_value_args &&
                index < owner_type->cxx_template_arg_count &&
                (owner_argument =
                     owner_type->cxx_template_value_args[index])) {
                return owner_argument;
            }
            if (owner_type->cxx_class &&
                index < owner_type->cxx_class->template_arg_count &&
                owner_type->cxx_class->template_value_present &&
                owner_type->cxx_class->template_value_present[index]) {
                return expr_int(
                    owner_type->cxx_class->template_value_args[index],
                    expression->loc);
            }
            return expression;
        }
        return expression;
    }
    switch (expression->kind) {
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            left = cxx_substitute_class_owner_value_expression(
                expression->unary_operand, substitution);
            if (left == expression->unary_operand) return expression;
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->unary_operand = left;
            return copy;
        case EXPR_CAST:
            left = cxx_substitute_class_owner_value_expression(
                expression->cast_expr, substitution);
            if (left == expression->cast_expr) return expression;
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->cast_expr = left;
            return copy;
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
            left = cxx_substitute_class_owner_value_expression(
                expression->binary_lhs, substitution);
            right = cxx_substitute_class_owner_value_expression(
                expression->binary_rhs, substitution);
            if (left == expression->binary_lhs &&
                right == expression->binary_rhs) {
                return expression;
            }
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->binary_lhs = left;
            copy->binary_rhs = right;
            return copy;
        case EXPR_COND: {
            Expr* test = cxx_substitute_class_owner_value_expression(
                expression->cond_test, substitution);
            Expr* then_expression = cxx_substitute_class_owner_value_expression(
                expression->cond_then, substitution);
            Expr* else_expression = cxx_substitute_class_owner_value_expression(
                expression->cond_else, substitution);
            if (test == expression->cond_test &&
                then_expression == expression->cond_then &&
                else_expression == expression->cond_else) {
                return expression;
            }
            copy = ast_arena_alloc(sizeof(*copy));
            *copy = *expression;
            copy->cond_test = test;
            copy->cond_then = then_expression;
            copy->cond_else = else_expression;
            return copy;
        }
        default:
            return expression;
    }
}

static Type* cxx_substitute_class_owner_value_expressions(
    Type* type, const CxxAliasOwnerValueSubstitution* substitution) {
    Type* base = type ? type->base : NULL;
    Expr* array_bound = type ? type->array_bound : NULL;
    Type* return_type = type ? type->ret_type : NULL;
    TypeParam* parameters = NULL;
    TypeParam** parameter_tail = &parameters;
    Type** template_arguments = NULL;
    Expr** template_value_arguments = NULL;
    bool changed = false;
    Type* copy;
    if (!type || !substitution) return type;
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) {
        base = cxx_substitute_class_owner_value_expressions(
            type->base, substitution);
        changed = base != type->base;
    }
    if (type->kind == TYPE_ARRAY && type->array_bound) {
        array_bound = cxx_substitute_class_owner_value_expression(
            type->array_bound, substitution);
        changed = changed || array_bound != type->array_bound;
    }
    if (type->kind == TYPE_FUNC) {
        return_type = cxx_substitute_class_owner_value_expressions(
            type->ret_type, substitution);
        changed = return_type != type->ret_type;
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            TypeParam* parameter_copy = ast_arena_alloc(sizeof(*parameter_copy));
            *parameter_copy = *parameter;
            parameter_copy->type = cxx_substitute_class_owner_value_expressions(
                parameter->type, substitution);
            parameter_copy->next = NULL;
            if (parameter_copy->type != parameter->type) changed = true;
            *parameter_tail = parameter_copy;
            parameter_tail = &parameter_copy->next;
        }
    }
    if (type->cxx_template_arg_count > 0 && type->cxx_template_args) {
        template_arguments = ast_arena_alloc(
            sizeof(Type*) * (size_t)type->cxx_template_arg_count);
        for (int index = 0; index < type->cxx_template_arg_count; ++index) {
            template_arguments[index] =
                cxx_substitute_class_owner_value_expressions(
                    type->cxx_template_args[index], substitution);
            if (template_arguments[index] != type->cxx_template_args[index]) {
                changed = true;
            }
        }
    }
    if (type->cxx_template_arg_count > 0 &&
        type->cxx_template_value_args) {
        template_value_arguments = ast_arena_alloc(
            sizeof(Expr*) * (size_t)type->cxx_template_arg_count);
        for (int index = 0; index < type->cxx_template_arg_count; ++index) {
            template_value_arguments[index] =
                cxx_substitute_class_owner_value_expression(
                    type->cxx_template_value_args[index], substitution);
            if (template_value_arguments[index] !=
                type->cxx_template_value_args[index]) {
                changed = true;
            }
        }
    }
    if (!changed) return type;
    copy = ast_arena_alloc(sizeof(*copy));
    *copy = *type;
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) copy->base = base;
    if (type->kind == TYPE_ARRAY && array_bound != type->array_bound) {
        int64_t value;
        copy->array_bound = array_bound;
        if (expr_eval_integer_constant(array_bound, &value)) {
            if (value <= 0 || value > INT_MAX) {
                rcc_error(array_bound->loc,
                          "template array bound is out of range");
                return NULL;
            }
            copy->array_len = (int)value;
            copy->array_bound = NULL;
            copy->size = copy->base->size * copy->array_len;
        }
    }
    if (type->kind == TYPE_FUNC) {
        copy->ret_type = return_type;
        copy->params = parameters;
    }
    if (template_arguments) copy->cxx_template_args = template_arguments;
    if (template_value_arguments) {
        copy->cxx_template_value_args = template_value_arguments;
    }
    return copy;
}

static Type* parse_alias_template_specialization(CxxTemplate* tmpl,
                                                  SourceLoc loc,
                                                  Type* owner_type) {
    Type** arguments = NULL;
    int64_t* values = NULL;
    bool* value_present = NULL;
    Expr** value_expressions = NULL;
    CxxClass* owner_instance = owner_type ? owner_type->cxx_class : NULL;
    CxxTemplate* owner_template = owner_instance
        ? owner_instance->templ
        : owner_type ? owner_type->cxx_template : NULL;
    Type** owner_arguments = NULL;
    int64_t* owner_values = NULL;
    bool* owner_value_present = NULL;
    int argument_count = 0;
    int owner_argument_count = 0;
    int substitution_argument_count;
    int declared_argument_count;
    size_t substitution_slot_count;
    CxxAliasOwnerValueSubstitution owner_value_substitution = { 0 };

    if (!tmpl || tmpl->kind != TMPL_ALIAS || !tmpl->alias_type) {
        rcc_error(loc, "invalid alias template declaration");
        return type_int;
    }
    if (tmpl->enclosing_template &&
        owner_template == tmpl->enclosing_template) {
        if (owner_instance) {
            owner_argument_count = owner_instance->template_arg_count;
            owner_arguments = owner_instance->template_args;
            owner_values = owner_instance->template_value_args;
            owner_value_present = owner_instance->template_value_present;
        } else if (owner_type && owner_type->cxx_dependent) {
            owner_argument_count = owner_type->cxx_template_arg_count;
            owner_arguments = owner_type->cxx_template_args;
        }
        if (owner_argument_count != tmpl->enclosing_template->param_count ||
            (owner_argument_count > 0 && !owner_arguments)) {
            rcc_error(loc,
                      "class alias-template owner arguments are incomplete");
            return type_int;
        }
    }
    if (tmpl->param_count < 0 || owner_argument_count < 0 ||
        owner_argument_count > INT_MAX - tmpl->param_count) {
        rcc_error(loc, "alias template argument count overflow");
        return type_int;
    }
    substitution_argument_count = tmpl->param_count + owner_argument_count;
    declared_argument_count = tmpl->param_count;
    if (tmpl->enclosing_template) {
        if (tmpl->enclosing_template->param_count < 0 ||
            tmpl->enclosing_template->param_count >
                INT_MAX - declared_argument_count) {
            rcc_error(loc, "alias template argument count overflow");
            return type_int;
        }
        declared_argument_count += tmpl->enclosing_template->param_count;
    }
    substitution_slot_count = (size_t)(substitution_argument_count >
                                               declared_argument_count
                                           ? substitution_argument_count
                                           : declared_argument_count);
    if (substitution_slot_count == 0u) substitution_slot_count = 1u;
    if (substitution_slot_count > SIZE_MAX / sizeof(*arguments) ||
        substitution_slot_count > SIZE_MAX / sizeof(*values) ||
        substitution_slot_count > SIZE_MAX / sizeof(*value_present) ||
        substitution_slot_count > SIZE_MAX / sizeof(*value_expressions)) {
        rcc_error(loc, "alias template argument storage size overflow");
        return type_int;
    }
    arguments = ast_arena_alloc(sizeof(*arguments) * substitution_slot_count);
    values = ast_arena_alloc(sizeof(*values) * substitution_slot_count);
    value_present =
        ast_arena_alloc(sizeof(*value_present) * substitution_slot_count);
    value_expressions = ast_arena_alloc(
        sizeof(*value_expressions) * substitution_slot_count);
    memset(arguments, 0, sizeof(*arguments) * substitution_slot_count);
    memset(values, 0, sizeof(*values) * substitution_slot_count);
    memset(value_present, 0,
           sizeof(*value_present) * substitution_slot_count);
    memset(value_expressions, 0,
           sizeof(*value_expressions) * substitution_slot_count);
    owner_value_substitution.alias_template = tmpl;
    owner_value_substitution.alias_value_expressions = value_expressions;
    owner_value_substitution.alias_values = values;
    owner_value_substitution.alias_value_present = value_present;
    owner_value_substitution.owner_template = tmpl->enclosing_template;
    owner_value_substitution.owner_type = owner_type;
    expect(TOK_LT, "<");
    if (!check(TOK_GT)) {
        do {
            TemplateParam* parameter;
            if (argument_count >= tmpl->param_count) {
                rcc_error(peek()->loc, "too many alias template arguments");
                while (!check(TOK_COMMA) && !check(TOK_GT) && !at_end()) {
                    advance();
                }
                continue;
            }
            parameter = &tmpl->params[argument_count];
            if (parameter->kind == TPARAM_TYPE) {
                arguments[argument_count] = parse_cxx_type_spec();
            } else if (parameter->kind == TPARAM_NONTYPE) {
                Expr* value_expression;
                int64_t value;
                rcc_parser_set_cxx_template_default_mode(true);
                value_expression = parse_assignment_expression();
                rcc_parser_set_cxx_template_default_mode(false);
                value_expressions[argument_count] = value_expression;
                if (expr_eval_integer_constant(value_expression, &value)) {
                    values[argument_count] = value;
                    value_present[argument_count] = true;
                } else if (!cxx_expression_references_template_non_type_parameter(
                               value_expression, active_template)) {
                    rcc_error(loc,
                              "alias template non-type argument must be an "
                              "integer constant expression");
                    values[argument_count] = 0;
                    value_present[argument_count] = true;
                }
                arguments[argument_count] = parameter->type;
            } else {
                rcc_error(loc,
                          "alias template template-arguments are not "
                          "supported by the bounded type ABI");
            }
            ++argument_count;
        } while (match(TOK_COMMA));
    }
    if (owner_argument_count > 0) {
        memcpy(arguments + tmpl->param_count, owner_arguments,
               sizeof(arguments[0]) * (size_t)owner_argument_count);
        if (owner_values && owner_value_present) {
            memcpy(values + tmpl->param_count,
                   owner_values,
                   sizeof(values[0]) * (size_t)owner_argument_count);
            memcpy(value_present + tmpl->param_count,
                   owner_value_present,
                   sizeof(value_present[0]) *
                       (size_t)owner_argument_count);
        }
    }
    while (argument_count < tmpl->param_count &&
           tmpl->params[argument_count].has_default) {
        TemplateParam* parameter = &tmpl->params[argument_count];
        if (parameter->kind == TPARAM_TYPE && parameter->default_type) {
            arguments[argument_count] = substitute_template_type(
                tmpl, parameter->default_type, arguments,
                substitution_argument_count, values, value_present);
        } else if (parameter->kind == TPARAM_NONTYPE &&
                   parameter->default_value) {
            int64_t value;
            CxxTemplate* default_context = parameter->default_context
                ? parameter->default_context : tmpl;
            if (parameter->default_context) {
                parameter->default_context->pending_pack_count =
                    tmpl->pending_pack_count;
            }
            if (!eval_template_integer_expression(
                    parameter->default_value, default_context,
                    values, value_present,
                    &value)) {
                if (cxx_expression_references_template_non_type_parameter(
                        parameter->default_value, default_context)) {
                    value_expressions[argument_count] =
                        cxx_substitute_class_owner_value_expression(
                            parameter->default_value,
                            &owner_value_substitution);
                } else {
                    rcc_error(loc,
                              "alias template non-type default must be an "
                              "integer constant expression");
                    values[argument_count] = 0;
                    value_present[argument_count] = true;
                }
            } else {
                values[argument_count] = value;
                value_present[argument_count] = true;
            }
            arguments[argument_count] = parameter->type;
        } else {
            break;
        }
        ++argument_count;
    }
    expect(TOK_GT, ">");
    if (argument_count != tmpl->param_count) {
        rcc_error(loc, "alias template '%s' expects %d argument(s), got %d",
                  tmpl->name ? tmpl->name : "<alias>", tmpl->param_count,
                  argument_count);
        return type_int;
    }
    if (!cxx_template_constraint_satisfied(
            tmpl, arguments, values, value_present, loc, true, NULL)) {
        return type_int;
    }
    {
        Type* result = substitute_template_type(
            tmpl, tmpl->alias_type, arguments, substitution_argument_count,
            values, value_present);
        if (result && owner_type && owner_type->cxx_dependent &&
            owner_template == tmpl->enclosing_template) {
            result = cxx_substitute_class_owner_value_expressions(
                result, &owner_value_substitution);
        }
        if (!result) {
            rcc_error(loc, "alias template '%s' could not be substituted",
                      tmpl->name ? tmpl->name : "<alias>");
            return type_int;
        }
        return result;
    }
}

static bool consume_cxx_class_alias_template_owner(
    const char** owner_name_out) {
    char owner_name[512];
    size_t length = 0u;
    bool global_qualified = false;

    if (owner_name_out) *owner_name_out = NULL;
    owner_name[0] = '\0';
    if (check(TOK_SCOPE)) {
        if (!parser.cur->next || parser.cur->next->type != TOK_IDENT) {
            return false;
        }
        owner_name[length++] = ':';
        owner_name[length++] = ':';
        owner_name[length] = '\0';
        advance();
        global_qualified = true;
    }
    while (check(TOK_IDENT)) {
        const char* component = peek()->value.str_val;
        size_t component_length = strlen(component);
        if (component_length >= sizeof(owner_name) - length) return false;
        memcpy(owner_name + length, component, component_length);
        length += component_length;
        owner_name[length] = '\0';
        advance();
        if (check(TOK_SCOPE) && parser.cur->next &&
            parser.cur->next->type == TOK_IDENT) {
            CxxClass* candidate_owner = find_class(owner_name);
            CxxClassAliasTemplate* inherited_alias = NULL;
            bool ambiguous = false;
            bool accessible = false;
            if (!candidate_owner) {
                Type* candidate_type = rcc_parser_lookup_type(owner_name);
                if (candidate_type &&
                    (candidate_type->kind == TYPE_STRUCT ||
                     candidate_type->kind == TYPE_UNION)) {
                    candidate_owner = candidate_type->cxx_class;
                }
            }
            if (candidate_owner) {
                cxx_resolve_known_class_bases(candidate_owner);
                inherited_alias = cxx_parser_find_inherited_alias_template(
                    candidate_owner, parser.cur->next->value.str_val,
                    active_class, NULL, &ambiguous, &accessible);
            }
            if (candidate_owner && parser.cur->next->next &&
                parser.cur->next->next->type == TOK_LT &&
                (inherited_alias || ambiguous)) {
                break;
            }
            if (length + 2u >= sizeof(owner_name)) return false;
            owner_name[length++] = ':';
            owner_name[length++] = ':';
            owner_name[length] = '\0';
            advance();
            continue;
        }
        break;
    }
    if (length == 0u || (!global_qualified && owner_name[0] == '\0')) {
        return false;
    }
    if (owner_name_out) *owner_name_out = rcc_intern(owner_name);
    return true;
}

static bool cxx_class_template_id_owner_starts(void) {
    Token* token = parser.cur;
    if (token && token->type == TOK_SCOPE) token = token->next;
    while (token && token->type == TOK_IDENT) {
        if (token->next && token->next->type == TOK_LT) return true;
        if (!token->next || token->next->type != TOK_SCOPE ||
            !token->next->next || token->next->next->type != TOK_IDENT) {
            return false;
        }
        token = token->next->next;
    }
    return false;
}

static bool cxx_class_template_alias_template_starts(
    const char** owner_template_name_out) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    const char* owner_template_name = NULL;
    CxxTemplate* owner_template;
    Token* token;
    int depth = 0;
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;
    bool starts = false;

    if (owner_template_name_out) *owner_template_name_out = NULL;
    if (!cxx_class_template_id_owner_starts() ||
        !cxx_template_id_followed_by_scope(&owner_template_name)) {
        goto done;
    }
    owner_template = find_class_template(owner_template_name);
    if (!owner_template && active_template &&
        active_template->kind == TMPL_CLASS && active_template->name &&
        strcmp(active_template->name, owner_template_name) == 0) {
        owner_template = active_template;
    }
    if (!owner_template) goto done;

    (void)parse_qualified_name();
    for (token = parser.cur; token && token->type != TOK_EOF;
         token = token->next) {
        if (token->type == TOK_LPAREN) {
            ++parentheses;
            continue;
        }
        if (token->type == TOK_RPAREN && parentheses > 0) {
            --parentheses;
            continue;
        }
        if (token->type == TOK_LBRACKET) {
            ++brackets;
            continue;
        }
        if (token->type == TOK_RBRACKET && brackets > 0) {
            --brackets;
            continue;
        }
        if (token->type == TOK_LBRACE) {
            ++braces;
            continue;
        }
        if (token->type == TOK_RBRACE && braces > 0) {
            --braces;
            continue;
        }
        if (parentheses || brackets || braces) continue;
        if (token->type == TOK_LT) {
            ++depth;
        } else if (token->type == TOK_GT) {
            if (depth > 0) --depth;
            if (depth == 0) {
                token = token->next;
                break;
            }
        } else if (token->type == TOK_RSHIFT) {
            depth -= 2;
            if (depth <= 0) {
                token = token->next;
                break;
            }
        }
    }
    if (token && token->type == TOK_SCOPE) {
        token = token->next;
        if (token && token->type == TOK_TEMPLATE) token = token->next;
        starts = token && token->type == TOK_IDENT && token->next &&
                 token->next->type == TOK_LT;
    }
done:
    parser.cur = saved_cur;
    parser.prev = saved_prev;
    if (starts && owner_template_name_out) {
        *owner_template_name_out = owner_template_name;
    }
    return starts;
}

static bool cxx_qualified_class_alias_template_starts(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    bool starts = false;
    const char* owner_name = NULL;

    if (cxx_class_template_alias_template_starts(NULL)) {
        starts = true;
    } else if (consume_cxx_class_alias_template_owner(&owner_name) &&
               owner_name) {
        bool has_template_keyword;
        CxxClass* owner;
        if (match(TOK_SCOPE)) {
            has_template_keyword = match(TOK_TEMPLATE);
            if (check(TOK_IDENT) && parser.cur->next &&
                parser.cur->next->type == TOK_LT) {
                const char* alias_name = peek()->value.str_val;
                owner = find_class(owner_name);
                if (!owner) {
                    Type* owner_type = rcc_parser_lookup_type(owner_name);
                    if (owner_type &&
                        (owner_type->kind == TYPE_STRUCT ||
                         owner_type->kind == TYPE_UNION)) {
                        owner = owner_type->cxx_class;
                    }
                }
                if (owner) cxx_resolve_known_class_bases(owner);
                {
                    CxxClass* declaring_class = NULL;
                    bool ambiguous = false;
                    bool accessible = false;
                    starts = has_template_keyword ||
                        cxx_parser_find_inherited_alias_template(
                            owner, alias_name, active_class,
                            &declaring_class, &ambiguous,
                            &accessible) != NULL || ambiguous;
                }
            }
        }
    }
    parser.cur = saved_cur;
    parser.prev = saved_prev;
    return starts;
}

static void cxx_resolve_known_class_bases(CxxClass* cls) {
    if (!cls) return;
    for (int index = 0; index < cls->base_count; ++index) {
        if (!cls->bases[index].base && cls->bases[index].base_name) {
            CxxClass* base = find_class(cls->bases[index].base_name);
            if (base && base != cls) cls->bases[index].base = base;
        }
    }
}

static bool cxx_class_access_context_is_derived_from(
    CxxClass* context, CxxClass* target, CxxClass* access_context,
    unsigned depth) {
    if (!context || !target || depth > 64u) return false;
    if (context == target) return true;
    cxx_resolve_known_class_bases(context);
    for (int index = 0; index < context->base_count; ++index) {
        CxxClass* base = context->bases[index].base;
        if (context->bases[index].access == ACCESS_PRIVATE &&
            context != access_context) {
            continue;
        }
        if (cxx_class_access_context_is_derived_from(
                base, target, access_context, depth + 1u)) {
            return true;
        }
    }
    return false;
}

static CxxClassAliasTemplate* cxx_parser_find_inherited_alias_template(
    CxxClass* owner, const char* name, CxxClass* access_context,
    CxxClass** declaring_class, bool* ambiguous, bool* accessible) {
    CxxClass* found_declaring_class = NULL;
    bool found_ambiguous = false;
    bool found_accessible = false;
    CxxClassAliasTemplate* found = cxx_class_find_inherited_alias_template(
        owner, name, access_context, &found_declaring_class,
        &found_ambiguous, &found_accessible);
    if (declaring_class) *declaring_class = found_declaring_class;
    if (ambiguous) *ambiguous = found_ambiguous;
    if (accessible) *accessible = found_accessible;
    if (!found || found_ambiguous || found_accessible) {
        return found;
    }
    for (CxxFriendAccess* grant = active_friend_access_context; grant;
         grant = grant->next) {
        CxxClass* friend_declaring_class = NULL;
        bool friend_ambiguous = false;
        bool friend_accessible = false;
        CxxClassAliasTemplate* friend_result =
            cxx_class_find_inherited_alias_template(
                owner, name, grant->owner, &friend_declaring_class,
                &friend_ambiguous, &friend_accessible);
        if (friend_result == found && !friend_ambiguous &&
            friend_accessible) {
            if (declaring_class) *declaring_class = friend_declaring_class;
            if (accessible) *accessible = true;
            return found;
        }
    }
    if (active_friend_signature_collection) {
        for (CxxFriendAccess* grant = active_friend_signature_candidates;
             grant; grant = grant->next) {
            CxxClass* friend_declaring_class = NULL;
            bool friend_ambiguous = false;
            bool friend_accessible = false;
            CxxClassAliasTemplate* friend_result =
                cxx_class_find_inherited_alias_template(
                    owner, name, grant->owner, &friend_declaring_class,
                    &friend_ambiguous, &friend_accessible);
            if (friend_result == found && !friend_ambiguous &&
                friend_accessible) {
                cxx_record_friend_signature_alias_use(owner, name, found);
                if (declaring_class) {
                    *declaring_class = friend_declaring_class;
                }
                if (accessible) *accessible = true;
                return found;
            }
        }
    }
    return found;
}

static Type* parse_qualified_class_alias_template_type(SourceLoc loc) {
    const char* owner_template_name = NULL;
    const char* owner_name = NULL;
    const char* alias_name;
    CxxClass* owner;
    CxxClass* owner_instance;
    CxxClassAliasTemplate* alias_template;
    Type* owner_type;
    Type* result;
    bool accessible;

    if (cxx_class_template_alias_template_starts(&owner_template_name)) {
        CxxTemplate* owner_class_template =
            find_class_template(owner_template_name);
        CxxClass* owner_definition;
        CxxClass* alias_declaring_class = NULL;
        CxxClass* lookup_owner;
        Type* alias_owner_type;
        bool ambiguous = false;
        if (!owner_class_template && active_template &&
            active_template->kind == TMPL_CLASS && active_template->name &&
            strcmp(active_template->name, owner_template_name) == 0) {
            owner_class_template = active_template;
        }
        (void)parse_qualified_name();
        if (!owner_class_template) {
            rcc_error(loc, "unknown class-template alias owner '%s'",
                      owner_template_name);
            return type_int;
        }
        owner_type = parse_class_template_specialization(
            owner_class_template, loc);
        if (!match(TOK_SCOPE)) {
            rcc_error(peek()->loc, "expected :: before nested alias template");
            return type_int;
        }
        (void)match(TOK_TEMPLATE);
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc, "expected nested alias template name");
            return type_int;
        }
        alias_name = rcc_intern(advance()->value.str_val);
        owner_instance = owner_type ? owner_type->cxx_class : NULL;
        owner_definition = owner_instance && owner_instance->templ &&
                owner_instance->templ->templated_class
            ? owner_instance->templ->templated_class
            : owner_class_template->templated_class;
        if (!owner_definition && active_class &&
            active_template == owner_class_template) {
            owner_definition = active_class;
        }
        cxx_resolve_known_class_bases(active_class);
        lookup_owner = owner_instance ? owner_instance : owner_definition;
        alias_template = cxx_parser_find_inherited_alias_template(
            lookup_owner, alias_name, active_class, &alias_declaring_class,
            &ambiguous, &accessible);
        if (!alias_template || !alias_template->declaration) {
            if (ambiguous) {
                rcc_error(loc,
                          "nested alias template '%s' is ambiguous in class '%s'",
                          alias_name, owner_template_name);
            } else {
                rcc_error(loc,
                          "class-template specialization '%s' has no nested alias template '%s'",
                          owner_template_name, alias_name);
            }
            skip_cxx_template_arguments();
            return type_int;
        }
        if (!accessible &&
            alias_template->access != ACCESS_PRIVATE &&
            cxx_class_access_context_is_derived_from(
                active_class, alias_declaring_class, active_class, 0u)) {
            accessible = true;
        }
        alias_owner_type = owner_type;
        if (owner_instance && alias_declaring_class &&
            alias_declaring_class != owner_instance &&
            alias_declaring_class->templ &&
            alias_declaring_class->templ->templated_class !=
                alias_declaring_class) {
            alias_owner_type = alias_declaring_class->type;
        }
        result = parse_alias_template_specialization(
            alias_template->declaration, loc, alias_owner_type);
        if (!accessible) {
            rcc_error(loc,
                      "nested alias template '%s' is inaccessible in class '%s'",
                      alias_name,
                      owner_definition->name ? owner_definition->name
                                              : owner_template_name);
            return type_int;
        }
        return result;
    }

    if (!consume_cxx_class_alias_template_owner(&owner_name) ||
        !owner_name) {
        rcc_error(loc, "invalid qualified class alias-template owner");
        return type_int;
    }
    owner = find_class(owner_name);
    expect(TOK_SCOPE, ":: before nested alias template");
    (void)match(TOK_TEMPLATE);
    if (!check(TOK_IDENT)) {
        rcc_error(peek()->loc, "expected nested alias template name");
        return type_int;
    }
    alias_name = rcc_intern(advance()->value.str_val);
    if (!owner) {
        owner_type = rcc_parser_lookup_type(owner_name);
        if (owner_type && (owner_type->kind == TYPE_STRUCT ||
                           owner_type->kind == TYPE_UNION)) {
            owner = owner_type->cxx_class;
        }
    }
    if (!owner) {
        rcc_error(loc, "unknown class alias-template owner '%s'", owner_name);
        skip_cxx_template_arguments();
        return type_int;
    }
    {
        CxxClass* alias_declaring_class = NULL;
        bool ambiguous = false;
        cxx_resolve_known_class_bases(active_class);
        alias_template = cxx_parser_find_inherited_alias_template(
            owner, alias_name, active_class, &alias_declaring_class,
            &ambiguous, &accessible);
        if (!alias_template || !alias_template->declaration) {
            if (ambiguous) {
                rcc_error(loc,
                          "nested alias template '%s' is ambiguous in class '%s'",
                          alias_name, owner->name ? owner->name : owner_name);
            } else {
                rcc_error(loc, "class '%s' has no nested alias template '%s'",
                          owner->name ? owner->name : owner_name, alias_name);
            }
            skip_cxx_template_arguments();
            return type_int;
        }
        if (!accessible && alias_template->access != ACCESS_PRIVATE &&
            cxx_class_access_context_is_derived_from(
                active_class, alias_declaring_class, active_class, 0u)) {
            accessible = true;
        }
        result = parse_alias_template_specialization(
            alias_template->declaration, loc,
            alias_declaring_class ? alias_declaring_class->type : owner->type);
    }
    if (!accessible) {
        rcc_error(loc,
                  "nested alias template '%s' is inaccessible in class '%s'",
                  alias_name, owner->name ? owner->name : owner_name);
        return type_int;
    }
    return result;
}

typedef struct CxxParsedTemplateArgument {
    bool is_type;
    Type* type;
    int64_t value;
    bool value_valid;
} CxxParsedTemplateArgument;

typedef struct CxxFunctionTemplateMatch {
    CxxTemplate* tmpl;
    Type* arguments[32];
    Type* pack_arguments[32];
    int64_t pack_values[32];
    bool pack_value_present[32];
    int pack_count;
    int64_t values[32];
    bool value_present[32];
    int argument_count;
    int specificity;
    int conversion_ranks[32];
    Type* conversion_targets[32];
    int conversion_rank_count;
    Decl* instance;
    Type* substitution_failure_type;
} CxxFunctionTemplateMatch;

typedef struct CxxConstraintAtom {
    const Expr* origin;
    const Expr* expression;
    const CxxTemplate* parameter_context;
} CxxConstraintAtom;

typedef enum CxxConstraintNodeKind {
    CXX_CONSTRAINT_ATOM,
    CXX_CONSTRAINT_CONJUNCTION,
    CXX_CONSTRAINT_DISJUNCTION
} CxxConstraintNodeKind;

typedef struct CxxConstraintNode {
    CxxConstraintNodeKind kind;
    CxxConstraintAtom atom;
    struct CxxConstraintNode* left;
    struct CxxConstraintNode* right;
} CxxConstraintNode;

typedef struct CxxConstraintClause {
    CxxConstraintAtom* atoms;
    size_t atom_count;
} CxxConstraintClause;

typedef struct CxxConstraintForm {
    CxxConstraintClause* clauses;
    size_t clause_count;
} CxxConstraintForm;

typedef struct CxxConstraintNormalForms {
    CxxConstraintForm dnf;
    CxxConstraintForm cnf;
} CxxConstraintNormalForms;

typedef struct CxxConstraintExpansionFrame {
    CxxTemplate* concept_template;
    const struct CxxConstraintExpansionFrame* parent;
} CxxConstraintExpansionFrame;

typedef struct CxxConstraintLocalParameters {
    const DeclList* parameters;
    const struct CxxConstraintLocalParameters* parent;
} CxxConstraintLocalParameters;

static bool cxx_parser_template_qualification_targets_related(
    Type* left, Type* right, bool ignore_current_qualification,
    bool* left_subset, bool* right_subset, unsigned depth) {
    if (!left || !right || depth > 64u || left->kind != right->kind) {
        return false;
    }
    if (!ignore_current_qualification) {
        if ((left->is_const && !right->is_const) ||
            (left->is_volatile && !right->is_volatile)) {
            *left_subset = false;
        }
        if ((!left->is_const && right->is_const) ||
            (!left->is_volatile && right->is_volatile)) {
            *right_subset = false;
        }
    }
    if (left->kind == TYPE_PTR) {
        if (left->cxx_is_member_pointer || right->cxx_is_member_pointer ||
            !left->base || !right->base) {
            return false;
        }
        return cxx_parser_template_qualification_targets_related(
            left->base, right->base, false, left_subset, right_subset,
            depth + 1u);
    }
    if (left->kind == TYPE_ARRAY) {
        if (left->array_len >= 0 && right->array_len >= 0 &&
            left->array_len != right->array_len) {
            return false;
        }
        return cxx_parser_template_qualification_targets_related(
            left->base, right->base, false, left_subset, right_subset,
            depth + 1u);
    }
    return type_is_compatible(left, right);
}

static int cxx_template_conversion_vector_relation(
    const CxxFunctionTemplateMatch* left,
    const CxxFunctionTemplateMatch* right, bool* incomparable);

static bool cxx_constraint_identifier_is_local(
    const CxxConstraintLocalParameters* scopes, const Expr* expression) {
    for (; scopes; scopes = scopes->parent) {
        for (const DeclList* parameter = scopes->parameters;
             parameter; parameter = parameter->next) {
            if (parameter->decl && parameter->decl->name &&
                expression->ident_name &&
                strcmp(parameter->decl->name,
                       expression->ident_name) == 0) {
                return true;
            }
        }
    }
    return false;
}

static void cxx_constraint_substitute_nontype_list(
    ExprList* list, CxxTemplate* tmpl, Expr** arguments,
    const CxxConstraintLocalParameters* local_parameters);

static Expr* cxx_constraint_substitute_nontype_expression(
    Expr* expression, CxxTemplate* tmpl, Expr** arguments,
    const CxxConstraintLocalParameters* local_parameters) {
    if (!expression) return NULL;
    if (expression->kind == EXPR_IDENT && expression->ident_name &&
        arguments &&
        !cxx_constraint_identifier_is_local(local_parameters, expression)) {
        for (int index = 0; tmpl && index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                arguments[index] &&
                strcmp(parameter->name, expression->ident_name) == 0) {
                return arguments[index];
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
            expression->unary_operand =
                cxx_constraint_substitute_nontype_expression(
                    expression->unary_operand, tmpl, arguments,
                    local_parameters);
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
            expression->binary_lhs =
                cxx_constraint_substitute_nontype_expression(
                    expression->binary_lhs, tmpl, arguments,
                    local_parameters);
            expression->binary_rhs =
                cxx_constraint_substitute_nontype_expression(
                    expression->binary_rhs, tmpl, arguments,
                    local_parameters);
            break;
        case EXPR_COND:
            expression->cond_test =
                cxx_constraint_substitute_nontype_expression(
                    expression->cond_test, tmpl, arguments,
                    local_parameters);
            expression->cond_then =
                cxx_constraint_substitute_nontype_expression(
                    expression->cond_then, tmpl, arguments,
                    local_parameters);
            expression->cond_else =
                cxx_constraint_substitute_nontype_expression(
                    expression->cond_else, tmpl, arguments,
                    local_parameters);
            break;
        case EXPR_CALL:
            expression->call_func =
                cxx_constraint_substitute_nontype_expression(
                    expression->call_func, tmpl, arguments,
                    local_parameters);
            cxx_constraint_substitute_nontype_list(
                expression->call_args, tmpl, arguments, local_parameters);
            expression->call_new_count =
                cxx_constraint_substitute_nontype_expression(
                    expression->call_new_count, tmpl, arguments,
                    local_parameters);
            cxx_constraint_substitute_nontype_list(
                expression->call_new_args, tmpl, arguments,
                local_parameters);
            break;
        case EXPR_CXX_TYPEID:
            expression->cxx_typeid_operand =
                cxx_constraint_substitute_nontype_expression(
                    expression->cxx_typeid_operand, tmpl, arguments,
                    local_parameters);
            break;
        case EXPR_INDEX:
            expression->index_base =
                cxx_constraint_substitute_nontype_expression(
                    expression->index_base, tmpl, arguments,
                    local_parameters);
            expression->index_expr =
                cxx_constraint_substitute_nontype_expression(
                    expression->index_expr, tmpl, arguments,
                    local_parameters);
            break;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            expression->member_base =
                cxx_constraint_substitute_nontype_expression(
                    expression->member_base, tmpl, arguments,
                    local_parameters);
            break;
        case EXPR_CAST:
            expression->cast_expr =
                cxx_constraint_substitute_nontype_expression(
                    expression->cast_expr, tmpl, arguments,
                    local_parameters);
            break;
        case EXPR_COMPOUND:
            cxx_constraint_substitute_nontype_list(
                expression->compound_init, tmpl, arguments,
                local_parameters);
            break;
        case EXPR_GENERIC:
            expression->generic_control =
                cxx_constraint_substitute_nontype_expression(
                    expression->generic_control, tmpl, arguments,
                    local_parameters);
            for (GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                association->expr =
                    cxx_constraint_substitute_nontype_expression(
                        association->expr, tmpl, arguments,
                        local_parameters);
            }
            break;
        case EXPR_CXX_FOLD:
            expression->cxx_fold_init =
                cxx_constraint_substitute_nontype_expression(
                    expression->cxx_fold_init, tmpl, arguments,
                    local_parameters);
            expression->cxx_fold_pattern =
                cxx_constraint_substitute_nontype_expression(
                    expression->cxx_fold_pattern, tmpl, arguments,
                    local_parameters);
            break;
        case EXPR_CXX_REQUIRES: {
            CxxConstraintLocalParameters nested_scope = {
                expression->cxx_requires_params, local_parameters};
            cxx_constraint_substitute_nontype_list(
                expression->cxx_requires_items, tmpl, arguments,
                &nested_scope);
            cxx_constraint_substitute_nontype_list(
                expression->cxx_requires_nested, tmpl, arguments,
                &nested_scope);
            for (CxxCompoundRequirement* requirement =
                     expression->cxx_requires_compound;
                 requirement; requirement = requirement->next) {
                requirement->expr =
                    cxx_constraint_substitute_nontype_expression(
                        requirement->expr, tmpl, arguments, &nested_scope);
            }
            break;
        }
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            expression->va_list_operand =
                cxx_constraint_substitute_nontype_expression(
                    expression->va_list_operand, tmpl, arguments,
                    local_parameters);
            expression->va_second_operand =
                cxx_constraint_substitute_nontype_expression(
                    expression->va_second_operand, tmpl, arguments,
                    local_parameters);
            break;
        default:
            break;
    }
    return expression;
}

static void cxx_constraint_substitute_nontype_list(
    ExprList* list, CxxTemplate* tmpl, Expr** arguments,
    const CxxConstraintLocalParameters* local_parameters) {
    for (; list; list = list->next) {
        list->expr = cxx_constraint_substitute_nontype_expression(
            list->expr, tmpl, arguments, local_parameters);
    }
}

static CxxConstraintNode* cxx_constraint_expand(
    const Expr* source, Expr* mapped, CxxTemplate* parameter_context,
    const CxxConstraintExpansionFrame* concept_stack, unsigned depth) {
    CxxConstraintNode* node;
    if (!source || !mapped || depth > 256u) return NULL;

    if (source->kind == EXPR_CALL && source->cxx_concept_template) {
        CxxTemplate* concept = source->cxx_concept_template;
        Type* arguments[32] = { NULL };
        Expr* symbolic_arguments[32] = { NULL };
        int64_t values[32] = { 0 };
        bool value_present[32] = { false };
        ExprList* argument = mapped->call_args;
        Expr* instantiated;
        CxxConstraintExpansionFrame frame;
        if (mapped->kind != EXPR_CALL ||
            mapped->cxx_concept_template != concept ||
            concept->param_count < 0 || concept->param_count > 32 ||
            !concept->constraint) {
            return NULL;
        }
        for (const CxxConstraintExpansionFrame* active = concept_stack;
             active; active = active->parent) {
            if (active->concept_template == concept) return NULL;
        }
        for (int index = 0; index < concept->param_count; ++index) {
            TemplateParam* parameter = &concept->params[index];
            if (!argument || !argument->expr) return NULL;
            if (parameter->kind == TPARAM_TYPE) {
                arguments[index] = argument->expr->type;
                if (!arguments[index]) return NULL;
            } else if (parameter->kind == TPARAM_NONTYPE) {
                symbolic_arguments[index] = argument->expr;
                if (eval_template_integer_expression(
                        argument->expr, parameter_context, NULL, NULL,
                        &values[index])) {
                    value_present[index] = true;
                }
            } else {
                return NULL;
            }
            argument = argument->next;
        }
        if (argument) return NULL;
        instantiated = cxx_template_clone_expr_with_values(
            concept, concept->constraint, arguments, concept->param_count,
            values, value_present);
        if (!instantiated) return NULL;
        instantiated = cxx_constraint_substitute_nontype_expression(
            instantiated, concept, symbolic_arguments, NULL);
        frame.concept_template = concept;
        frame.parent = concept_stack;
        return cxx_constraint_expand(concept->constraint, instantiated,
                                     parameter_context, &frame, depth + 1u);
    }

    if ((source->kind == EXPR_AND || source->kind == EXPR_OR) &&
        mapped->kind == source->kind) {
        node = ast_arena_alloc(sizeof(*node));
        memset(node, 0, sizeof(*node));
        node->kind = source->kind == EXPR_AND
            ? CXX_CONSTRAINT_CONJUNCTION : CXX_CONSTRAINT_DISJUNCTION;
        node->left = cxx_constraint_expand(
            source->binary_lhs, mapped->binary_lhs, parameter_context,
            concept_stack, depth + 1u);
        node->right = cxx_constraint_expand(
            source->binary_rhs, mapped->binary_rhs, parameter_context,
            concept_stack, depth + 1u);
        return node->left && node->right ? node : NULL;
    }

    node = ast_arena_alloc(sizeof(*node));
    memset(node, 0, sizeof(*node));
    node->kind = CXX_CONSTRAINT_ATOM;
    node->atom.origin = source;
    node->atom.expression = mapped;
    node->atom.parameter_context = parameter_context;
    return node;
}

static CxxConstraintForm cxx_constraint_form_single(
    const CxxConstraintAtom* atom) {
    CxxConstraintForm form;
    form.clauses = ast_arena_alloc(sizeof(*form.clauses));
    form.clauses[0].atoms = ast_arena_alloc(sizeof(*form.clauses[0].atoms));
    form.clauses[0].atoms[0] = *atom;
    form.clauses[0].atom_count = 1u;
    form.clause_count = 1u;
    return form;
}

static CxxConstraintForm cxx_constraint_form_combine(
    CxxConstraintForm left, CxxConstraintForm right, bool cross_product) {
    CxxConstraintForm result;
    if (!cross_product) {
        if (left.clause_count > SIZE_MAX - right.clause_count) {
            rcc_fatal("C++ constraint normal form is too large");
        }
        result.clause_count = left.clause_count + right.clause_count;
        if (result.clause_count > SIZE_MAX / sizeof(*result.clauses)) {
            rcc_fatal("C++ constraint normal form is too large");
        }
        result.clauses = ast_arena_alloc(
            result.clause_count * sizeof(*result.clauses));
        memcpy(result.clauses, left.clauses,
               left.clause_count * sizeof(*result.clauses));
        memcpy(result.clauses + left.clause_count, right.clauses,
               right.clause_count * sizeof(*result.clauses));
        return result;
    }
    if (left.clause_count != 0u &&
        right.clause_count > SIZE_MAX / left.clause_count) {
        rcc_fatal("C++ constraint normal form is too large");
    }
    result.clause_count = left.clause_count * right.clause_count;
    if (result.clause_count > SIZE_MAX / sizeof(*result.clauses)) {
        rcc_fatal("C++ constraint normal form is too large");
    }
    result.clauses = ast_arena_alloc(
        result.clause_count * sizeof(*result.clauses));
    size_t output = 0u;
    for (size_t left_index = 0; left_index < left.clause_count;
         ++left_index) {
        for (size_t right_index = 0; right_index < right.clause_count;
             ++right_index) {
            const CxxConstraintClause* left_clause =
                &left.clauses[left_index];
            const CxxConstraintClause* right_clause =
                &right.clauses[right_index];
            CxxConstraintClause* clause = &result.clauses[output++];
            if (left_clause->atom_count > SIZE_MAX -
                                               right_clause->atom_count) {
                rcc_fatal("C++ constraint clause is too large");
            }
            clause->atom_count = left_clause->atom_count +
                                 right_clause->atom_count;
            if (clause->atom_count > SIZE_MAX / sizeof(*clause->atoms)) {
                rcc_fatal("C++ constraint clause is too large");
            }
            clause->atoms = ast_arena_alloc(
                clause->atom_count * sizeof(*clause->atoms));
            memcpy(clause->atoms, left_clause->atoms,
                   left_clause->atom_count * sizeof(*clause->atoms));
            memcpy(clause->atoms + left_clause->atom_count,
                   right_clause->atoms,
                   right_clause->atom_count * sizeof(*clause->atoms));
        }
    }
    return result;
}

static bool cxx_constraint_build_normal_forms(
    const CxxConstraintNode* node, CxxConstraintNormalForms* forms) {
    CxxConstraintNormalForms left;
    CxxConstraintNormalForms right;
    bool conjunction;
    if (!node || !forms) return false;
    if (node->kind == CXX_CONSTRAINT_ATOM) {
        forms->dnf = cxx_constraint_form_single(&node->atom);
        forms->cnf = forms->dnf;
        return true;
    }
    if (!cxx_constraint_build_normal_forms(node->left, &left) ||
        !cxx_constraint_build_normal_forms(node->right, &right)) {
        return false;
    }
    conjunction = node->kind == CXX_CONSTRAINT_CONJUNCTION;
    forms->dnf = cxx_constraint_form_combine(
        left.dnf, right.dnf, conjunction);
    forms->cnf = cxx_constraint_form_combine(
        left.cnf, right.cnf, !conjunction);
    return true;
}

static bool cxx_constraint_atoms_match(const CxxConstraintAtom* left,
                                       const CxxConstraintAtom* right) {
    return left && right && left->origin == right->origin &&
        cxx_template_constraint_mapping_matches(
            left->expression, left->parameter_context,
            right->expression, right->parameter_context);
}

static bool cxx_constraint_clause_subsumes(
    const CxxConstraintClause* disjunctive,
    const CxxConstraintClause* conjunctive) {
    for (size_t available = 0;
         available < disjunctive->atom_count; ++available) {
        for (size_t required = 0; required < conjunctive->atom_count;
             ++required) {
            if (cxx_constraint_atoms_match(
                    &disjunctive->atoms[available],
                    &conjunctive->atoms[required])) {
                return true;
            }
        }
    }
    return false;
}

static bool cxx_constraint_form_subsumes(
    const CxxConstraintForm* dnf, const CxxConstraintForm* cnf) {
    for (size_t disjunct = 0; disjunct < dnf->clause_count; ++disjunct) {
        for (size_t conjunct = 0; conjunct < cnf->clause_count; ++conjunct) {
            if (!cxx_constraint_clause_subsumes(
                    &dnf->clauses[disjunct], &cnf->clauses[conjunct])) {
                return false;
            }
        }
    }
    return true;
}

static bool cxx_constraint_function_parameters_match(
    const CxxTemplate* left, const CxxTemplate* right) {
    Type* left_function;
    Type* right_function;
    TypeParam* left_parameter;
    TypeParam* right_parameter;
    if (!left || !right || !left->func_def || !right->func_def) return false;
    left_function = left->func_def->type;
    right_function = right->func_def->type;
    if (!left_function || !right_function ||
        left_function->kind != TYPE_FUNC ||
        right_function->kind != TYPE_FUNC ||
        left_function->variadic != right_function->variadic ||
        left_function->has_prototype != right_function->has_prototype) {
        return false;
    }
    left_parameter = left_function->params;
    right_parameter = right_function->params;
    while (left_parameter && right_parameter) {
        if (!cxx_template_type_parameter_mapping_matches(
                left_parameter->type, left,
                right_parameter->type, right)) {
            return false;
        }
        left_parameter = left_parameter->next;
        right_parameter = right_parameter->next;
    }
    return !left_parameter && !right_parameter;
}

/* Positive means left is more constrained; negative means right is.  A false
 * `comparable` leaves candidates to the existing ordering rules when a
 * concept's dependent non-type argument cannot yet be mapped symbolically. */
static int cxx_function_template_constraint_relation(
    CxxTemplate* left, CxxTemplate* right, bool* comparable) {
    CxxTemplate* left_context;
    CxxTemplate* right_context;
    CxxConstraintNode* left_node;
    CxxConstraintNode* right_node;
    CxxConstraintNormalForms left_forms;
    CxxConstraintNormalForms right_forms;
    bool left_subsumes_right;
    bool right_subsumes_left;
    if (comparable) *comparable = true;
    if (!left || !right) {
        if (comparable) *comparable = false;
        return 0;
    }
    if (!cxx_constraint_function_parameters_match(left, right)) {
        if (comparable) *comparable = false;
        return 0;
    }
    if (!left->constraint || !right->constraint) {
        return left->constraint ? 1 : (right->constraint ? -1 : 0);
    }
    left_context = left->constraint_context ? left->constraint_context : left;
    right_context = right->constraint_context ? right->constraint_context : right;
    left_node = cxx_constraint_expand(left->constraint, left->constraint,
                                      left_context, NULL, 0u);
    right_node = cxx_constraint_expand(right->constraint, right->constraint,
                                       right_context, NULL, 0u);
    if (!left_node || !right_node ||
        !cxx_constraint_build_normal_forms(left_node, &left_forms) ||
        !cxx_constraint_build_normal_forms(right_node, &right_forms)) {
        if (comparable) *comparable = false;
        return 0;
    }
    left_subsumes_right = cxx_constraint_form_subsumes(
        &left_forms.dnf, &right_forms.cnf);
    right_subsumes_left = cxx_constraint_form_subsumes(
        &right_forms.dnf, &left_forms.cnf);
    if (left_subsumes_right == right_subsumes_left) return 0;
    return left_subsumes_right ? 1 : -1;
}

static int cxx_function_template_match_relation(
    const CxxFunctionTemplateMatch* left,
    const CxxFunctionTemplateMatch* right) {
    int conversion_relation;
    int constraint_relation;
    bool constraints_comparable;
    bool conversions_incomparable;
    if (!left || !right) return 0;
    conversion_relation = cxx_template_conversion_vector_relation(
        left, right, &conversions_incomparable);
    if (conversions_incomparable) return 0;
    if (conversion_relation != 0) return conversion_relation;
    if (left->specificity != right->specificity) {
        return left->specificity > right->specificity ? 1 : -1;
    }
    constraint_relation = cxx_function_template_constraint_relation(
        left->tmpl, right->tmpl, &constraints_comparable);
    return constraints_comparable ? constraint_relation : 0;
}

static int cxx_parser_type_pack_index(CxxTemplate* tmpl, Type* pattern) {
    if (!tmpl || !pattern || pattern->kind != TYPE_STRUCT ||
        !pattern->tag) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind == TPARAM_TYPE && parameter->is_pack &&
            parameter->name && strcmp(parameter->name, pattern->tag) == 0) {
            return index;
        }
    }
    return -1;
}

static int cxx_parser_template_pack_parameter_index(CxxTemplate* tmpl) {
    if (!tmpl) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].is_pack) return index;
    }
    return -1;
}

static bool cxx_parser_expression_is_lvalue(Expr* expression) {
    if (!expression) return false;
    switch (expression->kind) {
        case EXPR_IDENT:
        case EXPR_DEREF:
        case EXPR_INDEX:
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return true;
        default:
            return false;
    }
}

/* `type_is_compatible()` intentionally ignores cv-qualification on pointer
 * targets.  For overload ranking, however, identity and qualification
 * conversions are distinct exact-match sequences, and qualification may not
 * be added through an unprotected pointer level (`T**` -> `const T**`). */
static bool cxx_parser_template_qualification_relation_internal(
    const Type* source, const Type* target, bool protected_level,
    bool nested_level, bool ignore_current_qualification,
    bool* requires_qualification) {
    bool added_const;
    bool added_volatile;
    if (!source || !target) return false;
    added_const = target->is_const && !source->is_const;
    added_volatile = target->is_volatile && !source->is_volatile;
    if (!ignore_current_qualification &&
        ((source->is_const && !target->is_const) ||
         (source->is_volatile && !target->is_volatile) ||
         (nested_level && (added_const || added_volatile) &&
          !protected_level))) {
        return false;
    }
    if (!ignore_current_qualification &&
        (added_const || added_volatile) && requires_qualification) {
        *requires_qualification = true;
    }
    if (source->kind == TYPE_PTR || target->kind == TYPE_PTR) {
        if (source->kind != TYPE_PTR || target->kind != TYPE_PTR) {
            return false;
        }
        /* Top-level cv on a by-value pointer is ignored.  Its pointee is the
         * first qualification level; deeper additions need a const/volatile
         * intermediate pointer to protect them. */
        return cxx_parser_template_qualification_relation_internal(
            source->base, target->base,
            ignore_current_qualification
                ? false : (target->is_const || target->is_volatile),
            !ignore_current_qualification, false,
            requires_qualification);
    }
    return true;
}

static bool cxx_parser_template_qualification_relation(
    const Type* source, const Type* target, bool ignore_top_level,
    bool* requires_qualification) {
    if (requires_qualification) *requires_qualification = false;
    return cxx_parser_template_qualification_relation_internal(
        source, target, false, false, ignore_top_level,
        requires_qualification);
}

static int cxx_parser_template_conversion_rank(Expr* argument,
                                                Type* target) {
    Type* source;
    Type* source_base;
    Type* target_base;
    bool target_is_reference;
    if (!argument || !target) return -1;
    source = cxx_parser_expression_type(argument);
    if (!source) return -1;
    if (source->is_reference) source = source->base;
    if (!source) return -1;

    target_is_reference = target->is_reference;
    if (target->is_reference) {
        bool is_lvalue = cxx_parser_expression_is_lvalue(argument);
        bool binds_const_lvalue = target->base && target->base->is_const;
        if ((!target->is_rvalue_reference && !is_lvalue &&
             !binds_const_lvalue) ||
            (target->is_rvalue_reference && is_lvalue) || !target->base) {
            return -1;
        }
        target = target->base;
    }
    if (type_is_compatible(source, target)) {
        return cxx_parser_template_qualification_relation(
                   source, target, !target_is_reference, NULL)
            ? 0 : -1;
    }
    if (source->kind == TYPE_NULLPTR && target->kind == TYPE_BOOL) {
        return 1;
    }
    if (source->kind == TYPE_PTR && target->kind == TYPE_BOOL) {
        return 2;
    }
    if (source->kind == TYPE_FLOAT && target->kind == TYPE_DOUBLE) {
        return 1;
    }
    if (type_is_integer(source) && target->kind == TYPE_INT &&
        source->kind < TYPE_INT) {
        return 1;
    }
    if (type_is_arithmetic(source) && type_is_arithmetic(target)) return 2;
    if (source->kind == TYPE_ARRAY && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (source_base && target_base &&
            (type_is_compatible(source_base, target_base) ||
             target_base->kind == TYPE_VOID)) {
            return 1;
        }
    }
    if (source->kind == TYPE_FUNC && target->kind == TYPE_PTR &&
        target->base && target->base->kind == TYPE_FUNC) {
        return type_is_compatible(source, target->base) ? 1 : -1;
    }
    if (source->kind == TYPE_PTR && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (source_base && target_base) {
            if (type_is_compatible(source_base, target_base)) return 1;
            if (source_base->kind == TYPE_VOID ||
                target_base->kind == TYPE_VOID) return 2;
        }
    }
    if (source->kind == TYPE_NULLPTR && target->kind == TYPE_PTR) return 1;
    if (source->kind == TYPE_INT && target->kind == TYPE_PTR &&
        argument->kind == EXPR_INT_LIT && argument->int_val == 0) {
        return 2;
    }
    return -1;
}

static int cxx_parser_function_template_overload_conversion_rank(
    Expr* argument, Type* target) {
    Type* source;
    Type* conversion_target;
    bool target_is_reference;
    bool requires_qualification = false;
    int rank = cxx_parser_template_conversion_rank(argument, target);
    if (rank < 0) {
        source = cxx_parser_expression_type(argument);
        int semantic_rank = rcc_sema_cxx_conversion_rank(
            argument, source, target);
        if (semantic_rank == 2) {
            /* The parser-local template ranker does not model every
             * class-pointer standard conversion.  Semantic analysis uses 2
             * for Conversion-rank sequences such as derived-to-base and
             * pointer-to-void; keep them viable and comparable here. */
            return 8;
        }
        if (semantic_rank >= 3) {
            /* Standard conversions rank ahead of user-defined conversions;
             * retain the standard conversion applied to the conversion
             * function's result when comparing two user-defined sequences. */
            return 12 + semantic_rank - 3;
        }
        return rank;
    }
    if (rank == 1 && target && !target->is_reference &&
        target->kind == TYPE_PTR) {
        source = cxx_parser_expression_type(argument);
        if (source && source->is_reference) source = source->base;
        if (source && source->kind == TYPE_ARRAY && source->base &&
            target->base) {
            if (type_is_compatible(source->base, target->base)) {
                if (!cxx_parser_template_qualification_relation(
                        source->base, target->base, false,
                        &requires_qualification)) {
                    return -1;
                }
                /* Array-to-pointer decay is an exact-match transformation;
                 * adding cv to its pointed-to element is a qualification
                 * conversion and must remain distinguishable from identity. */
                return requires_qualification ? 1 : 0;
            }
            if (target->base->kind == TYPE_VOID) {
                /* Decay followed by pointer-to-void conversion has Conversion
                 * rank, not Promotion rank. */
                return 8;
            }
        }
    }
    if (rank == 0) {
        source = cxx_parser_expression_type(argument);
        if (!source || !target) return -1;
        if (source->is_reference) source = source->base;
        target_is_reference = target->is_reference;
        conversion_target = target_is_reference ? target->base : target;
        if (!source || !conversion_target ||
            !cxx_parser_template_qualification_relation(
                source, conversion_target, !target_is_reference,
                &requires_qualification)) {
            return -1;
        }
        return requires_qualification ? 1 : 0;
    }
    /* Preserve the existing coarse order while leaving space between exact
     * match, qualification, promotion, standard/user-defined conversions,
     * and ellipsis sequences. */
    return rank * 4;
}

static Type* cxx_function_template_find_dependent_nested_type(
    Type* type, unsigned depth, bool unresolved_only) {
    Type* unresolved;
    if (!type || depth > 64u) return NULL;
    if (type->cxx_dependent && type->cxx_dependent_member_name &&
        (!unresolved_only || type->cxx_template_param_index < 0)) {
        return type;
    }
    if (type->cxx_is_member_pointer && type->cxx_member_pointer_owner) {
        unresolved = cxx_function_template_find_dependent_nested_type(
            type->cxx_member_pointer_owner, depth + 1u, unresolved_only);
        if (unresolved) return unresolved;
    }
    switch (type->kind) {
        case TYPE_PTR:
        case TYPE_ARRAY:
        case TYPE_VECTOR:
            return cxx_function_template_find_dependent_nested_type(
                type->base, depth + 1u, unresolved_only);
        case TYPE_FUNC:
            unresolved = cxx_function_template_find_dependent_nested_type(
                type->ret_type, depth + 1u, unresolved_only);
            if (unresolved) return unresolved;
            for (TypeParam* parameter = type->params; parameter;
                 parameter = parameter->next) {
                unresolved = cxx_function_template_find_dependent_nested_type(
                    parameter->type, depth + 1u, unresolved_only);
                if (unresolved) return unresolved;
            }
            return NULL;
        default:
            for (int index = 0; index < type->cxx_template_arg_count; ++index) {
                unresolved = cxx_function_template_find_dependent_nested_type(
                    type->cxx_template_args[index], depth + 1u,
                    unresolved_only);
                if (unresolved) return unresolved;
            }
            return NULL;
    }
}

static void cxx_report_function_template_nested_type_failure(
    Type* unresolved, CxxTemplate* tmpl, SourceLoc loc) {
    const char* member_name;
    CxxClass* owner;
    CxxTypeAlias* alias;
    bool ambiguous = false;
    bool accessible = false;
    CxxClass* access_context = tmpl ? tmpl->active_class_instance : NULL;
    if (!unresolved || !unresolved->cxx_dependent_member_name) return;
    member_name = unresolved->cxx_dependent_member_name;
    owner = unresolved->cxx_class;
    if (!owner) {
        rcc_error(loc, "dependent nested type '%s' cannot be resolved",
                  member_name);
        return;
    }
    if (!access_context && tmpl && tmpl->func_def &&
        tmpl->func_def->func_method_owner) {
        access_context = tmpl->func_def->func_method_owner->cxx_class;
    }
    if (!access_context && tmpl) access_context = tmpl->templated_class;
    alias = cxx_class_find_inherited_type_alias(
        owner, member_name, access_context, &ambiguous, &accessible);
    if (alias && !accessible) {
        rcc_error(loc,
                  "dependent nested type '%s' is inaccessible in class '%s'",
                  member_name, owner->name ? owner->name : "<unnamed>");
    } else if (!alias && ambiguous) {
        rcc_error(loc, "nested type '%s' is ambiguous in class '%s'",
                  member_name, owner->name ? owner->name : "<unnamed>");
    } else if (!alias) {
        rcc_error(loc,
                  "class '%s' has no unique accessible nested type '%s'",
                  owner->name ? owner->name : "<unnamed>", member_name);
    } else {
        rcc_error(loc,
                  "dependent nested type '%s' could not be substituted in class '%s'",
                  member_name, owner->name ? owner->name : "<unnamed>");
    }
}

static bool cxx_function_template_instance_viable(
    Decl* instance, ExprList* call_arguments, int* ranks,
    Type** conversion_targets, int rank_capacity, int* rank_count) {
    TypeParam* parameter;
    DeclList* declaration;
    ExprList* argument;
    if (!instance || instance->kind != DECL_FUNC || !instance->type ||
        instance->type->kind != TYPE_FUNC) return false;
    parameter = instance->type->params;
    declaration = instance->func_params;
    argument = call_arguments;
    if (rank_count) *rank_count = 0;
    while (argument && parameter) {
        int rank = cxx_parser_function_template_overload_conversion_rank(
            argument->expr, parameter->type);
        if (rank < 0) return false;
        if (ranks && rank_count && *rank_count >= rank_capacity) return false;
        if (ranks && rank_count) {
            int index = (*rank_count)++;
            ranks[index] = rank;
            if (conversion_targets) {
                conversion_targets[index] = parameter->type;
            }
        }
        argument = argument->next;
        parameter = parameter->next;
        if (declaration) declaration = declaration->next;
    }
    if (argument) {
        if (!instance->type->variadic) return false;
        while (argument) {
            if (ranks && rank_count && *rank_count >= rank_capacity) {
                return false;
            }
            if (ranks && rank_count) {
                int index = (*rank_count)++;
                ranks[index] = 32;
                if (conversion_targets) conversion_targets[index] = NULL;
            }
            argument = argument->next;
        }
    }
    while (parameter && declaration) {
        if (!declaration->decl || !declaration->decl->param_default) {
            return false;
        }
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return parameter == NULL && declaration == NULL;
}

static bool cxx_template_class_derives_from(const CxxClass* derived,
                                           const CxxClass* base,
                                           unsigned depth) {
    if (!derived || !base || depth > 64u) return false;
    if (derived == base) return true;
    for (int index = 0; index < derived->base_count; ++index) {
        CxxClass* direct_base = derived->bases[index].base;
        if (direct_base &&
            (direct_base == base ||
             cxx_template_class_derives_from(direct_base, base,
                                             depth + 1u))) {
            return true;
        }
    }
    return false;
}

static int cxx_template_conversion_vector_relation(
    const CxxFunctionTemplateMatch* left,
    const CxxFunctionTemplateMatch* right, bool* incomparable) {
    bool left_better = false;
    bool right_better = false;
    if (incomparable) *incomparable = false;
    if (!left || !right || left->conversion_rank_count !=
        right->conversion_rank_count) return 0;
    for (int index = 0; index < left->conversion_rank_count; ++index) {
        if (left->conversion_ranks[index] < right->conversion_ranks[index]) {
            left_better = true;
        } else if (left->conversion_ranks[index] >
                   right->conversion_ranks[index]) {
            right_better = true;
        } else if (left->conversion_ranks[index] < 12) {
            Type* left_target = left->conversion_targets[index];
            Type* right_target = right->conversion_targets[index];
            int qualification_relation = 0;
            if (left_target && right_target &&
                left_target->is_reference && right_target->is_reference &&
                left_target->is_rvalue_reference !=
                    right_target->is_rvalue_reference) {
                /* A viable rvalue-reference binding can only reach this
                 * comparison for a non-lvalue argument, so it is better than
                 * binding that same rvalue to an lvalue reference. */
                qualification_relation = left_target->is_rvalue_reference
                    ? 1 : -1;
            }
            if (qualification_relation == 0 &&
                (left->conversion_ranks[index] == 1 ||
                 left->conversion_ranks[index] == 8) &&
                left_target && right_target &&
                !left_target->is_reference &&
                !right_target->is_reference &&
                left_target->kind == TYPE_PTR &&
                right_target->kind == TYPE_PTR &&
                !left_target->cxx_is_member_pointer &&
                !right_target->cxx_is_member_pointer) {
                Type* left_pointee = left_target->base;
                Type* right_pointee = right_target->base;
                CxxClass* left_class = left_pointee
                    ? left_pointee->cxx_class : NULL;
                CxxClass* right_class = right_pointee
                    ? right_pointee->cxx_class : NULL;
                if (left_pointee && right_pointee &&
                    left_pointee->kind == TYPE_VOID && right_class) {
                    qualification_relation = -1;
                } else if (left_pointee && right_pointee && left_class &&
                           right_pointee->kind == TYPE_VOID) {
                    qualification_relation = 1;
                } else if (left_class && right_class &&
                           left_class != right_class) {
                    if (cxx_template_class_derives_from(
                            left_class, right_class, 0u)) {
                        qualification_relation = 1;
                    } else if (cxx_template_class_derives_from(
                                   right_class, left_class, 0u)) {
                        qualification_relation = -1;
                    }
                }
                bool left_subset = true;
                bool right_subset = true;
                if (qualification_relation == 0 &&
                    cxx_parser_template_qualification_targets_related(
                        left_target, right_target, true, &left_subset,
                        &right_subset, 0u)) {
                    /* A pointer-to-void conversion can include a trailing
                     * pointee qualification addition. Preserve the subset
                     * ordering even though the overall sequence has
                     * Conversion rank rather than Qualification rank. */
                    if (left_subset && !right_subset) {
                        qualification_relation = 1;
                    } else if (right_subset && !left_subset) {
                        qualification_relation = -1;
                    }
                }
            }
            if (qualification_relation > 0) left_better = true;
            if (qualification_relation < 0) right_better = true;
        }
    }
    if (left_better && !right_better) return 1;
    if (right_better && !left_better) return -1;
    if (left_better && right_better && incomparable) *incomparable = true;
    return 0;
}

static bool prepare_cxx_function_template_match(
    CxxTemplate* tmpl, ExprList* call_arguments,
    const CxxParsedTemplateArgument* explicit_arguments,
    int explicit_argument_count, CxxFunctionTemplateMatch* match,
    bool* constraint_invalid, bool* constraint_unsupported) {
    int specificity = 0;
    bool has_type_pack = false;
    bool has_value_pack = false;
    if (!tmpl || !match || tmpl->kind != TMPL_FUNCTION || !tmpl->func_def ||
        tmpl->param_count < 0 || tmpl->param_count > 32) return false;
    memset(match, 0, sizeof(*match));
    match->tmpl = tmpl;
    match->argument_count = tmpl->param_count;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].is_pack) {
            if (tmpl->params[index].kind == TPARAM_TYPE) has_type_pack = true;
            else has_value_pack = true;
        }
    }

    if (explicit_arguments) {
        int pack_parameter_index =
            cxx_parser_template_pack_parameter_index(tmpl);
        if (pack_parameter_index < 0 &&
            explicit_argument_count > tmpl->param_count) return false;
        for (int index = 0; index < explicit_argument_count; ++index) {
            int parameter_index = pack_parameter_index >= 0 &&
                                  index >= pack_parameter_index
                ? pack_parameter_index : index;
            TemplateParam* parameter;
            if (parameter_index >= tmpl->param_count) return false;
            parameter = &tmpl->params[parameter_index];
            if ((parameter->kind == TPARAM_TYPE) !=
                explicit_arguments[index].is_type) {
                return false;
            }
            if (parameter->is_pack) {
                if (match->pack_count >= 32) return false;
                if (parameter->kind == TPARAM_TYPE) {
                    if (!explicit_arguments[index].is_type ||
                        !explicit_arguments[index].type) return false;
                    match->pack_arguments[match->pack_count] =
                        explicit_arguments[index].type;
                } else {
                    if (explicit_arguments[index].is_type ||
                        !explicit_arguments[index].value_valid) return false;
                    match->pack_values[match->pack_count] =
                        explicit_arguments[index].value;
                    match->pack_value_present[match->pack_count] = true;
                }
                ++match->pack_count;
            } else if (parameter->kind == TPARAM_TYPE) {
                match->arguments[parameter_index] =
                    explicit_arguments[index].type;
                if (!match->arguments[parameter_index]) return false;
            } else {
                if (!explicit_arguments[index].value_valid) return false;
                match->arguments[parameter_index] = parameter->type;
                match->values[parameter_index] = explicit_arguments[index].value;
                match->value_present[parameter_index] = true;
            }
        }
        int fixed_explicit_count = pack_parameter_index >= 0
            ? pack_parameter_index : explicit_argument_count;
        for (int index = fixed_explicit_count;
             index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack) continue;
            if (!parameter->has_default) return false;
            if (parameter->kind == TPARAM_TYPE) {
                if (!parameter->default_type) return false;
                match->arguments[index] = substitute_template_type(
                    tmpl, parameter->default_type, match->arguments,
                    tmpl->param_count, match->values, match->value_present);
            } else if (parameter->kind == TPARAM_NONTYPE) {
                if (parameter->default_context) {
                    parameter->default_context->pending_pack_count =
                        tmpl->pending_pack_count;
                }
                if (!parameter->default_value ||
                    !eval_template_integer_expression(
                        parameter->default_value,
                        parameter->default_context
                            ? parameter->default_context : tmpl,
                        match->values,
                        match->value_present, &match->values[index])) {
                    return false;
                }
                match->arguments[index] = parameter->type;
                match->value_present[index] = true;
            } else if (parameter->kind == TPARAM_TEMPLATE &&
                       parameter->default_type &&
                       parameter->default_type->cxx_template) {
                match->arguments[index] = parameter->default_type;
            } else {
                return false;
            }
        }
        for (DeclList* parameter = tmpl->func_def->func_params;
             parameter; parameter = parameter->next) {
            /* Explicit template arguments still have to make each dependent
             * parameter pattern viable.  This also supplies the partial-
             * ordering score for `T` versus `T*`. */
            ExprList* argument = call_arguments;
            int index = parameter->decl ? parameter->decl->param_index : 0;
            while (argument && index-- > 0) argument = argument->next;
            if (parameter->decl && parameter->decl->param_is_pack) {
                continue;
            }
            if (argument) {
                Type* actual = cxx_parser_expression_type(argument->expr);
                bool deduced = actual && deduce_function_template_type(
                    tmpl, parameter->decl->type,
                    cxx_parser_template_deduction_argument(
                        parameter->decl->type, actual),
                    match->arguments, match->values,
                    match->value_present, &specificity);
                if (!actual ||
                    (!deduced &&
                     !cxx_function_template_find_dependent_nested_type(
                         parameter->decl->type, 0u, false) &&
                     (cxx_function_template_type_contains_parameter(
                          tmpl, parameter->decl->type) ||
                      cxx_parser_function_template_overload_conversion_rank(
                          argument->expr, parameter->decl->type) < 0))) {
                    return false;
                }
            }
        }
        for (int index = 0; index < tmpl->param_count; ++index) {
            if (tmpl->params[index].is_pack) {
                if (tmpl->params[index].kind == TPARAM_TYPE) {
                    match->arguments[index] = match->pack_count > 0
                        ? match->pack_arguments[0] : type_void;
                } else {
                    match->arguments[index] = tmpl->params[index].type;
                }
            }
        }
    } else {
        if (!deduce_function_template_arguments(
                tmpl, call_arguments, match->arguments, match->values,
                match->value_present, &specificity, false,
                match->pack_arguments, &match->pack_count)) {
            return false;
        }
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack) {
                if (match->pack_count > 0) {
                    match->arguments[index] = match->pack_arguments[0];
                } else {
                    /* The argument slot is only a cache key placeholder;
                     * the expanded parameter list carries the actual ABI. */
                    match->arguments[index] = type_void;
                }
            } else if (parameter->kind == TPARAM_NONTYPE) {
                if (!match->value_present[index] && parameter->has_default &&
                    parameter->default_context) {
                    parameter->default_context->pending_pack_count =
                        tmpl->pending_pack_count;
                }
                if (!match->value_present[index] && parameter->has_default &&
                    parameter->default_value &&
                    eval_template_integer_expression(
                        parameter->default_value,
                        parameter->default_context
                            ? parameter->default_context : tmpl,
                        match->values, match->value_present,
                        &match->values[index])) {
                    match->value_present[index] = true;
                }
                if (!match->value_present[index]) return false;
                match->arguments[index] = parameter->type;
            } else if (!match->arguments[index] && parameter->has_default &&
                       parameter->default_type) {
                match->arguments[index] = substitute_template_type(
                    tmpl, parameter->default_type, match->arguments,
                    tmpl->param_count, match->values, match->value_present);
            }
            if (!match->arguments[index]) {
                return false;
            }
        }
    }

    {
        Type** saved_pending_pack_args = tmpl->pending_pack_args;
        int64_t* saved_pending_pack_values = tmpl->pending_pack_values;
        bool* saved_pending_pack_value_present =
            tmpl->pending_pack_value_present;
        int saved_pending_pack_count = tmpl->pending_pack_count;
        bool constraint_satisfied;
        tmpl->pending_pack_args = has_type_pack
            ? match->pack_arguments : NULL;
        tmpl->pending_pack_values = has_value_pack
            ? match->pack_values : NULL;
        tmpl->pending_pack_value_present = has_value_pack
            ? match->pack_value_present : NULL;
        tmpl->pending_pack_count = has_type_pack || has_value_pack
            ? match->pack_count : -1;
        constraint_satisfied = cxx_template_constraint_satisfied(
            tmpl, match->arguments, match->values, match->value_present,
            tmpl->func_def->loc, false, constraint_unsupported);
        tmpl->pending_pack_args = saved_pending_pack_args;
        tmpl->pending_pack_values = saved_pending_pack_values;
        tmpl->pending_pack_value_present =
            saved_pending_pack_value_present;
        tmpl->pending_pack_count = saved_pending_pack_count;
        if (!constraint_satisfied) {
            if (constraint_invalid) *constraint_invalid = true;
            return false;
        }
    }
    match->specificity = specificity;
    tmpl->pending_pack_args = has_type_pack ? match->pack_arguments : NULL;
    tmpl->pending_pack_values = has_value_pack ? match->pack_values : NULL;
    tmpl->pending_pack_value_present = has_value_pack
        ? match->pack_value_present : NULL;
    tmpl->pending_pack_count = has_type_pack ? match->pack_count : -1;
    if (has_value_pack) tmpl->pending_pack_count = match->pack_count;
    match->instance = (Decl*)cxx_template_instantiate_with_values(
        tmpl, match->arguments, match->values, match->value_present,
        match->argument_count);
    tmpl->pending_pack_args = NULL;
    tmpl->pending_pack_values = NULL;
    tmpl->pending_pack_value_present = NULL;
    tmpl->pending_pack_count = -1;
    if (match->instance && match->instance->type &&
        match->instance->type->kind == TYPE_FUNC) {
        match->substitution_failure_type =
            cxx_function_template_find_dependent_nested_type(
                match->instance->type->ret_type, 0u, true);
        for (TypeParam* parameter = match->instance->type->params;
             !match->substitution_failure_type && parameter;
             parameter = parameter->next) {
            match->substitution_failure_type =
                cxx_function_template_find_dependent_nested_type(
                    parameter->type, 0u, true);
        }
        if (match->substitution_failure_type) return false;
    }
    if (!cxx_function_template_instance_viable(
            match->instance, call_arguments, match->conversion_ranks,
            match->conversion_targets,
            (int)(sizeof(match->conversion_ranks) /
                  sizeof(match->conversion_ranks[0])),
            &match->conversion_rank_count)) {
        return false;
    }
    return true;
}

Type* rcc_parse_cxx_direct_list_type(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name;
    CxxTemplate* tmpl;
    Type* type;

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    name = parse_qualified_name();
    tmpl = check(TOK_LT) ? find_class_template(name) : NULL;
    if (!tmpl) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    type = parse_class_template_specialization(tmpl, loc);
    if (!type || !type->cxx_class || !type_is_complete(type) ||
        !check(TOK_LBRACE)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    return type;
}

Expr* rcc_parse_cxx_functional_cast(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name = NULL;
    CxxTemplate* tmpl;
    CxxClass* cls = NULL;
    Type* type;
    ExprList* arguments = NULL;
    Expr* initializer;
    bool keyword_type = false;
    bool brace_form = false;
    bool aggregate_cast = false;

    switch (peek()->type) {
        case TOK_VOID:
        case TOK_BOOL:
        case TOK_CHAR8_T:
        case TOK_CHAR:
        case TOK_SHORT:
        case TOK_INT:
        case TOK_LONG:
        case TOK_SIGNED:
        case TOK_UNSIGNED:
        case TOK_FLOAT:
        case TOK_DOUBLE:
        case TOK___BUILTIN_VA_LIST:
        case TOK_CONST:
        case TOK_VOLATILE:
        case TOK_DECLTYPE:
            keyword_type = true;
            break;
        default:
            break;
    }

    if (keyword_type) {
        type = parse_cxx_type_spec();
    } else {
        if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
        name = parse_qualified_name();
        tmpl = check(TOK_LT) ? find_class_template(name) : NULL;
        if (tmpl) {
            type = parse_class_template_specialization(tmpl, loc);
        } else {
            cls = find_class(name);
            type = cls ? cls->type : NULL;
        }
        if (!type) {
            /* Classes registered through the common aggregate path are also
             * visible in the parser type table.  The cxx_class guard keeps a
             * C aggregate or typedef from becoming a constructor expression. */
            type = rcc_parser_lookup_type(name);
            if (!type || ((type->kind == TYPE_STRUCT ||
                           type->kind == TYPE_UNION) && !type->cxx_class)) {
                type = NULL;
            }
        }
    }
    aggregate_cast = type && type->cxx_class &&
        rcc_parser_cxx_constructor_arity_mask(type) == 0u &&
        (check(TOK_LBRACE) ||
         (check(TOK_LPAREN) && rcc_parser_cxx_standard_at_least(20)));
    if (!type || (!check(TOK_LPAREN) && !check(TOK_LBRACE)) ||
        (type->cxx_class &&
         rcc_parser_cxx_constructor_arity_mask(type) == 0u &&
         !aggregate_cast)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }

    brace_form = match(TOK_LBRACE);
    if (!brace_form) advance(); /* `(` */
    if ((!brace_form && !check(TOK_RPAREN)) ||
        (brace_form && !check(TOK_RBRACE))) {
        do {
            exprlist_append(&arguments, parse_assignment_expression());
        } while (match(TOK_COMMA));
    }
    expect(brace_form ? TOK_RBRACE : TOK_RPAREN,
           brace_form ? "}" : ")");

    if (!type->cxx_class &&
        !(type->cxx_dependent && type->cxx_template &&
          (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION))) {
        if (!arguments) {
            arguments = exprlist_new(expr_int(0, loc));
        } else if (arguments->next) {
            rcc_error(loc,
                      "C++ functional scalar cast requires one argument");
        }
        initializer = expr_cast(type, arguments->expr, loc);
        return initializer;
    }

    initializer = expr_initializer_list(arguments, loc);
    initializer->compound_type = type;
    initializer->compound_value_init = arguments == NULL;
    if (type->cxx_class) {
        rcc_parser_validate_cxx_constructor_initializer(type, initializer);
    }
    return initializer;
}

static TypeField* versioned_public_integer_field(Type* type,
                                                const char* name) {
    TypeField* field;
    if (!type || !name) return NULL;
    for (field = type->fields; field; field = field->next) {
        if (field->name && strcmp(field->name, name) == 0) {
            if (field->cxx_access != 0 || !field->type ||
                (!type_is_integer(field->type) &&
                 field->type->kind != TYPE_ENUM)) {
                return NULL;
            }
            return field;
        }
    }
    return NULL;
}

/* A function template that constructs a dependent aggregate and then writes
 * its members needs a dedicated typed lowering.  Do not instantiate such a
 * body through the scalar function-template path: that would emit a partial
 * object while silently dropping the remaining member writes. */
static bool template_has_unsupported_versioned_shape(CxxTemplate* tmpl) {
    Decl* function;
    StmtList* statements;
    const char* variable;
    bool has_member_assignment = false;
    if (!tmpl || tmpl->kind != TMPL_FUNCTION || tmpl->param_count != 1 ||
        tmpl->params[0].kind != TPARAM_TYPE) {
        return false;
    }
    function = tmpl->func_def;
    if (!function || function->func_params || !function->type ||
        !template_type_parameter_matches(tmpl, function->type->ret_type, 0) ||
        !function->func_body || function->func_body->kind != STMT_BLOCK) {
        return false;
    }
    statements = function->func_body->block_stmts;
    if (!statements || !statements->stmt ||
        statements->stmt->kind != STMT_DECL || !statements->stmt->decl ||
        !statements->stmt->decl->name ||
        !template_type_parameter_matches(tmpl,
                                          statements->stmt->decl->type, 0) ||
        !statements->stmt->decl->var_init ||
        statements->stmt->decl->var_init->kind != EXPR_COMPOUND ||
        !statements->stmt->decl->var_init->compound_value_init) {
        return false;
    }
    variable = statements->stmt->decl->name;
    for (statements = statements->next; statements;
         statements = statements->next) {
        Stmt* statement = statements->stmt;
        if (statement && statement->kind == STMT_EXPR && statement->expr &&
            statement->expr->kind == EXPR_ASSIGN &&
            statement->expr->binary_lhs &&
            statement->expr->binary_lhs->kind == EXPR_MEMBER &&
            expression_is_identifier(
                statement->expr->binary_lhs->member_base, variable) &&
            statement->expr->binary_lhs->member_name &&
            strcmp(statement->expr->binary_lhs->member_name, "struct_size") != 0 &&
            strcmp(statement->expr->binary_lhs->member_name, "version") != 0) {
            has_member_assignment = true;
            break;
        }
    }
    return has_member_assignment;
}

Expr* rcc_parse_cxx_template_call(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name;
    CxxTemplate* tmpl;
    CxxTemplate* candidate_templates[32];
    int candidate_count;
    bool qualified_call;
    Type* argument;
    TypeField* size_field;
    TypeField* version_field;
    ExprList* items = NULL;
    Expr* initializer;
    bool unsafe_versioned_shape;

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    name = parse_qualified_name();
    qualified_call = strstr(name, "::") != NULL;
    candidate_count = find_function_template_candidates(
        name, candidate_templates,
        (int)(sizeof(candidate_templates) / sizeof(candidate_templates[0])));
    if (candidate_count <= 0) {
        CxxTemplate* variable_template = find_variable_template(name);
        if (variable_template && check(TOK_LT)) {
            Type* arguments[32] = { NULL };
            int64_t values[32] = { 0 };
            bool value_present[32] = { false };
            int argument_count = 0;
            Decl* instance;

            if (variable_template->param_count >
                    (int)(sizeof(arguments) / sizeof(arguments[0]))) {
                rcc_error(loc,
                          "variable template argument limit exceeded");
                parser.cur = saved_cur;
                parser.prev = saved_prev;
                return NULL;
            }
            for (int index = 0; index < variable_template->param_count;
                 ++index) {
                if (variable_template->params[index].is_pack) {
                    rcc_error(loc,
                              "RCC++ variable template parameter packs are not supported");
                    parser.cur = saved_cur;
                    parser.prev = saved_prev;
                    return NULL;
                }
            }
            expect(TOK_LT, "<");
            if (!check(TOK_GT)) {
                do {
                    TemplateParam* parameter = argument_count <
                        variable_template->param_count
                        ? &variable_template->params[argument_count] : NULL;
                    if (!parameter) {
                        rcc_error(peek()->loc,
                                  "too many variable template arguments");
                        while (!check(TOK_GT) && !at_end()) advance();
                        break;
                    }
                    if (parameter->kind == TPARAM_TYPE) {
                        arguments[argument_count] = parse_cxx_type_spec();
                        if (!arguments[argument_count]) {
                            rcc_error(peek()->loc,
                                      "variable template type argument is invalid");
                        }
                    } else if (parameter->kind == TPARAM_NONTYPE) {
                        Expr* value_expression;
                        rcc_parser_set_cxx_template_default_mode(true);
                        value_expression = parse_assignment_expression();
                        rcc_parser_set_cxx_template_default_mode(false);
                        if (!expr_eval_integer_constant(value_expression,
                                                        &values[argument_count])) {
                            rcc_error(value_expression ? value_expression->loc : loc,
                                      "variable template argument must be an integer constant expression");
                        } else {
                            value_present[argument_count] = true;
                        }
                    } else {
                        rcc_error(peek()->loc,
                                  "RCC++ variable templates do not support template-template parameters");
                        while (!check(TOK_COMMA) && !check(TOK_GT) &&
                               !at_end()) advance();
                    }
                    ++argument_count;
                } while (match(TOK_COMMA));
            }
            expect(TOK_GT, ">");
            if (argument_count != variable_template->param_count) {
                rcc_error(loc,
                          "variable template specialization requires all template arguments");
                return expr_int(0, loc);
            }
            instance = (Decl*)cxx_template_instantiate_with_values(
                variable_template, arguments, values, value_present,
                argument_count);
            if (!instance || instance->kind != DECL_VAR) {
                rcc_error(loc, "could not instantiate variable template '%s'",
                          name);
                return expr_int(0, loc);
            }
            if (active_ast) {
                bool present = false;
                for (DeclList* item = active_ast->decls; item;
                     item = item->next) {
                    if (item->decl == instance) {
                        present = true;
                        break;
                    }
                }
                if (!present) ast_add_decl(active_ast, instance);
            }
            {
                Expr* expression = expr_ident(instance->name, loc);
                expression->ident_decl = instance;
                expression->type = instance->type;
                return expression;
            }
        }
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    if (candidate_count > (int)(sizeof(candidate_templates) /
                                sizeof(candidate_templates[0]))) {
        rcc_error(loc, "too many function template overloads for '%s'", name);
        candidate_count = (int)(sizeof(candidate_templates) /
                                sizeof(candidate_templates[0]));
    }
    tmpl = candidate_templates[0];
    unsafe_versioned_shape = template_has_unsupported_versioned_shape(tmpl);
    if (candidate_count != 1 ||
        tmpl->function_lowering != TMPL_FUNCTION_VERSIONED_STRUCT ||
        tmpl->is_hidden_friend) {
        CxxParsedTemplateArgument explicit_arguments[32];
        int explicit_argument_count = 0;
        ExprList* call_arguments = NULL;
        Expr* function;
        bool explicit_template_arguments = check(TOK_LT);
        int pack_parameter_index =
            cxx_parser_template_pack_parameter_index(tmpl);

        if (explicit_template_arguments) {
            expect(TOK_LT, "<");
            if (!check(TOK_GT)) {
                do {
                    int parameter_index = pack_parameter_index >= 0 &&
                                          explicit_argument_count >=
                                              pack_parameter_index
                        ? pack_parameter_index : explicit_argument_count;
                    TemplateParam* parameter = parameter_index <
                        tmpl->param_count ? &tmpl->params[parameter_index] : NULL;
                    CxxParsedTemplateArgument* parsed;
                    if (explicit_argument_count >=
                        (int)(sizeof(explicit_arguments) /
                              sizeof(explicit_arguments[0]))) {
                        rcc_error(loc, "function template argument limit exceeded");
                        while (!check(TOK_GT) && !at_end()) advance();
                        break;
                    }
                    parsed = &explicit_arguments[explicit_argument_count++];
                    memset(parsed, 0, sizeof(*parsed));
                    if (parameter && parameter->kind == TPARAM_TYPE) {
                        parsed->is_type = true;
                        parsed->type = parse_cxx_type_spec();
                    } else if (parameter && parameter->kind == TPARAM_NONTYPE) {
                        Expr* value_expression;
                        int64_t value;
                        rcc_parser_set_cxx_template_default_mode(true);
                        value_expression = parse_assignment_expression();
                        rcc_parser_set_cxx_template_default_mode(false);
                        if (!expr_eval_integer_constant(value_expression, &value)) {
                            SourceLoc error_location;
                            cxx_parser_expr_loc(&error_location,
                                                value_expression, &loc);
                            rcc_error(error_location,
                                      "function template non-type argument must be "
                                      "an integer constant expression");
                        } else {
                            parsed->value = value;
                            parsed->value_valid = true;
                        }
                    } else {
                        rcc_error(peek()->loc,
                                  "too many function template arguments");
                        while (!check(TOK_COMMA) && !check(TOK_GT) &&
                               !at_end()) {
                            advance();
                        }
                    }
                } while (match(TOK_COMMA));
            }
        } else if (!check(TOK_LPAREN)) {
            parser.cur = saved_cur;
            parser.prev = saved_prev;
            return NULL;
        }
        if (explicit_template_arguments) expect(TOK_GT, ">");
        if (!match(TOK_LPAREN)) {
            rcc_error(loc, "function template specialization must be called");
            return expr_int(0, loc);
        }
        if (!check(TOK_RPAREN)) {
            do {
                exprlist_append(&call_arguments, parse_assignment_expression());
            } while (match(TOK_COMMA));
        }
        expect(TOK_RPAREN, ")");

        if (unsafe_versioned_shape && candidate_count == 1) {
            rcc_error(loc, "function template '%s' is not safely lowerable",
                      name);
            return expr_int(0, loc);
        }

        CxxFunctionTemplateMatch matches[32];
        int match_count = 0;
        bool constraint_invalid = false;
        bool constraint_unsupported = false;
        Type* substitution_failure_type = NULL;
        CxxTemplate* substitution_failure_template = NULL;
        for (int index = 0; index < candidate_count; ++index) {
            CxxFunctionTemplateMatch candidate_match = { 0 };
            if (qualified_call &&
                candidate_templates[index]->is_hidden_friend) {
                continue;
            }
            if (!cxx_hidden_friend_template_matches_adl(
                    candidate_templates[index], call_arguments)) {
                continue;
            }
            if (template_has_unsupported_versioned_shape(
                    candidate_templates[index])) {
                continue;
            }
            if (prepare_cxx_function_template_match(
                    candidate_templates[index], call_arguments,
                    explicit_template_arguments ? explicit_arguments : NULL,
                    explicit_argument_count, &candidate_match,
                    &constraint_invalid, &constraint_unsupported)) {
                matches[match_count++] = candidate_match;
            } else if (!substitution_failure_type &&
                       candidate_match.substitution_failure_type) {
                substitution_failure_type =
                    candidate_match.substitution_failure_type;
                substitution_failure_template =
                    candidate_templates[index];
            }
        }
        if (constraint_unsupported) {
            rcc_error(loc, "template constraint could not be evaluated");
            return expr_int(0, loc);
        }
        if (match_count == 0) {
            if (constraint_invalid) {
                rcc_error(loc, "template constraints are not satisfied");
            } else if (substitution_failure_type) {
                cxx_report_function_template_nested_type_failure(
                    substitution_failure_type,
                    substitution_failure_template, loc);
            } else if (unsafe_versioned_shape && candidate_count == 1) {
                rcc_error(loc, "function template '%s' is not safely lowerable",
                          name);
            } else if (!explicit_template_arguments) {
                /* Preserve the useful single-template diagnostics for the
                 * common failure cases, while overload sets receive one
                 * consolidated error after every candidate was examined. */
                Type* diagnostic_arguments[32] = { NULL };
                int64_t diagnostic_values[32] = { 0 };
                bool diagnostic_present[32] = { false };
                Type* diagnostic_pack[32] = { NULL };
                int diagnostic_pack_count = 0;
                (void)deduce_function_template_arguments(
                    tmpl, call_arguments, diagnostic_arguments,
                    diagnostic_values, diagnostic_present, NULL, true,
                    diagnostic_pack, &diagnostic_pack_count);
                rcc_error(loc, "no matching function template overload for '%s'",
                          name);
            } else {
                rcc_error(loc, "no matching function template overload for '%s'",
                          name);
            }
            return expr_int(0, loc);
        }
        int selected = -1;
        int best_count = 0;
        for (int candidate_index = 0; candidate_index < match_count;
             ++candidate_index) {
            bool dominated = false;
            for (int other_index = 0; other_index < match_count; ++other_index) {
                if (candidate_index != other_index &&
                    cxx_function_template_match_relation(
                        &matches[other_index], &matches[candidate_index]) > 0) {
                    dominated = true;
                    break;
                }
            }
            if (!dominated) {
                selected = candidate_index;
                ++best_count;
            }
        }
        if (best_count != 1) {
            rcc_error(loc, "ambiguous function template overload for '%s'",
                      name);
            return expr_int(0, loc);
        }
        Decl* instance = matches[selected].instance;
        if (!instance || instance->kind != DECL_FUNC) {
            rcc_error(loc, "could not instantiate function template '%s'", name);
            return expr_int(0, loc);
        }
        if (active_ast) {
            bool present = false;
            for (DeclList* item = active_ast->decls; item; item = item->next) {
                if (item->decl == instance) {
                    present = true;
                    break;
                }
            }
            if (!present) ast_add_decl(active_ast, instance);
        }
        function = expr_ident(instance->name, loc);
        function->ident_decl = instance;
        function->type = instance->type;
        return expr_call(function, call_arguments, loc);
    }

    expect(TOK_LT, "<");
    argument = parse_cxx_type_spec();
    if (match(TOK_COMMA)) {
        rcc_error(loc, "versioned structure template requires one type argument");
        while (!check(TOK_GT) && !at_end()) advance();
    }
    expect(TOK_GT, ">");
    if (!match(TOK_LPAREN)) {
        rcc_error(loc, "versioned structure specialization must be called");
        return expr_int(0, loc);
    }
    if (!check(TOK_RPAREN)) {
        rcc_error(loc, "versioned structure template takes no arguments");
        while (!check(TOK_RPAREN) && !at_end()) advance();
    }
    expect(TOK_RPAREN, ")");

    if (!argument || argument->kind != TYPE_STRUCT ||
        !type_is_complete(argument) || argument->size <= 0) {
        rcc_error(loc,
                  "versioned structure template requires a complete public "
                  "struct with integer struct_size and version fields");
        return expr_int(0, loc);
    }
    size_field = versioned_public_integer_field(argument, "struct_size");
    version_field = versioned_public_integer_field(argument, "version");
    if (!size_field || !version_field || size_field == version_field) {
        rcc_error(loc,
                  "versioned structure template requires a complete public "
                  "struct with integer struct_size and version fields");
        return expr_int(0, loc);
    }

    exprlist_append_designated(&items, expr_int(argument->size, loc),
                               INIT_DESIGNATOR_FIELD, 0, "struct_size");
    exprlist_append_designated(&items,
                               expr_int(tmpl->function_constant, loc),
                               INIT_DESIGNATOR_FIELD, 0, "version");
    initializer = expr_initializer_list(items, loc);
    initializer->compound_type = argument;
    initializer->type = argument;
    return initializer;
}

static Type* cxx_decltype_member_type(Type* object_type,
                                      const char* member_name,
                                      SourceLoc loc) {
    TypeField* field;
    if (!object_type || !member_name) return NULL;
    if (object_type->kind == TYPE_PTR && object_type->is_reference) {
        object_type = object_type->base;
    }
    if (object_type->kind == TYPE_PTR) object_type = object_type->base;
    if (!object_type || (object_type->kind != TYPE_STRUCT &&
                         object_type->kind != TYPE_UNION)) {
        rcc_error(loc, "decltype member expression requires an aggregate object");
        return NULL;
    }
    for (field = object_type->fields; field; field = field->next) {
        if (field->name && strcmp(field->name, member_name) == 0) {
            if (field->cxx_access != ACCESS_PUBLIC) {
                rcc_error(loc, "decltype cannot name a non-public member '%s'",
                          member_name);
                return NULL;
            }
            return field->type;
        }
    }
    rcc_error(loc, "unknown member '%s' in decltype expression", member_name);
    return NULL;
}

static void cxx_skip_decltype_expression(void) {
    int depth = 0;
    while (!at_end()) {
        if (check(TOK_LPAREN)) {
            ++depth;
            advance();
        } else if (check(TOK_RPAREN)) {
            if (depth == 0) {
                advance();
                return;
            }
            --depth;
            advance();
        } else {
            advance();
        }
    }
}

static void cxx_skip_decltype_call(void) {
    int depth = 0;
    if (!match(TOK_LPAREN)) return;
    while (!at_end()) {
        if (check(TOK_LPAREN)) {
            ++depth;
        } else if (check(TOK_RPAREN)) {
            if (depth == 0) {
                advance();
                return;
            }
            --depth;
        }
        advance();
    }
}

static Type* cxx_decltype_function_return(const char* name, SourceLoc loc) {
    Type* result = NULL;
    const char* suffix = name ? strrchr(name, ':') : NULL;
    suffix = suffix && suffix > name && suffix[-1] == ':' ? suffix + 1 : name;
    for (DeclList* item = active_ast ? active_ast->decls : NULL;
         item; item = item->next) {
        Decl* declaration = item->decl;
        bool matches;
        if (!declaration || declaration->kind != DECL_FUNC ||
            !declaration->type || declaration->type->kind != TYPE_FUNC) {
            continue;
        }
        matches = name && declaration->name &&
            (strcmp(declaration->name, name) == 0 ||
             strcmp(declaration->name, suffix) == 0);
        if (!matches) continue;
        if (result && !type_is_compatible(result,
                                          declaration->type->ret_type)) {
            rcc_error(loc,
                      "decltype call names overloaded functions with different return types");
            return NULL;
        }
        result = declaration->type->ret_type;
    }
    return result;
}

/* Parse the expression forms for which the parser already has an exact
 * source-level type.  `decltype` is intentionally not an integer fallback:
 * an unsupported dependent or side-effecting expression is diagnosed at its
 * grammar boundary rather than being assigned a guessed type. */
static Type* parse_cxx_decltype_type_legacy(SourceLoc loc) {
    Type* result = NULL;
    bool extra_parentheses = false;
    bool valid = true;
    bool dereference = false;
    bool address = false;
    bool expression_is_lvalue = false;
    bool needs_lvalue_reference = false;

    expect(TOK_DECLTYPE, "decltype");
    expect(TOK_LPAREN, "(");
    if (match(TOK_LPAREN)) extra_parentheses = true;
    if (match(TOK_STAR)) {
        dereference = true;
    } else if (match(TOK_AMP)) {
        address = true;
    }

    if (check(TOK_IDENT)) {
        const char* name = advance()->value.str_val;
        result = cxx_parser_value_type(name);
        if (!result && check(TOK_LPAREN)) {
            result = cxx_decltype_function_return(name, loc);
        }
        if (!result) {
            Type* named_type = rcc_parser_lookup_type(name);
            if (named_type) {
                rcc_error(loc,
                          "decltype requires an expression, not a type name");
            } else {
                rcc_error(loc, "unknown identifier '%s' in decltype expression",
                          name);
            }
            valid = false;
        }
        expression_is_lvalue = result != NULL;
        if (result && check(TOK_LPAREN)) {
            Type* function_type = result;
            if (function_type->kind == TYPE_PTR && function_type->base) {
                function_type = function_type->base;
            }
            if (function_type->kind != TYPE_FUNC) {
                function_type = cxx_decltype_function_return(name, loc);
            } else {
                function_type = function_type->ret_type;
            }
            cxx_skip_decltype_call();
            result = function_type;
            expression_is_lvalue = false;
            if (!result) {
                rcc_error(loc, "unknown function '%s' in decltype expression",
                          name);
                valid = false;
            }
        }
        if (result && (check(TOK_DOT) || check(TOK_ARROW))) {
            bool through_pointer = match(TOK_ARROW);
            if (!through_pointer) expect(TOK_DOT, ".");
            if (!check(TOK_IDENT)) {
                rcc_error(peek()->loc, "expected member name in decltype expression");
                valid = false;
            } else {
                const char* member_name = advance()->value.str_val;
                if (through_pointer && result->kind != TYPE_PTR) {
                    rcc_error(loc,
                              "decltype '->' expression requires a pointer object");
                    valid = false;
                } else {
                    result = cxx_decltype_member_type(result, member_name, loc);
                    if (!result) valid = false;
                    expression_is_lvalue = result != NULL;
                    needs_lvalue_reference = expression_is_lvalue;
                }
            }
        }
    } else if (match(TOK_INT_LIT) || match(TOK_CHAR_LIT)) {
        result = type_int;
    } else if (match(TOK_FLOAT_LIT)) {
        result = previous()->float_suffix ? type_float : type_double;
    } else if (match(TOK_TRUE) || match(TOK_FALSE)) {
        result = type_bool;
    } else if (match(TOK_NULLPTR)) {
        result = type_nullptr;
    } else {
        rcc_error(loc,
                  "unsupported expression in decltype; expected a simple value expression");
        valid = false;
    }

    if (extra_parentheses) {
        if (!check(TOK_RPAREN)) {
            rcc_error(peek()->loc, "expected ')' in decltype expression");
            valid = false;
            cxx_skip_decltype_expression();
        } else {
            advance();
        }
    } else if (!check(TOK_RPAREN)) {
        rcc_error(peek()->loc,
                  "unsupported operator in decltype expression");
        valid = false;
        cxx_skip_decltype_expression();
    }
    if (extra_parentheses && !check(TOK_RPAREN)) {
        cxx_skip_decltype_expression();
    } else {
        expect(TOK_RPAREN, ")");
    }

    if (!valid || !result) return type_int;
    if (address) return type_ptr(result);
    if (dereference) {
        if (result->kind != TYPE_PTR || !result->base) {
            rcc_error(loc, "decltype dereference requires a pointer expression");
            return type_int;
        }
        result = result->base;
        expression_is_lvalue = true;
        needs_lvalue_reference = true;
    }
    if ((extra_parentheses || needs_lvalue_reference) &&
        expression_is_lvalue) {
        Type* reference;
        if (result->kind == TYPE_PTR && result->is_reference) {
            result = result->base;
        }
        reference = type_reference(result, false);
        return reference ? reference : type_int;
    }
    return result;
}

static bool cxx_decltype_expression_is_lvalue(Expr* expression) {
    if (!expression) return false;
    switch (expression->kind) {
        case EXPR_IDENT:
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
        case EXPR_INDEX:
        case EXPR_CXX_MEMBER_PTR_ARROW:
        case EXPR_DEREF:
            return true;
        case EXPR_CXX_MEMBER_PTR_DOT:
            return !expression->cxx_member_xvalue;
        case EXPR_COMMA:
            return cxx_decltype_expression_is_lvalue(
                expression->binary_rhs);
        case EXPR_COND:
            return cxx_decltype_expression_is_lvalue(expression->cond_then) &&
                   cxx_decltype_expression_is_lvalue(expression->cond_else);
        default:
            return false;
    }
}

static Type* cxx_decltype_parsed_expression_type(Expr* expression,
                                                 SourceLoc loc) {
    Type* object_type;
    if (!expression) return NULL;
    switch (expression->kind) {
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
            /* Assignment expressions are valid C++, but this frontend does
             * not retain their lvalue category in a type-only parse.  Keep
             * the boundary explicit until that category is represented. */
            return NULL;
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_AND:
        case EXPR_OR:
            return type_bool;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            object_type = cxx_parser_expression_type(
                expression->member_base);
            if (expression->kind == EXPR_PTR_MEMBER &&
                (!object_type || object_type->kind != TYPE_PTR)) {
                return NULL;
            }
            return cxx_decltype_member_type(object_type,
                                            expression->member_name,
                                            loc);
        case EXPR_IDENT:
            if (!cxx_parser_value_type(expression->ident_name) &&
                rcc_parser_lookup_type(expression->ident_name)) {
                return NULL;
            }
            return cxx_parser_expression_type(expression);
        default:
            return cxx_parser_expression_type(expression);
    }
}

static Type* parse_cxx_decltype_type(SourceLoc loc) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    Expr* expression;
    Type* result;
    bool extra_parentheses;
    bool is_lvalue;
    int errors_before = g_error_count;

    expect(TOK_DECLTYPE, "decltype");
    expect(TOK_LPAREN, "(");
    extra_parentheses = check(TOK_LPAREN);
    expression = parse_expression();
    if (!check(TOK_RPAREN)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return parse_cxx_decltype_type_legacy(loc);
    }
    advance();
    if (g_error_count != errors_before) return type_int;

    result = cxx_decltype_parsed_expression_type(expression, loc);
    if (!result) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return parse_cxx_decltype_type_legacy(loc);
    }

    is_lvalue = cxx_decltype_expression_is_lvalue(expression);
    if ((extra_parentheses || expression->kind == EXPR_MEMBER ||
         expression->kind == EXPR_PTR_MEMBER ||
         expression->kind == EXPR_INDEX ||
         expression->kind == EXPR_DEREF) && is_lvalue) {
        Type* reference = type_reference(result, false);
        return reference ? reference : type_int;
    }
    return result;
}

/* Parse the owner suffix in a type-id such as `int ns::Base::*`.  In a
 * declaration the shared declarator parser can retain a member pointer too,
 * but named casts and sizeof(type-id) enter through this type parser and must
 * consume the same owner syntax here. */
static bool parse_cxx_member_pointer_owner_type(Type** owner_out) {
    Token* cursor = parser.cur;
    Token* star = NULL;
    char owner_name[256];
    size_t owner_length = 0u;
    SourceLoc loc;

    if (!cursor || !owner_out) return false;
    owner_name[0] = '\0';
    if (cursor->type == TOK_SCOPE) {
        owner_name[owner_length++] = ':';
        owner_name[owner_length++] = ':';
        cursor = cursor->next;
    }
    while (cursor && cursor->type == TOK_IDENT) {
        size_t segment_length = strlen(cursor->value.str_val);
        if (owner_length + segment_length >= sizeof(owner_name)) return false;
        memcpy(owner_name + owner_length, cursor->value.str_val,
               segment_length);
        owner_length += segment_length;
        owner_name[owner_length] = '\0';
        if (!cursor->next || cursor->next->type != TOK_SCOPE ||
            !cursor->next->next) {
            return false;
        }
        if (cursor->next->next->type == TOK_STAR) {
            star = cursor->next->next;
            break;
        }
        if (cursor->next->next->type != TOK_IDENT ||
            owner_length + 2u >= sizeof(owner_name)) {
            return false;
        }
        memcpy(owner_name + owner_length, "::", 2u);
        owner_length += 2u;
        owner_name[owner_length] = '\0';
        cursor = cursor->next->next;
    }
    if (!star) return false;

    loc = parser.cur->loc;
    while (parser.cur && parser.cur != star) advance();
    if (parser.cur == star) advance();
    *owner_out = rcc_parser_lookup_type(owner_name);
    if (!*owner_out || ((*owner_out)->kind != TYPE_STRUCT &&
                        (*owner_out)->kind != TYPE_UNION)) {
        rcc_error(loc, "unknown C++ member-pointer owner type '%s'",
                  owner_name);
        *owner_out = type_int;
    }
    return true;
}

static Type* parse_cxx_type_spec(void) {
    SourceLoc loc = peek()->loc;
    Type* t = NULL;
    bool is_unsigned = false;
    bool is_const = false;
    bool is_volatile = false;
    bool saw_sign = false;
    bool saw_reference = false;
    int long_count = 0;
    bool is_short = false;

    /* Qualifiers */
    while (1) {
        if (match(TOK_CONST)) is_const = true;
        else if (match(TOK_VOLATILE)) is_volatile = true;
        else if (match(TOK_UNSIGNED)) {
            is_unsigned = true;
            saw_sign = true;
        } else if (match(TOK_SIGNED)) {
            is_unsigned = false;
            saw_sign = true;
        } else if (match(TOK_LONG)) {
            ++long_count;
        }
        else if (match(TOK_SHORT)) is_short = true;
        else break;
    }

    /* Base type */
    if (match(TOK_TYPENAME)) {
        Type* owner_type = NULL;
        CxxTypeAlias* owner_alias = NULL;
        CxxTypeAlias* member_alias = NULL;
        const char* owner_name = NULL;
        const char* member_name = NULL;
        const char* owner_template_name = NULL;
        bool alias_ambiguous = false;
        bool alias_accessible = false;
        bool typename_resolved = false;

        if (cxx_qualified_class_alias_template_starts()) {
            t = parse_qualified_class_alias_template_type(loc);
            typename_resolved = true;
        } else if (cxx_template_id_followed_by_scope(&owner_template_name)) {
            CxxTemplate* owner_template;
            CxxTemplate* owner_alias_template;
            CxxClass* owner_class;
            (void)parse_qualified_name();
            owner_template = find_class_template(owner_template_name);
            owner_alias_template = owner_template
                ? NULL : find_alias_template(owner_template_name);
            owner_type = owner_template
                ? parse_class_template_specialization(owner_template, loc)
                : owner_alias_template
                    ? parse_alias_template_specialization(
                          owner_alias_template, loc, NULL)
                    : NULL;
            if (!owner_type || !match(TOK_SCOPE) || !check(TOK_IDENT)) {
                rcc_error(loc,
                          "typename must name a nested type of a class type");
                t = type_int;
            } else {
                member_name = rcc_intern(advance()->value.str_val);
                owner_class = owner_type->cxx_class;
                if (!owner_class && owner_type->cxx_template &&
                    owner_type->cxx_template->kind == TMPL_CLASS) {
                    owner_class = owner_type->cxx_template->templated_class;
                }
                if (!owner_class && active_class && active_template &&
                    owner_type->cxx_template == active_template) {
                    owner_class = active_class;
                }
                member_alias = owner_class
                    ? cxx_class_find_inherited_type_alias(
                          owner_class, member_name, active_class,
                          &alias_ambiguous, &alias_accessible)
                    : NULL;
                if (member_alias && alias_accessible) {
                    t = member_alias->type;
                } else if (member_alias) {
                    rcc_error(loc,
                              "nested type '%s' is inaccessible in class '%s'",
                              member_name,
                              owner_class->name ? owner_class->name
                                                : "<unnamed>");
                    t = type_int;
                } else if (alias_ambiguous) {
                    rcc_error(loc,
                              "nested type '%s' is ambiguous in class '%s'",
                              member_name,
                              owner_class && owner_class->name
                                  ? owner_class->name : owner_template_name);
                    t = type_int;
                } else {
                    rcc_error(loc,
                              "unknown or inaccessible nested type '%s'",
                              member_name);
                    t = type_int;
                }
            }
            typename_resolved = true;
        } else if (check(TOK_IDENT) || check(TOK_SCOPE)) {
            Token* saved_cur = parser.cur;
            Token* saved_prev = parser.prev;
            const char* qualified_name = parse_qualified_name();
            CxxTypeAlias* qualified_alias = find_qualified_class_type_alias(
                qualified_name, NULL, &alias_ambiguous, &alias_accessible);
            if (qualified_alias && alias_accessible) {
                t = qualified_alias->type;
                typename_resolved = true;
            } else if (qualified_alias) {
                rcc_error(loc, "nested type '%s' is inaccessible",
                          qualified_name);
                t = type_int;
                typename_resolved = true;
            } else if (alias_ambiguous) {
                rcc_error(loc, "nested type '%s' is ambiguous",
                          qualified_name);
                t = type_int;
                typename_resolved = true;
            }
            if (!typename_resolved) {
                parser.cur = saved_cur;
                parser.prev = saved_prev;
            }
        }

        if (!typename_resolved) {
            if (check(TOK_IDENT) && parser.cur->next &&
                parser.cur->next->type == TOK_LT) {
                bool is_current_class_template = active_class &&
                    active_template && active_template->kind == TMPL_CLASS &&
                    active_class->name &&
                    strcmp(peek()->value.str_val, active_class->name) == 0;
                if (is_current_class_template) {
                    /* The primary class template is not registered until its
                     * body has been parsed.  Parse its self-specialization
                     * from the active template context. */
                    advance();
                    owner_type = parse_class_template_specialization(
                        active_template, loc);
                } else {
                    owner_type = parse_cxx_type_spec();
                }
            } else if (check(TOK_IDENT)) {
                Token* owner_token = advance();
                owner_name = rcc_intern(owner_token->value.str_val);
                if (active_class) {
                    owner_alias = cxx_class_find_type_alias(
                        active_class, owner_name);
                }
                owner_type = owner_alias ? owner_alias->type
                                         : rcc_parser_lookup_type(owner_name);
                if (!owner_type && is_active_template_type(owner_name)) {
                    owner_type = type_struct(owner_name);
                    owner_type->cxx_dependent = true;
                    owner_type->cxx_template_param_index =
                        active_template_type_index(owner_name);
                }
            }
            if (!owner_type || !match(TOK_SCOPE) || !check(TOK_IDENT)) {
                rcc_error(loc,
                          "typename must name a nested type of a class type");
                t = type_int;
            } else {
                CxxClass* owner_class = owner_type->cxx_class;
                if (!owner_class && owner_type->cxx_template &&
                    owner_type->cxx_template->kind == TMPL_CLASS) {
                    owner_class = owner_type->cxx_template->templated_class;
                }
                if (!owner_class && active_class && active_template &&
                    owner_type->cxx_template == active_template) {
                    owner_class = active_class;
                }
                member_name = rcc_intern(advance()->value.str_val);
                member_alias = owner_class
                    ? cxx_class_find_type_alias(owner_class, member_name)
                    : NULL;
                if (member_alias && member_alias->access == ACCESS_PUBLIC) {
                    t = member_alias->type;
                } else if (owner_type->cxx_dependent &&
                           owner_type->cxx_template_param_index >= 0) {
                    char dependent_name[512];
                    int written = snprintf(dependent_name,
                                           sizeof(dependent_name),
                                           "%s::%s",
                                           owner_name ? owner_name
                                                      : owner_type->tag,
                                           member_name);
                    if (written < 0 ||
                        (size_t)written >= sizeof(dependent_name)) {
                        rcc_error(loc,
                                  "dependent nested type name is too long");
                        t = type_int;
                    } else {
                        t = type_struct(rcc_intern(dependent_name));
                        t->cxx_dependent = true;
                        t->cxx_template_param_index =
                            owner_type->cxx_template_param_index;
                        t->cxx_dependent_member_name = member_name;
                    }
                } else {
                    rcc_error(loc,
                              "unknown or inaccessible nested type '%s'",
                              member_name);
                    t = type_int;
                }
            }
        }
    } else if (check(TOK_DECLTYPE)) {
        t = parse_cxx_decltype_type(loc);
    } else if (match(TOK_VOID)) {
        t = type_void;
    } else if (match(TOK_BOOL)) {
        t = type_bool;
    } else if (match(TOK_CHAR8_T)) {
        if (is_unsigned || saw_sign || long_count != 0 || is_short) {
            rcc_error(previous()->loc,
                      "integer sign/width specifier is invalid on char8_t");
        }
        t = type_char8;
    } else if (match(TOK_CHAR)) {
        t = is_unsigned ? type_uchar : saw_sign ? type_schar : type_char;
    } else if (match(TOK___BUILTIN_VA_LIST)) {
        t = rcc_parser_builtin_va_list_type();
    } else if (match(TOK_INT) || long_count > 0 || is_short || saw_sign) {
        if (is_short) {
            t = is_unsigned ? type_ushort : type_short;
        } else if (long_count > 1) {
            t = is_unsigned ? type_ullong : type_llong;
        } else if (long_count == 1) {
            t = is_unsigned ? type_ulong : type_long;
        } else {
            t = is_unsigned ? type_uint : type_int;
        }
    } else if (match(TOK_FLOAT)) {
        t = type_float;
    } else if (match(TOK_DOUBLE)) {
        t = type_double;
    } else if (match(TOK_CLASS) || match(TOK_STRUCT)) {
        const char* name = parse_qualified_name();
        CxxClass* known_class = find_class(name);
        if (!known_class && active_class && active_class->name &&
            strcmp(name, active_class->name) == 0) {
            known_class = active_class;
        }
        t = known_class ? known_class->type : NULL;
        if (!t) {
            rcc_error(loc, "unknown C++ class type '%s'", name);
            t = type_int;
        }
    } else if (cxx_qualified_class_alias_template_starts()) {
        t = parse_qualified_class_alias_template_type(loc);
    } else if ((!parser.prev || parser.prev->type != TOK_TYPENAME) &&
               cxx_template_id_followed_by_scope(NULL)) {
        const char* template_name = parse_qualified_name();
        CxxTemplate* owner_template = find_class_template(template_name);
        CxxTemplate* owner_alias_template = owner_template
            ? NULL : find_alias_template(template_name);
        Type* owner_type = owner_template
            ? parse_class_template_specialization(owner_template, loc)
            : owner_alias_template
                ? parse_alias_template_specialization(owner_alias_template,
                                                      loc, NULL)
                : NULL;
        CxxClass* owner_class = owner_type ? owner_type->cxx_class : NULL;
        CxxTypeAlias* member_alias = NULL;
        bool alias_ambiguous = false;
        bool alias_accessible = false;
        const char* member_name = NULL;

        if (!match(TOK_SCOPE) || !check(TOK_IDENT)) {
            rcc_error(loc,
                      "template-id qualified type must name a nested type");
            t = type_int;
        } else {
            member_name = rcc_intern(advance()->value.str_val);
            if (owner_class) {
                member_alias = cxx_class_find_inherited_type_alias(
                    owner_class, member_name, active_class,
                    &alias_ambiguous, &alias_accessible);
            }
            if (member_alias && alias_accessible) {
                t = member_alias->type;
            } else if (member_alias) {
                rcc_error(loc,
                          "nested type '%s' is inaccessible in class '%s'",
                          member_name,
                          owner_class->name ? owner_class->name : "<unnamed>");
                t = type_int;
            } else if (alias_ambiguous) {
                rcc_error(loc, "nested type '%s' is ambiguous in class '%s'",
                          member_name,
                          owner_class && owner_class->name
                              ? owner_class->name : template_name);
                t = type_int;
            } else {
                rcc_error(loc,
                          "class template '%s' has no accessible nested type '%s'",
                          template_name, member_name);
                t = type_int;
            }
        }
    } else if (check(TOK_IDENT) || check(TOK_SCOPE)) {
        /* Class or namespace qualified type */
        const char* name = parse_qualified_name();
        CxxTypeAlias* class_scope_alias = NULL;
        CxxTypeAlias* qualified_class_alias = NULL;
        bool class_alias_ambiguous = false;
        bool class_alias_accessible = false;
        bool qualified_alias_ambiguous = false;
        bool qualified_alias_accessible = false;
        CxxTemplate* tmpl = find_class_template(name);
        CxxClass* class_scope_alias_template_owner = NULL;
        bool class_scope_alias_template_ambiguous = false;
        bool class_scope_alias_template_accessible = false;
        CxxClassAliasTemplate* class_scope_alias_template = NULL;
        if (active_class && !strstr(name, "::") && check(TOK_LT)) {
            cxx_resolve_known_class_bases(active_class);
            class_scope_alias_template =
                cxx_parser_find_inherited_alias_template(
                    active_class, name, active_class,
                    &class_scope_alias_template_owner,
                    &class_scope_alias_template_ambiguous,
                    &class_scope_alias_template_accessible);
            if (class_scope_alias_template &&
                !class_scope_alias_template_accessible &&
                class_scope_alias_template->access != ACCESS_PRIVATE &&
                cxx_class_access_context_is_derived_from(
                    active_class, class_scope_alias_template_owner,
                    active_class, 0u)) {
                class_scope_alias_template_accessible = true;
            }
        }
        CxxTemplate* alias_tmpl = check(TOK_LT)
            ? class_scope_alias_template
                ? class_scope_alias_template->declaration
                : find_alias_template(name)
            : NULL;
        int template_template_index =
            active_template_template_parameter_index(name);
        CxxClass* known_class = find_class(name);
        if (!known_class && active_class && active_class->name &&
            strcmp(name, active_class->name) == 0) {
            known_class = active_class;
        }
        Type* known_type = rcc_parser_lookup_type(name);
        if (class_scope_alias_template_ambiguous) {
            rcc_error(loc,
                      "nested alias template '%s' is ambiguous in class '%s'",
                      name, active_class->name ? active_class->name
                                               : "<unnamed>");
            skip_cxx_template_arguments();
            t = type_int;
        }
        if (active_class && name && !strstr(name, "::")) {
            class_scope_alias = cxx_class_find_inherited_type_alias(
                active_class, name, active_class, &class_alias_ambiguous,
                &class_alias_accessible);
            if (class_scope_alias) {
                if (class_alias_accessible) {
                    t = class_scope_alias->type;
                } else {
                    rcc_error(loc,
                              "nested type '%s' is inaccessible in class '%s'",
                              name,
                              active_class->name ? active_class->name
                                                 : "<unnamed>");
                    t = type_int;
                }
            } else if (class_alias_ambiguous) {
                rcc_error(loc, "nested type '%s' is ambiguous in class '%s'",
                          name, active_class->name ? active_class->name
                                                   : "<unnamed>");
                t = type_int;
            }
        }
        if (!t && name && strstr(name, "::")) {
            CxxClass* alias_owner = NULL;
            qualified_class_alias = find_qualified_class_type_alias(
                name, &alias_owner, &qualified_alias_ambiguous,
                &qualified_alias_accessible);
            if (qualified_class_alias && qualified_alias_accessible) {
                t = qualified_class_alias->type;
            } else if (qualified_class_alias) {
                rcc_error(loc,
                          "nested type '%s' is inaccessible in class '%s'",
                          name,
                          alias_owner && alias_owner->name
                              ? alias_owner->name : "<unnamed>");
                t = type_int;
            } else if (qualified_alias_ambiguous) {
                rcc_error(loc, "nested type '%s' is ambiguous", name);
                t = type_int;
            }
        }
        if (known_type && known_type->cxx_class &&
            known_type->cxx_scope_identity) {
            known_class = known_type->cxx_class;
        }
        if (t) {
            /* A class-scope alias was resolved above, before namespace types
             * and class templates with the same unqualified spelling. */
        } else if (tmpl && !check(TOK_LT)) {
            bool direct_initialization = check(TOK_IDENT) &&
                (check_next(TOK_LPAREN) || check_next(TOK_LBRACE));
            if (!rcc_parser_cxx_standard_at_least(17)) {
                rcc_error(loc,
                          "class template argument deduction requires "
                          "C++17 or newer");
                t = type_int;
            } else if (!direct_initialization) {
                rcc_error(loc,
                          "class template argument deduction requires "
                          "direct initialization");
                t = type_int;
            } else {
                /* Keep the template identity on a bounded placeholder.  The
                 * declaration parser resolves it only after it has parsed the
                 * constructor arguments, so a class template name can never
                 * silently become an arbitrary scalar type. */
                t = type_struct(name);
                t->cxx_dependent = true;
                t->cxx_template = tmpl;
                t->cxx_template_param_index = -1;
                t->cxx_template_arg_count = 0;
            }
        } else if (template_template_index >= 0 && check(TOK_LT)) {
            TemplateParam* parameter = &active_template->params[
                template_template_index];
            Type* dependent = type_struct(name);
            int nested_count = 0;
            dependent->cxx_dependent = true;
            dependent->cxx_template_param_index = template_template_index;
            expect(TOK_LT, "template-template argument list");
            if (!check(TOK_GT)) {
                do {
                    if (nested_count >= 32 || !parameter->template_signature ||
                        nested_count >=
                            parameter->template_signature->param_count) {
                        rcc_error(peek()->loc,
                                  "template-template argument count exceeded");
                        while (!check(TOK_COMMA) && !check(TOK_GT) &&
                               !at_end()) {
                            advance();
                        }
                        if (check(TOK_COMMA)) advance();
                        continue;
                    }
                    if (parameter->template_signature->params[nested_count].kind !=
                        TPARAM_TYPE) {
                        Expr* value_expression;
                        rcc_parser_set_cxx_template_default_mode(true);
                        value_expression = parse_assignment_expression();
                        rcc_parser_set_cxx_template_default_mode(false);
                        dependent->cxx_template_args = ast_arena_grow(
                            dependent->cxx_template_args,
                            sizeof(Type*) * (size_t)nested_count,
                            sizeof(Type*) * (size_t)(nested_count + 1));
                        dependent->cxx_template_value_args = ast_arena_grow(
                            dependent->cxx_template_value_args,
                            sizeof(Expr*) * (size_t)nested_count,
                            sizeof(Expr*) * (size_t)(nested_count + 1));
                        dependent->cxx_template_args[nested_count] =
                            parameter->template_signature->params[nested_count].type
                                ? parameter->template_signature->params[nested_count].type
                                : type_int;
                        dependent->cxx_template_value_args[nested_count++] =
                            value_expression;
                    } else {
                        dependent->cxx_template_args = ast_arena_grow(
                            dependent->cxx_template_args,
                            sizeof(Type*) * (size_t)nested_count,
                            sizeof(Type*) * (size_t)(nested_count + 1));
                        dependent->cxx_template_value_args = ast_arena_grow(
                            dependent->cxx_template_value_args,
                            sizeof(Expr*) * (size_t)nested_count,
                            sizeof(Expr*) * (size_t)(nested_count + 1));
                        dependent->cxx_template_args[nested_count++] =
                            parse_cxx_type_spec();
                        dependent->cxx_template_value_args[nested_count - 1] =
                            NULL;
                    }
                } while (match(TOK_COMMA));
            }
            expect(TOK_GT, ">");
            dependent->cxx_template_arg_count = nested_count;
            if (!parameter->template_signature ||
                nested_count != parameter->template_signature->param_count) {
                rcc_error(loc,
                          "template-template argument count does not match "
                          "its parameter list");
            }
            t = dependent;
        } else if (alias_tmpl) {
            if (class_scope_alias_template &&
                !class_scope_alias_template_accessible) {
                (void)parse_alias_template_specialization(
                    alias_tmpl, loc,
                    class_scope_alias_template_owner
                        ? class_scope_alias_template_owner->type : NULL);
                rcc_error(loc,
                          "nested alias template '%s' is inaccessible in class '%s'",
                          name, active_class && active_class->name
                              ? active_class->name : "<unnamed>");
                t = type_int;
            } else {
                t = parse_alias_template_specialization(
                    alias_tmpl, loc,
                    class_scope_alias_template_owner
                        ? class_scope_alias_template_owner->type : NULL);
            }
        } else if (tmpl) {
            t = parse_class_template_specialization(tmpl, loc);
        } else if (known_class && active_template &&
                   active_class == known_class) {
            /* A self-reference in a class template is dependent on the
             * specialization even when the primary class is already visible
             * in the parser's class table. */
            t = type_struct(name);
            t->cxx_dependent = true;
        } else if (known_class) {
            t = known_class->type;
        } else if (known_type) {
            t = known_type;
        } else if (is_active_template_type(name)) {
            /* A dependent type remains an incomplete placeholder until
             * template substitution. */
            t = type_struct(name);
            t->cxx_dependent = true;
        } else {
            if (check(TOK_LT)) {
                rcc_error(loc, "unknown C++ class template '%s'", name);
                skip_cxx_template_arguments();
            } else {
                rcc_error(loc, "unknown C++ type name '%s'", name);
            }
            t = type_int;
        }
    } else {
        rcc_error(loc, "expected C++ type specifier, got '%s'",
                  token_type_str(peek()->type));
        t = is_unsigned ? type_uint : type_int;
    }

    if (!t && (saw_sign || long_count > 0 || is_short)) {
        t = is_unsigned ? type_uint : type_int;
    }

    /* Match declaration-specifier grammar for qualifiers written after a
     * named or built-in base type (for example, `Widget const&`).  These
     * qualifiers belong to the base object and must be applied before any
     * pointer/reference layers below are constructed. */
    if (t && (check(TOK_CONST) || check(TOK_VOLATILE))) {
        Type* qualified = ast_arena_alloc(sizeof(*qualified));
        *qualified = *t;
        while (match(TOK_CONST) || match(TOK_VOLATILE)) {
            if (previous()->type == TOK_CONST) qualified->is_const = true;
            else qualified->is_volatile = true;
        }
        t = qualified;
    }

    /* Prefix cv-qualifiers apply to the base type, before pointer and
     * reference declarators are layered on top. */
    if ((is_const || is_volatile) && t) {
        Type* ct = ast_arena_alloc(sizeof(Type));
        *ct = *t;
        ct->is_const = ct->is_const || is_const;
        ct->is_volatile = ct->is_volatile || is_volatile;
        t = ct;
    }

    /* Reference and pointer */
    while (1) {
        Type* member_owner = NULL;
        if (parse_cxx_member_pointer_owner_type(&member_owner)) {
            Type* member_pointer = type_ptr(t);
            if (!member_pointer) {
                rcc_error(peek()->loc,
                          "invalid C++ pointer-to-member type");
                return type_int;
            }
            type_cxx_member_pointer(member_pointer, member_owner);
            while (match(TOK_CONST) || match(TOK_VOLATILE) ||
                   match(TOK_RESTRICT)) {
                if (previous()->type == TOK_CONST) {
                    member_pointer->is_const = true;
                } else if (previous()->type == TOK_VOLATILE) {
                    member_pointer->is_volatile = true;
                } else {
                    member_pointer->is_restrict = true;
                }
            }
            t = member_pointer;
        } else if (match(TOK_STAR)) {
            t = type_ptr(t);
            while (match(TOK_CONST)) t->is_const = true;
        } else if (match(TOK_AMP)) {
            if (saw_reference) {
                rcc_error(previous()->loc,
                          "a C++ type cannot directly bind a reference "
                          "to a reference");
                return type_int;
            }
            saw_reference = true;
            t = type_reference(t, false);
        } else if (match(TOK_AND)) {
            if (saw_reference) {
                rcc_error(previous()->loc,
                          "a C++ type cannot directly bind a reference "
                          "to a reference");
                return type_int;
            }
            saw_reference = true;
            t = type_reference(t, true);
        } else {
            break;
        }
        if (!t) {
            rcc_error(peek()->loc, "invalid C++ reference type");
            return type_int;
        }
    }

    return t;
}

Expr* rcc_parse_cxx_special_expression(void) {
    SourceLoc loc = peek()->loc;

    if (match(TOK_NEW)) {
        Type* object_type = parse_cxx_type_spec();
        Expr* count = NULL;
        Expr* bytes;
        Expr* allocation;
        ExprList* new_args = NULL;
        ExprList* allocation_args = NULL;
        bool is_array = false;
        bool value_init = false;
        bool brace_init = false;

        if (match(TOK_LBRACKET)) {
            is_array = true;
            if (check(TOK_RBRACKET)) {
                rcc_error(loc, "array new requires an element count");
            } else {
                count = parse_expression();
            }
            expect(TOK_RBRACKET, "]");
        }
        if (match(TOK_LPAREN)) {
            value_init = check(TOK_RPAREN);
            while (!check(TOK_RPAREN) && !at_end()) {
                exprlist_append(&new_args, parse_assignment_expression());
                if (!match(TOK_COMMA)) break;
            }
            expect(TOK_RPAREN, ")");
        } else if (match(TOK_LBRACE)) {
            brace_init = true;
            value_init = check(TOK_RBRACE);
            while (!check(TOK_RBRACE) && !at_end()) {
                exprlist_append(&new_args, parse_assignment_expression());
                if (!match(TOK_COMMA)) break;
            }
            expect(TOK_RBRACE, "}");
        }
        if (!object_type || object_type == type_void ||
            object_type->kind == TYPE_FUNC ||
            !type_is_complete(object_type)) {
            rcc_error(loc, "new requires a complete object type");
        }
        bytes = expr_sizeof_type(object_type, loc);
        if (is_array && count) {
            bytes = expr_binary(EXPR_MUL, bytes, count, loc);
        }
        exprlist_append(&allocation_args, bytes);
        allocation = expr_call(expr_ident("rin_malloc", loc),
                               allocation_args, loc);
        allocation->call_is_new = true;
        allocation->call_new_value_init = value_init;
        allocation->call_new_is_array = is_array;
        allocation->call_new_brace_init = brace_init;
        allocation->call_new_type = object_type;
        allocation->call_new_count = count;
        /* The allocation size is the ordinary call argument.  Keep the
         * constructor arguments separate so they are evaluated exactly once
         * after the allocator returns the object address. */
        allocation->call_new_args = new_args;
        return allocation;
    }

    if (match(TOK_DELETE)) {
        Expr* operand;
        ExprList* arguments = NULL;
        bool is_array = match(TOK_LBRACKET);
        if (is_array) expect(TOK_RBRACKET, "]");
        operand = parse_expression();
        if (!operand) return expr_call(
            expr_ident("rin_free", loc), NULL, loc);
        exprlist_append(&arguments, operand);
        /* Both scalar and array allocation use the RinOS allocator.  Semantic
         * analysis attaches a destructor only after proving the complete
         * cleanup shape; unsupported object lifetimes remain diagnostics
         * instead of silently becoming free-only calls. */
        Expr* result = expr_call(expr_ident("rin_free", loc), arguments, loc);
        result->call_is_delete = true;
        result->call_delete_is_array = is_array;
        return result;
    }

    rcc_error(loc, "internal C++ special-expression parser entry");
    return expr_int(0, loc);
}

/* Parse a member selected through a dependent type parameter, such as
 * `T::value`.  The common qualified-name parser intentionally treats `::` as
 * a single identifier spelling, but a type parameter's member must survive
 * function-template cloning so the substituted class can resolve its static
 * data member later.  Keep this hook transactional: only the exact
 * type-parameter `::` identifier form is claimed. */
Expr* rcc_parse_cxx_dependent_member(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* owner_name;
    const char* member_name;
    int parameter_index;
    Type* dependent;
    Expr* base;
    Expr* result;

    if (!check(TOK_IDENT)) return NULL;
    owner_name = peek()->value.str_val;
    parameter_index = active_template_type_index(owner_name);
    if (parameter_index < 0 || !is_active_template_type(owner_name)) {
        return NULL;
    }
    advance();
    if (!match(TOK_SCOPE) || !check(TOK_IDENT)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    member_name = advance()->value.str_val;
    dependent = type_struct(owner_name);
    dependent->cxx_dependent = true;
    dependent->cxx_template_param_index = parameter_index;
    base = expr_ident(owner_name, loc);
    base->type = dependent;
    result = expr_member(base, member_name, loc);
    return result;
}

/* Parse a static member selected through a class-template specialization,
 * such as `Counter<int>::value`.  The common qualified-name parser cannot
 * consume the angle-bracket portion, so keep this narrow hook transactional:
 * ordinary comparisons, namespace names, and unsupported members are left
 * for the normal expression parser without inventing a fallback value. */
Expr* rcc_parse_cxx_qualified_template_member(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name;
    CxxTemplate* tmpl;
    Type* type;
    const char* member_name;
    CxxClass* cls;
    Expr* result = NULL;

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    name = parse_qualified_name();
    tmpl = check(TOK_LT) ? find_class_template(name) : NULL;
    if (!tmpl) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    type = parse_class_template_specialization(tmpl, loc);
    if (!type || !type->cxx_class || !match(TOK_SCOPE) ||
        !check(TOK_IDENT)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    member_name = advance()->value.str_val;
    cls = type->cxx_class;
    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        const char* final_name;
        Decl* declaration = member->decl;
        if (!member->is_static || !declaration ||
            (declaration->kind != DECL_VAR && declaration->kind != DECL_FUNC) ||
            !declaration->name) {
            continue;
        }
        final_name = strrchr(declaration->name, ':');
        final_name = final_name ? final_name + 1 : declaration->name;
        if (strcmp(final_name, member_name) != 0) continue;
        result = expr_ident(declaration->name, loc);
        result->ident_decl = declaration;
        result->type = declaration->type;
        break;
    }
    if (!result) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
    }
    return result;
}

Type* rcc_parse_cxx_type_name(void) {
    return parse_cxx_type_spec();
}

static Expr* parse_cxx_expression(void) {
    /* The common parser provides the complete precedence grammar.  Its C++
     * primary hook handles this/nullptr/new/delete and its C++ mode branch
     * handles true/false, so expressions are not truncated at one token. */
    return parse_expression();
}

/* ═══════════════════════════════════════
 * C++ Statement Parsing
 * ═══════════════════════════════════════ */

extern Stmt* parse_declaration(void);

static bool is_active_template_type(const char* name) {
    int index;
    if (!active_template || !name) return false;
    for (index = 0; index < active_template->param_count; ++index) {
        TemplateParam* parameter = &active_template->params[index];
        if (parameter->kind == TPARAM_TYPE && parameter->name &&
            strcmp(parameter->name, name) == 0) {
            return true;
        }
    }
    if (active_template->enclosing_template) {
        CxxTemplate* enclosing = active_template->enclosing_template;
        for (index = 0; index < enclosing->param_count; ++index) {
            TemplateParam* parameter = &enclosing->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->name &&
                strcmp(parameter->name, name) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool cxx_structured_binding_starts(void) {
    Token* token = parser.cur;
    if (!token) return false;
    if (token->type == TOK_CONST) token = token->next;
    if (!token || token->type != TOK_AUTO) return false;
    token = token->next;
    if (token && (token->type == TOK_AMP || token->type == TOK_AND)) {
        token = token->next;
    }
    return token && token->type == TOK_LBRACKET;
}

static Type* cxx_structured_binding_unqualified_type(Type* type) {
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        return type->base;
    }
    return type;
}

static Type* cxx_structured_binding_const_type(Type* type) {
    Type* qualified;
    if (!type || type->is_const) return type;
    qualified = ast_arena_alloc(sizeof(*qualified));
    *qualified = *type;
    qualified->is_const = true;
    return qualified;
}

static bool cxx_structured_binding_initializer_is_lvalue(Expr* expression) {
    if (!expression) return false;
    switch (expression->kind) {
        case EXPR_IDENT:
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
        case EXPR_INDEX:
        case EXPR_CXX_MEMBER_PTR_ARROW:
        case EXPR_DEREF:
            return true;
        case EXPR_CXX_MEMBER_PTR_DOT:
            return !expression->cxx_member_xvalue;
        case EXPR_COMMA:
            return cxx_structured_binding_initializer_is_lvalue(
                expression->binary_rhs);
        case EXPR_COND:
            return cxx_structured_binding_initializer_is_lvalue(
                       expression->cond_then) &&
                   cxx_structured_binding_initializer_is_lvalue(
                       expression->cond_else);
        default:
            return false;
    }
}

static Stmt* parse_cxx_structured_binding_declaration(void) {
    SourceLoc loc = peek()->loc;
    bool is_const = match(TOK_CONST);
    bool is_reference = false;
    bool is_rvalue_reference = false;
    bool materialize_rvalue = false;
    bool direct_list_initializer = false;
    Type* initializer_type;
    Type* binding_source_type;
    Expr* initializer;
    Decl* hidden;
    StmtList* statements = NULL;
    const char* binding_names[32];
    int binding_count = 0;
    char hidden_name[64];
    int written;

    if (!check(TOK_AUTO)) return NULL;
    advance();
    if (match(TOK_AMP)) {
        is_reference = true;
    } else if (match(TOK_AND)) {
        is_reference = true;
        is_rvalue_reference = true;
    }
    expect(TOK_LBRACKET, "structured binding list");
    if (check(TOK_RBRACKET)) {
        rcc_error(loc, "structured binding list cannot be empty");
    }
    while (!check(TOK_RBRACKET) && !at_end()) {
        Token* name = expect(TOK_IDENT, "structured binding name");
        if (name && binding_count < (int)(sizeof(binding_names) /
                                          sizeof(binding_names[0]))) {
            binding_names[binding_count++] = name->value.str_val;
        } else if (name) {
            rcc_error(name->loc, "too many structured binding names");
        }
        if (!match(TOK_COMMA)) break;
    }
    expect(TOK_RBRACKET, "]");
    if (!match(TOK_ASSIGN)) {
        if (check(TOK_LBRACE)) {
            direct_list_initializer = true;
            initializer = rcc_parser_parse_initializer();
        } else {
            rcc_error(peek()->loc,
                      "RinOS structured bindings require an '=' initializer");
            while (!check(TOK_SEMICOLON) && !at_end()) advance();
            (void)match(TOK_SEMICOLON);
            return stmt_null(loc);
        }
    } else {
        initializer = parse_cxx_expression();
    }
    expect(TOK_SEMICOLON, ";");
    if (!initializer || binding_count == 0) return stmt_null(loc);

    if (direct_list_initializer) {
        ExprList* item = initializer->kind == EXPR_COMPOUND
            ? initializer->compound_init : NULL;
        if (!item || item->next || item->designator_kind !=
                INIT_DESIGNATOR_NONE) {
            rcc_error(loc,
                      "direct-list structured binding requires one initializer expression");
            return stmt_null(loc);
        }
        initializer = item->expr;
    }

    initializer_type = initializer->type;
    if (!initializer_type && initializer->kind == EXPR_IDENT) {
        initializer_type = cxx_parser_value_type(initializer->ident_name);
    }
    if (!initializer_type && initializer->kind == EXPR_COMPOUND) {
        initializer_type = initializer->compound_type;
    }
    if (is_rvalue_reference) {
        materialize_rvalue =
            !cxx_structured_binding_initializer_is_lvalue(initializer);
    }
    binding_source_type = cxx_structured_binding_unqualified_type(
        initializer_type);
    if (!binding_source_type ||
        (binding_source_type->kind != TYPE_STRUCT &&
         binding_source_type->kind != TYPE_UNION &&
         binding_source_type->kind != TYPE_ARRAY)) {
        rcc_error(loc,
                  "structured binding requires a complete aggregate initializer");
        return stmt_null(loc);
    }
    if (!type_is_complete(binding_source_type)) {
        rcc_error(loc,
                  "structured binding requires a complete aggregate type");
        return stmt_null(loc);
    }
    if (binding_source_type->kind == TYPE_ARRAY &&
        binding_source_type->array_len < 0) {
        rcc_error(loc,
                  "structured binding requires a fixed-size array");
        return stmt_null(loc);
    }
    if (binding_source_type->kind == TYPE_ARRAY) {
        if (binding_count != binding_source_type->array_len) {
            rcc_error(loc,
                      "structured binding count does not match array extent");
            return stmt_null(loc);
        }
    } else {
        int field_count = 0;
        for (TypeField* field = binding_source_type->fields; field;
             field = field->next) {
            if (field->name) ++field_count;
        }
        if (binding_count != field_count) {
            rcc_error(loc,
                      "structured binding count does not match aggregate fields");
            return stmt_null(loc);
        }
    }
    written = snprintf(hidden_name, sizeof(hidden_name),
                       "__rcc_structured_binding_%u",
                       ++cxx_structured_binding_counter);
    if (written < 0 || (size_t)written >= sizeof(hidden_name)) {
        rcc_error(loc, "structured binding temporary name exceeds compiler limits");
        return stmt_null(loc);
    }

    Type* hidden_type = binding_source_type;
    if (binding_source_type->kind == TYPE_ARRAY ||
        (is_reference && !materialize_rvalue)) {
        hidden_type = type_ptr(binding_source_type);
        hidden_type->is_reference = true;
    } else if (is_const) {
        hidden_type = cxx_structured_binding_const_type(binding_source_type);
    }
    hidden = decl_var(rcc_intern(hidden_name), hidden_type, initializer, loc);
    rcc_parser_cxx_add_value_binding(hidden->name, hidden_type);
    stmtlist_append(&statements, stmt_decl(hidden, loc));

    {
        TypeField* field = binding_source_type->fields;
        for (int index = 0; index < binding_count; ++index) {
            Expr* object = expr_ident(hidden->name, loc);
            Expr* element;
            Type* element_type;
            Type* reference_type;
            Decl* binding;

            object->ident_decl = hidden;
            object->type = binding_source_type;
            if (binding_source_type->kind == TYPE_ARRAY) {
                element = expr_index(object, expr_int(index, loc), loc);
                element_type = binding_source_type->base;
            } else {
                while (field && !field->name) field = field->next;
                if (!field) {
                    rcc_error(loc, "structured binding selected an unnamed field");
                    return stmt_null(loc);
                }
                element = expr_member(object, field->name, loc);
                element->member_field = field;
                element_type = field->type;
                field = field->next;
            }
            if (is_const) element_type = cxx_structured_binding_const_type(
                element_type);
            reference_type = type_ptr(element_type);
            reference_type->is_reference = true;
            binding = decl_var(binding_names[index], reference_type,
                               element, loc);
            rcc_parser_cxx_add_value_binding(binding->name, reference_type);
            stmtlist_append(&statements, stmt_decl(binding, loc));
        }
    }
    Stmt* structured = stmt_block(statements, loc);
    structured->block_no_scope = true;
    return structured;
}

/* Return whether a `for` header contains the range separator at its outer
 * parameter-list depth.  Nested conditional expressions may contain `:`;
 * only a separator directly inside the header belongs to range-for. */
static bool cxx_range_for_header(void) {
    Token* token;
    int depth = 0;
    int conditional_depth = 0;
    if (!parser.cur || parser.cur->type != TOK_FOR || !parser.cur->next ||
        parser.cur->next->type != TOK_LPAREN) return false;
    for (token = parser.cur->next; token; token = token->next) {
        if (token->type == TOK_LPAREN || token->type == TOK_LBRACKET ||
            token->type == TOK_LBRACE) {
            ++depth;
        } else if (token->type == TOK_RPAREN ||
                   token->type == TOK_RBRACKET ||
                   token->type == TOK_RBRACE) {
            if (token->type == TOK_RPAREN && depth == 1) break;
            if (depth > 0) --depth;
        } else if (depth == 1 && token->type == TOK_QUESTION) {
            ++conditional_depth;
        } else if (token->type == TOK_COLON && depth == 1) {
            if (conditional_depth > 0) {
                --conditional_depth;
            } else {
                return true;
            }
        }
    }
    return false;
}

static int active_template_type_index(const char* name) {
    if (!active_template || !name) return -1;
    for (int index = 0; index < active_template->param_count; ++index) {
        TemplateParam* parameter = &active_template->params[index];
        if (parameter->kind == TPARAM_TYPE && parameter->name &&
            strcmp(parameter->name, name) == 0) {
            return index;
        }
    }
    if (active_template->enclosing_template) {
        CxxTemplate* enclosing = active_template->enclosing_template;
        for (int index = 0; index < enclosing->param_count; ++index) {
            TemplateParam* parameter = &enclosing->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->name &&
                strcmp(parameter->name, name) == 0) {
                return active_template->param_count + index;
            }
        }
    }
    return -1;
}

static TypeMethod* cxx_range_find_method(Type* type, const char* name) {
    TypeMethod* method;
    TypeMethod* result = NULL;
    if (!type || !name ||
        (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION)) {
        return NULL;
    }
    for (method = type->methods; method; method = method->next) {
        if (method->kind != TYPE_METHOD_FUNCTION || !method->name ||
            strcmp(method->name, name) != 0 || method->cxx_access != 0u ||
            !method->function_decl || !method->function_decl->type) {
            continue;
        }
        if (result) return NULL;
        result = method;
    }
    return result;
}

static bool cxx_range_initializer_element_type(Type* type) {
    return type && (type_is_arithmetic(type) || type->kind == TYPE_ENUM ||
                    type->kind == TYPE_PTR || type->kind == TYPE_NULLPTR);
}

/* A braced range expression is the C++20 initializer-list form.  The RinOS
 * range ABI does not expose std::initializer_list, so materialize the
 * bounded scalar list as a compiler-owned array compound expression.  This
 * keeps the ordinary array range lowering in charge of one-time evaluation,
 * element lifetime, and both target ABIs without pretending that arbitrary
 * class initializer-list constructors are supported. */
static Expr* parse_cxx_range_initializer_list(SourceLoc loc) {
    ExprList* items = NULL;
    Type* element_type = NULL;
    int item_count = 0;
    bool valid = true;

    expect(TOK_LBRACE, "'{' after range-for colon");
    if (!check(TOK_RBRACE)) {
        do {
            Expr* item = parse_assignment_expression();
            Type* item_type = cxx_parser_expression_type(item);
            if (!item || !item_type ||
                !cxx_range_initializer_element_type(item_type)) {
                rcc_error(item ? item->loc : loc,
                          "RinOS braced range-for requires scalar element expressions");
                valid = false;
            } else if (!element_type) {
                element_type = item_type;
            } else if (type_is_arithmetic(element_type) &&
                       type_is_arithmetic(item_type)) {
                element_type = type_common(element_type, item_type);
            } else if (!type_is_compatible(element_type, item_type)) {
                rcc_error(item->loc,
                          "RinOS braced range-for elements require one compatible element type");
                valid = false;
            }
            if (item) {
                exprlist_append(&items, item);
                if (item_count == INT_MAX) {
                    rcc_error(item->loc,
                              "RinOS braced range-for has too many elements");
                    valid = false;
                } else {
                    ++item_count;
                }
            }
        } while (match(TOK_COMMA) && !check(TOK_RBRACE));
    }
    expect(TOK_RBRACE, "'}' after braced range-for initializer");
    if (item_count == 0) {
        rcc_error(loc,
                  "RinOS braced range-for requires at least one element");
        valid = false;
    }
    if (!valid || !element_type || item_count <= 0) return NULL;
    {
        Expr* result = expr_initializer_list(items, loc);
        result->compound_type = type_array(element_type, item_count);
        result->type = result->compound_type;
        return result;
    }
}

static Type* cxx_range_value_type(Type* type) {
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        return type->base;
    }
    return type;
}

/* Lower array ranges into indexed loops and the bounded member-iterator
 * profile into the equivalent begin/end loop.  Both paths materialize the
 * range expression once; unsupported protocols are diagnosed before any
 * executable fallback can be emitted. */
Stmt* rcc_parse_cxx_range_for_statement(void) {
    SourceLoc loc;
    Type* item_type = NULL;
    Type* element_type = NULL;
    Type* iterator_type = NULL;
    bool is_auto = false;
    bool auto_const = false;
    bool auto_reference = false;
    bool auto_rvalue_reference = false;
    bool iterator_range = false;
    bool range_invalid = false;
    const char* item_name = NULL;
    Expr* range;
    Type* range_type = NULL;
    Expr* index_expression;
    Expr* element_expression;
    Expr* count_expression;
    Expr* condition;
    Expr* increment;
    Expr* range_storage;
    Expr* begin_expression;
    Expr* end_expression;
    Expr* begin_identifier;
    Expr* end_identifier;
    Decl* index_decl;
    Decl* range_decl = NULL;
    Decl* begin_decl = NULL;
    Decl* end_decl = NULL;
    Decl* item_decl;
    TypeMethod* begin_method = NULL;
    TypeMethod* end_method = NULL;
    TypeMethod* increment_method = NULL;
    TypeMethod* compare_method = NULL;
    TypeMethod* dereference_method = NULL;
    Stmt* original_body;
    Stmt* loop;
    StmtList* outer_statements = NULL;
    StmtList* body_statements = NULL;
    char index_name[64];
    char range_name[64];
    char begin_name[64];
    char end_name[64];
    unsigned range_id;
    int written;

    if (!cxx_range_for_header()) return NULL;
    loc = parser.cur->loc;
    advance(); /* for */
    expect(TOK_LPAREN, "(");
    if (check(TOK_AUTO) || (check(TOK_CONST) && check_next(TOK_AUTO))) {
        auto_const = match(TOK_CONST);
        match(TOK_AUTO);
        is_auto = true;
        if (match(TOK_AMP)) {
            auto_reference = true;
        } else if (match(TOK_AND)) {
            /* An array identifier is an lvalue range.  `auto&&` therefore
             * deduces an lvalue reference to its element, while retaining
             * the spelling here lets us reject the cv-qualified form that
             * cannot bind to this range. */
            auto_reference = true;
            auto_rvalue_reference = true;
        }
        {
            Token* name = expect(TOK_IDENT, "range variable name");
            if (name) item_name = name->value.str_val;
        }
    } else {
        item_type = parse_cxx_type_spec();
        item_type = rcc_parser_parse_cxx_declarator(
            item_type, &item_name, NULL);
        if (!item_name) {
            rcc_error(peek()->loc, "range-for requires an element declaration");
        }
    }
    expect(TOK_COLON, ":");
    range = check(TOK_LBRACE)
        ? parse_cxx_range_initializer_list(loc)
        : parse_cxx_expression();
    expect(TOK_RPAREN, ")");
    range_type = cxx_parser_expression_type(range);
    if (!range) {
        rcc_error(loc,
                  "RinOS range-for requires an array or supported iterator range expression");
        range_invalid = true;
    } else if (!range_type || range_type->kind != TYPE_ARRAY ||
               range_type->array_len < 0 || !range_type->base) {
        if (range_type && (range_type->kind == TYPE_STRUCT ||
                           range_type->kind == TYPE_UNION) &&
            range_type->is_complete) {
            begin_method = cxx_range_find_method(range_type, "begin");
            end_method = cxx_range_find_method(range_type, "end");
            if (!begin_method || !end_method) {
                rcc_error(loc,
                          "RinOS range-for requires one public non-overloaded begin() and end() member");
                range_invalid = true;
            } else {
                iterator_type = cxx_range_value_type(
                    begin_method->return_type);
                if (!iterator_type ||
                    !type_is_complete(iterator_type)) {
                    rcc_error(loc,
                              "RinOS range-for begin() must return a complete iterator object");
                    range_invalid = true;
                }
                if (!range_invalid) {
                    if (iterator_type->kind == TYPE_PTR &&
                        iterator_type->base &&
                        iterator_type->base->kind != TYPE_VOID &&
                        type_is_complete(iterator_type->base)) {
                        /* Raw object pointers already provide the three
                         * required iterator operations through the built-in
                         * pointer ABI. */
                        iterator_range = true;
                        element_type = iterator_type->base;
                    } else {
                        increment_method = cxx_range_find_method(
                            iterator_type, "operator++");
                        compare_method = cxx_range_find_method(
                            iterator_type, "operator!=");
                        dereference_method = cxx_range_find_method(
                            iterator_type, "operator*");
                        if (!increment_method || !compare_method ||
                            !dereference_method) {
                            rcc_error(loc,
                                      "RinOS range-for iterator requires public operator++, operator!=, and operator* members");
                            range_invalid = true;
                        } else {
                            iterator_range = true;
                            element_type = cxx_range_value_type(
                                dereference_method->return_type);
                            if (!element_type ||
                                !type_is_complete(element_type)) {
                                rcc_error(loc,
                                          "RinOS range-for operator* must return a complete object type");
                                iterator_range = false;
                                range_invalid = true;
                            }
                        }
                    }
                }
            }
        } else {
            rcc_error(loc,
                      "RinOS range-for requires a complete array or supported iterator range expression");
            range_invalid = true;
        }
    } else {
        if (auto_reference && range_type && range_type->base) {
            if (auto_rvalue_reference && auto_const) {
                rcc_error(loc,
                          "const auto&& range variable cannot bind to an array lvalue");
            }
            Type* referred_type = range_type->base;
            if (auto_const) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *referred_type;
                qualified->is_const = true;
                referred_type = qualified;
            }
            item_type = type_ptr(referred_type);
            item_type->is_reference = true;
        }
    }

    if (range_invalid && !iterator_range) {
        /* Keep parsing the loop body after the required diagnostic, but do not
         * lower an unsupported range as an array or emit an invented loop. */
        original_body = parse_cxx_statement();
        return original_body ? original_body : stmt_null(loc);
    }

    range_storage = range;
    range_id = ++cxx_range_for_counter;
    if (iterator_range) {
        written = snprintf(range_name, sizeof(range_name),
                           "__rcc_range_object_%u", range_id);
        if (written < 0 || (size_t)written >= sizeof(range_name)) {
            rcc_error(loc, "range-for storage name exceeds compiler limits");
            range_name[0] = '\0';
        }
        range_decl = decl_var(rcc_intern(range_name), range_type,
                              range, loc);
        range_decl->var_is_auto = true;
        range_decl->var_is_auto_reference = true;
        range_decl->var_is_auto_rvalue_reference = true;
        rcc_parser_cxx_add_value_binding(range_decl->name, range_type);
        range_storage = expr_ident(range_decl->name, loc);
    } else if (range_type && range_type->kind == TYPE_ARRAY &&
        range_type->array_len >= 0 && range_type->base) {
        written = snprintf(range_name, sizeof(range_name),
                           "__rcc_range_base_%u", range_id);
        if (written < 0 || (size_t)written >= sizeof(range_name)) {
            rcc_error(loc, "range-for storage name exceeds compiler limits");
            range_name[0] = '\0';
        }
        if (range->kind == EXPR_COMPOUND &&
            range->compound_type == range_type) {
            /* Braced range-for initializers are prvalues, not addressable
             * source expressions. Give their backing array automatic storage
             * for the complete synthesized loop scope. */
            range_decl = decl_var(rcc_intern(range_name), range_type,
                                  range, loc);
            rcc_parser_cxx_add_value_binding(range_decl->name, range_type);
            range_storage = expr_ident(range_decl->name, loc);
        } else {
            range_decl = decl_var(rcc_intern(range_name), type_ptr(range_type),
                                  expr_unary(EXPR_ADDR, range, loc), loc);
            range_storage = expr_unary(
                EXPR_DEREF, expr_ident(range_decl->name, loc), loc);
        }
    }

    if (iterator_range) {
        written = snprintf(begin_name, sizeof(begin_name),
                           "__rcc_range_begin_%u", range_id);
        if (written < 0 || (size_t)written >= sizeof(begin_name)) {
            rcc_error(loc, "range-for iterator name exceeds compiler limits");
            begin_name[0] = '\0';
        }
        written = snprintf(end_name, sizeof(end_name),
                           "__rcc_range_end_%u", range_id);
        if (written < 0 || (size_t)written >= sizeof(end_name)) {
            rcc_error(loc, "range-for iterator name exceeds compiler limits");
            end_name[0] = '\0';
        }
        begin_expression = expr_call(
            expr_member(expr_ident(range_decl->name, loc), "begin", loc),
            NULL, loc);
        end_expression = expr_call(
            expr_member(expr_ident(range_decl->name, loc), "end", loc),
            NULL, loc);
        begin_decl = decl_var(rcc_intern(begin_name), iterator_type,
                              begin_expression, loc);
        end_decl = decl_var(rcc_intern(end_name), iterator_type,
                            end_expression, loc);
        rcc_parser_cxx_add_value_binding(begin_decl->name, iterator_type);
        rcc_parser_cxx_add_value_binding(end_decl->name, iterator_type);
        begin_identifier = expr_ident(begin_decl->name, loc);
        end_identifier = expr_ident(end_decl->name, loc);
        element_expression = expr_unary(
            EXPR_DEREF, expr_ident(begin_decl->name, loc), loc);
        if (auto_reference) {
            Type* referred_type = element_type;
            if (auto_const) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *referred_type;
                qualified->is_const = true;
                referred_type = qualified;
            }
            item_type = type_ptr(referred_type);
            item_type->is_reference = true;
            item_type->is_rvalue_reference = auto_rvalue_reference;
        } else if (is_auto) {
            item_type = element_type;
        }
        item_decl = decl_var(
            item_name ? item_name : rcc_intern("__rcc_range_item"),
            item_type, element_expression, loc);
        item_decl->var_is_auto = is_auto && !auto_reference;
        rcc_parser_cxx_add_value_binding(
            item_decl->name, item_type ? item_type : element_type);
        condition = expr_binary(EXPR_NE, begin_identifier,
                                end_identifier, loc);
        increment = expr_unary(
            EXPR_PREINC, expr_ident(begin_decl->name, loc), loc);
        original_body = parse_cxx_statement();
        stmtlist_append(&body_statements, stmt_decl(item_decl, loc));
        if (original_body) stmtlist_append(&body_statements, original_body);
        loop = stmt_for(NULL, condition, increment,
                        stmt_block(body_statements, loc), loc);
        stmtlist_append(&outer_statements, stmt_decl(range_decl, loc));
        stmtlist_append(&outer_statements, stmt_decl(begin_decl, loc));
        stmtlist_append(&outer_statements, stmt_decl(end_decl, loc));
        stmtlist_append(&outer_statements, loop);
        return stmt_block(outer_statements, loc);
    }

    written = snprintf(index_name, sizeof(index_name),
                       "__rcc_range_index_%u", range_id);
    if (written < 0 || (size_t)written >= sizeof(index_name)) {
        rcc_error(loc, "range-for index name exceeds compiler limits");
        index_name[0] = '\0';
    }
    index_decl = decl_var(rcc_intern(index_name), type_int,
                          expr_int(0, loc), loc);
    index_expression = expr_ident(index_decl->name, loc);
    element_expression = expr_index(range_storage, index_expression, loc);
    item_decl = decl_var(item_name ? item_name : rcc_intern("__rcc_range_item"),
                         item_type, element_expression, loc);
    item_decl->var_is_auto = is_auto && !auto_reference;
    rcc_parser_cxx_add_value_binding(item_decl->name,
                                     item_type ? item_type : type_int);

    count_expression = expr_binary(
        EXPR_DIV, expr_sizeof_expr(range_storage, loc),
        expr_sizeof_expr(expr_index(range_storage, expr_int(0, loc), loc), loc),
        loc);
    condition = expr_binary(EXPR_LT, expr_ident(index_decl->name, loc),
                            count_expression, loc);
    increment = expr_unary(EXPR_PREINC,
                           expr_ident(index_decl->name, loc), loc);

    original_body = parse_cxx_statement();
    stmtlist_append(&body_statements, stmt_decl(item_decl, loc));
    if (original_body) stmtlist_append(&body_statements, original_body);
    loop = stmt_for(stmt_decl(index_decl, loc), condition, increment,
                    stmt_block(body_statements, loc), loc);
    if (!range_decl) return loop;
    stmtlist_append(&outer_statements, stmt_decl(range_decl, loc));
    stmtlist_append(&outer_statements, loop);
    return stmt_block(outer_statements, loc);
}

static Stmt* parse_cxx_dependent_local_declaration(void) {
    SourceLoc loc = peek()->loc;
    Type* base_type = NULL;
    Token* name;
    StmtList* declarations = NULL;
    bool is_decltype_auto = check(TOK_DECLTYPE) && parser.cur->next &&
        parser.cur->next->type == TOK_LPAREN && parser.cur->next->next &&
        parser.cur->next->next->type == TOK_AUTO &&
        parser.cur->next->next->next &&
        parser.cur->next->next->next->type == TOK_RPAREN;
    bool is_auto_const = match(TOK_CONST);
    bool is_auto = match(TOK_AUTO);

    if (is_decltype_auto) {
        advance();
        advance();
        advance();
        advance();
    } else if (is_auto) {
    } else {
        base_type = parse_cxx_type_spec();
    }

    for (;;) {
        Type* type = base_type;
        Expr* initializer = NULL;
        bool direct_list_initializer = false;
        bool is_auto_reference = false;
        bool is_auto_rvalue_reference = false;
        bool is_auto_pointer = false;

        if (is_auto) {
            if (match(TOK_STAR)) {
                is_auto_pointer = true;
            } else if (match(TOK_AMP)) {
                is_auto_reference = true;
            } else if (match(TOK_AND)) {
                is_auto_reference = true;
                is_auto_rvalue_reference = true;
            }
        }
        name = expect(TOK_IDENT, "local variable name");
        if (!name) return NULL;
        if (match(TOK_ASSIGN)) {
            if (is_decltype_auto && check(TOK_LBRACE)) {
                rcc_error(peek()->loc,
                          "decltype(auto) variable requires an expression initializer");
                skip_balanced(TOK_LBRACE, TOK_RBRACE);
            } else if (check(TOK_LBRACE)) {
                if (check_next(TOK_RBRACE)) {
                    SourceLoc initializer_loc = peek()->loc;
                    advance();
                    advance();
                    initializer = expr_initializer_list(
                        exprlist_new(expr_int(0, initializer_loc)),
                        initializer_loc);
                    initializer->compound_type = type;
                    initializer->type = type;
                    initializer->compound_value_init = true;
                } else {
                    skip_balanced(TOK_LBRACE, TOK_RBRACE);
                }
            } else {
                /* A declaration initializer is an assignment-expression.
                 * Stop before the comma that begins the next declarator. */
                initializer = parse_assignment_expression();
            }
        } else if (is_decltype_auto && check(TOK_LBRACE)) {
            rcc_error(peek()->loc,
                      "decltype(auto) variable requires an expression initializer");
            skip_balanced(TOK_LBRACE, TOK_RBRACE);
        } else if (check(TOK_LBRACE)) {
            if (!is_auto && check_next(TOK_RBRACE)) {
                SourceLoc initializer_loc = peek()->loc;
                advance();
                advance();
                initializer = expr_initializer_list(
                    exprlist_new(expr_int(0, initializer_loc)),
                    initializer_loc);
                initializer->compound_type = type;
                initializer->type = type;
                initializer->compound_value_init = true;
            } else if (is_auto) {
                direct_list_initializer = true;
                initializer = rcc_parser_parse_initializer();
            } else {
                skip_balanced(TOK_LBRACE, TOK_RBRACE);
            }
        }
        if (direct_list_initializer) {
            ExprList* item = initializer && initializer->kind == EXPR_COMPOUND
                ? initializer->compound_init : NULL;
            if (!item || item->next || item->designator_kind !=
                    INIT_DESIGNATOR_NONE) {
                rcc_error(name->loc,
                          "direct-list auto initialization requires one initializer expression");
                initializer = NULL;
            } else {
                initializer = item->expr;
            }
        }
        if (is_auto && initializer) {
            type = initializer->type;
            if (!type && initializer->kind == EXPR_COMPOUND) {
                type = initializer->compound_type;
            }
        }

        {
            Decl* declaration = decl_var(name->value.str_val, type,
                                          initializer, loc);
            declaration->var_is_auto = is_auto;
            declaration->var_is_decltype_auto = is_decltype_auto;
            declaration->var_is_auto_reference = is_auto_reference;
            declaration->var_is_auto_rvalue_reference = is_auto_rvalue_reference;
            declaration->var_is_auto_pointer = is_auto_pointer;
            declaration->var_is_auto_const = is_auto_const;
            rcc_parser_cxx_add_value_binding(declaration->name,
                                             declaration->type);
            stmtlist_append(&declarations, stmt_decl(declaration, loc));
        }
        if (!match(TOK_COMMA)) break;
    }

    expect(TOK_SEMICOLON, ";");
    if (!declarations) return NULL;
    if (!declarations->next) return declarations->stmt;
    {
        Stmt* result = stmt_block(declarations, loc);
        result->block_no_scope = true;
        return result;
    }
}

Stmt* rcc_parse_cxx_auto_local_declaration(void) {
    bool decltype_auto = check(TOK_DECLTYPE) && parser.cur->next &&
        parser.cur->next->type == TOK_LPAREN && parser.cur->next->next &&
        parser.cur->next->next->type == TOK_AUTO &&
        parser.cur->next->next->next &&
        parser.cur->next->next->next->type == TOK_RPAREN;
    if (!check(TOK_AUTO) && !(check(TOK_CONST) && parser.cur->next &&
                              parser.cur->next->type == TOK_AUTO) &&
        !decltype_auto) {
        return NULL;
    }
    if (cxx_structured_binding_starts()) {
        return parse_cxx_structured_binding_declaration();
    }
    return parse_cxx_dependent_local_declaration();
}

Stmt* rcc_parse_cxx_class_local_declaration(Type* base_type,
                                            int storage, bool is_inline,
                                            bool is_constexpr,
                                            bool is_constinit,
                                            bool is_thread_local,
                                            SourceLoc loc) {
    Token* name;
    ExprList* arguments = NULL;
    Expr* initializer;
    Decl* declaration;
    uint32_t constructor_mask;
    bool aggregate_type;
    bool paren_form;
    bool paren_has_arguments;
    bool expression_paren_initializer;
    bool deferred_template_class_initializer;
    bool is_ctad_placeholder = base_type && base_type->cxx_dependent &&
        base_type->cxx_template && base_type->cxx_template_param_index < 0 &&
        base_type->cxx_template_arg_count == 0;
    bool brace_form;

    constructor_mask = base_type
        ? rcc_parser_cxx_constructor_arity_mask(base_type) : 0u;
    aggregate_type = cxx_type_is_aggregate(base_type);
    paren_form = check_next(TOK_LPAREN);
    paren_has_arguments = paren_form && parser.cur->next->next &&
        parser.cur->next->next->type != TOK_RPAREN;
    expression_paren_initializer = paren_form && paren_has_arguments &&
        !cxx_paren_looks_like_function_parameters();
    deferred_template_class_initializer =
        active_template && active_template->kind == TMPL_FUNCTION &&
        base_type && base_type->cxx_class && expression_paren_initializer;

    /* Preserve expression-shaped initialization of a local class while its
     * function-template body is still a pattern.  Its concrete layout and
     * constructor set do not exist yet, so arity verification must happen
     * after specialization.  Outside that deferred case, only constructors
     * already proven ABI-safe and C++20 aggregate paren initialization are
     * consumed here; `T value();` remains a function declaration. */
    if (!base_type || (base_type->kind != TYPE_STRUCT &&
                       base_type->kind != TYPE_UNION) ||
        (!is_ctad_placeholder && !deferred_template_class_initializer &&
         (!type_is_complete(base_type) ||
          (constructor_mask == 0u &&
           !(aggregate_type && (check_next(TOK_LBRACE) ||
                                (paren_form && paren_has_arguments)))))) ||
        !check(TOK_IDENT) ||
        (!check_next(TOK_LPAREN) && !check_next(TOK_LBRACE))) {
        return NULL;
    }

    if (paren_form && paren_has_arguments && constructor_mask == 0u &&
        cxx_paren_looks_like_function_parameters()) {
        /* The common declarator parser must own declarations such as
         * `Pair make_pair(int first, int second);`; otherwise C++20 aggregate
         * parenthesized initialization would steal the function signature. */
        return NULL;
    }

    if (aggregate_type && paren_form && paren_has_arguments &&
        !rcc_parser_cxx_standard_at_least(20)) {
        rcc_error(parser.cur->next->loc,
                  "C++20 aggregate parenthesized initialization requires C++20 or newer");
    }

    name = advance();
    brace_form = check(TOK_LBRACE);
    if (brace_form && parser.cur->next &&
        (parser.cur->next->type == TOK_DOT ||
         parser.cur->next->type == TOK_LBRACKET)) {
        /* Use the shared initializer parser for list initialization.  The
         * direct constructor path used to parse every clause as an
         * assignment expression, which rejected standard C++20 designated
         * initializers such as `T value{.field = 1}` before sema could apply
         * the bounded aggregate checks. */
        initializer = rcc_parser_parse_initializer();
        arguments = initializer ? initializer->compound_init : NULL;
    } else {
        brace_form = match(TOK_LBRACE);
        if (!brace_form) advance(); /* `(` */
        if ((!brace_form && !check(TOK_RPAREN)) ||
            (brace_form && !check(TOK_RBRACE))) {
            do {
                Expr* argument = brace_form && check(TOK_LBRACE)
                    ? rcc_parser_parse_initializer()
                    : parse_assignment_expression();
                exprlist_append(&arguments, argument);
            } while (match(TOK_COMMA));
        }
        expect(brace_form ? TOK_RBRACE : TOK_RPAREN,
               brace_form ? "}" : ")");
        initializer = expr_initializer_list(arguments, loc);
    }

    if (is_ctad_placeholder) {
        base_type = deduce_class_template_from_constructor(
            base_type->cxx_template, arguments, loc, brace_form);
        if (!base_type) {
            expect(TOK_SEMICOLON, ";");
            return stmt_null(loc);
        }
        if (rcc_parser_cxx_constructor_arity_mask(base_type) == 0u &&
            !brace_form && !rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(loc,
                      "aggregate class template argument deduction requires "
                      "braced initialization before C++20");
            expect(TOK_SEMICOLON, ";");
            return stmt_null(loc);
        }
    }

    initializer->compound_type = base_type;
    initializer->compound_paren_init = !brace_form;
    rcc_parser_validate_cxx_object_type(base_type, loc);
    rcc_parser_validate_cxx_constructor_initializer(base_type, initializer);
    expect(TOK_SEMICOLON, ";");

    declaration = decl_var(name->value.str_val, base_type, initializer, loc);
    declaration->storage = (StorageClass)storage;
    declaration->var_is_inline = is_inline;
    declaration->var_is_constexpr = is_constexpr;
    declaration->var_is_constinit = is_constinit;
    declaration->var_is_thread_local = is_thread_local;
    rcc_parser_cxx_add_value_binding(declaration->name, declaration->type);
    return stmt_decl(declaration, loc);
}

static bool cxx_identifier_is(const Token* token, const char* spelling) {
    return token && token->type == TOK_IDENT && token->value.str_val &&
        spelling && strcmp(token->value.str_val, spelling) == 0;
}

static void cxx_skip_unsupported_statement(void) {
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    while (!at_end()) {
        if (check(TOK_SEMICOLON) && paren_depth == 0 &&
            bracket_depth == 0 && brace_depth == 0) {
            advance();
            return;
        }
        if (check(TOK_LPAREN)) {
            ++paren_depth;
        } else if (check(TOK_RPAREN) && paren_depth > 0) {
            --paren_depth;
        } else if (check(TOK_LBRACKET)) {
            ++bracket_depth;
        } else if (check(TOK_RBRACKET) && bracket_depth > 0) {
            --bracket_depth;
        } else if (check(TOK_LBRACE)) {
            ++brace_depth;
        } else if (check(TOK_RBRACE)) {
            if (brace_depth == 0) return;
            --brace_depth;
        }
        advance();
    }
}

static const char* cxx_unsupported_coroutine_keyword(void) {
    if (!rcc_parser_cxx_standard_at_least(20)) return NULL;
    if (cxx_identifier_is(peek(), "co_await")) return "co_await";
    if (cxx_identifier_is(peek(), "co_yield")) return "co_yield";
    if (cxx_identifier_is(peek(), "co_return")) return "co_return";
    return NULL;
}

static bool cxx_unsupported_module_directive(void) {
    if (!rcc_parser_cxx_standard_at_least(20)) return false;
    if (cxx_identifier_is(peek(), "module") ||
        cxx_identifier_is(peek(), "import")) return true;
    return cxx_identifier_is(peek(), "export") && parser.cur->next &&
        (cxx_identifier_is(parser.cur->next, "module") ||
         cxx_identifier_is(parser.cur->next, "import"));
}

static bool cxx_local_class_definition_starts(void) {
    Token* cursor = parser.cur;
    Token* body;
    int brace_depth = 0;
    if (!cursor || (cursor->type != TOK_CLASS &&
                    cursor->type != TOK_STRUCT)) {
        return false;
    }
    cursor = cursor->next;
    if (!cursor || cursor->type != TOK_IDENT) return false;
    cursor = cursor->next;
    if (cursor && cursor->type == TOK_FINAL) cursor = cursor->next;
    while (cursor && cursor->type != TOK_LBRACE &&
           cursor->type != TOK_SEMICOLON) {
        cursor = cursor->next;
    }
    if (!cursor || cursor->type != TOK_LBRACE) return false;
    for (body = cursor; body; body = body->next) {
        if (body->type == TOK_LBRACE) {
            ++brace_depth;
        } else if (body->type == TOK_RBRACE && --brace_depth == 0) {
            body = body->next;
            return body && (body->type == TOK_SEMICOLON ||
                            body->type == TOK_RBRACE);
        }
    }
    return false;
}

static Stmt* parse_cxx_local_class_definition(void) {
    SourceLoc loc = peek()->loc;
    bool is_struct = match(TOK_STRUCT);
    Token* name;
    const char* local_identity;
    CxxClass* local_class;
    CxxTemplate* enclosing_template = active_template;
    CxxTemplate* local_template = NULL;
    if (!is_struct) expect(TOK_CLASS, "class");
    name = expect(TOK_IDENT, "local class name");
    if (!name) {
        return stmt_null(loc);
    }
    local_identity = rcc_parser_new_local_type_identity();

    /* Install the local class's substitution environment before parsing its
     * members.  Member-function bodies can themselves declare local classes;
     * those nested definitions must belong to this class specialization,
     * rather than the enclosing function template's flat list. */
    if (enclosing_template &&
        (enclosing_template->kind == TMPL_FUNCTION ||
         enclosing_template->kind == TMPL_CLASS)) {
        char template_name[128];
        int written = snprintf(template_name, sizeof(template_name),
                               "__rcc_local_class_%s", local_identity);
        if (written < 0 || (size_t)written >= sizeof(template_name)) {
            rcc_error(loc, "local class template identity is too long");
            return stmt_null(loc);
        }
        local_template = cxx_template_new(loc);
        local_template->name = rcc_intern(template_name);
        local_template->kind = TMPL_CLASS;
        local_template->is_local_class_template = true;
        local_template->ns = enclosing_template->ns
            ? enclosing_template->ns
            : (active_namespace ? active_namespace : g_global_namespace);
        local_template->param_count = enclosing_template->param_count;
        if (local_template->param_count > 0) {
            local_template->params = ast_arena_alloc(
                sizeof(local_template->params[0]) *
                (size_t)local_template->param_count);
            memcpy(local_template->params, enclosing_template->params,
                   sizeof(local_template->params[0]) *
                   (size_t)local_template->param_count);
        }
        active_template = local_template;
    }
    local_class = parse_cxx_class_named(
        loc, is_struct, name->value.str_val, local_identity);
    active_template = enclosing_template;

    /* A local class in a template is a distinct class in every enclosing
     * specialization.  Keep its dependent definition as a private class
     * template; template substitution materializes the class only when the
     * containing function or class specialization is cloned. */
    if (enclosing_template && local_template && local_class) {
        local_template->templated_class = local_class;
        local_template->class_def = local_class;
        local_template->local_class_pattern = local_class;
        enclosing_template->local_classes = ast_arena_grow(
            enclosing_template->local_classes,
            sizeof(enclosing_template->local_classes[0]) *
                (size_t)enclosing_template->local_class_count,
            sizeof(enclosing_template->local_classes[0]) *
                (size_t)(enclosing_template->local_class_count + 1));
        enclosing_template->local_classes[
            enclosing_template->local_class_count].pattern = local_class;
        enclosing_template->local_classes[
            enclosing_template->local_class_count].templ = local_template;
        ++enclosing_template->local_class_count;
    }
    return stmt_null(loc);
}

static Stmt* parse_cxx_statement(void) {
    const char* coroutine_keyword = cxx_unsupported_coroutine_keyword();
    if (coroutine_keyword) {
        SourceLoc loc = peek()->loc;
        rcc_error(loc,
                  "C++20 coroutine keyword '%s' is not supported by RCC++",
                  coroutine_keyword);
        cxx_skip_unsupported_statement();
        return stmt_null(loc);
    }

    if (cxx_local_class_definition_starts()) {
        return parse_cxx_local_class_definition();
    }

    if (check(TOK_CONSTEXPR)) return parse_declaration();

    if (match(TOK_USING)) {
        SourceLoc loc = previous()->loc;
        if (check(TOK_ENUM)) {
            parse_cxx_using(active_namespace ? active_namespace
                                             : g_global_namespace);
            return stmt_null(loc);
        }
        if (check(TOK_IDENT) && check_next(TOK_ASSIGN)) {
            parse_cxx_using(active_namespace ? active_namespace
                                             : g_global_namespace);
            return stmt_null(loc);
        }
        parse_cxx_local_using();
        return stmt_null(loc);
    }

    if (cxx_structured_binding_starts()) {
        return parse_cxx_structured_binding_declaration();
    }

    if (check(TOK_TYPENAME) || check(TOK_AUTO) ||
        (check(TOK_DECLTYPE) && parser.cur->next &&
         parser.cur->next->type == TOK_LPAREN && parser.cur->next->next &&
         parser.cur->next->next->type == TOK_AUTO &&
         parser.cur->next->next->next &&
         parser.cur->next->next->next->type == TOK_RPAREN) ||
        (check(TOK_IDENT) &&
         is_active_template_type(peek()->value.str_val))) {
        return parse_cxx_dependent_local_declaration();
    }

    /* try-catch */
    if (match(TOK_TRY)) {
        SourceLoc loc = previous()->loc;
        Stmt* try_body;
        CxxCatch* catches = NULL;
        CxxCatch** catch_tail = &catches;

        /* Parse try block */
        expect(TOK_LBRACE, "{");
        StmtList* stmts = NULL;
        void* try_enum_scope = rcc_parser_enum_scope_mark();
        while (!check(TOK_RBRACE) && !at_end()) {
            Stmt* s = parse_cxx_statement();
            if (s) stmtlist_append(&stmts, s);
        }
        expect(TOK_RBRACE, "}");
        rcc_parser_enum_scope_restore(try_enum_scope);
        try_body = stmt_block(stmts, loc);

        /* Parse catch blocks */
        while (match(TOK_CATCH)) {
            CxxCatch* handler = ast_arena_alloc(sizeof(*handler));
            StmtList* handler_stmts = NULL;
            SourceLoc handler_loc = previous()->loc;
            Type* handler_type = NULL;
            const char* handler_name = NULL;
            bool is_ellipsis = false;

            memset(handler, 0, sizeof(*handler));
            expect(TOK_LPAREN, "(");
            if (!check(TOK_ELLIPSIS)) {
                handler_type = parse_cxx_type_spec();
                if (check(TOK_IDENT)) handler_name = advance()->value.str_val;
            } else {
                advance();  /* ... */
                is_ellipsis = true;
            }
            expect(TOK_RPAREN, ")");

            expect(TOK_LBRACE, "{");
            void* catch_enum_scope = rcc_parser_enum_scope_mark();
            while (!check(TOK_RBRACE) && !at_end()) {
                Stmt* s = parse_cxx_statement();
                if (s) stmtlist_append(&handler_stmts, s);
            }
            expect(TOK_RBRACE, "}");
            rcc_parser_enum_scope_restore(catch_enum_scope);

            /* Make the catch parameter a normal block declaration so lookup,
             * stack layout, and template cloning all use the existing paths. */
            if (handler_name && handler_type) {
                handler->parameter = decl_var(handler_name, handler_type,
                                               NULL, handler_loc);
                {
                    StmtList* parameter = ast_arena_alloc(sizeof(*parameter));
                    parameter->stmt = stmt_decl(handler->parameter,
                                                handler_loc);
                    parameter->next = handler_stmts;
                    handler_stmts = parameter;
                }
            }
            handler->type = handler_type;
            handler->name = handler_name;
            handler->is_ellipsis = is_ellipsis;
            handler->body = stmt_block(handler_stmts, handler_loc);
            handler->next = NULL;
            *catch_tail = handler;
            catch_tail = &handler->next;
        }

        return stmt_try(try_body, catches, loc);
    }

    /* throw */
    if (match(TOK_THROW)) {
        SourceLoc loc = previous()->loc;
        Expr* expression = check(TOK_SEMICOLON) ? NULL
                                               : parse_cxx_expression();
        expect(TOK_SEMICOLON, ";");
        return stmt_throw(expression, loc);
    }

    /* Fall back to C statement parsing */
    return parse_declaration();
}

Stmt* rcc_parse_cxx_statement(void) {
    return parse_cxx_statement();
}

static void add_cxx_declaration_one(AST* ast, Stmt* statement,
                                    bool c_language_linkage,
                                    bool nodiscard,
                                    bool weak,
                                    bool deprecated,
                                    const char* deprecated_message,
                                    CxxNamespace* ns) {
    if (statement && statement->kind == STMT_DECL) {
        Decl* declaration = statement->decl;
        if (declaration->kind == DECL_FUNC && nodiscard) {
            declaration->func_is_nodiscard = true;
        }
        if (weak) declaration->is_weak = true;
        if (deprecated && declaration->kind == DECL_FUNC) {
            declaration->func_is_deprecated = true;
            declaration->func_deprecated_message = deprecated_message;
        } else if (deprecated && declaration->kind == DECL_VAR) {
            declaration->var_is_deprecated = true;
            declaration->var_deprecated_message = deprecated_message;
        }
        if (ns && ns != g_global_namespace &&
            declaration->kind != DECL_STATIC_ASSERT && declaration->name) {
            const char* source_name = declaration->name;
            set_cxx_link_name(declaration, ns, c_language_linkage);
            if (c_language_linkage &&
                (declaration->kind == DECL_FUNC ||
                 declaration->kind == DECL_VAR) &&
                !declaration->link_name) {
                declaration->link_name = rcc_intern(source_name);
            }
            if (declaration->kind == DECL_FUNC) {
                declaration->func_cxx_namespace =
                    cxx_namespace_qualified_name(ns);
                declaration->func_cxx_namespace_scope = ns;
            }
            declaration->name = namespace_qualified_decl_name(
                ns, source_name, declaration->loc);
            cxx_namespace_add_decl(ns, declaration);
        } else {
            /* Keep the established global-scope linkage behavior unchanged. */
            set_cxx_link_name(declaration, NULL, c_language_linkage);
            if (ns == g_global_namespace &&
                declaration->kind == DECL_FUNC) {
                /* Global functions are direct members of the global
                 * namespace too.  Retain them there for second-phase ADL,
                 * including declarations parsed after a template body. */
                cxx_namespace_add_decl(ns, declaration);
            }
        }
        ast_add_decl(ast, declaration);
    }
}

static void add_cxx_declaration(AST* ast, Stmt* statement,
                                bool c_language_linkage,
                                CxxNamespace* ns) {
    bool nodiscard = take_cxx_nodiscard();
    bool weak = take_cxx_weak();
    bool no_unique_address = take_cxx_no_unique_address();
    const char* deprecated_message = NULL;
    bool deprecated = take_cxx_deprecated(&deprecated_message);
    if (!deprecated) deprecated_message = NULL;
    if (no_unique_address) {
        rcc_error(statement && statement->loc.filename ? statement->loc
                                                   : (SourceLoc){"<declaration>", 0, 0},
                  "[[no_unique_address]] requires a class data member");
    }

    if (statement && statement->kind == STMT_BLOCK &&
        statement->block_no_scope) {
        for (StmtList* item = statement->block_stmts; item; item = item->next) {
            add_cxx_declaration_one(ast, item->stmt, c_language_linkage,
                                    nodiscard, weak, deprecated,
                                    deprecated_message, ns);
        }
        return;
    }
    add_cxx_declaration_one(ast, statement, c_language_linkage, nodiscard,
                            weak, deprecated, deprecated_message, ns);
}

/* Preserve C ABI symbol spelling inside extern "C" while extern "C++" and
 * ordinary declarations use Itanium ABI link names. */
static void parse_cxx_language_linkage(AST* ast, CxxNamespace* ns) {
    SourceLoc loc = peek()->loc;
    Token* language;
    bool c_language_linkage;
    advance(); /* extern */
    language = expect(TOK_STRING_LIT, "language linkage string");
    if (!language) return;
    if (strcmp(language->value.str_val, "C") != 0 &&
        strcmp(language->value.str_val, "C++") != 0) {
        rcc_error(loc, "unsupported language linkage '%s'",
                  language->value.str_val);
    }
    c_language_linkage = strcmp(language->value.str_val, "C") == 0;
    if (match(TOK_LBRACE)) {
        while (!check(TOK_RBRACE) && !at_end()) {
            Token* start = parser.cur;
            if (match(TOK_CLASS) || match(TOK_STRUCT)) {
                CxxClass* cls = parse_cxx_class();
                (void)take_cxx_nodiscard();
                if (take_cxx_weak()) {
                    rcc_error(loc,
                              "[[gnu::weak]] requires a file-scope declaration");
                }
                (void)take_cxx_deprecated(NULL);
                if (take_cxx_no_unique_address()) {
                    rcc_error(loc,
                              "[[no_unique_address]] requires a class data member");
                }
                if (ns) {
                    cxx_namespace_add_class(ns, cls);
                }
            } else if ((check(TOK_AUTO) && parser.cur->next &&
                 parser.cur->next->type == TOK_IDENT &&
                 parser.cur->next->next &&
                 parser.cur->next->next->type == TOK_LPAREN) ||
                cxx_decltype_auto_starts_function()) {
                bool is_constexpr = false;
                bool is_noexcept = false;
                bool is_consteval = false;
                Decl* declaration = parse_cxx_function_declaration(
                    true, &is_constexpr, &is_noexcept, &is_consteval);
                if (declaration) {
                    add_cxx_declaration(
                        ast, stmt_decl(declaration, declaration->loc),
                        c_language_linkage, ns);
                }
            } else {
                add_cxx_declaration(ast, parse_cxx_statement(),
                                    c_language_linkage, ns);
            }
            if (parser.cur == start && !at_end()) advance();
        }
        expect(TOK_RBRACE, "}");
        return;
    }
    if ((check(TOK_AUTO) && parser.cur->next &&
         parser.cur->next->type == TOK_IDENT && parser.cur->next->next &&
         parser.cur->next->next->type == TOK_LPAREN) ||
        cxx_decltype_auto_starts_function()) {
        bool is_constexpr = false;
        bool is_noexcept = false;
        bool is_consteval = false;
        Decl* declaration = parse_cxx_function_declaration(
            true, &is_constexpr, &is_noexcept, &is_consteval);
        if (declaration) {
            add_cxx_declaration(
                ast, stmt_decl(declaration, declaration->loc),
                c_language_linkage, ns);
        }
    } else {
        add_cxx_declaration(ast, parse_cxx_statement(), c_language_linkage,
                            ns);
    }
}

/* ═══════════════════════════════════════
 * C++ Top-level Parsing
 * ═══════════════════════════════════════ */

/* C++20 abbreviated function templates are ordinary function declarations
 * whose parameter list contains one or more `auto` placeholders.  Detect the
 * shape before the common C parser claims the declaration, then route it
 * through the existing function-template substitution path. */
static bool cxx_abbreviated_function_starts(void) {
    Token* token = parser.cur;
    int paren_depth = 0;
    bool saw_parameter_list = false;

    /* `decltype(auto)` contains a parenthesized AUTO token in its type
     * spelling.  It is not an abbreviated function parameter list; skip that
     * prefix before looking for the declaration's actual parameter list. */
    if (token && token->type == TOK_DECLTYPE && token->next &&
        token->next->type == TOK_LPAREN && token->next->next &&
        token->next->next->type == TOK_AUTO && token->next->next->next &&
        token->next->next->next->type == TOK_RPAREN) {
        token = token->next->next->next->next;
    }
    for (; token && token->type != TOK_EOF; token = token->next) {
        if (paren_depth == 0 &&
            (token->type == TOK_ASSIGN || token->type == TOK_SEMICOLON ||
             token->type == TOK_LBRACE)) {
            return false;
        }
        if (token->type == TOK_LPAREN) {
            ++paren_depth;
            if (paren_depth == 1) saw_parameter_list = true;
            continue;
        }
        if (token->type == TOK_RPAREN) {
            if (paren_depth == 0) return false;
            --paren_depth;
            if (saw_parameter_list && paren_depth == 0) return false;
            continue;
        }
        if (!saw_parameter_list || paren_depth != 1) continue;
        if (token->type == TOK_AUTO ||
            (token->type == TOK_CONST && token->next &&
             token->next->type == TOK_AUTO)) {
            return true;
        }
        if (token->type == TOK_SEMICOLON || token->type == TOK_LBRACE) {
            return false;
        }
    }
    return false;
}

/* Parse C++ translation unit */
AST* rcc_parse_cxx(TokenList* tokens) {
    rcc_parser_set_cxx_mode(true);
    rcc_parser_reset_type_scopes();
    rcc_parser_initialize_builtin_va_list_type();
    parser.cur = tokens->head;
    parser.prev = NULL;
    pending_cxx_nodiscard = false;
    pending_cxx_deprecated = false;
    pending_cxx_no_unique_address = false;
    pending_cxx_weak = false;
    pending_cxx_deprecated_message = NULL;
    cxx_standard_feature_tokens_valid(tokens->head);

    AST* ast = ast_new();
    active_ast = ast;

    while (!at_end()) {
        Token* declaration_start = parser.cur;
        SourceLoc loc = peek()->loc;
        skip_cxx_attributes();

        if (cxx_leading_alignas_class_starts()) {
            SourceLoc alignment_loc = peek()->loc;
            int explicit_alignment = 0;
            while (check(TOK__ALIGNAS)) {
                int alignment = rcc_parser_parse_explicit_alignment();
                if (alignment > explicit_alignment) {
                    explicit_alignment = alignment;
                }
            }
            if (match(TOK_CLASS) || match(TOK_STRUCT)) {
                CxxClass* cls = parse_cxx_class();
                cxx_class_apply_explicit_alignment(
                    cls, explicit_alignment, alignment_loc);
                (void)take_cxx_nodiscard();
                if (take_cxx_weak()) {
                    rcc_error(loc, "[[gnu::weak]] requires a file-scope declaration");
                }
                (void)take_cxx_deprecated(NULL);
                if (take_cxx_no_unique_address()) {
                    rcc_error(loc,
                              "[[no_unique_address]] requires a class data member");
                }
                if (g_global_namespace) {
                    cxx_namespace_add_class(g_global_namespace, cls);
                }
            }
        } else if (match(TOK_PRAGMA_PACK)) {
            rcc_parser_apply_pragma_pack(previous());
        } else if (check(TOK_EXTERN) && parser.cur->next &&
               parser.cur->next->type == TOK_STRING_LIT) {
            parse_cxx_language_linkage(ast, g_global_namespace);
        } else if (check(TOK_INLINE) && check_next(TOK_NAMESPACE)) {
            advance();
            advance();
            (void)parse_cxx_namespace(ast, g_global_namespace, true);
        } else if (match(TOK_NAMESPACE)) {
            (void)parse_cxx_namespace(ast, g_global_namespace, false);
        } else if (match(TOK_TEMPLATE)) {
            CxxTemplate* tmpl = parse_cxx_template();
            if (g_global_namespace && tmpl &&
                tmpl->kind != TMPL_DEDUCTION_GUIDE) {
                cxx_namespace_add_template(g_global_namespace, tmpl);
            }
        } else if (rcc_parse_cxx_deduction_guide()) {
        } else if (match(TOK_CLASS) || match(TOK_STRUCT)) {
            CxxClass* cls = parse_cxx_class();
            (void)take_cxx_nodiscard();
            if (take_cxx_weak()) {
                rcc_error(loc, "[[gnu::weak]] requires a file-scope declaration");
            }
            (void)take_cxx_deprecated(NULL);
            if (take_cxx_no_unique_address()) {
                rcc_error(loc,
                          "[[no_unique_address]] requires a class data member");
            }
            /* Class is stored in global namespace */
            if (g_global_namespace) {
                cxx_namespace_add_class(g_global_namespace, cls);
            }
            (void)loc;
        } else if (cxx_abbreviated_function_starts()) {
            CxxTemplate* abbreviated = cxx_template_new(loc);
            CxxTemplate* outer_template = active_template;
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration;

            abbreviated->kind = TMPL_FUNCTION;
            abbreviated->ns = g_global_namespace;
            active_template = abbreviated;
            declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            active_template = outer_template;
            if (declaration) {
                abbreviated->name = declaration->name;
                abbreviated->func_def = declaration;
                abbreviated->is_constexpr = is_constexpr;
                abbreviated->is_noexcept = is_noexcept;
                cxx_namespace_add_template(g_global_namespace, abbreviated);
            }
        } else if ((check(TOK_CONSTEXPR) || check(TOK_CONSTEVAL)) &&
                   !cxx_constexpr_starts_function()) {
            Stmt* statement = parse_cxx_statement();
            if (statement && statement->kind == STMT_DECL) {
                add_cxx_declaration(ast, statement, false,
                                    g_global_namespace);
            }
        } else if ((check(TOK_AUTO) && parser.cur->next &&
                   parser.cur->next->type == TOK_IDENT &&
                   parser.cur->next->next &&
                   parser.cur->next->next->type == TOK_LPAREN) ||
                   cxx_decltype_auto_starts_function()) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            if (g_global_namespace && declaration) {
                add_namespace_declaration(ast, g_global_namespace,
                                          declaration);
            }
        } else if (check(TOK_CONSTEXPR) || check(TOK_CONSTEVAL) ||
                   cxx_inline_starts_function()) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            if (g_global_namespace && declaration) {
                add_namespace_declaration(ast, g_global_namespace,
                                           declaration);
            }
        } else if (cxx_unsupported_module_directive()) {
            rcc_error(loc,
                      "C++20 modules (module/import/export module) are not "
                      "supported by RCC++");
            cxx_skip_unsupported_statement();
        } else if (match(TOK_USING)) {
            parse_cxx_using(g_global_namespace);
        } else {
            /* Regular C declaration */
            Stmt* s = parse_cxx_statement();
            add_cxx_declaration(ast, s, false, g_global_namespace);
        }

        /* Individual declaration and scope parsers synchronize at their own
         * grammar boundary.  Only force one-token progress here; using the
         * cumulative error count would otherwise discard every declaration
         * following the first recovered error. */
        if (parser.cur == declaration_start && !at_end()) advance();
    }

    active_ast = NULL;
    return ast;
}
