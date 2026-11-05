#include "cartwalker.h"
#include <string.h>
static uint32_t le32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned le16(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static int partition(CartStorage *out,uint32_t first,const unsigned char *image,
                     CartReadSector read,void *arg,uint32_t start,uint32_t capacity){
 unsigned char b[512];
 if(!read(arg,start,b)||b[510]!=0x55||b[511]!=0xaa||le16(b+11)!=512)return 0;
 unsigned spc=b[13],reserved=le16(b+14),fats=b[16];
 uint32_t sectors=le32(b+32),fatSize=le32(b+36);
 if(!spc||(spc&(spc-1))||spc>128||!reserved||fats<1||fats>2||le16(b+17)||le16(b+22)||
    !fatSize||!sectors||sectors>capacity||start>UINT32_MAX-sectors)return 0;
 uint64_t overhead=(uint64_t)reserved+(uint64_t)fats*fatSize;
 if(overhead>=sectors)return 0;
 uint32_t count=(sectors-(uint32_t)overhead)/spc;
 if(count<65525||count>=0x0ffffff5u||(uint64_t)(count+2)*4>(uint64_t)fatSize*512)return 0;
 /* FAT32 can select a non-mirrored active FAT. */
 unsigned ext=le16(b+40),active=(ext&0x80)?ext&15:0;
 if(active>=fats)return 0;
 uint32_t fat=start+reserved+active*fatSize,data=start+(uint32_t)overhead,cluster=first;
 unsigned n=0;
 while(n<128){
  if(cluster<2||cluster>=count+2)return 0;
  uint32_t sector=data+(cluster-2)*spc;
  for(unsigned k=0;k<spc&&n<128;k++,n++){
   uint32_t physical=sector+k;
   if(physical>=start+sectors)return 0;
   for(unsigned old=0;old<n;old++)if(out->sector[old]==physical)return 0;
   if(!read(arg,physical,b)||memcmp(b,image+n*512,512))return 0;
   out->sector[n]=physical;out->crc[n]=cartCrc(b,512);
  }
  uint32_t entry=cluster*4;
  if(!read(arg,fat+entry/512,b))return 0;
  cluster=le32(b+(entry&511))&0x0fffffff;
  if(n==128){if(cluster<0x0ffffff8u)return 0;}
  else if(cluster>=0x0ffffff8u)return 0;
 }
 out->initialized=out->error=0;return 1;
}
int cartMapFile(CartStorage *out,uint32_t cluster,const unsigned char *image,
                CartReadSector read,void *arg){
 unsigned char mbr[512];
 if(!out||!image||!read||!read(arg,0,mbr))return 0;
 memset(out,0,sizeof(*out));
 if(partition(out,cluster,image,read,arg,0,UINT32_MAX))return 1;
 if(mbr[510]!=0x55||mbr[511]!=0xaa)return 0;
 for(unsigned i=0;i<4;i++){
  const unsigned char *p=mbr+446+i*16;
  if(p[4]!=0x0b&&p[4]!=0x0c)continue;
  uint32_t start=le32(p+8),size=le32(p+12);
  if(start&&size&&partition(out,cluster,image,read,arg,start,size))return 1;
 }
 memset(out,0,sizeof(*out));return 0;
}
