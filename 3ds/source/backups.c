/* Only immutable history snapshots are managed here. Live EEPROMs, .phone,
 * .lease, ordinary .bak fallback and session receipts are never enumerated. */
#include "backups.h"
#include "walkerfile.h"
#include "species.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "recovery_limit.h"
#define MAX_BACKUPS 128
typedef struct { char name[256]; time_t stamp; } Entry;
static Entry entries[MAX_BACKUPS];
static char directory[600],prefix[256];
static int count,limited;
static int allowed(const char *name) {
    unsigned n=(unsigned)strlen(prefix),length=(unsigned)strlen(name);
    if(strchr(name,'/')||strchr(name,'\\')||length<n+5||strncmp(name,prefix,n)||strcmp(name+length-4,".bak"))return 0;
    const char *kind=name+n;
    return !strncmp(kind,"prelaunch-",10)||!strncmp(kind,"checkout-",9)||
           !strncmp(kind,"return-",7)||!strncmp(kind,"recovery-",9)||!strncmp(kind,"conflict-",9)||!strncmp(kind,"deleted-",8)||!strncmp(kind,"reward-",7);
}
static int pathFor(int index,char path[900]) {
    return index>=0 && index<count && allowed(entries[index].name) &&
           snprintf(path,900,"%s/%s",directory,entries[index].name)<900;
}
static int regularSnapshot(const char *path) {
    struct stat st;
    return !lstat(path,&st) && S_ISREG(st.st_mode) && st.st_size==WALKER_FILE_SIZE;
}
static int newest(const void *a,const void *b) {
    const Entry *x=a,*y=b;
    return x->stamp<y->stamp?1:x->stamp>y->stamp?-1:strcmp(x->name,y->name);
}
int backupsScan(const char *walker) {
    count=limited=0;directory[0]=prefix[0]=0;
    const char *base=strrchr(walker,'/');base=base?base+1:walker;
    if(strlen(base)+2>sizeof(prefix))return 0;
    snprintf(prefix,sizeof(prefix),"%s.",base);
    if(snprintf(directory,sizeof(directory),"%.*stransfers/backups",(int)(base-walker),walker)>=(int)sizeof(directory))return 0;
    DIR *dir=opendir(directory);if(!dir)return 1;
    struct dirent *item;
    while((item=readdir(dir))) {
        if(!allowed(item->d_name))continue;
        char path[900];struct stat st;
        if(snprintf(path,sizeof(path),"%s/%s",directory,item->d_name)>=(int)sizeof(path)||!regularSnapshot(path)||stat(path,&st))continue;
        if(count==MAX_BACKUPS){limited=1;continue;}
        snprintf(entries[count].name,sizeof(entries[count].name),"%s",item->d_name);
        entries[count++].stamp=walkerRecoveryTimestamp(path,&st);
    }
    closedir(dir);qsort(entries,count,sizeof(entries[0]),newest);return 1;
}
int backupsCount(void){return count;}
int backupsLimited(void){return limited;}
int backupsDescribe(int index,char *name,unsigned capacity,char *detail,unsigned detailSize) {
    char path[900];if(!pathFor(index,path)||!regularSnapshot(path))return 0;
    snprintf(name,capacity,"%s",entries[index].name);
    char date[24]="Unknown date";struct tm *when=localtime(&entries[index].stamp);
    if(entries[index].stamp && when)strftime(date,sizeof(date),"%Y-%m-%d %H:%M",when);
    unsigned char *data=malloc(WALKER_FILE_SIZE);WalkerInfo info;
    FILE *f=data?fopen(path,"rb"):0;int valid=f && fread(data,1,WALKER_FILE_SIZE,f)==WALKER_FILE_SIZE && walkerRead(data,&info);
    if(f)fclose(f);
    free(data);
    if(valid)snprintf(detail,detailSize,"%s\n%s / ID %05lu",date,info.registered&&info.trainer[0]?info.trainer:"Trainer unavailable",(unsigned long)info.trainerId);
    else snprintf(detail,detailSize,"%s\n64 KiB recovery snapshot",date);
    return 1;
}
int backupsDelete(int index) {
    char path[900];if(!pathFor(index,path)||!regularSnapshot(path))return 0;
    if(remove(path))return 0;
    memmove(entries+index,entries+index+1,(count-index-1)*sizeof(entries[0]));count--;return 1;
}
static int matches(const char *path,const unsigned char *data) {
    FILE *f=fopen(path,"rb");if(!f)return 0;unsigned char buffer[512];int ok=1;
    for(unsigned n=0;ok && n<WALKER_FILE_SIZE;n+=sizeof(buffer))ok=fread(buffer,1,sizeof(buffer),f)==sizeof(buffer)&&!memcmp(buffer,data+n,sizeof(buffer));
    if(ok)ok=fgetc(f)==EOF&&!ferror(f);
    if(fclose(f))ok=0;
    return ok;
}
static int preserve(const char *walker,const unsigned char *data,const char *kind) {
    WalkerInfo info;if(!walkerRead(data,&info)||!backupsScan(walker))return 0;
    const char *base=strrchr(walker,'/');base=base?base+1:walker;
    char parent[600],path[900],temporary[910];
    if(snprintf(parent,sizeof(parent),"%.*stransfers",(int)(base-walker),walker)>=(int)sizeof(parent))return 0;
    struct stat st;
    if(stat(parent,&st) && mkdir(parent,0777))return 0;
    if(stat(directory,&st) && mkdir(directory,0777))return 0;
    if(snprintf(path,sizeof(path),"%s/%s%s-%08lx.bak",directory,prefix,kind,(unsigned long)walkerCrc(data,WALKER_FILE_SIZE))>=(int)sizeof(path))return 0;
    if(!stat(path,&st))return matches(path,data)&&walkerPruneRecovery(walker,path);
    snprintf(temporary,sizeof(temporary),"%s.tmp",path);FILE *f=fopen(temporary,"wb");if(!f)return 0;
    int ok=fwrite(data,1,WALKER_FILE_SIZE,f)==WALKER_FILE_SIZE;
    if(fflush(f)||fsync(fileno(f)))ok=0;
    if(fclose(f))ok=0;
    if(ok)ok=matches(temporary,data)&&!rename(temporary,path)&&matches(path,data);
    return ok && walkerPruneRecovery(walker,path);
}

