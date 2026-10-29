int c23_reproducible(int value) [[reproducible(1)]];
int c23_unsequenced(int value) [[unsequenced("reason")]];
