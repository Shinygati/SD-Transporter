/* Fault-injected FS API checks; no actual files or SD card are touched. */
#include "sdrename.h"
#include "sdrename_test_api.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned calls, deletions, opens, closes;
static Result firstError, deleteError;
FS_Path fsMakePath(int type, const void *data) {
    FS_Path p = {type, data};
    return p;
}
Result FSUSER_OpenArchive(FS_Archive *out, int type, FS_Path path) {
    (void)type;
    (void)path;
    opens++;
    *out = 1;
    return 0;
}
Result FSUSER_CloseArchive(FS_Archive a) {
    (void)a;
    closes++;
    return 0;
}
ssize_t utf8_to_utf16(uint16_t *out, const uint8_t *in, size_t capacity) {
    size_t n = strlen((const char *)in);
    if (n >= capacity)
        return -1;
    for (size_t i = 0; i < n; i++)
        out[i] = in[i];
    return (ssize_t)n;
}
Result FSUSER_RenameFile(FS_Archive a, FS_Path from, FS_Archive b, FS_Path to) {
    (void)a;
    (void)b;
    const uint16_t *s = from.data, *d = to.data;
    assert(s != d && s[0] == '/' && d[0] == '/');
    assert(s[1] == 's' && d[1] == 's');
    calls++;
    return calls == 1 ? firstError : 0;
}
Result FSUSER_DeleteFile(FS_Archive a, FS_Path to) {
    (void)a;
    (void)to;
    deletions++;
    return deleteError;
}
static void reset(Result first, Result removal) {
    calls = deletions = opens = closes = 0;
    firstError = first;
    deleteError = removal;
}
int main(void) {
    uint32_t native;
    const char *s = "sdmc:/saves/Pokemon HeartGold.walker.bak.tmp";
    const char *d = "sdmc:/saves/Pokemon HeartGold.walker.bak";
    reset(0, 0);
    assert(sdRenameReplace(s, d, &native) == 0);
    assert(calls == 1 && deletions == 0 && opens == closes);
    reset((Result)0xC82044BE, 0);
    assert(sdRenameReplace(s, d, &native) == 0);
    assert(calls == 2 && deletions == 1 && !native && opens == closes);
    reset((Result)0xC80003FC, 0);
    assert(sdRenameReplace(s, d, &native) == 0);
    assert(calls == 2 && deletions == 1);
    reset((Result)0xC8804478, 0);
    assert(sdRenameReplace(s, d, &native) == -1);
    assert(errno == ENOENT && native == 0xC8804478u && deletions == 0 && opens == closes);
    reset((Result)0xC82044BE, (Result)0xC80003EA);
    assert(sdRenameReplace(s, d, &native) == -1);
    assert(errno == EACCES && calls == 1 && deletions == 1 && opens == closes);
    reset(0, 0);
    assert(sdRenameReplace("relative", d, &native) == -1 && errno == EINVAL);
    assert(opens == 0);
    puts("native SD replacement checks passed (existing target, errors, separate paths)");
    return 0;
}
