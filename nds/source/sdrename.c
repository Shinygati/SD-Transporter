#include "sdrename.h"
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
int sdRenameReplace(const char *from,const char *to,uint32_t *nativeError){
    *nativeError=0;
    if(!rename(from,to))return 0;
    struct stat source,target;
    if(stat(from,&source)||stat(to,&target)||!S_ISREG(source.st_mode)||!S_ISREG(target.st_mode))return -1;
    if(remove(to))return -1;
    return rename(from,to);
}
