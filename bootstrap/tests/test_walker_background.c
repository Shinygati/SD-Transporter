/* Exercise the production DSi/3DS background writer with a checked-I/O mock. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nds/ndstypes.h>
#include "my_fat.h"
#include "walker_emu.h"
#include "walker_file.h"
u32 walkerControl;
static unsigned chunks,failAt;
static bool mutate;
static unsigned char media[WALKER_EEPROM_SIZE];
static WalkerControl* ctl;
void getFileFromCluster(aFile* f,unsigned cluster,bool unused){(void)unused;f->cluster=cluster;}
bool gen5CheckedIO(void* p,aFile* f,u32 at,u32 size,bool writing){(void)f;if(!writing||size!=512||at+size>sizeof(media))exit(3);chunks++;if(failAt&&chunks==failAt)return false;memcpy(media+at,p,size);if(mutate){mutate=false;ctl->dirty[0]=1;ctl->writeSeq++;}return true;}
static void check(int condition,const char* message){if(!condition){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
static void dirtyAll(void){memset(ctl->dirty,1,sizeof(ctl->dirty));ctl->writeSeq++;chunks=0;}
int main(void){void* memory=VirtualAlloc((void*)0x10000000,0x20000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);check(memory!=NULL,"map native test pointers below 4 GiB");ctl=(WalkerControl*)memory;walkerControl=(u32)(uintptr_t)ctl;ctl->magic=WALKER_MAGIC;ctl->version=WALKER_VERSION;ctl->cluster=2;ctl->eeprom=(u32)(uintptr_t)((unsigned char*)memory+0x10000);memset((void*)(uintptr_t)ctl->eeprom,0x6b,WALKER_EEPROM_SIZE);
 dirtyAll();check(!walkerDue(false),"first change restarts idle delay");for(int n=0;n<29;n++)check(!walkerDue(false),"keep initial coalescing delay");check(walkerDue(false),"idle work becomes due");
 for(unsigned frame=0;frame<16;frame++){chunks=0;walkerWrite(false);check(chunks==8,"one 4 KiB sector per background frame");if(frame<15){check(ctl->flushedSeq!=ctl->writeSeq,"do not report partial flush complete");check(walkerDue(false),"next background sector is due immediately");}}
 check(ctl->flushedSeq==ctl->writeSeq,"complete only after all sectors");check(!memcmp(media,(void*)(uintptr_t)ctl->eeprom,sizeof(media)),"all data persisted");
 dirtyAll();walkerWrite(true);check(chunks==128&&ctl->flushedSeq==ctl->writeSeq,"forced flush drains all sixteen sectors");
 dirtyAll();failAt=3;walkerWrite(false);check(chunks==3&&ctl->dirty[0]&&ctl->flushedSeq!=ctl->writeSeq,"failed write remains dirty");failAt=0;chunks=0;walkerWrite(true);check(chunks==128&&ctl->flushedSeq==ctl->writeSeq,"forced retry completes");
 memset(ctl->dirty,0,sizeof(ctl->dirty));ctl->dirty[0]=1;ctl->writeSeq++;mutate=true;chunks=0;walkerWrite(false);check(ctl->dirty[0]&&ctl->flushedSeq!=ctl->writeSeq,"concurrent ARM9 change is not cleared");chunks=0;walkerWrite(true);check(chunks==8&&ctl->flushedSeq==ctl->writeSeq,"concurrent sector saved on next pass");
 ctl->dirty[0]=1;ctl->writeSeq++;ctl->respLen=8;ctl->resp[0]=0x68;ctl->state=0;check(walkerDue(false),"completion starts flushing immediately");chunks=0;walkerWrite(false);check(chunks==8&&ctl->writeSeq==ctl->flushedSeq,"completion flush is still verified");
 VirtualFree(memory,0,MEM_RELEASE);puts("PASS: production background writer: 8 checked chunks per frame vs 128 in a forced flush; completion, failed writes/retry and concurrent re-dirty preserved.");return 0;}
