#ifndef RCC_BOOTSTRAP_UNISTD_H
#define RCC_BOOTSTRAP_UNISTD_H

#include <sys/types.h>

int close(int descriptor);
pid_t fork(void);
int execvp(const char* file, char* const arguments[]);
void _exit(int status);

#endif
