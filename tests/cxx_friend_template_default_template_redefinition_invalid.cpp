template<class T>
struct TemplateDefaultBox {};

template<class T, template<class> class Container = TemplateDefaultBox>
int duplicate_template_default(T value);

class FriendTemplateDefaultHost {
    template<class U, template<class> class Box>
    friend int duplicate_template_default(U value);
};

template<class V, template<class> class Value = TemplateDefaultBox>
int duplicate_template_default(V value) {
    return value;
}
