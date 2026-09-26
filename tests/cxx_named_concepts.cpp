template<int N> concept Positive = N > 0;

template<int N> requires Positive<N>
int positive_value() {
    return N;
}

template<int N> requires (!Positive<N>)
int nonpositive_value() {
    return N;
}

struct ConceptCarrier {
    int value;
};

template<typename T>
concept HasValue = requires(T candidate) {
    candidate.value;
};

template<typename T>
requires HasValue<T>
int read_value(T candidate) {
    return candidate.value;
}

struct NoValue {
};

template<typename T>
requires (!HasValue<T>)
int read_missing(T) {
    return 7;
}

int main() {
    ConceptCarrier carrier = {41};
    NoValue missing = {};
    return positive_value<3>() == 3 &&
                   nonpositive_value<-2>() == -2 &&
                   read_value(carrier) == 41 &&
                   read_missing(missing) == 7
               ? 0
               : 1;
}