int backupsPreserveActive(const char *walker,const unsigned char *data) {return preserve(walker,data,"deleted");}
int backupsPreserveReward(const char *walker,const unsigned char *data) {return preserve(walker,data,"reward");}
#include <errno.h>
/* Explicit recovery for a missing final file; never invoked during normal load. */
static const char *interruptedSuffixes[]={".tmp",".replace-old",".bak",".bak.tmp"};
static int interruptedSource(const char *walker,int index,char out[900],unsigned char *data) {
    char name[900];struct stat st;WalkerInfo info;
    if(!stat(walker,&st)||errno!=ENOENT)return 0;
    snprintf(name,sizeof(name),"%s.lease",walker);
    if(!stat(name,&st)||errno!=ENOENT)return 0; /* Phone recovery has its own revocation flow. */
    int found=0;
    for(unsigned i=0;i<4;i++) {
        if(snprintf(name,sizeof(name),"%s%s",walker,interruptedSuffixes[i])>=(int)sizeof(name)||!regularSnapshot(name))continue;
        FILE *f=fopen(name,"rb");if(!f)continue;
        int ok=fread(data,1,WALKER_FILE_SIZE,f)==WALKER_FILE_SIZE&&fgetc(f)==EOF&&!ferror(f)&&walkerRead(data,&info);fclose(f);
        if(ok && found++==index){snprintf(out,900,"%s",name);return 1;}
    }
    return 0;
}
int backupsInterruptedCount(const char *walker) {
    unsigned char *data=malloc(WALKER_FILE_SIZE);char name[900];int count=0;if(!data)return 0;
    while(count<4 && interruptedSource(walker,count,name,data))count++;
    free(data);return count;
}
int backupsInterruptedDescribe(const char *walker,int index,char *name,unsigned size,char *detail,unsigned detailSize) {
    unsigned char *data=malloc(WALKER_FILE_SIZE);char source[900];if(!data)return 0;
    int ok=interruptedSource(walker,index,source,data);WalkerInfo info;
    if(ok && walkerRead(data,&info)) {
        const char *base=strrchr(source,'/');base=base?base+1:source;snprintf(name,size,"%s",base);
        snprintf(detail,detailSize,"%s / ID %05lu\nPack: %u Pokemon, %u items",info.registered&&info.trainer[0]?info.trainer:"Trainer unavailable",(unsigned long)info.trainerId,info.pokemonCount,info.itemCount);
    }
    free(data);return ok;
}
int backupsRestoreInterrupted(const char *walker,int index) {
    unsigned char *data=malloc(WALKER_FILE_SIZE);char source[900],temp[900];if(!data)return 0;
    int ok=interruptedSource(walker,index,source,data);
    if(ok)ok=snprintf(temp,sizeof(temp),"%s.recover.tmp",walker)<(int)sizeof(temp);
    if(ok) {
        FILE *f=fopen(temp,"wb");ok=f!=0;
        if(f){ok=fwrite(data,1,WALKER_FILE_SIZE,f)==WALKER_FILE_SIZE;if(fflush(f)||fsync(fileno(f)))ok=0;if(fclose(f))ok=0;}
        struct stat st;
        if(ok)ok=matches(temp,data)&&stat(walker,&st)&&errno==ENOENT;
        if(ok)ok=!rename(temp,walker)&&matches(walker,data);
    }
    free(data);return ok; /* Original recovery candidates are retained. */
}
/* Read a selected history into caller-owned memory before creating/pruning any
 * new history. Restoring is an explicit UI action, never normal file loading. */
