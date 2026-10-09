namespace api {
struct exposed {};
namespace target {}
namespace exposed = target;
}

int main() {
    return 0;
}
