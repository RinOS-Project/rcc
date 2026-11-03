struct EmptyPrefix {};

struct DataPrefix {
    long long marker;
};

struct ReferenceBase {
    virtual int& value() = 0;
};

struct SideBase {
    virtual int side_value() = 0;
};

struct EmptyMarker {};

struct EmptyMarkerPrimary : EmptyMarker {
    virtual int value() { return 59; }
};

struct EmptyCarrierA : EmptyMarker {
    int first;
};

struct EmptyCarrierB : EmptyMarker {
    int second;
};

struct DuplicateEmptyBase : EmptyMarker, EmptyMarkerPrimary {
    long long payload;
};

struct EmptyBasePaths : EmptyMarker, EmptyCarrierA, EmptyCarrierB {
    long long tail;
};

struct EmptyBasePathOrder : EmptyCarrierA, EmptyMarker, EmptyCarrierB {
    long long tail;
};

struct ReferenceDerived : EmptyPrefix, DataPrefix, ReferenceBase, SideBase {
    int payload;

    int& value() override { return payload; }
    int side_value() override { return payload + 1; }
};

int main() {
    ReferenceDerived object{};
    object.marker = 41;
    object.payload = 43;

    ReferenceBase* reference_base = &object;
    SideBase* side_base = &object;
    EmptyPrefix* empty_prefix = &object;
    if (static_cast<void*>(empty_prefix) !=
        static_cast<void*>(reference_base)) return 4;
    if (sizeof(void*) == 8 && sizeof(ReferenceDerived) != 32) return 5;
    int& result = reference_base->value();
    if (&result != &object.payload || result != 43) return 1;
    result = 47;
    if (object.payload != 47 || object.marker != 41) return 2;
    if (side_base->side_value() != 48) return 3;

    DuplicateEmptyBase duplicate_empty{};
    EmptyMarkerPrimary* primary = &duplicate_empty;
    if (static_cast<void*>(primary) !=
        static_cast<void*>(&duplicate_empty)) return 6;
    if (sizeof(void*) == 8 && sizeof(DuplicateEmptyBase) != 16) return 7;

    EmptyBasePaths paths{};
    paths.first = 61;
    paths.second = 62;
    paths.tail = 63;
    if (paths.first != 61 || paths.second != 62 || paths.tail != 63) return 8;
    EmptyBasePathOrder ordered_paths{};
    ordered_paths.first = 71;
    ordered_paths.second = 72;
    ordered_paths.tail = 73;
    if (ordered_paths.first != 71 || ordered_paths.second != 72 ||
        ordered_paths.tail != 73) return 9;
    if (sizeof(void*) == 8 &&
        (sizeof(EmptyBasePaths) != 24 || sizeof(EmptyBasePathOrder) != 24)) {
        return 10;
    }
    return 0;
}