int backupsLoad(int index,unsigned char *data) {
    char path[900];WalkerInfo info;if(!pathFor(index,path)||!regularSnapshot(path))return 0;
    FILE *f=fopen(path,"rb");if(!f)return 0;
    int ok=fread(data,1,WALKER_FILE_SIZE,f)==WALKER_FILE_SIZE&&fgetc(f)==EOF&&!ferror(f)&&walkerRead(data,&info)&&info.setUp;
    if(fclose(f))ok=0;
    return ok;
}
int backupsRestoreImage(const char *walker,const unsigned char *data,int (*replace)(const char *,const char *)) {
    WalkerInfo info;if(!walkerRead(data,&info)||!info.setUp)return 0;
    char path[900],temp[900];struct stat st;
    snprintf(path,sizeof(path),"%s.lease",walker);if(!stat(path,&st)||errno!=ENOENT)return 0;
    if(!stat(walker,&st)) {
        if(st.st_size) {
            if(st.st_size!=WALKER_FILE_SIZE)return 0;
            unsigned char *current=malloc(WALKER_FILE_SIZE);if(!current)return 0;
            FILE *f=fopen(walker,"rb");int ok=f&&fread(current,1,WALKER_FILE_SIZE,f)==WALKER_FILE_SIZE&&fgetc(f)==EOF&&!ferror(f);if(f)fclose(f);
            char a[37],b[37];
            if(ok && walkerTrainerKey(current,a)&&walkerTrainerKey(data,b)&&strcmp(a,b))ok=0;
            if(ok)ok=preserve(walker,current,"recovery");
            free(current);
            if(!ok)return 0;
        }
    }else if(errno!=ENOENT)return 0;
    if(snprintf(temp,sizeof(temp),"%s.recover.tmp",walker)>=(int)sizeof(temp))return 0;
    FILE *f=fopen(temp,"wb");if(!f)return 0;
    int ok=fwrite(data,1,WALKER_FILE_SIZE,f)==WALKER_FILE_SIZE;
    if(fflush(f)||fsync(fileno(f)))ok=0;
    if(fclose(f))ok=0;
    if(ok)ok=matches(temp,data)&&(replace?replace(temp,walker):rename(temp,walker))==0&&matches(walker,data);
    return ok;
}
