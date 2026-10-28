[[deprecated("use replacement_function")]] int legacy_function(int value);
extern int legacy_object [[deprecated("use replacement_object")]];
extern int legacy_object_redeclared
    [[deprecated("use replacement_object_redeclared")]];
extern int legacy_object_redeclared;
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

int deprecated_redeclaration_use(void) {
    return legacy_redeclared_after(
        legacy_redeclared_before(2));
}
