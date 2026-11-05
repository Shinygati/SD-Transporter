/* SD Transporter real-cartridge bridge, GPL-3.0-or-later. */
#ifndef CARTWALKER_H
#define CARTWALKER_H
#include <stdint.h>
#define CART_STAGE_ADDR 0x02F00000u
#define CART_STAGE_BYTES 0x12000u
#define CART_MAGIC 0x43574C4Bu
#define CART_STAGE_IMAGE 0x1000u
#define CART_BASE 0x023C0000u
#define CART_STORAGE_OFF 0x10800u
typedef struct {
 uint32_t sector[128];
 uint32_t crc[128];
 uint32_t initialized, error;
 uint32_t cid[4];
 uint32_t mapCrc;
} CartStorage;
typedef struct {
 uint32_t magic, version, gameCode, headerCrc;
 CartStorage storage;
 uint32_t imageCrc;
} CartStage;
typedef int (*CartReadSector)(void *,uint32_t,unsigned char *);
#ifdef __cplusplus
extern "C" {
#endif
static inline uint32_t cartCrc(const void *ptr,unsigned n) {
 const unsigned char *p=(const unsigned char *)ptr;uint32_t crc=~0u;
 while(n--){crc^=*p++;for(unsigned b=0;b<8;b++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
 return ~crc;
}
static inline uint32_t cartMapSignature(const CartStorage *s){
 return cartCrc(s->sector,sizeof(s->sector))^cartCrc(s->cid,sizeof(s->cid));
}
/* FAT32 only. Validate the map against every byte of the opened file before
 * raw writes are possible; FAT tables and directory entries are never written. */
int cartMapFile(CartStorage *,uint32_t,const unsigned char *,CartReadSector,void *);
#ifdef __cplusplus
}
#endif
#endif
