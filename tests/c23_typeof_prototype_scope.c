void ordinary_vla_pointer_prototype(int count, int (*values)[count]);
void ordinary_vla_pointer_prototype(int limit,
                                    int (*values)[limit + 1]);

void typeof_vla_pointer_prototype(int count, typeof(int[count]) *values);
void typeof_vla_pointer_prototype(int limit,
                                  typeof(int[limit + 1]) *values);

void typeof_vla_parameter_prototype(int count, typeof(int[count]) values);
void typeof_vla_parameter_prototype(int limit,
                                   typeof(int[limit + 1]) values);

int main(void) {
    return 0;
}
