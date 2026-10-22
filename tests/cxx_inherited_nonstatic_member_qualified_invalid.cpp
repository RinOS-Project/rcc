template<class T>
class QualifiedObjectRequiredBase {
public:
    T read(T value) const { return value; }
};

template<class T>
class QualifiedObjectRequiredDerived :
    public QualifiedObjectRequiredBase<T> {};

int main() {
    return QualifiedObjectRequiredDerived<int>::read(7);
}
