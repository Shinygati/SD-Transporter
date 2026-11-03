/* Local experimental Poke Transfer SD source preparation. GPL-3.0-or-later. */
#ifndef GEN5_PREPARE_H
#define GEN5_PREPARE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "configuration.h"
#include "gen5_dlp_patched_crc.h"
static const char *gen5PrepareError="";
static u32 gen5DumpBytes=0,gen5DumpCRC=0,gen5SourceSaveBytes=0;
static char gen5BackupPath[1024];
static bool gen5PrepareFail(const char *reason) {gen5PrepareError=reason;return false;}
static bool gen5EnsureBackupDir(const char *path) {
    struct stat info;
    if(!stat(path,&info))return (info.st_mode&S_IFMT)==S_IFDIR;
    return mkdir(path,0777)==0;
}
static bool gen5Prepare(configuration *c) {
    c->gen5RomCluster=c->gen5SaveCluster=c->gen5RomSize=0;
    gen5PrepareError="";gen5DumpBytes=gen5DumpCRC=gen5SourceSaveBytes=0;gen5BackupPath[0]=0;
    FILE *f=fopen(c->ndsPath,"rb");if(!f)return gen5PrepareFail("G01: Cannot open Download Play");
    unsigned char h[512],chunk[512];bool ok=fread(h,1,512,f)==512;fclose(f);
    if(!ok)return gen5PrepareFail("G01: Cannot read Download Play");
    if(memcmp(h+12,"HNDA",4) || h[0x12])return true;
    if(!c->gen5RomPath || !c->gen5RomPath[0])return true;
    f=fopen(c->ndsPath,"rb");if(!f)return gen5PrepareFail("G01: Cannot reopen Download Play");
    u32 crc=~0u,count=0;size_t got;
    while((got=fread(chunk,1,sizeof(chunk),f))!=0) {
        count+=got;
        for(size_t i=0;i<got;i++){crc^=chunk[i];for(int bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
    }
    gen5DumpBytes=count;gen5DumpCRC=~crc;
    ok=!ferror(f) && count==433088 && ((~crc)==0x57d48e69 || (~crc)==GEN5_DLP_PATCHED_CRC);fclose(f);
    if(!ok)return gen5PrepareFail("G02: Download Play dump differs");
    if(c->b4dsMode || !c->sdFound || c->gameOnFlashcard || c->saveOnFlashcard)
        return gen5PrepareFail("G03: Console SD access required");
    if(!c->gen5SavePath || strncmp(c->gen5RomPath,"sd:/",4) || strncmp(c->gen5SavePath,"sd:/",4) ||
       strlen(c->gen5RomPath)>850 || strlen(c->gen5SavePath)>850 || !strcmp(c->ndsPath,c->gen5RomPath))
        return gen5PrepareFail("G04: Invalid source paths");
    struct stat rom,save;
    if(stat(c->gen5RomPath,&rom))return gen5PrepareFail("R01: Source ROM not found");
    if(stat(c->gen5SavePath,&save))return gen5PrepareFail("S01: Source save not found");
    gen5SourceSaveBytes=save.st_size;
    if(rom.st_ino<2 || save.st_ino<2 || rom.st_ino==save.st_ino)
        return gen5PrepareFail("G05: Invalid source FAT clusters");
    if(rom.st_size<0x20000 || rom.st_size>0x10000000)return gen5PrepareFail("R02: Unsupported source ROM size");
    if(save.st_size!=0x80000)return gen5PrepareFail("S02: Save must be 524288 bytes");
    f=fopen(c->gen5RomPath,"rb");if(!f)return gen5PrepareFail("R03: Cannot open source ROM");
    ok=fread(h,1,512,f)==512;fclose(f);
    if(!ok || h[0x12])return gen5PrepareFail("R04: Invalid source ROM header");
    // Gen IV source releases: English (US E, EU/AUS P), French, German,
    // Italian, Spanish, Japanese and Korean. Keep the actual source header.
    const char region = h[15];
    const bool supportedRegion = region=='E' || region=='P' || region=='F' ||
                         region=='D' || region=='I' || region=='S' || region=='J' || region=='K';
    const bool isDiamond = !memcmp(h+12,"ADA",3);
    const bool isPearl = !memcmp(h+12,"APA",3);
    const bool isPlatinum = !memcmp(h+12,"CPU",3);
    const bool isHeartGold = !memcmp(h+12,"IPK",3);
    const bool isSoulSilver = !memcmp(h+12,"IPG",3);
    if(!supportedRegion || !(isDiamond || isPearl || isPlatinum || isHeartGold || isSoulSilver))
        return gen5PrepareFail("R04: Unsupported Gen 4 game/language");
    // Trimmed and untrimmed dumps are both fine as long as the file holds
    // every byte the header says the game uses.
    const u32 usedRomSize = h[0x80] | (h[0x81]<<8) | (h[0x82]<<16) | ((u32)h[0x83]<<24);
    const u32 maxRomSize = (isDiamond || isPearl) ? 0x04000000 : 0x08000000;
    if(usedRomSize < 0x4000 || usedRomSize > maxRomSize ||
       (u32)rom.st_size < usedRomSize || (u32)rom.st_size > maxRomSize)
        return gen5PrepareFail("R06: Source ROM is truncated or wrong size");

    // Keep a verified full-save snapshot under the source save folder's
    // transfers/backups/ directory, separate from the active saves.
    unsigned char *data=(unsigned char*)malloc(0x80000);
    if(!data)return gen5PrepareFail("M01: Cannot allocate save backup");
    FILE *input=fopen(c->gen5SavePath,"rb");if(!input){free(data);return gen5PrepareFail("S03: Cannot open source save");}
    ok=fread(data,1,0x80000,input)==0x80000;fclose(input);
    if(!ok){free(data);return gen5PrepareFail("S04: Source save read failed");}
    u32 hash=2166136261u;
    for(u32 i=0;i<0x80000;i++)hash=(hash^data[i])*16777619u;
    const char *saveName=strrchr(c->gen5SavePath,'/');
    if(!saveName){free(data);return gen5PrepareFail("G04: Invalid source paths");}
    const int parentLength=(int)(saveName-c->gen5SavePath);
    char directory[1024];
    int length=snprintf(directory,sizeof(directory),"%.*s/transfers",parentLength,c->gen5SavePath);
    if(length<0 || (size_t)length>=sizeof(directory)){free(data);return gen5PrepareFail("B06: Backup path too long");}
    if(!gen5EnsureBackupDir(directory)){free(data);return gen5PrepareFail("B05: Cannot create backup folder");}
    length=snprintf(directory,sizeof(directory),"%.*s/transfers/backups",parentLength,c->gen5SavePath);
    if(length<0 || (size_t)length>=sizeof(directory)){free(data);return gen5PrepareFail("B06: Backup path too long");}
    if(!gen5EnsureBackupDir(directory)){free(data);return gen5PrepareFail("B05: Cannot create backup folder");}
    length=snprintf(gen5BackupPath,sizeof(gen5BackupPath),"%s/%s.poketransfer-%08lx.bak",directory,saveName+1,(unsigned long)hash);
    if(length<0 || (size_t)length>=sizeof(gen5BackupPath)){free(data);return gen5PrepareFail("B06: Backup path too long");}
    struct stat existing;
    if(stat(gen5BackupPath,&existing)) {
        FILE *output=fopen(gen5BackupPath,"wb");
        if(!output){free(data);return gen5PrepareFail("B01: Cannot create save backup");}
        ok=fwrite(data,1,0x80000,output)==0x80000;if(fclose(output))ok=false;
        if(!ok){free(data);return gen5PrepareFail("B02: Save backup copy failed");}
    }
    // Original verification compares the backup with the full buffered save.
    FILE *backup=fopen(gen5BackupPath,"rb");
    if(!backup){free(data);return gen5PrepareFail("B03: Cannot read save backup");}
    ok=true;
    for(u32 off=0;ok && off<0x80000;off+=512)
        ok=fread(chunk,1,512,backup)==512 && !memcmp(chunk,data+off,512);
    if(ok && fgetc(backup)!=EOF)ok=false;
    fclose(backup);free(data);
    if(!ok)return gen5PrepareFail("B04: Backup verification failed");
    c->gen5RomCluster=rom.st_ino;c->gen5SaveCluster=save.st_ino;c->gen5RomSize=rom.st_size;
    return true;
}
#endif
