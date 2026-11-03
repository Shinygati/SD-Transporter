#include <assert.h>
#define main coreChecks
#include "test_walkerfile.c"
#undef main
#include "lease.h"
#include <sys/stat.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif
static int replaceLocal(const char *a,const char *b) {
#ifdef _WIN32
 return MoveFileExA(a,b,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
#else
 return rename(a,b);
#endif
}
static void fileWrite(const char *p,const void *data,unsigned n){FILE*f=fopen(p,"wb");assert(f);assert(fwrite(data,1,n,f)==n);fclose(f);}
static void sameContents(const char *p,const uint8_t *data) {
 static uint8_t check[65536];FILE*f=fopen(p,"rb");assert(f);assert(fread(check,1,sizeof(check),f)==sizeof(check));assert(fgetc(f)==EOF);fclose(f);assert(!memcmp(check,data,sizeof(check)));
}
static void historyName(char name[600],const char *p,const uint8_t token[16],const char *kind,const uint8_t *data) {
 char text[33];leaseTokenText(token,text);const char *base=strrchr(p,'/');assert(base);
 snprintf(name,600,"%.*s/transfers/backups/%s.%s-%s-%08lx.bak",(int)(base-p),p,base+1,kind,text,(unsigned long)walkerCrc(data,65536));
}
static int failAt, writes, dropWrite;
static int faultyReplace(const char *a,const char *b) {
 if(++writes==failAt)return dropWrite?0:-1;
 return replaceLocal(a,b);
}
int main(void) {
 char p[256],name[600],error[192];uint8_t token[16]={99};
 static uint8_t snapshot[65536],incoming[65536];WalkerLease loan;
#ifdef _WIN32
 unsigned long long run=(unsigned long long)GetTickCount64();_mkdir("work/lease-tests");
#else
 unsigned long long run=(unsigned long long)time(0);mkdir("work/lease-tests",0700);
#endif
 for(int phase=0;phase<2;phase++)for(int drop=0;drop<2;drop++)for(int boundary=1;boundary<=(phase?4:3);boundary++) {
  snprintf(p,sizeof(p),"work/lease-tests/fault-%llu-%d-%d-%d.walker",run,phase,drop,boundary);
  makeStrolling(1,7000000);walkerResume(ee,7000000,10);fileWrite(p,ee,sizeof(ee));
  memcpy(incoming,ee,sizeof(incoming));leaseSetReplace(replaceLocal);
  if(phase)assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));
  failAt=boundary;writes=0;dropWrite=drop;leaseSetReplace(faultyReplace);
  if(!phase) {
   assert(!leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));
   sameContents(p,ee); /* No unconfirmed checkout has cleared the original. */
   leaseSetReplace(replaceLocal);
   assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));
   snprintf(name,sizeof(name),"%s.phone",p);sameContents(name,ee);
   /* Receiver never saved, app exited: explicit recovery must restore it. */
   assert(leaseForceUnlock(p,error,sizeof(error)));sameContents(p,ee);
  } else {
   WalkerReport report;assert(walkerAddSteps(incoming,7000100,123,&report));
   assert(!leaseReturn(p,token,incoming,error,sizeof(error)));
   snprintf(name,sizeof(name),"%s.phone",p);sameContents(name,ee);
   leaseSetReplace(replaceLocal);
   assert(leaseReturn(p,token,incoming,error,sizeof(error))==1);sameContents(p,incoming);
   historyName(name,p,token,"return",incoming);sameContents(name,incoming);
   assert(leaseReturn(p,token,incoming,error,sizeof(error))==2);
  }
 }
 puts("14 handoff storage-fault cases passed: failed/silently dropped checkout and return writes retain recoverable data; unreceived checkout recovers; return retry stores progress before acknowledgement.");
 return 0;
}
