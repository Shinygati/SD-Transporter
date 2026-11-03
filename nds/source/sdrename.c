#include "sdrename.h"
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
/* Some FAT drivers cannot replace an existing target with rename(). Retain the
 * old file until the replacement has moved into place; never unlink it first. */
int sdRenameReplace(const char *from,const char *to,uint32_t *nativeError){
    *nativeError=0;
    if(!rename(from,to))return 0;
    int original=errno;
    if(original!=EEXIST && original!=EACCES && original!=EPERM && original!=ENOTEMPTY)return -1;
    struct stat source,target;
    if(stat(from,&source)||stat(to,&target)||!S_ISREG(source.st_mode)||!S_ISREG(target.st_mode))return -1;
    char rollback[600];
    if(snprintf(rollback,sizeof(rollback),"%s.replace-old",to)>=(int)sizeof(rollback)){errno=ENAMETOOLONG;return -1;}
    if(!stat(rollback,&target)){errno=EEXIST;return -1;}
    if(errno!=ENOENT)return -1;
    if(rename(to,rollback))return -1;
    if(rename(from,to)){
        int failure=errno;
        rename(rollback,to); /* If rollback also fails, both complete files remain. */
        errno=failure;return -1;
    }
    remove(rollback); /* Failure only leaves an extra recovery copy. */
    return 0;
}
