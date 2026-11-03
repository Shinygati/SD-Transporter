#ifndef SDRENAME_TEST_API_H
#define SDRENAME_TEST_API_H
#include <stddef.h>
#include <stdint.h>
#ifdef _MSC_VER
typedef intptr_t ssize_t;
#else
#include <sys/types.h>
#endif
typedef int32_t Result;
typedef uint64_t FS_Archive;
typedef struct {
    int type;
    const void *data;
} FS_Path;
#define ARCHIVE_SDMC 9
#define PATH_EMPTY 1
#define PATH_UTF16 4
#define RD_ALREADY_EXISTS 1020
#define RD_NOT_FOUND 1018
#define RD_NOT_AUTHORIZED 1002
#define R_FAILED(r) ((int32_t)(r) < 0)
#define R_SUCCEEDED(r) (!R_FAILED(r))
#define R_DESCRIPTION(r) ((unsigned)(r) & 1023)
FS_Path fsMakePath(int type, const void *data);
Result FSUSER_OpenArchive(FS_Archive *, int, FS_Path);
Result FSUSER_CloseArchive(FS_Archive);
Result FSUSER_RenameFile(FS_Archive, FS_Path, FS_Archive, FS_Path);
Result FSUSER_DeleteFile(FS_Archive, FS_Path);
ssize_t utf8_to_utf16(uint16_t *, const uint8_t *, size_t);
#endif
