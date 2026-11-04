#ifndef PW_HOST_NDSTYPES_H
#define PW_HOST_NDSTYPES_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#if defined(_MSC_VER) && !defined(__clang__)
#define __builtin_offsetof offsetof
#endif
#endif
