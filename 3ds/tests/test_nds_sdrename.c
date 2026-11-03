#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
static int source,target,old,failPublish,failRollback;
static int mockedStat(const char *p,struct stat *s){int exists=!strcmp(p,"source")?source:!strcmp(p,"target")?target:old;if(!exists){errno=ENOENT;return -1;}memset(s,0,sizeof(*s));s->st_mode=_S_IFREG;return 0;}
static int mockedRename(const char *a,const char *b){
 if(!strcmp(a,"source")&&!strcmp(b,"target")){
  if(target){errno=EEXIST;return -1;}if(failPublish){errno=EIO;return -1;}source=0;target=1;return 0;
 }
 if(!strcmp(a,"target")){if(old){errno=EEXIST;return -1;}target=0;old=1;return 0;}
 if(failRollback){errno=EIO;return -1;}old=0;target=1;return 0;
}
static int mockedRemove(const char *p){assert(!strcmp(p,"target.replace-old"));old=0;return 0;}
#define S_ISREG(m) (((m)&_S_IFMT)==_S_IFREG)
#define stat(...) mockedStat(__VA_ARGS__)
#define rename mockedRename
#define remove mockedRemove
#include "../../nds/source/sdrename.c"
#undef stat
int main(void){uint32_t error;
 source=target=1;old=0;failPublish=1;
 assert(sdRenameReplace("source","target",&error)==-1);assert(source&&target&&!old);
 failRollback=1;assert(sdRenameReplace("source","target",&error)==-1);assert(source&&!target&&old);
 target=1;old=0;failPublish=failRollback=0;
 assert(!sdRenameReplace("source","target",&error));assert(!source&&target&&!old);
 puts("DS FAT replacement: failed replacement restores original; failed rollback retains both files; success never deletes the target first.");return 0;
}
