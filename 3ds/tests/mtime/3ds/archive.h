/* Minimal libctru timestamp API for host fault injection. */
#ifndef TEST_ARCHIVE_H
#define TEST_ARCHIVE_H
#include <stdint.h>
int32_t archive_getmtime(const char *path,uint64_t *seconds);
#endif
