template<int N> concept Positive = N > 0;

template<int N> requires Positive<N>
int positive_value() {
    return N;
}

template<int N> requires (!Positive<N>)
int nonpositive_value() {
    return N;
}

int main() {
    return positive_value<3>() == 3 &&
                   nonpositive_value<-2>() == -2
               ? 0
               : 1;
}
