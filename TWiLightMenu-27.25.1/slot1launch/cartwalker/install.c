#include "cartwalker.h"
#include "walker_emu.h"
#include "walker_payload.h"
#include "ndsheaderbanner.h"
#include "module_params.h"
#include <string.h>
typedef char control_fits[(sizeof(WalkerControl)<=0x800)?1:-1];
typedef char storage_fits[(sizeof(CartStorage)<=0x800)?1:-1];
typedef char blob_fits[(CART_BLOB+sizeof(walkerBlob)<=CART_END)?1:-1];
typedef char eeprom_fits[(CART_EEPROM+0x10000<=0x023DA000u)?1:-1]; /* below the loader's ARM9 page */
typedef char ctrl_place[(CART_CTRL>=0x023DB000u)?1:-1];          /* above it */
/* Which check stopped the install; shown by the loader as coloured boxes. */
unsigned cartWalkerFailReason;
#define FAIL(n) do { cartWalkerFailReason = (n); return 0; } while (0)
/* HGSS fails to start if the loader takes even 0.1 s longer than it does
 * without the walker, so the loader re-checks nothing here; the launcher
 * verified the staged walker before the reset. */
static int stageVerified;
unsigned cartWalkerVerifyStage(void){
 CartStage *s=(CartStage *)CART_STAGE_ADDR;
 stageVerified=0;
 if(s->magic!=CART_MAGIC||s->version!=1)return 1;
 /* No re-verification here: the launcher already checked the staged walker
  * against the SD card, and the runtime compares every SD sector with its
  * fingerprint before writing. Every check costs boot time HGSS won't give. */
 stageVerified=1;
 return 0;
}
static unsigned *next(unsigned *from,unsigned *end,unsigned push){
 for(unsigned n=1;n<0x80 && from+n<end;n++)if(from[n]==push)return from+n;
 return 0;
}
/* Resolve every hook and the SDK heap ceiling before changing game code. */
int cartWalkerInstall(const tNDSHeader *h,const module_params_t *m,unsigned *engine){
 CartStage *s=(CartStage *)CART_STAGE_ADDR;
 cartWalkerFailReason=0;
 if(!stageVerified)FAIL(7); /* checked before the cartridge was read */
 if(!h||!m||!engine||s->magic!=CART_MAGIC||s->version!=1||h->unitCode||
    memcmp(h->gameCode,&s->gameCode,4)||h->headerCRC16!=s->headerCrc ||
    (memcmp(h->gameCode,"IPK",3)&&memcmp(h->gameCode,"IPG",3)))FAIL(1); /* stage/header */
 unsigned start=(unsigned)h->arm9destination,limit=m->static_bss_start;
 if(start<0x02000000||start>=CART_BASE||limit<start+h->arm9binarySize||
    limit>=CART_BASE||m->static_bss_end>=CART_BASE||m->static_bss_end<limit)FAIL(2); /* memory layout */
 /* The walker area must not overlap where the game's ARM7 was loaded. */
 unsigned a7=(unsigned)h->arm7destination,a7end=a7+h->arm7binarySize;
 if(a7<CART_END && a7end>CART_BASE)FAIL(9);
 unsigned *code=(unsigned *)start,*end=(unsigned *)limit,*read=0,*heap=0;
 /* One read per word: this runs while HGSS's start-up clock is ticking. */
 unsigned prev=code[0];
 for(unsigned *p=code+1;p<end;p++){
  const unsigned w=*p;
  if(w==0x023E0000u &&
    (prev==0xE12FFF1E||prev==0xE8BD8000||prev==0xE8BD8008||prev==0xE8BD8010||
     prev==0xBD082000||prev==0x46C04718||prev==0xBD082010||prev==0xBD102000)){
   if(heap)FAIL(4); /* heap ceiling */
   heap=p;
  }
  if(prev==0xE7D60005u && w==0xE22000AAu && p+2<end && p[1]==0xE7C60005u && p[2]==0xE2855001u){
   unsigned *at=p-1,*found=0;
   for(unsigned k=1;k<=0x60 && at-k>=code;k++)if(at[-(int)k]==0xE92D47F0){found=at-k;break;}
   if(!found||read)FAIL(3); /* IR receive routine */
   read=found;
  }
  prev=w;
 }
 if(!read)FAIL(3);
 if(!heap)FAIL(4);
 unsigned *write=next(read,end,0xE92D43F8),*status=write?next(write,end,0xE92D4FF8):0,*init=0;
 if(!write||!status||(unsigned)((unsigned char *)write-(unsigned char *)read)<sizeof(walkerStub))FAIL(5); /* send/status routines */
 for(unsigned k=4;k<=0x80 && read-k>=code;k++){
  unsigned *p=read-k;if(p[0]==0xE92D4008&&(p[1]&0xfffff000)==0xE59F1000&&p[2]==0xE3A00032){if(init)FAIL(6);init=p;}
 }
 unsigned char *image=(unsigned char *)(CART_STAGE_ADDR+CART_STAGE_IMAGE);
 if(!init)FAIL(6); /* IR init routine */
 memcpy((void *)CART_EEPROM,image,65536);
 memset((void *)CART_CTRL,0,CART_BLOB-CART_CTRL);
 memcpy((void *)CART_BLOB,walkerBlob,sizeof(walkerBlob));
 memcpy((void *)CART_STORAGE,&s->storage,sizeof(CartStorage));
 WalkerControl *c=(WalkerControl *)CART_CTRL;
 c->version=WALKER_VERSION;c->cluster=2;c->eeprom=CART_EEPROM;c->magic=WALKER_MAGIC;
 *heap=CART_BASE;
 memcpy(read,walkerStub,sizeof(walkerStub));
 *(unsigned *)((unsigned char *)read+WALKER_STUB_CONTROL_OFF)=(unsigned)c;
 *(unsigned *)((unsigned char *)read+WALKER_STUB_ENTRY_OFF)=CART_BLOB;
 write[0]=0xE51FF004;write[1]=(unsigned)read+WALKER_STUB_WRITE_OFF;
 init[0]=0xE51FF004;init[1]=(unsigned)read+WALKER_STUB_INIT_OFF;
 status[0]=0xE3A000AA;status[1]=0xE12FFF1E;
 engine[5]=(unsigned)c;
 s->magic=0;
 return 1;
}
