struct AliasHolder {
    int value;
};

template<typename T> using Identity = T;
template<typename T> using Pointer = T*;

template<typename T>
struct Box {
    T value;
};

template<typename T> using BoxAlias = Box<T>;
template<int N> using IntArray = int[N];

struct ClassAliasTemplateOwner {
    template<typename T> using Identity = T;
    template<int N> using Array = int[N];
    using Local = Identity<int>;

protected:
    template<typename T> using ProtectedIdentity = T;

private:
    template<typename T> using PrivateIdentity = T;
};

struct ClassAliasTemplateDerived : ClassAliasTemplateOwner {
    using ProtectedValue =
        ClassAliasTemplateOwner::template ProtectedIdentity<int>;
};

namespace ClassAliasTemplateNamespace {
struct Owner {
    template<typename T> using Identity = T;
};
}

ClassAliasTemplateOwner::template Identity<long> class_alias_value = 47;
ClassAliasTemplateOwner::Identity<Box<int>> nested_class_alias_value =
    {61};
ClassAliasTemplateOwner::Identity<long> alias_without_template_keyword = 59;
typename ClassAliasTemplateNamespace::Owner::template Identity<int>
    namespace_class_alias_value = 53;
ClassAliasTemplateOwner::template Array<3> class_alias_array = {2, 3, 5};
ClassAliasTemplateOwner::Local class_alias_local_value = 7;
ClassAliasTemplateDerived::ProtectedValue protected_alias_value = 11;

int alias_identity(Identity<int> value) {
    return value + 2;
}

int alias_pointer(Pointer<int> value) {
    return *value;
}

int alias_class(BoxAlias<int>* value) {
    return value->value;
}

int main() {
    BoxAlias<int> box;
    box.value = 40;
    IntArray<3> values = {1, 2, 3};
    int value = 5;
    return alias_identity(3) == 5 &&
                   alias_pointer(&value) == 5 &&
                   alias_class(&box) == 40 &&
                   values[2] == 3 &&
                   class_alias_value == 47 &&
                   nested_class_alias_value.value == 61 &&
                   alias_without_template_keyword == 59 &&
                   namespace_class_alias_value == 53 &&
                   class_alias_array[2] == 5 &&
                   class_alias_local_value == 7 &&
                   protected_alias_value == 11
               ? 0 : 1;
}
