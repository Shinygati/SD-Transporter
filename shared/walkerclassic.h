/* Original-style phone presentation of the shared walker. GPL-3.0-or-later. */
#ifndef WALKER_CLASSIC_H
#define WALKER_CLASSIC_H
#include <stdint.h>
#include "walkerfile.h"
#define WALKER_CLASSIC_STATE 40
#define WALKER_CLASSIC_PIXELS (96*64)
/* Buttons: -1 tick, 0 left, 1 centre, 2 right, 3 held centre (back). state[10] is a pending
 * EEPROM sound score + 1; the host consumes it after persisting changes. */
int walkerClassicInput(uint8_t *,int32_t[40],int,uint32_t,uint32_t,int);
void walkerClassicFrame(const uint8_t *,const int32_t[40],uint8_t[6144],uint32_t);
/* Radar rounds are zero-based. */
int walkerRadarMarkCount(int round);
/* Mono PCM, 22050Hz. Bounded score synthesis from the uploaded EEPROM. */
unsigned walkerClassicSound(const uint8_t *,unsigned,int16_t *,unsigned);
#define WALKER_PEER_PACKET_SIZE 1688
int walkerPeerExport(const uint8_t *,uint8_t[1688]);
int walkerPeerImport(const uint8_t *,unsigned,uint8_t[65536]);
/* The committed visit's 40 quarter-second LCD frames. */
#define WALKER_VISIT_FRAME_COUNT 40
int walkerVisitFrame(const uint8_t *,const WalkerVisitReward *,unsigned,uint8_t[6144]);
const char *walkerMoveName(unsigned);
unsigned walkerPartnerMove(const uint8_t *,unsigned);
void walkerFriendlyStart(const uint8_t *,const uint8_t *,int32_t[24],uint32_t);
int walkerFriendlyTurn(const uint8_t *,const uint8_t *,int32_t[24],unsigned,unsigned);
void walkerFriendlyFrame(const uint8_t *,const uint8_t *,const int32_t[24],unsigned,unsigned,uint8_t[6144],uint32_t);
int walkerChooseBattleItem(uint8_t *,unsigned *,int);
void walkerMenuIcon(unsigned,uint16_t[16]);
#endif
