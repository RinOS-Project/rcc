static float g_float = 1.5f + 0.25f;
static double g_double = -2.5 + 1.0;
static float g_cast = (float)3;
static double g_widen = 4.0f;
_Thread_local float tls_value = 2.0f;

int floating_static_sizes(void) {
    return sizeof(g_float) + sizeof(g_double) + sizeof(g_cast) +
           sizeof(g_widen) + sizeof(tls_value);
}
