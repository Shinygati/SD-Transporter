/* Execute the production HTTP parser and lease implementation without a socket. */
#ifdef TEST_NDS
#include "../../nds/source/syncnet.c"
#else
#include "../source/syncnet.c"
#endif
#include <windows.h>
#include <assert.h>
static int counts[3];
static int falsePublish;
static const char *boundTrainer="";
static void event(int value){assert(value==1||value==2);counts[value]++;}
static int replaceLocal(const char *a,const char *b){
    int result=MoveFileExA(a,b,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
    if(!result && falsePublish && !strcmp(b,selected)) { FILE *f=fopen(b,"wb");assert(f);fclose(f); }
    return result;
}
static unsigned char fixture[65536],newer[65536],conflict[65536],check[65536];
static void writeFile(const char *path,const void *data,unsigned size){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,size,f)==size);assert(!fclose(f));}
static unsigned readFile(const char *path,void *data){FILE *f=fopen(path,"rb");assert(f);unsigned n=(unsigned)fread(data,1,65536,f);assert(!fclose(f));return n;}
static void matches(const char *path,const void *data){assert(readFile(path,check)==65536);assert(!memcmp(check,data,65536));}
static int exchange(const char *method,const char *token,const void *body){
    memset(request,0,sizeof(request));
    headLength=(unsigned)snprintf((char*)request,sizeof(request),"%s /walker HTTP/1.1\r\nX-Walker-Session: %s\r\nX-Walker-Trainer: %s\r\nContent-Length: %u\r\n\r\n",method,token,boundTrainer,body?65536:0);
    received=headLength+(body?65536:0);bodyLength=0;responding=0;
    if(body)memcpy(request+headLength,body,65536);
    if(!*boundTrainer){char *at=strstr((char*)request,"X-Walker-Trainer: \r\n");if(at){unsigned removed=(unsigned)strlen("X-Walker-Trainer: \r\n");memmove(at,at+removed,received-(unsigned)(at-(char*)request)-removed);headLength-=removed;received-=removed;}}
    handle();assert(responding);int status=0;assert(sscanf(response,"HTTP/1.1 %d",&status)==1);
    if(status==200 && body){char expected[80];snprintf(expected,sizeof(expected),"X-Walker-Stored-CRC32: %08lX",(unsigned long)walkerCrc(body,65536));assert(strstr(response,expected));snprintf(expected,sizeof(expected),"PWRETURN1 %s %08lX\n",token,(unsigned long)walkerCrc(body,65536));assert(responseLength==strlen(expected)&&!memcmp(responseBody,expected,responseLength));}
    return status;
}
int main(int argc,char **argv){
    assert(argc==3);assert(readFile(argv[1],fixture)==65536);
    snprintf(selected,sizeof(selected),"%s",argv[2]);writeFile(selected,fixture,65536);
    leaseSetReplace(replaceLocal);onEvent=event;
    const char *a="11111111111111111111111111111111",*b="22222222222222222222222222222222",*c="33333333333333333333333333333333";
    char actualTrainer[37],wrongTrainer[37];assert(walkerTrainerKey(fixture,actualTrainer));strcpy(wrongTrainer,actualTrainer);wrongTrainer[0]=wrongTrainer[0]=='0'?'1':'0';
    boundTrainer=wrongTrainer;assert(exchange("GET",a,0)==409);matches(selected,fixture);assert(leaseInspect(selected,0)==0&&counts[1]==0);
    boundTrainer=actualTrainer;
    assert(exchange("GET",a,0)==200);assert(counts[1]==1);boundTrainer="";assert(readFile(selected,check)==0);
    falsePublish=1;assert(exchange("POST",a,fixture)==503);assert(counts[2]==0);assert(leaseInspect(selected,0)==1);
    falsePublish=0;
    assert(exchange("POST",a,fixture)==200);assert(counts[2]==1);matches(selected,fixture);
    memcpy(newer,fixture,65536);newer[0x933e]^=1;writeFile(selected,newer,65536);
    assert(exchange("POST",a,fixture)==200);assert(counts[2]==1);matches(selected,newer);
    assert(exchange("GET",b,0)==200);assert(counts[1]==2);
    assert(exchange("POST",a,fixture)==409);assert(counts[2]==1);assert(readFile(selected,check)==0); /* Never acknowledge an empty final filename, even for an old receipt. */
    memcpy(conflict,newer,65536);conflict[0x933f]^=2;writeFile(selected,conflict,65536);
    assert(exchange("GET",b,0)==409);matches(selected,conflict);
    assert(exchange("POST",b,newer)==503);assert(counts[2]==1);matches(selected,conflict);
    char error[192];assert(leaseForceUnlock(selected,error,sizeof(error)));matches(selected,newer);
    assert(exchange("POST",b,newer)==410);assert(exchange("GET",c,0)==200);
    assert(exchange("POST",b,newer)==410);assert(exchange("POST",c,newer)==200);assert(counts[2]==2);
    puts("Production HTTP parser + real leases: false rename success never acknowledged; stored CRC confirmed; duplicate callbacks suppressed; conflicts and old revocations preserved.");
    return 0;
}
