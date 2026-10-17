namespace NamespaceUsingTypeAmbiguity {
struct Collision {
    char outer[3];
};

namespace Imported {
struct Collision {
    char imported[7];
};
}

namespace Inner {
using namespace NamespaceUsingTypeAmbiguity::Imported;

int use_collision_type() {
    return sizeof(Collision);
}
}

}
