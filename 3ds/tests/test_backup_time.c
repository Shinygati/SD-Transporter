/* Exercise the real __3DS__ timestamp branch with libctru's UNIX-seconds ABI. */
#define main coreChecks
#include "test_walkerfile.c"
#undef main
#include "backups.h"
#include "recovery_limit.h"
#include <assert.h>
#include <direct.h>
static int unavailable;
int32_t archive_getmtime(const char *path,uint64_t *seconds) {
    if(unavailable)return -1;
    const char *reward=strstr(path,".reward-");unsigned id=99;
    if(reward)sscanf(reward,".reward-%x",&id);
    *seconds=1760000000ULL+(id<12?(12-id)*60:1000);return 0;
}
int main(void) {
    struct stat st={0};uint64_t native;archive_getmtime("test",&native);
    assert(walkerRecoveryTimestamp("test",&st)==(time_t)native); /* No second epoch conversion. */
    unavailable=1;assert(walkerRecoveryTimestamp("test",&st)==0);
    st.st_mtime=1750000000;assert(walkerRecoveryTimestamp("test",&st)==st.st_mtime);unavailable=0;
    char root[256],walker[300],name[600],label[200],detail[200];
    snprintf(root,sizeof(root),"work/mtime-%llu",(unsigned long long)GetTickCount64());_mkdir(root);
    snprintf(name,sizeof(name),"%s/transfers",root);_mkdir(name);snprintf(name,sizeof(name),"%s/transfers/backups",root);_mkdir(name);
    snprintf(walker,sizeof(walker),"%s/test.walker",root);makeSolo(1000000);
    for(unsigned id=0;id<12;id++){
        snprintf(name,sizeof(name),"%s/transfers/backups/test.walker.reward-%08x.bak",root,id);
        FILE *f=fopen(name,"wb");assert(f);assert(fwrite(ee,1,sizeof(ee),f)==sizeof(ee));fclose(f);
    }
    assert(backupsScan(walker)&&backupsCount()==12);
    assert(backupsDescribe(0,label,sizeof(label),detail,sizeof(detail)));
    assert(strstr(label,"00000000.bak")&&strstr(detail,"2025-")&&!strstr(detail,"1970-"));
    assert(backupsPreserveReward(walker,ee));assert(backupsScan(walker)&&backupsCount()==10);
    /* Native times deliberately reverse filename order: remove the real oldest. */
    for(unsigned id=9;id<12;id++){snprintf(name,sizeof(name),"%s/transfers/backups/test.walker.reward-%08x.bak",root,id);assert(stat(name,&st));}
    snprintf(name,sizeof(name),"%s/transfers/backups/test.walker.reward-00000000.bak",root);assert(!stat(name,&st));
    puts("3DS backup time: native UNIX seconds displayed, zero/failed stat is not 1970, native dates determine sorting and oldest-first retention.");return 0;
}
