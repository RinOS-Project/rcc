template<class T>
int wide_explicit_candidates(
    T, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int
);

template<class T>
int wide_explicit_candidates(
    T, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int, int, int, int, int, int, int, int,
    int
);

template<class T>
int wide_explicit_candidates(T);

template<>
int wide_explicit_candidates<int>(int) {
    return 76;
}

int main() {
    return wide_explicit_candidates<int>(1) == 76 ? 0 : 1;
}
