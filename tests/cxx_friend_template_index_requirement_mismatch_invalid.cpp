class IndexedRequirementMismatchHost;

template<class T>
requires requires(T* candidate, int index) {
    candidate[index];
}
int indexed_requirement_mismatch(IndexedRequirementMismatchHost& host,
                                 T* values, int index);

class IndexedRequirementMismatchHost {
private:
    template<class U>
    requires requires(U* probe, int offset) {
        probe[offset + 1];
    }
    friend int indexed_requirement_mismatch(
        IndexedRequirementMismatchHost& host, U* values, int index);
};

template<class V>
requires requires(V* item, int position) {
    item[position];
}
int indexed_requirement_mismatch(IndexedRequirementMismatchHost& host,
                                 V* values, int index) {
    return values[index];
}

int main() {
    IndexedRequirementMismatchHost host;
    int values[] = {3, 5};
    return indexed_requirement_mismatch(host, values, 0);
}
