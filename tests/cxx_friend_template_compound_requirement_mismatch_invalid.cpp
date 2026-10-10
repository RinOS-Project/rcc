struct CompoundRequirementMismatchAggregate {
    int value;
};

template<class T>
requires requires { CompoundRequirementMismatchAggregate{1}; }
int compound_requirement_mismatch(T& host);

class CompoundRequirementMismatchHost {
    template<class U>
    requires requires { CompoundRequirementMismatchAggregate{1}; }
    friend int compound_requirement_mismatch(U& host);
};

template<class V>
requires requires { CompoundRequirementMismatchAggregate{2}; }
int compound_requirement_mismatch(V& host) {
    (void)host;
    return 47;
}

int main() {
    CompoundRequirementMismatchHost host;
    return compound_requirement_mismatch(host) == 47 ? 0 : 1;
}
