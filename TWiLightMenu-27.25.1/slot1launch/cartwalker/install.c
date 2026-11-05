#include "cartwalker.h"
#include "walker_emu.h"
#include "walker_payload.h"
#include "ndsheaderbanner.h"
#include "module_params.h"
#include <string.h>
typedef char control_fits[(sizeof(WalkerControl)<=0x800)?1:-1];
typedef char storage_fits[(sizeof(CartStorage)<=0x800)?1:-1];
static unsigned *next(unsigned *from,unsigned *end,unsigned push){
 for(unsigned n=1;n<0x80 && from+n<end;n++)if(from[n]==push)return from+n;
 return 0;
}
/* Resolve every hook and the SDK heap ceiling before changing game code. */
int cartWalkerInstall(const tNDSHeader *h,const module_params_t *m,unsigned *engine){
 CartStage *s=(CartStage *)CART_STAGE_ADDR;
 if(!h||!m||!engine||s->magic!=CART_MAGIC||s->version!=1||h->unitCode||
    memcmp(h->gameCode,&s->gameCode,4)||h->headerCRC16!=s->headerCrc ||
    (memcmp(h->gameCode,"IPK",3)&&memcmp(h->gameCode,"IPG",3)))return 0;
 unsigned start=(unsigned)h->arm9destination,limit=m->static_bss_start;
 if(start<0x02000000||start>=CART_BASE||limit<start+h->arm9binarySize||
    limit>=CART_BASE||m->static_bss_end>=CART_BASE||m->static_bss_end<limit)return 0;
 unsigned *code=(unsigned *)start,*end=(unsigned *)limit,*read=0,*heap=0;
 for(unsigned *p=code;p+4<=end;p++){
  if(p[0]==0xE7D60005 && p[1]==0xE22000AA && p[2]==0xE7C60005 && p[3]==0xE2855001){
   unsigned *found=0;for(unsigned k=1;k<=0x60 && p-k>=code;k++)if(p[-(int)k]==0xE92D47F0){found=p-k;break;}
   if(!found||read)return 0;
   read=found;
  }
  if(p+1<end && p[1]==0x023E0000 &&
    (p[0]==0xE12FFF1E||p[0]==0xE8BD8000||p[0]==0xE8BD8008||p[0]==0xE8BD8010||
     p[0]==0xBD082000||p[0]==0x46C04718||p[0]==0xBD082010||p[0]==0xBD102000)){
   if(heap)return 0;
   heap=p+1;
  }
 }
 if(!read||!heap)return 0;
 unsigned *write=next(read,end,0xE92D43F8),*status=write?next(write,end,0xE92D4FF8):0,*init=0;
 if(!write||!status||(unsigned)((unsigned char *)write-(unsigned char *)read)<sizeof(walkerStub))return 0;
 for(unsigned k=4;k<=0x80 && read-k>=code;k++){
  unsigned *p=read-k;if(p[0]==0xE92D4008&&(p[1]&0xfffff000)==0xE59F1000&&p[2]==0xE3A00032){if(init)return 0;init=p;}
 }
 unsigned char *image=(unsigned char *)(CART_STAGE_ADDR+CART_STAGE_IMAGE);
 if(!init||s->imageCrc!=cartCrc(image,65536)||s->storage.mapCrc!=cartMapSignature(&s->storage))return 0;
 for(unsigned i=0;i<128;i++){
  if(!s->storage.sector[i]||s->storage.crc[i]!=cartCrc(image+i*512,512))return 0;
  for(unsigned j=0;j<i;j++)if(s->storage.sector[j]==s->storage.sector[i])return 0;
 }
 memset((void *)CART_BASE,0,WALKER_RESERVE);
 memcpy((void *)CART_BASE,image,65536);
 memcpy((void *)(CART_BASE+WALKER_BLOB_OFF),walkerBlob,sizeof(walkerBlob));
 memcpy((void *)(CART_BASE+CART_STORAGE_OFF),&s->storage,sizeof(CartStorage));
 WalkerControl *c=(WalkerControl *)(CART_BASE+WALKER_CONTROL_OFF);
 c->version=WALKER_VERSION;c->cluster=2;c->eeprom=CART_BASE;c->magic=WALKER_MAGIC;
 *heap=CART_BASE;
 memcpy(read,walkerStub,sizeof(walkerStub));
 *(unsigned *)((unsigned char *)read+WALKER_STUB_CONTROL_OFF)=(unsigned)c;
 *(unsigned *)((unsigned char *)read+WALKER_STUB_ENTRY_OFF)=CART_BASE+WALKER_BLOB_OFF;
 write[0]=0xE51FF004;write[1]=(unsigned)read+WALKER_STUB_WRITE_OFF;
 init[0]=0xE51FF004;init[1]=(unsigned)read+WALKER_STUB_INIT_OFF;
 status[0]=0xE3A000AA;status[1]=0xE12FFF1E;
 engine[5]=(unsigned)c;
 s->magic=0;
 return 1;
}
