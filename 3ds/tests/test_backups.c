#define main coreChecks
#include "test_walkerfile.c"
#undef main
#include "backups.h"
#include <assert.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif
static int replaceRecovery(const char *from,const char *to) {
#ifdef _WIN32
    return MoveFileExA(from,to,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
#else
    return rename(from,to);
#endif
}
int main(void) {
    char root[256],walker[300],snapshot[900],receipt[900];struct stat st;
#ifdef _WIN32
    snprintf(root,sizeof(root),"work/backups-test-%llu",(unsigned long long)GetTickCount64());_mkdir(root);
#else
    snprintf(root,sizeof(root),"/tmp/backups-test-%ld",(long)getpid());mkdir(root,0700);
#endif
    snprintf(walker,sizeof(walker),"%s/test.walker",root);makeSolo(1000000);walkerResume(ee,1000000,10);
    FILE *f=fopen(walker,"wb");assert(f);assert(fwrite(ee,1,sizeof(ee),f)==sizeof(ee));fclose(f);
    assert(backupsPreserveActive(walker,ee));assert(backupsScan(walker));assert(backupsCount()==1);
    snprintf(snapshot,sizeof(snapshot),"%s/transfers/backups/test.walker.deleted-%08lx.bak",root,(unsigned long)walkerCrc(ee,sizeof(ee)));
    snprintf(receipt,sizeof(receipt),"%s/transfers/backups/test.walker.session-11111111111111111111111111111111.receipt",root);
    f=fopen(receipt,"wb");assert(f);fputs("ownership metadata",f);fclose(f);
    assert(backupsScan(walker)&&backupsCount()==1);
    assert(!backupsDelete(-1)&&!backupsDelete(1));assert(!stat(walker,&st)&&st.st_size==65536);
    assert(backupsDelete(0));assert(stat(snapshot,&st));assert(!stat(receipt,&st));assert(!stat(walker,&st)&&st.st_size==65536);
    assert(backupsPreserveActive(walker,ee));
    f=fopen(snapshot,"wb");assert(f);fputc(1,f);fclose(f);
    assert(!backupsDelete(0));assert(!backupsPreserveActive(walker,ee));
    assert(!stat(walker,&st)&&st.st_size==65536);
    puts("Backup manager: active data preserved, history deletion works, ownership receipts protected, changed-size/corrupt copies refused.");
    assert(backupsPreserveReward(walker,ee));
    assert(backupsScan(walker)&&backupsCount()>=1);
    for(int n=0;n<14;n++){ee[0x9fff]=(unsigned char)n;assert(backupsPreserveReward(walker,ee));}
    assert(backupsScan(walker)&&backupsCount()==10);assert(!stat(receipt,&st));
    /* Missing final file, uncommitted caught reward in .tmp: explicit recovery. */
    makeStrolling(1,2000000);memset(ee+WALKER_POKEMON,0xff,48);
    ee[WALKER_ROUTE+0x52]=21;ee[WALKER_ROUTE+0x53]=0;
    assert(walkerStorePokemon(ee,0,-1,2000000)==1);
    char temporary[600];snprintf(temporary,sizeof(temporary),"%s.tmp",walker);
    f=fopen(temporary,"wb");assert(f);assert(fwrite(ee,1,sizeof(ee),f)==sizeof(ee));fclose(f);
    assert(backupsInterruptedCount(walker)==0); /* Never overwrite an active file. */
    assert(!remove(walker));assert(backupsInterruptedCount(walker)==1);
    assert(backupsRestoreInterrupted(walker,0));assert(!stat(temporary,&st));
    unsigned char restored[65536];f=fopen(walker,"rb");assert(fread(restored,1,sizeof(restored),f)==sizeof(restored));fclose(f);
    assert(walkerPokemonId(restored,0)==21 && !memcmp(restored,ee,sizeof(ee)));
    /* Deliberately restore a selected historical partner over a valid empty file. */
    assert(backupsPreserveReward(walker,restored));assert(backupsScan(walker));
    unsigned char selected[65536];assert(backupsLoad(0,selected));
    ee[0xed+0x5b]=1;seal(0xed,0x1ed,104);memset(ee+WALKER_ROUTE,0,16);memset(ee+WALKER_POKEMON,0,48);
    f=fopen(walker,"wb");assert(fwrite(ee,1,sizeof(ee),f)==sizeof(ee));fclose(f);
    assert(backupsRestoreImage(walker,restored,replaceRecovery));
    f=fopen(walker,"rb");assert(fread(selected,1,sizeof(selected),f)==sizeof(selected));fclose(f);assert(!memcmp(selected,restored,sizeof(selected)));
    ee[0xed+12]^=1;seal(0xed,0x1ed,104);f=fopen(walker,"wb");assert(fwrite(ee,1,sizeof(ee),f)==sizeof(ee));fclose(f);
    assert(!backupsRestoreImage(walker,restored,replaceRecovery)); /* Different public Trainer ID. */
    puts("Selected-backup recovery restores a partner over an empty file, preserves current history and refuses a different trainer.");
    puts("Recovery: 10 snapshots per walker, receipts protected; missing final file restored only explicitly, caught Pokemon survives disk reload, source .tmp retained.");
    return 0;
}
