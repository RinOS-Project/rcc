#ifndef RCC_BOOTSTRAP_SYS_WAIT_H
#define RCC_BOOTSTRAP_SYS_WAIT_H

#include <sys/types.h>

pid_t waitpid(pid_t pid, int* status, int options);

#define WIFEXITED(status) (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)

#endif
