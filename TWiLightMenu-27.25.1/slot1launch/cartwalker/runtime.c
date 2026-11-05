#include <nds/ndstypes.h>
#include <string.h>
#include "cartwalker.h"
#include "walker_emu.h"
#include "my_sdmmc.h"
extern unsigned cartwalker_control;
static unsigned char verify[512] __attribute__((aligned(4)));
static int busy;
/* Raw writes are confined to the validated, preallocated 64 KiB file map.
 * Do not acknowledge completion after short/failed/silently dropped writes. */
int cartWalkerPoll(int force){
 WalkerControl *c=(WalkerControl *)cartwalker_control;
 if(!c)return 1;
 if((unsigned)c!=CART_BASE+WALKER_CONTROL_OFF||c->magic!=WALKER_MAGIC||c->version!=WALKER_VERSION)return 0;
 if(c->writeSeq==c->flushedSeq)return 1;
 if(busy)return 0;
 if(!force && (c->state || c->respLen<8 || c->resp[0]!=0x68))return 0;
 busy=1;
 CartStorage *store=(CartStorage *)(CART_BASE+CART_STORAGE_OFF);
 if(store->mapCrc!=cartMapSignature(store))goto failed;
 if(!store->initialized){
  sdmmc_init();
  if(SD_Init())goto failed;
  /* Detect a changed card/map before the first raw write. */
  for(unsigned n=0;n<128;n++)
   if(my_sdmmc_sdcard_readsectors(store->sector[n],1,verify)||cartCrc(verify,512)!=store->crc[n])goto failed;
  store->initialized=1;
 }
 u32 cid[4];
 if(my_sdmmc_get_cid(false,cid)||memcmp(cid,store->cid,sizeof(cid)))goto failed;
 unsigned seq=c->writeSeq;
 for(unsigned sector=0;sector<WALKER_SECTORS;sector++){
  if(!c->dirty[sector])continue;
  c->dirty[sector]=0;
  for(unsigned part=0;part<8;part++){
   unsigned n=sector*8+part;
   const unsigned char *data=(const unsigned char *)(c->eeprom+n*512);
   if(my_sdmmc_sdcard_readsectors(store->sector[n],1,verify)){
    c->dirty[sector]=1;goto failed;
   }
   unsigned desired=cartCrc(data,512),current=cartCrc(verify,512);
   if(current==desired){store->crc[n]=desired;continue;}
   if(current!=store->crc[n]){c->dirty[sector]=1;goto failed;}
   if(my_sdmmc_sdcard_writesectors(store->sector[n],1,data)||
      my_sdmmc_sdcard_readsectors(store->sector[n],1,verify)||memcmp(verify,data,512)){
    c->dirty[sector]=1;goto failed;
   }
   store->crc[n]=cartCrc(data,512);
  }
  if(!force)break;
 }
 for(unsigned n=0;n<WALKER_SECTORS;n++)if(c->dirty[n]){busy=0;return 0;}
 c->flushedSeq=seq;c->flushes++;store->error=0;busy=0;return 1;
failed:
 store->error=1;busy=0;return 0;
}
