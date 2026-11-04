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
int main(void) {
 char walkerPath[256];
#ifdef _WIN32
 snprintf(walkerPath,sizeof(walkerPath),"work/lease-tests/test-%llu.walker",(unsigned long long)GetTickCount64());
#else
 snprintf(walkerPath,sizeof(walkerPath),"work/lease-tests/test-%ld-%ld.walker",(long)getpid(),(long)time(0));
#endif
 const char *p=walkerPath;char error[192];uint8_t token[16]={1},newToken[16]={2};
 static uint8_t snapshot[65536],phone[65536];WalkerLease loan;struct stat st;
#ifdef _WIN32
 _mkdir("work/lease-tests");
#else
 mkdir("work/lease-tests",0700);
#endif
 const char *suffix[]={".lease",".phone",".returned",".tmp"};char name[600];
 for(unsigned n=0;n<4;n++){snprintf(name,sizeof(name),"%s%s",p,suffix[n]);remove(name);}
 leaseSetReplace(replaceLocal);makeStrolling(1,1000000);walkerResume(ee,1000000,10);
 fileWrite(p,ee,sizeof(ee));assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));
 assert(!stat(p,&st)&&st.st_size==0);assert(leaseInspect(p,&loan)==1);
 assert(!leaseCheckout(p,newToken,phone,&loan,error,sizeof(error)));
 assert(leaseCheckout(p,token,phone,&loan,error,sizeof(error))&&loan.token[0]==1);
 /* Simulate power loss before the placeholder was truncated. Retry must repair it. */
 fileWrite(p,snapshot,sizeof(snapshot));assert(leaseCheckout(p,token,phone,&loan,error,sizeof(error)));
 assert(!stat(p,&st)&&st.st_size==0);
 memcpy(phone,snapshot,sizeof(phone));WalkerReport report;assert(walkerAddSteps(phone,1000100,123,&report));
 assert(!leaseReturn(p,newToken,phone,error,sizeof(error)));assert(!stat(p,&st)&&st.st_size==0);
 assert(leaseReturn(p,token,phone,error,sizeof(error))==1);assert(leaseInspect(p,0)==0);
 assert(leaseReturn(p,token,phone,error,sizeof(error))==2); /* idempotent response */
 assert(!remove(p));assert(!leaseReturn(p,token,phone,error,sizeof(error)));
 fileWrite(p,phone,sizeof(phone));assert(leaseReturn(p,token,phone,error,sizeof(error))==2);
 assert(leaseCheckout(p,newToken,snapshot,&loan,error,sizeof(error)));assert(leaseForceUnlock(p,error,sizeof(error)));
 assert(leaseInspect(p,0)==0&&!stat(p,&st)&&st.st_size==65536);
 assert(!leaseReturn(p,newToken,phone,error,sizeof(error))); /* revoked phone can never upload */
 assert(leaseSessionRevoked(p,newToken));assert(!leaseCheckout(p,newToken,snapshot,&loan,error,sizeof(error)));
 token[0]=3;assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));
 assert(!leaseReturn(p,newToken,snapshot,error,sizeof(error)));assert(leaseReturn(p,token,snapshot,error,sizeof(error)));

 /* HGSS was reset / a different populated walker was copied over the placeholder.
  * Same-token retries and old returns must leave this new file intact. */
 token[0]=4;newToken[0]=5;
 makeStrolling(1,2000000);walkerResume(ee,2000000,10);fileWrite(p,ee,sizeof(ee));
 assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));memcpy(phone,snapshot,sizeof(phone));
 static uint8_t newer[65536];makeStrolling(1,3000000);ee[0x8f00]=25;memcpy(newer,ee,sizeof(newer));fileWrite(p,newer,sizeof(newer));
 assert(!leaseCheckout(p,token,ee,&loan,error,sizeof(error)));sameContents(p,newer);
 assert(!leaseCheckout(p,newToken,ee,&loan,error,sizeof(error)));sameContents(p,newer);
 assert(!leaseReturn(p,token,phone,error,sizeof(error)));sameContents(p,newer);
 historyName(name,p,token,"checkout",snapshot);sameContents(name,snapshot);
 historyName(name,p,token,"return",phone);sameContents(name,phone);
 assert(leaseForceUnlock(p,error,sizeof(error)));sameContents(p,snapshot);
 historyName(name,p,token,"conflict",newer);sameContents(name,newer);
 assert(leaseSessionRevoked(p,token));
 /* A later, valid checkout can use the new Pokemon without erasing old history. */
 fileWrite(p,newer,sizeof(newer));assert(leaseCheckout(p,newToken,ee,&loan,error,sizeof(error)));
 assert(!leaseReturn(p,token,phone,error,sizeof(error)));
 assert(leaseReturn(p,newToken,newer,error,sizeof(error)));sameContents(p,newer);
 assert(leaseSessionRevoked(p,token)); /* revocation survives subsequent receipts */
 assert(!leaseCheckout(p,token,ee,&loan,error,sizeof(error)));sameContents(p,newer);
 assert(!leaseCheckout(p,newToken,ee,&loan,error,sizeof(error))); /* completed token cannot be reused */
 makeStrolling(1,4000000);fileWrite(p,ee,sizeof(ee));
 assert(leaseReturn(p,newToken,newer,error,sizeof(error))==2);sameContents(p,ee);
 assert(!leaseReturn(p,token,phone,error,sizeof(error)));sameContents(p,ee);
 /* A damaged existing history file is never overwritten to make sync proceed. */
 token[0]=9;historyName(name,p,token,"checkout",ee);memcpy(newer,ee,sizeof(newer));newer[0]^=1;fileWrite(name,newer,sizeof(newer));
 assert(!leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));sameContents(p,ee);sameContents(name,newer);assert(leaseInspect(p,0)==0);
 /* An empty registered walker may return unchanged, or with its own course join.
  * It may not use that exception to swap trainer, course, or companion. */
 token[0]=10;makeSolo(5000000);walkerResume(ee,5000000,10);fileWrite(p,ee,sizeof(ee));
 assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));
 assert(leaseReturn(p,token,snapshot,error,sizeof(error))==1);
 token[0]=11;makeSolo(6000000);walkerResume(ee,6000000,10);soloGuaranteedRoll();fileWrite(p,ee,sizeof(ee));
 assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));
 walkerAddSteps(ee,6003600,300,&report);assert(report.joined==1);memcpy(phone,ee,sizeof(phone));
 ee[0xed+12]^=1;seal(0xed,0x1ed,104);
 assert(!leaseReturn(p,token,ee,error,sizeof(error)) && leaseInspect(p,0)==1);
 memcpy(ee,phone,sizeof(ee));ee[WALKER_ROUTE]=26;
 assert(!leaseReturn(p,token,ee,error,sizeof(error)));
 memcpy(ee,phone,sizeof(ee));ee[WALKER_ROUTE+0x52+32]=26;
 assert(!leaseReturn(p,token,ee,error,sizeof(error)));
 assert(leaseReturn(p,token,phone,error,sizeof(error))==1);sameContents(p,phone);
 assert(leaseReturn(p,token,phone,error,sizeof(error))==2);
 /* Missing target after an old-build delete: normal revoked upload remains
  * refused, but explicitly selected phone recovery can preserve that Pokemon. */
 token[0]=21;makeStrolling(1,8000000);walkerResume(ee,8000000,10);fileWrite(p,ee,sizeof(ee));
 assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));memcpy(phone,snapshot,sizeof(phone));
 assert(walkerAddSteps(phone,8000100,123,&report));
 assert(leaseForceUnlock(p,error,sizeof(error)));assert(!remove(p));
 assert(!leaseReturn(p,token,phone,error,sizeof(error)));assert(leaseSessionRevoked(p,token));
 memcpy(ee,snapshot,sizeof(ee));ee[0xed+12]^=1;seal(0xed,0x1ed,104);memcpy(newer,ee,sizeof(newer));
 assert(!leaseRecoverPhone(p,token,newer,snapshot,error,sizeof(error)));
 assert(leaseRecoverPhone(p,token,phone,snapshot,error,sizeof(error))==1);sameContents(p,phone);
 assert(leaseSessionRevoked(p,token)); /* Explicit recovery does not erase revocation. */
 assert(leaseReturn(p,token,phone,error,sizeof(error))==2);sameContents(p,phone);
 assert(leaseRecoverPhone(p,token,phone,snapshot,error,sizeof(error))==2);
 assert(!remove(p));assert(!leaseReturn(p,token,phone,error,sizeof(error)));fileWrite(p,phone,sizeof(phone));
 newToken[0]=22;assert(leaseCheckout(p,newToken,ee,&loan,error,sizeof(error)));
 assert(!leaseRecoverPhone(p,token,phone,snapshot,error,sizeof(error)));assert(!leaseReturn(p,token,phone,error,sizeof(error)));
 assert(leaseReturn(p,newToken,ee,error,sizeof(error)));sameContents(p,ee);
 token[0]=23;makeStrolling(1,9000000);memcpy(newer,ee,sizeof(newer));fileWrite(p,newer,sizeof(newer));
 assert(!leaseRecoverPhone(p,token,phone,snapshot,error,sizeof(error)));sameContents(p,newer);
 /* No active file, receipt, sidecar or backup is required for an explicitly
  * selected new destination. Never acknowledge a receipt-only missing file. */
 char absent[600];snprintf(absent,sizeof(absent),"%s.missing.walker",p);
 token[0]=31;makeStrolling(1,10000000);walkerResume(ee,10000000,10);
 assert(stat(absent,&st)&&errno==ENOENT);
 assert(leaseRecoverPhone(absent,token,ee,0,error,sizeof(error))==1);sameContents(absent,ee);
 assert(leaseReturn(absent,token,ee,error,sizeof(error))==2);sameContents(absent,ee);
 assert(!remove(absent));assert(!leaseReturn(absent,token,ee,error,sizeof(error)));
 assert(!leaseRecoverPhone(absent,token,ee,0,error,sizeof(error)));
 puts("Explicit phone recovery also creates a verified target with all local files absent and refuses receipt-only missing-file success.");
 puts("Explicit phone recovery restores a deleted target, keeps old revocation, verifies final data, handles lost ACKs and refuses active/conflicting owners.");
 puts("registered empty handoff, original auto-joined companion return and mismatched trainer/course/Pokemon refusal passed");
 puts("conflicting Pokemon refusal, immutable snapshots, durable revocation and late-return checks passed");
 puts("checkout lock, crash retry, return, force-unlock revocation and new-session checks passed");
 return 0;
}
