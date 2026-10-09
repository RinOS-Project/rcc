namespace api {
inline namespace v1 {
namespace exposed {}
}
namespace target {}
namespace exposed = target;
}

namespace api::exposed {}

int main() {
    return 0;
}
