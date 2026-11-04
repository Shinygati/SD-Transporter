/* Format facts cross-checked against pret/pokeheartgold save.h/pokewalker.h
 * and PKHeX SAV4HGSS/SAV4. No game/save data is bundled. GPL-3.0-or-later. */
#include "hgss_courses.h"
#include "walkerfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#define SIZE 0x80000u
#define GENERAL 0xf628u
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned get16(const uint8_t *p){return p[0]|p[1]<<8;}
static void put32(uint8_t *p,uint32_t v){for(int n=0;n<4;n++)p[n]=(uint8_t)(v>>(n*8));}
static unsigned crc16(const uint8_t *p,unsigned size){
    unsigned crc=65535;
    while(size--){crc^=(unsigned)*p++<<8;for(int n=0;n<8;n++)crc=((crc<<1)^((crc&0x8000)?0x1021:0))&65535;}
    return crc;
}
static int valid(const uint8_t *p){
    const uint8_t *footer=p+GENERAL-16;
    unsigned version=p[0x80],language=p[0x7d];
    return (version==7||version==8) && (language==1||language==2||language==3||language==4||language==5||language==7||language==8) &&
      get32(footer+4)==GENERAL && get32(footer+8)==0x20060623 && get16(footer+12)==0 && crc16(p,GENERAL-16)==get16(p+GENERAL-2);
}
int hgssUnlockCourses(uint8_t *save,unsigned bytes,uint32_t *added){
    if(added)*added=0;
    if(bytes!=SIZE)return 0;
    int ok[2]={valid(save),valid(save+0x40000)};
    if(!ok[0]&&!ok[1])return 0;
    if(ok[0]&&ok[1]&&(save[0x80]!=save[0x40080]||save[0x7d]!=save[0x4007d]||memcmp(save+0x64,save+0x40064,20)))return 0;
    uint32_t changed=0;
    for(int n=0;n<2;n++)if(ok[n]){
        uint8_t *p=save+n*0x40000;unsigned language=p[0x7d];
        uint32_t available=language==1?0x07ffffff:language==8?0x037fffff:0x027fffff;
        uint32_t mask=available&0x07f00000,old=get32(p+0xe70c);
        changed|=mask&~old;put32(p+0xe70c,old|mask);
        unsigned crc=crc16(p,GENERAL-16);p[GENERAL-2]=crc;p[GENERAL-1]=crc>>8;
    }
    if(added)*added=changed;
    return changed?1:2;
}
static int readFile(const char *path,uint8_t *out){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,SIZE,f)==SIZE&&fgetc(f)==EOF&&!ferror(f);if(fclose(f))ok=0;
    return ok;}
static int writeFile(const char *path,const uint8_t *data){
    FILE *f=fopen(path,"wb");if(!f)return 0;int ok=fwrite(data,1,SIZE,f)==SIZE;
    if(fflush(f))ok=0;
#ifndef _WIN32
    if(ok&&fsync(fileno(f)))ok=0;
#endif
    if(fclose(f))ok=0;
    return ok;
}
static int fail(char *error,unsigned size,const char *text){snprintf(error,size,"%s",text);return 0;}
int hgssUnlockCourseFile(const char *walker,int (*replace)(const char *,const char *),char *error,unsigned size){
    unsigned length=(unsigned)strlen(walker);char path[600],temporary[640],backup[1280],folder[640];
    if(length<7||strcmp(walker+length-7,".walker")||length+80>=sizeof(path))return fail(error,size,"Invalid game save path.");
    snprintf(path,sizeof(path),"%.*s.sav",(int)length-7,walker);
    snprintf(temporary,sizeof(temporary),"%s.courses.tmp",path);
    uint8_t *data=malloc(SIZE),*check=malloc(SIZE);if(!data||!check){free(data);free(check);return fail(error,size,"Not enough memory to verify the save.");}
    int result=0;uint32_t added;
    if(!readFile(path,data)){fail(error,size,"Matching raw 512 KiB HGSS .sav not found. Existing files kept.");goto done;}
    memcpy(check,data,SIZE);int status=hgssUnlockCourses(data,SIZE,&added);
    if(!status){fail(error,size,"HGSS save identity/footer/checksum validation failed. Nothing changed.");goto done;}
    if(status==2){snprintf(error,size,"All supported special/event courses are already active.");result=2;goto done;}
    const char *slash=strrchr(path,'/');const char *base=slash?slash+1:path;
    snprintf(folder,sizeof(folder),"%.*stransfers",slash?(int)(slash-path+1):0,path);
    struct stat dir;
    if(stat(folder,&dir)){
#ifdef _WIN32
        int created=_mkdir(folder);
#else
        int created=mkdir(folder,0777);
#endif
        if(created){fail(error,size,"Could not create the backup folder. Nothing changed.");goto done;}
    }
    strcat(folder,"/backups");
    if(stat(folder,&dir)){
#ifdef _WIN32
        int created=_mkdir(folder);
#else
        int created=mkdir(folder,0777);
#endif
        if(created){fail(error,size,"Could not create the backup folder. Nothing changed.");goto done;}
    }
    snprintf(backup,sizeof(backup),"%s/%s.before-courses-%08lx.bak",folder,base,(unsigned long)walkerCrc(check,SIZE));
    struct stat st;
    if(stat(backup,&st)){
        if(errno!=ENOENT||!writeFile(backup,check)){fail(error,size,"Could not preserve the original game save. Nothing changed.");goto done;}
    }
    /* First compare the original backup, then the unchanged live source. */
    if(!readFile(backup,data)||memcmp(data,check,SIZE)||!readFile(path,data)||memcmp(data,check,SIZE)){
        fail(error,size,"Original backup or live save verification failed. Nothing changed.");goto done;
    }
    hgssUnlockCourses(data,SIZE,&added);
    if(!writeFile(temporary,data)||!readFile(temporary,check)||memcmp(data,check,SIZE)||!replace||replace(temporary,path)||!readFile(path,check)||memcmp(data,check,SIZE)){
        fail(error,size,"Save publication failed. Original .before-courses backup is preserved.");goto done;
    }
    snprintf(error,size,"Supported special/event courses activated. Select them in HGSS when sending a Pokemon for a stroll. Original game save backup kept.");result=1;
done:free(data);free(check);return result;
}
