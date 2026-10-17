namespace TransitiveTypeAmbiguity {
struct Collision {
    char outer[3];
};

namespace Source {
struct Collision {
    char imported[7];
};
}

namespace First {
using namespace TransitiveTypeAmbiguity::Source;
}

namespace Second {
using namespace TransitiveTypeAmbiguity::First;
}

namespace Consumer {
using namespace TransitiveTypeAmbiguity::Second;

int use_collision_type() {
    return sizeof(Collision);
}
}
}
