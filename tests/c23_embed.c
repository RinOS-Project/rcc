#define RCC_EMBED_NAME "c23_embed_resource.bin"

#if !defined(__has_embed)
#error "C23 __has_embed must be recognized as a defined operator"
#endif
#if !__has_embed(RCC_EMBED_NAME)
#error "__has_embed failed to find a quoted resource relative to this source"
#endif
#if __has_embed(<c23_embed_resource.bin>) != __STDC_EMBED_FOUND__
#error "__has_embed failed to report a nonempty angle-bracket resource"
#endif
#if __has_embed("c23_embed_resource.bin" limit(0)) != __STDC_EMBED_EMPTY__
#error "limit(0) must make a found resource empty"
#endif
#if __has_embed("c23_embed_resource.bin" __limit__(0)) != \
    __STDC_EMBED_EMPTY__
#error "the underscored standard parameter spelling must be accepted"
#endif
#if __has_embed("c23_embed_missing.bin") != __STDC_EMBED_NOT_FOUND__
#error "__has_embed must report a missing resource"
#endif
#if __has_embed("c23_embed_empty_resource.bin") != __STDC_EMBED_EMPTY__
#error "__has_embed must report a physically empty resource"
#endif
#if __has_embed("c23_embed_resource.bin" rcc::unsupported(1)) != \
    __STDC_EMBED_NOT_FOUND__
#error "unsupported implementation parameters must report not-found"
#endif
#if __has_embed("c23_embed_resource.bin" rcc::unsupported) != \
    __STDC_EMBED_NOT_FOUND__
#error "an unsupported prefixed parameter may omit its argument clause"
#endif

#define RCC_EMBED_LIMIT 2

static const unsigned char embedded[] = {
#embed RCC_EMBED_NAME
};

static const unsigned char embedded_limited[] = {
#embed <c23_embed_resource.bin> limit(RCC_EMBED_LIMIT)
};

static const int embedded_marked[] = {
#embed "c23_embed_resource.bin" prefix(99,) suffix(,88)
};

static const int embedded_aliases[] = {
#embed "c23_embed_resource.bin" __prefix__(77,) __limit__(1) __suffix__(,88)
};

static const int embedded_empty[] = {
#embed "c23_embed_resource.bin" limit(0) prefix(77,) suffix(,88) __if_empty__(55)
};

static const int physically_empty[] = {
#embed "c23_embed_empty_resource.bin" if_empty(42)
};

_Static_assert(sizeof(embedded) / sizeof(embedded[0]) == 4,
               "full resource byte count");
_Static_assert(sizeof(embedded_limited) / sizeof(embedded_limited[0]) == 2,
               "limit is measured in bytes");
_Static_assert(sizeof(embedded_marked) / sizeof(embedded_marked[0]) == 6,
               "prefix and suffix tokens surround the resource");
_Static_assert(sizeof(embedded_aliases) / sizeof(embedded_aliases[0]) == 3,
               "underscored standard parameter spellings are accepted");
_Static_assert(sizeof(embedded_empty) / sizeof(embedded_empty[0]) == 1,
               "if_empty replaces an empty expansion and ignores prefix/suffix");
_Static_assert(sizeof(physically_empty) / sizeof(physically_empty[0]) == 1,
               "if_empty handles an empty resource file");

int main(void) {
    return embedded[0] != 'A' || embedded[1] != 'B' ||
           embedded[2] != 'C' || embedded[3] != '\n' ||
           embedded_limited[0] != 'A' || embedded_limited[1] != 'B' ||
           embedded_marked[0] != 99 || embedded_marked[1] != 'A' ||
           embedded_marked[4] != '\n' || embedded_marked[5] != 88 ||
           embedded_aliases[0] != 77 || embedded_aliases[1] != 'A' ||
           embedded_aliases[2] != 88 ||
           embedded_empty[0] != 55 || physically_empty[0] != 42;
}
