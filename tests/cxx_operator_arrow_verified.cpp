struct VerifiedArrowTarget {
    int value;
};

#if defined(TEST_VERIFIED_ARROW_CLEANUP)
extern "C" {
int verified_arrow_cleanup_count = 0;
}
#endif

struct VerifiedArrowLeaf {
    VerifiedArrowTarget* target;

#if defined(TEST_VERIFIED_ARROW_CLEANUP)
    ~VerifiedArrowLeaf() {
        ++verified_arrow_cleanup_count;
    }
#endif

    VerifiedArrowTarget* operator->() {
        return target;
    }
};

struct VerifiedArrowRoot {
    VerifiedArrowTarget* target;

    VerifiedArrowLeaf operator->() {
        return VerifiedArrowLeaf{target};
    }
};

#if defined(TEST_VERIFIED_ARROW_SINGLE)
extern "C" int verified_arrow_read(VerifiedArrowLeaf& leaf) {
    return leaf->value;
}
#else
extern "C" int verified_arrow_read(VerifiedArrowRoot& root) {
    return root->value == 41 ? 0 : 1;
}
#endif
