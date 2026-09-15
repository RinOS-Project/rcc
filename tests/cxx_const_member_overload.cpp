class QualifierProbe {
public:
    int read() {
        return 11;
    }

    int read() const {
        return 22;
    }
};

int main() {
    QualifierProbe mutable_value;
    const QualifierProbe const_value{};
    return mutable_value.read() == 11 &&
                   const_value.read() == 22 ? 0 : 1;
}
