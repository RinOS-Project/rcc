typedef unsigned int DebugMemberWord;
using DebugUsingWord = unsigned short;

namespace DebugAliasScope {
using NamespaceWord = unsigned long long;
}

class DebugMemberObject {
private:
    using PrivateWord = unsigned int;
    int secret : 4;
    int secret_value() const { return secret; }
protected:
    using ProtectedWord = unsigned long long;
    int protected_value;
    int protected_read() const { return protected_value; }
public:
    using PublicWord = unsigned short;
    int value;
    int read() const { return value; }
    static int create_value(int seed) { return seed + 1; }
};

struct DebugInheritanceLeft {
    int left;
};

struct DebugInheritanceRight {
    char right;
};

class DebugInheritanceDerived : public DebugInheritanceLeft,
                                private DebugInheritanceRight {
public:
    short own;
};

struct DebugVirtualInheritanceBase {
    int virtual_member;
};

struct DebugVirtualInheritanceOther {
    short other_member;
};

struct DebugVirtualInheritanceDerived : virtual DebugVirtualInheritanceBase,
                                        virtual DebugVirtualInheritanceOther {
    int direct;
};

struct DebugVirtualInheritanceLayer : DebugVirtualInheritanceDerived {
    short layer;
};

int debug_member_object_entry(DebugMemberObject* object) {
    return object->read() + DebugMemberObject::create_value(object->value);
}

DebugMemberWord debug_typedef_entry(DebugMemberWord value) {
    return value;
}

DebugUsingWord debug_using_alias_entry(DebugUsingWord value) {
    return value;
}

int debug_local_alias_entry(int value) {
    using LocalWord = unsigned short;
    LocalWord local_value = static_cast<LocalWord>(value);
    {
        using NestedWord = unsigned long long;
        NestedWord nested_value = static_cast<NestedWord>(local_value);
        value += static_cast<int>(nested_value);
    }
    return value + static_cast<int>(local_value);
}

template<typename T>
T debug_template_local_alias_entry(T value) {
    using TemplateWord = T;
    TemplateWord local_value = value;
    return local_value;
}

int debug_template_local_alias_caller(int value) {
    return static_cast<int>(debug_template_local_alias_entry<unsigned short>(
        static_cast<unsigned short>(value)));
}

int debug_inheritance_entry(DebugInheritanceDerived* object) {
    return object->left + object->own;
}

int debug_virtual_inheritance_entry(DebugVirtualInheritanceLayer* object) {
    return object->virtual_member + object->direct + object->layer;
}

int debug_reference_type_entry(int& lvalue, int&& rvalue) {
    int& local_lvalue = lvalue;
    return local_lvalue + rvalue;
}

extern "C" int debug_cxx_for_initializer_scope(int limit) {
    int outer_value = 0;
    for (int loop_index = 0; loop_index < limit; ++loop_index) {
        outer_value += loop_index;
    }
    return outer_value;
}
