/* Experimental post-download connection checkpoint. GPL-3.0-or-later.
 * Only called on the existing Download Play / downloaded-SRL paths.
 * No file, save or wireless configuration is changed by this module. */
#ifndef TRANSFER_PROBE_H
#define TRANSFER_PROBE_H
#include <stdint.h>
#include <stdbool.h>
#include "transfer_probe_payload.h"
#include "gen5_client_profiles.h"

static inline uint32_t transferProbeCRC(const unsigned char *p,uint32_t n) {
 static const uint32_t nibble[16]={0,0x1db71064,0x3b6e20c8,0x26d930ac,0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,0xedb88320,0xf00f9344,0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,0xa00ae278,0xbdbdf21c};
 uint32_t c=~0u;
 while(n--) {c^=*p++;c=(c>>4)^nibble[c&15];c=(c>>4)^nibble[c&15];}
 return ~c;
}
static inline bool transferProbeRecognize(const void *header,uint32_t length) {
 const unsigned char *h=(const unsigned char*)header;const uint32_t *w=(const uint32_t*)header;
 if(w[0x38/4]!=0x02380000 || w[0x34/4]!=0x02380000)return false;
 // MB can omit the title/game code. Verify the complete decompressed code
 // and SDK module layout instead, before any generic patch changes it.
 for(uint32_t i=0;i<GEN5_CLIENT_PROFILE_COUNT;i++) {
  const Gen5ClientProfile *p=&gen5ClientProfiles[i];
  if(length!=p->length || w[0x28/4]!=p->base || w[0x24/4]!=p->entry)continue;
  // BW's retail client is DSi-enhanced; MB may reconstruct an NTR header.
  if(h[0x12]!=0 && !(p->base==0x02004000 && h[0x12]==2))return false;
  const uint32_t *m=(const uint32_t*)(p->base+p->module);
  if(m[3]!=p->bssStart || m[4]!=p->originalArena || m[5]!=0 ||
     m[6]!=p->sdk || m[7]!=0xdec00621 || m[8]!=0x2106c0de)return false;
  return transferProbeCRC((const unsigned char*)(p->base+0x800),length-0x800)==p->codeCRC;
 }
 return false;
}
static inline void transferProbeInstall(uint32_t *length) {
 // Recheck the still-unmodified application entry. Normal card/IRQ patches
 // can have changed other bytes since the pre-patch fingerprint was taken.
 volatile uint32_t *entry=(volatile uint32_t*)0x0201b040;
 if(entry[0]!=0xe92d41f8 || entry[1]!=0xe24dd004 ||
    *(volatile uint32_t*)0x02044f6c!=TP_PAYLOAD_ADDRESS)return;
 unsigned char *dest=(unsigned char*)TP_PAYLOAD_ADDRESS;
 for(uint32_t i=0;i<sizeof(transferProbePayload);i++)dest[i]=transferProbePayload[i];
 for(uint32_t i=sizeof(transferProbePayload);TP_PAYLOAD_ADDRESS+i<TP_ARENA_LO;i++)dest[i]=0;
 *(volatile uint32_t*)0x02044f6c=TP_ARENA_LO;
 entry[0]=0xe51ff004;entry[1]=TP_CONNECTED;
 volatile uint32_t *backup=(volatile uint32_t*)0x02039d30;
 backup[0]=0xe51ff004;backup[1]=TP_BACKUP_BLOCKED;
 *length=TP_ARENA_LO-0x02000000;
 // Keep actual MB boot parameters, parent BSSID, code and session identity.
 // In particular, 0x02010fe0 and the console's boot record are not modified.
}
#endif
