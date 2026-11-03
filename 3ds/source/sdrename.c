/* Direct SD file replacement. GPL-3.0-or-later. */
#include "sdrename.h"
#ifdef SDRENAME_TEST
#include "sdrename_test_api.h"
#else
#include <3ds.h>
#include <3ds/util/utf.h>
#endif
#include <errno.h>
#include <string.h>

static int path16(const char *input, uint16_t *out) {
    if (!strncmp(input, "sdmc:", 5))
        input += 5;
    if (input[0] != '/' || strchr(input, ':')) {
        errno = EINVAL;
        return -1;
    }
    ssize_t length = utf8_to_utf16(out, (const uint8_t *)input, 543);
    if (length < 0) {
        errno = EILSEQ;
        return -1;
    }
    if (length >= 543) {
        errno = ENAMETOOLONG;
        return -1;
    }
    out[length] = 0;
    return 0;
}
int sdRenameReplace(const char *source, const char *destination, uint32_t *nativeError) {
    uint16_t oldPath[544], newPath[544], rollbackPath[544];
    FS_Archive archive;
    *nativeError = 0;
    /* Independent buffers: constructing the destination cannot change the source. */
    if (path16(source, oldPath) || path16(destination, newPath))
        return -1;
    Result rc = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if (R_SUCCEEDED(rc)) {
        FS_Path from = fsMakePath(PATH_UTF16, oldPath), to = fsMakePath(PATH_UTF16, newPath);
        rc = FSUSER_RenameFile(archive, from, archive, to);
        /* FS also uses module-specific description 0xBE for an existing file.
         * Do not fall back to RenameDirectory and mask that as ENOENT. The
         * caller verifies the replacement and backup before reaching here. */
        if (R_FAILED(rc) && (R_DESCRIPTION(rc) == RD_ALREADY_EXISTS || R_DESCRIPTION(rc) == 0xBE)) {
            size_t n=0;while(newPath[n])n++;
            static const char suffix[]=".replace-old";
            if(n+sizeof(suffix)>544) { FSUSER_CloseArchive(archive);errno=ENAMETOOLONG;return -1; }
            memcpy(rollbackPath,newPath,n*sizeof(uint16_t));
            for(size_t i=0;i<sizeof(suffix);i++)rollbackPath[n+i]=(unsigned char)suffix[i];
            FS_Path rollback=fsMakePath(PATH_UTF16,rollbackPath);
            rc=FSUSER_RenameFile(archive,to,archive,rollback);
            if(R_SUCCEEDED(rc)) {
                rc=FSUSER_RenameFile(archive,from,archive,to);
                if(R_FAILED(rc))FSUSER_RenameFile(archive,rollback,archive,to);
                else FSUSER_DeleteFile(archive,rollback);
            }
        }
        FSUSER_CloseArchive(archive);
    }
    if (R_SUCCEEDED(rc))
        return 0;
    *nativeError = (uint32_t)rc;
    unsigned description = R_DESCRIPTION(rc);
    errno = description == RD_NOT_FOUND || description == 0x78 || description == 0xFA ? ENOENT
            : description == RD_NOT_AUTHORIZED                                        ? EACCES
                                                                                      : EIO;
    return -1;
}
