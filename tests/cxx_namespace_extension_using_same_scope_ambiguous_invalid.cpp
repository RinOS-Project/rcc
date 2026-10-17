namespace SameScopeUsingTypeAmbiguity {
namespace Imported {
struct ScopeCollision {
    char imported[7];
};
}

using namespace SameScopeUsingTypeAmbiguity::Imported;
struct ScopeCollision {
    char direct[3];
};

int use_collision_type() {
    return sizeof(ScopeCollision);
}
}
