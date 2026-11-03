/* Immutable pre-launch walker recovery snapshots. GPL-3.0-or-later. */
#ifndef WALKER_HISTORY_H
#define WALKER_HISTORY_H
#include "recovery_limit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
static bool walkerHistoryDirectory(const std::string& path) {
    struct stat st;
    if(!stat(path.c_str(),&st))return (st.st_mode&S_IFMT)==S_IFDIR;
#ifdef _WIN32
    return _mkdir(path.c_str())==0;
#else
    return mkdir(path.c_str(),0777)==0;
#endif
}
static bool walkerHistoryMatches(const std::string& path,const unsigned char *data) {
    FILE *f=fopen(path.c_str(),"rb");if(!f)return false;
    unsigned char block[512];bool ok=true;
    for(unsigned offset=0;ok && offset<65536;offset+=sizeof(block))
        ok=fread(block,1,sizeof(block),f)==sizeof(block) && !memcmp(block,data+offset,sizeof(block));
    if(ok)ok=fgetc(f)==EOF && !ferror(f);
    if(fclose(f))ok=false;
    return ok;
}
static bool walkerPrelaunchSnapshot(const std::string& source) {
    FILE *f=fopen(source.c_str(),"rb");if(!f)return false;
    unsigned char *data=(unsigned char*)malloc(65536);
    if(!data){fclose(f);return false;}
    bool ok=fread(data,1,65536,f)==65536 && fgetc(f)==EOF && !ferror(f);
    if(fclose(f))ok=false;
    if(!ok){free(data);return false;}
    unsigned crc=~0u;
    for(unsigned i=0;i<65536;i++) {
        crc^=data[i];
        for(unsigned b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    const size_t slash=source.find_last_of("/\\");
    const std::string parent=slash==std::string::npos?"":source.substr(0,slash+1);
    const std::string name=slash==std::string::npos?source:source.substr(slash+1);
    const std::string transfers=parent+"transfers", directory=transfers+"/backups";
    char suffix[40];snprintf(suffix,sizeof(suffix),".prelaunch-%08x.bak",~crc);
    const std::string backup=directory+"/"+name+suffix;
    ok=walkerHistoryDirectory(transfers) && walkerHistoryDirectory(directory);
    struct stat st;
    if(ok && !stat(backup.c_str(),&st))ok=walkerHistoryMatches(backup,data);
    else if(ok && errno!=ENOENT)ok=false;
    else if(ok) {
        const std::string temporary=backup+".tmp";
        f=fopen(temporary.c_str(),"wb");
        if(!f)ok=false;
        else {
            ok=fwrite(data,1,65536,f)==65536;
            if(fclose(f))ok=false;
            if(ok)ok=walkerHistoryMatches(temporary,data);
            if(ok)ok=rename(temporary.c_str(),backup.c_str())==0;
            if(ok)ok=walkerHistoryMatches(backup,data);
        }
    }
    free(data);return ok && walkerPruneRecovery(source.c_str(),backup.c_str());
}
#endif
