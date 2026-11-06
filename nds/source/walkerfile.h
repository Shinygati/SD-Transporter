/* SD Transporter walker companion. GPL-3.0-or-later. */
#ifndef WALKERFILE_H
#define WALKERFILE_H
#include <stdint.h>
#define WALKER_DEFAULT_SECONDS_PER_STEP 10u
#define WALKER_FILE_SIZE 0x10000
#define WALKER_EPOCH_UNIX 946684800u
#define WALKER_MAX_STEPS 9999999u
#define WALKER_MAX_WATTS 9999u
#define WALKER_STEPS_PER_WATT 20u
#define WALKER_ROUTE 0x8f00
#define WALKER_POKEMON 0xce8c
#define WALKER_ITEMS 0xcebc
typedef struct {
    int setUp, registered, hasPokemon, hasClock;
    char trainer[25], nickname[34], route[64];
    unsigned species, level, friendship, watts, stepsThisWatt;
    uint32_t walkStart, lastCredited, totalSteps, todaySteps, elapsedHours, strollSteps;
    unsigned pokemonCount, itemCount, lastEvent, lastEventValue;
    uint32_t eventCount;
    uint32_t trainerId;
} WalkerInfo;
typedef struct {
    uint32_t steps;
    unsigned items, watts, events, encounters;
    int clockBehind;
} WalkerReport;
typedef struct {
    unsigned index, species, level, steps, chance;
} WalkerEncounter;
typedef struct {
    int playerHp, enemyHp, row, finished, caught;
    unsigned encounter;
    int lastOutcome;
} WalkerBattle;
int walkerRead(const uint8_t *, WalkerInfo *);
/* Public Trainer ID plus canonical encoded trainer name, 36 lower-case hex chars. */
int walkerTrainerKey(const uint8_t *, char out[37]);
/* Registered empty walkers with valid course data may walk (steps and watts
 * only). Blank/unregistered and malformed occupied files may not. */
int walkerCanWalk(const uint8_t *);
uint32_t walkerPendingSteps(const uint8_t *, uint32_t, uint32_t);
int walkerAddSteps(uint8_t *image, uint32_t now, uint32_t steps, WalkerReport *report);
int walkerResume(uint8_t *, uint32_t, uint32_t);
int walkerAdvance(uint8_t *, uint32_t, uint32_t, WalkerReport *);
unsigned walkerPendingEncounter(const uint8_t *);
void walkerClearEncounter(uint8_t *);
uint32_t walkerRandom(uint8_t *);
int walkerSpend(uint8_t *, unsigned);
int walkerChooseEncounter(uint8_t *, WalkerEncounter *);
int walkerChooseItem(uint8_t *, unsigned *);
int walkerExpandedReward(uint8_t *,unsigned *,int battle);
int walkerChooseItemEx(uint8_t *,unsigned *,int expanded);
/* Original peer gift/history update for ONE side. Call on two cloned images,
 * then publish both through a transaction. 1=item, 2=watts, -1=already met,
 * 0=invalid/same device/no partner/clock backwards. */
int walkerVisit(uint8_t *,const uint8_t *,uint32_t);
unsigned walkerFriendItemId(const uint8_t *,unsigned);
int walkerStorePokemon(uint8_t *, unsigned, int, uint32_t);
int walkerStoreItem(uint8_t *, unsigned, int, uint32_t);
void walkerBattleStart(WalkerBattle *, unsigned);
void walkerBattleAction(uint8_t *, WalkerBattle *, int);
unsigned walkerItemId(const uint8_t *, unsigned);
unsigned walkerPokemonId(const uint8_t *, unsigned);
unsigned walkerRoutePokemonId(const uint8_t *, unsigned);
unsigned walkerRouteItemId(const uint8_t *, unsigned);
uint32_t walkerCrc(const uint8_t *, unsigned);
/* Mirrored identity and health records valid and identical, and every
 * Pokemon summary (partner and caught slots) well formed for HGSS. */
int walkerValidate(const uint8_t *);
/* Rewrite a damaged mirrored record from its good copy; 0 if both are bad. */
int walkerRepairRecords(uint8_t *);
#endif
