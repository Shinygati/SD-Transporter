#include <assert.h>
#define main coreChecks
#include "test_walkerfile.c"
#undef main
#include "lease.h"
#include <sys/stat.h>
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
int main(void) {
 const char *p="work/lease-tests/synthetic.walker";char error[192];uint8_t token[16]={1},newToken[16]={2};
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
 assert(leaseCheckout(p,newToken,snapshot,&loan,error,sizeof(error)));assert(leaseForceUnlock(p,error,sizeof(error)));
 assert(leaseInspect(p,0)==0&&!stat(p,&st)&&st.st_size==65536);
 assert(!leaseReturn(p,newToken,phone,error,sizeof(error))); /* revoked phone can never upload */
 assert(leaseSessionRevoked(p,newToken));assert(!leaseCheckout(p,newToken,snapshot,&loan,error,sizeof(error)));
 token[0]=3;assert(leaseCheckout(p,token,snapshot,&loan,error,sizeof(error)));
 assert(!leaseReturn(p,newToken,snapshot,error,sizeof(error)));assert(leaseReturn(p,token,snapshot,error,sizeof(error)));
 puts("checkout lock, crash retry, return, force-unlock revocation and new-session checks passed");
 return 0;
}
