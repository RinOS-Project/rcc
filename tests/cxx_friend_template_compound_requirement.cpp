struct CompoundRequirementAggregate {
    int value;
};

template<class T>
requires requires {
    CompoundRequirementAggregate{1};
    CompoundRequirementAggregate{.value = 1};
}
int compound_requirement(T& host);

class CompoundRequirementHost {
    template<class U>
    requires requires {
        CompoundRequirementAggregate{1};
        CompoundRequirementAggregate{.value = 1};
    }
    friend int compound_requirement(U& host);
};

template<class V>
requires requires {
    CompoundRequirementAggregate{1};
    CompoundRequirementAggregate{.value = 1};
}
int compound_requirement(V& host) {
    (void)host;
    return 43;
}

int main() {
    CompoundRequirementHost host;
    return compound_requirement(host) == 43 ? 0 : 1;
}
