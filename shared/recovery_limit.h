/* Verified recovery snapshots only. Ownership files are never retention targets. */
#ifndef RECOVERY_LIMIT_H
#define RECOVERY_LIMIT_H
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#ifdef __3DS__
#include <3ds/archive.h>
#endif
/* libctru stat() omits dates. archive_getmtime() already returns UNIX seconds;
 * do not apply another 2000 epoch offset or milliseconds conversion here. */
static time_t walkerRecoveryTimestamp(const char *path,const struct stat *st) {
#ifdef __3DS__
 uint64_t seconds=0;
 if(archive_getmtime(path,&seconds)==0) {
  time_t value=(time_t)seconds;
  if(value>=(time_t)315532800 && (uint64_t)value==seconds)return value;
 }
#else
 (void)path;
#endif
 return st->st_mtime>=(time_t)315532800?st->st_mtime:0;
}
static int walkerPruneRecovery(const char *walker,const char *keep) {
 const char *base=strrchr(walker,'/'),*back=strrchr(walker,'\\');if(back&&(!base||back>base))base=back;base=base?base+1:walker;
 char directory[600],prefix[256];
 if(strlen(base)+2>sizeof(prefix))return 0;
 snprintf(prefix,sizeof(prefix),"%s.",base);
 if(snprintf(directory,sizeof(directory),"%.*stransfers/backups",(int)(base-walker),walker)>=(int)sizeof(directory))return 0;
 for(;;) {
  DIR *dir=opendir(directory);if(!dir)return 0;
  struct dirent *entry;unsigned count=0;char oldest[900]="";time_t stamp=0;
  while((entry=readdir(dir))) {
   const char *name=entry->d_name;size_t n=strlen(name),p=strlen(prefix);
   if(n<p+5||strncmp(name,prefix,p)||strcmp(name+n-4,".bak"))continue;
   const char *kind=name+p;
   if(strncmp(kind,"prelaunch-",10)&&strncmp(kind,"checkout-",9)&&strncmp(kind,"return-",7)&&strncmp(kind,"recovery-",9)&&strncmp(kind,"conflict-",9)&&strncmp(kind,"deleted-",8)&&strncmp(kind,"reward-",7))continue;
   char path[900];struct stat st;
   if(snprintf(path,sizeof(path),"%s/%s",directory,name)>=(int)sizeof(path)||lstat(path,&st)||!S_ISREG(st.st_mode)||st.st_size!=65536)continue;
   count++;time_t modified=walkerRecoveryTimestamp(path,&st);
   if(strcmp(path,keep)&&(!oldest[0]||modified<stamp||(modified==stamp&&strcmp(path,oldest)<0))){snprintf(oldest,sizeof(oldest),"%s",path);stamp=modified;}
  }
  closedir(dir);if(count<=10)return 1;
  if(!oldest[0]||remove(oldest))return 0;
 }
}
#endif
