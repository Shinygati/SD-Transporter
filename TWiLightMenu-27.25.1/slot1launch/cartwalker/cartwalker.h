/* SD Transporter real-cartridge bridge, GPL-3.0-or-later. */
#ifndef CARTWALKER_H
#define CARTWALKER_H
#include <stdint.h>
#define CART_STAGE_ADDR 0x02F00000u
#define CART_STAGE_BYTES 0x12000u
#define CART_MAGIC 0x43574C4Bu
#define CART_STAGE_IMAGE 0x1000u
/* Memory layout in HGSS's 4 MiB main RAM. The loader's ARM9 page stays where
 * TWiLight puts it (0x023DA000-0x023DAFFF); moving it to 0x023E0000 broke
 * HGSS, which uses the memory above its 0x023E0000 heap ceiling. The walker
 * is split around that page:
 *   0x023CA000-0x023D9FFF  walker EEPROM image (64 KiB) - new heap ceiling
 *   0x023DB000-0x023DB7FF  WalkerControl
 *   0x023DB800-0x023DBFFF  CartStorage (SD sector map)
 *   0x023DC000-...         protocol blob (ends below 0x023E0000)
 * The heap ceiling ends up where the DS-flashcart walker puts it. */
#define CART_EEPROM  0x023CA000u
#define CART_CTRL    0x023DB000u
#define CART_STORAGE (CART_CTRL+0x800u)
#define CART_BLOB    (CART_CTRL+0x1000u)
#define CART_END     0x023E0000u
#define CART_BASE    CART_EEPROM /* lowest walker address and the new heap ceiling */
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
