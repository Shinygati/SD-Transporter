#include <nds.h>
#include <nds/disc_io.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "cartwalker.h"
#include "walker_history.h"
extern "C" {
#include "walkerfile.h"
#include "species.h"
extern const DISC_INTERFACE __io_dsisd;
}
static int sectorRead(void *,uint32_t sector,unsigned char *out){return __io_dsisd.readSectors(sector,1,out);}
int cartWalkerPrepare(const char code[4],unsigned headerCrc){
 CartStage *stage=(CartStage *)CART_STAGE_ADDR;
 unsigned char *image=(unsigned char *)(CART_STAGE_ADDR+CART_STAGE_IMAGE);
 stage->magic=0;
 if(!isDSiMode() || (memcmp(code,"IPK",3)&&memcmp(code,"IPG",3))){
  iprintf("Real HGSS cartridge and DSi mode\nare required.\n");return 0;
 }
 mkdir("sd:/roms",0777);mkdir("sd:/roms/nds",0777);mkdir("sd:/roms/nds/saves",0777);
 char path[160],lease[180];
 snprintf(path,sizeof(path),"sd:/roms/nds/saves/Cartridge-%.4s-%04X.walker",code,headerCrc);
 snprintf(lease,sizeof(lease),"%s.lease",path);
 struct stat info;
 if(!stat(lease,&info)||errno!=ENOENT){iprintf("Pokemon is checked out to a phone,\nor its ownership cannot be read.\n");return 0;}
 if(stat(path,&info)){
  if(errno!=ENOENT){iprintf("Cannot inspect the active walker.\n");return 0;}
  /* Do not hide an interrupted save by creating an empty replacement. */
  const char *suffix[]={".bak",".tmp",".replace-old",".bak.tmp",".phone"};
  for(unsigned i=0;i<sizeof(suffix)/sizeof(*suffix);i++){
   snprintf(lease,sizeof(lease),"%s%s",path,suffix[i]);
   if(!stat(lease,&info)||errno!=ENOENT){iprintf("Missing active walker. Recover it\nin the companion before launching.\n");return 0;}
  }
  memset(image,0xff,65536);FILE *f=fopen(path,"wb");
  if(!f){iprintf("Cannot create the walker file.\n");return 0;}
  bool ok=fwrite(image,1,65536,f)==65536;
  if(fflush(f)||fsync(fileno(f)))ok=false;
  if(fclose(f))ok=false;
  if(!ok){iprintf("Walker creation failed.\n");return 0;}
 }
 FILE *f=fopen(path,"rb");if(!f)return 0;
 bool ok=fread(image,1,65536,f)==65536 && fgetc(f)==EOF && !ferror(f);
 if(fclose(f))ok=false;
 WalkerInfo walker;
 if(!ok||!walkerRead(image,&walker)||stat(path,&info)||info.st_size!=65536){
  iprintf("Invalid walker. Use companion Recovery.\n");return 0;
 }
 if(!walkerPrelaunchSnapshot(path)){iprintf("Cannot verify a recovery snapshot.\n");return 0;}
 if(!cartMapFile(&stage->storage,(uint32_t)info.st_ino,image,sectorRead,0)){
  iprintf("Cannot verify the SD sector map.\nA FAT32 console SD card is required.\n");return 0;
 }
 stage->version=1;memcpy(&stage->gameCode,code,4);stage->headerCrc=headerCrc;
 DC_FlushRange(stage,CART_STAGE_BYTES);
 fifoSendValue32(FIFO_USER_08,CART_MAGIC);
 unsigned frames=0;
 while(!fifoCheckValue32(FIFO_USER_08)&&frames++<120)swiWaitForVBlank();
 if(!fifoCheckValue32(FIFO_USER_08)||fifoGetValue32(FIFO_USER_08)!=1){iprintf("Cannot verify the SD card identity.\n");return 0;}
 DC_InvalidateRange(stage,CART_STAGE_BYTES);
 if(!(stage->storage.cid[0]|stage->storage.cid[1]|stage->storage.cid[2]|stage->storage.cid[3]))return 0;
 stage->storage.mapCrc=cartMapSignature(&stage->storage);
 stage->imageCrc=cartCrc(image,65536);stage->magic=CART_MAGIC;
 DC_FlushRange(stage,CART_STAGE_BYTES);
 iprintf("Cartridge: %.4s\n%s\n",code,path);
 if(walker.registered)iprintf("Trainer: %s / ID %05lu\n",walker.trainer,(unsigned long)walker.trainerId);
 if(walker.hasPokemon)iprintf("Partner: %s\n",speciesName(walker.species));
 iprintf("\nA: Launch  B: Cancel\n");
 while(1){scanKeys();unsigned keys=keysDown();if(keys&KEY_B){stage->magic=0;return 0;}if(keys&KEY_A)break;swiWaitForVBlank();}
 return 1;
}
