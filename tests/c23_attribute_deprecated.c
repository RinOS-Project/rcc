[[deprecated("use replacement_function")]] int legacy_function(int value);
extern int legacy_object [[deprecated("use replacement_object")]];
extern int legacy_object_redeclared
    [[deprecated("use replacement_object_redeclared")]];
extern int legacy_object_redeclared;
enum LegacyKind {
    legacy_kind [[deprecated("use current_kind")]] = 4
};
struct LegacyRecord {
    int legacy_member [[deprecated("use current_member")]];
};
struct [[deprecated("use CurrentTag")]] LegacyTag {
    int value;
};
enum [[deprecated("use CurrentEnum")]] LegacyEnum {
    legacy_enum_value = 3
};
typedef int LegacyInteger [[deprecated("use CurrentInteger")]];
[[deprecated]] int legacy_without_message(void);
int legacy_redeclared_after(int value);
[[deprecated("use replacement_redeclared_after")]]
int legacy_redeclared_after(int value);
[[deprecated("use replacement_redeclared_before")]]
int legacy_redeclared_before(int value);
int legacy_redeclared_before(int value);

int replacement_function(int value) {
    return value + 1;
}

int deprecated_function_use(void) {
    return legacy_function(legacy_object + legacy_object_redeclared);
}

int deprecated_function_use_without_message(void) {
    return legacy_without_message();
}

int deprecated_enumerator_use(void) {
    return legacy_kind;
}

int deprecated_member_use(struct LegacyRecord* record) {
    return record->legacy_member;
}

int deprecated_parameter_use(
    int legacy_parameter [[deprecated("use current_parameter")]]) {
    return legacy_parameter;
}

int deprecated_tag_use(struct LegacyTag* record) {
    return record->value;
}

int deprecated_enum_tag_use(enum LegacyEnum value) {
    return (int)value;
}

int deprecated_typedef_use(LegacyInteger value) {
    return value;
}

int deprecated_redeclaration_use(void) {
    return legacy_redeclared_after(
        legacy_redeclared_before(2));
}
