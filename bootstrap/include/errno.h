#ifndef RCC_BOOTSTRAP_ERRNO_H
#define RCC_BOOTSTRAP_ERRNO_H

int* __errno_location(void);
#define errno (*__errno_location())

#define EINTR 4
#define EIO 5
#define EINVAL 22
#define EDOM 33
#define ENAMETOOLONG 36
#define EILSEQ 84
#define ERANGE 34

#endif
