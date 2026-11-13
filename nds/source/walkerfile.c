/* Original portable companion logic, GPL-3.0-or-later.
 * Records/mechanics cross-checked against Dmitry Grinberg and lumirth/pw
 * (CC0). See REFERENCES.md. No retail firmware or bundled game artwork.
 * Time stepping, offline event replay and PW3D metadata are app extensions.
 */
#include "walkerfile.h"
#include <string.h>
#define ID1 0xed
#define ID2 0x1ed
#define HD1 0x156
#define HD2 0x256
#define CLOCK 0x272
#define META 0x10
#define MSIZE 0x60
enum {
    M_START = 8,
    M_DAY = 12,
    M_DAILY = 16,
    M_HOUR = 20,
    M_HSTEPS = 24,
    M_RNG = 28,
    M_EVENT = 32,
    M_KIND = 36,
    M_VALUE = 40,
    M_COUNT = 44,
    M_TIME = 48,
    M_RATE = 52,
    M_PENDING = 56,
    M_STEPS = 60,
    M_STEPS_MARK = 64,
    M_CRC = 92
};
static unsigned le16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | p[2] << 8 | p[3];
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void putle(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (i * 8));
}
static void putbe(uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (24 - i * 8));
}
static uint8_t sum(const uint8_t *p, unsigned n) {
    unsigned s = 1;
    while (n--)
        s += *p++;
    return (uint8_t)s;
}
static const uint8_t *reliable(const uint8_t *ee, unsigned a, unsigned b, unsigned n) {
    if (sum(ee + a, n) == ee[a + n])
        return ee + a;
    if (sum(ee + b, n) == ee[b + n])
        return ee + b;
    return 0;
}
static void mirror(uint8_t *ee, unsigned a, unsigned b, const uint8_t *p, unsigned n) {
    memmove(ee + a, p, n);
    ee[a + n] = sum(ee + a, n);
    memmove(ee + b, ee + a, n + 1);
}
uint32_t walkerCrc(const uint8_t *p, unsigned n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++)
            c = (c >> 1) ^ ((0u - (c & 1u)) & 0xedb88320u);
    }
    return ~c;
}
static int metaValid(const uint8_t *ee) {
    const uint8_t *m = ee + META;
    return !memcmp(m, "PW3D", 4) && m[4] == 1 && le32(m + M_CRC) == walkerCrc(m, M_CRC);
}
static uint32_t meta(const uint8_t *ee, unsigned k) { return le32(ee + META + k); }
static void setmeta(uint8_t *ee, unsigned k, uint32_t n) { putle(ee + META + k, n); }
static void commitmeta(uint8_t *ee) { setmeta(ee, M_CRC, walkerCrc(ee + META, M_CRC)); }
unsigned walkerPendingEncounter(const uint8_t *ee) {
    return metaValid(ee) && meta(ee, M_PENDING) <= 3 ? meta(ee, M_PENDING) : 0;
}
void walkerClearEncounter(uint8_t *ee) {
    setmeta(ee, M_PENDING, 0);
    commitmeta(ee);
}
static void text(const uint8_t *p, unsigned n, char *out) {
    unsigned j = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned c = le16(p + i * 2);
        if (!c || c == 0xffff)
            break;
        out[j++] = c >= 0x121 && c <= 0x12a   ? '0' + c - 0x121
                   : c >= 0x12b && c <= 0x144 ? 'A' + c - 0x12b
                   : c >= 0x145 && c <= 0x15e ? 'a' + c - 0x145
                   : c == 0x1de               ? ' '
                                              : '?';
    }
    out[j] = 0;
}
unsigned walkerPokemonId(const uint8_t *ee, unsigned slot) {
    return slot < 3 ? le16(ee + WALKER_POKEMON + slot * 16) : 0;
}
unsigned walkerItemId(const uint8_t *ee, unsigned slot) {
    return slot < 3 ? le16(ee + WALKER_ITEMS + slot * 4) : 0;
}
unsigned walkerRoutePokemonId(const uint8_t *ee, unsigned i) {
    return i < 3 ? le16(ee + WALKER_ROUTE + 0x52 + i * 16) : 0;
}
unsigned walkerRouteItemId(const uint8_t *ee, unsigned i) {
    if(i>=0x8001 && i<0x8000+537)return i&0x7fff;
    return i < 10 ? le16(ee + WALKER_ROUTE + 0x8c + i * 2) : 0;
}
static int validSpecies(unsigned id) { return id > 0 && id <= 493; }
static int validItem(unsigned id) { return id > 0 && id < 537; }
static int walkable(const uint8_t *ee, const WalkerInfo *i) {
    if (!i->setUp || !i->registered) return 0;
    if (i->hasPokemon) return 1;
    const uint8_t *id = reliable(ee, ID1, ID2, 104);
    const uint8_t *candidate = ee + WALKER_ROUTE + 0x52 + 2 * 16;
    return id && !(id[0x5b] & 2) && validSpecies(le16(candidate)) &&
           candidate[12] >= 1 && candidate[12] <= 100;
}
int walkerRead(const uint8_t *ee, WalkerInfo *i) {
    memset(i, 0, sizeof(*i));
    i->setUp = !memcmp(ee, "nintendo", 8);
    if (!i->setUp) {
        for (unsigned j = 0; j < WALKER_FILE_SIZE; j++)
            if (ee[j] != 0xff)
                return 0;
        return 1;
    }
    const uint8_t *id = reliable(ee, ID1, ID2, 104), *hd = reliable(ee, HD1, HD2, 24);
    if (!id || !hd)
        return 0;
    i->registered = id[0x5b] & 1;
    i->hasPokemon = (id[0x5b] & 2) != 0;
    text(id + 0x48, 8, i->trainer);
    i->trainerId = le16(id + 12);
    i->totalSteps = be32(hd);
    i->elapsedHours = be32(hd + 4);
    i->watts = hd[14] * 256 + hd[15];
    i->stepsThisWatt = hd[18] % 20;
    if (i->totalSteps > WALKER_MAX_STEPS || i->watts > WALKER_MAX_WATTS)
        return 0;
    if (i->hasPokemon) {
        const uint8_t *r = ee + WALKER_ROUTE;
        i->species = le16(r);
        i->level = r[12];
        text(r + 16, 11, i->nickname);
    }
    if (!validSpecies(i->species))
        i->hasPokemon = 0;
    text(ee + WALKER_ROUTE + 0x28, 21, i->route);
    i->friendship = ee[WALKER_ROUTE + 0x26];
    const uint8_t *c = ee + CLOCK;
    i->hasClock = c[0] == 'P' && c[1] == 'T';
    if (i->hasClock) {
        i->walkStart = be32(c + 2);
        i->lastCredited = be32(c + 6);
    }
    for (unsigned s = 0; s < 3; s++) {
        if (validSpecies(walkerPokemonId(ee, s)))
            i->pokemonCount++;
        if (validItem(walkerItemId(ee, s)))
            i->itemCount++;
    }
    if (metaValid(ee) && i->hasClock && meta(ee, M_START) == i->walkStart) {
        i->todaySteps = meta(ee, M_DAILY);
        i->lastEvent = meta(ee, M_KIND);
        i->lastEventValue = meta(ee, M_VALUE);
        i->eventCount = meta(ee, M_COUNT);
        if (walkable(ee, i) && meta(ee, M_STEPS_MARK) == 0x53544550u)
            i->strollSteps = meta(ee, M_STEPS);
    }
    return 1;
}
int walkerTrainerKey(const uint8_t *ee,char out[37]) {
    WalkerInfo info;out[0]=0;
    if(!walkerRead(ee,&info)||!info.registered)return 0;
    const uint8_t *id=reliable(ee,ID1,ID2,104);if(!id)return 0;
    uint8_t key[18];key[0]=id[12];key[1]=id[13];int ended=0;
    for(unsigned n=0;n<8;n++) {
        unsigned value=le16(id+0x48+n*2);if(value==0xffff)ended=1;
        key[2+n*2]=ended?0xff:(uint8_t)value;key[3+n*2]=ended?0xff:(uint8_t)(value>>8);
    }
    static const char hex[]="0123456789abcdef";
    for(unsigned n=0;n<18;n++){out[n*2]=hex[key[n]>>4];out[n*2+1]=hex[key[n]&15];}out[36]=0;return 1;
}
int walkerCanWalk(const uint8_t *ee) {
    WalkerInfo i;
    return walkerRead(ee, &i) && walkable(ee, &i);
}
static void initmeta(uint8_t *ee, uint32_t start, uint32_t stamp, unsigned rate) {
    memset(ee + META, 0, MSIZE);
    memcpy(ee + META, "PW3D", 4);
    ee[META + 4] = 1;
    setmeta(ee, M_START, start);
    setmeta(ee, M_STEPS_MARK, 0x53544550u);
    setmeta(ee, M_DAY, stamp / 86400);
    setmeta(ee, M_HOUR, stamp / 3600);
    setmeta(ee, M_RNG, walkerCrc(ee + ID1, 104) ^ start ^ 0x913ab237u);
    setmeta(ee, M_EVENT, stamp + 3600);
    setmeta(ee, M_TIME, stamp);
    setmeta(ee, M_RATE, rate);
    commitmeta(ee);
}
uint32_t walkerRandom(uint8_t *ee) {
    uint32_t v = meta(ee, M_RNG) * 1664525u + 1013904223u;
    setmeta(ee, M_RNG, v);
    commitmeta(ee);
    return v;
}
static uint32_t capped(uint32_t a, uint32_t b, uint32_t cap) {
    uint64_t s = (uint64_t)a + b;
    return s > cap ? cap : (uint32_t)s;
}
static int emptyPokemon(const uint8_t *ee) {
    for (int s = 0; s < 3; s++)
        if (!validSpecies(walkerPokemonId(ee, s)))
            return s;
    return -1;
}
static int emptyItem(const uint8_t *ee) {
    for (int s = 0; s < 3; s++)
        if (!validItem(walkerItemId(ee, s)))
            return s;
    return -1;
}
static void wattsAdd(uint8_t *ee, unsigned n) {
    uint8_t h[24];
    memcpy(h, reliable(ee, HD1, HD2, 24), 24);
    unsigned v = h[14] * 256 + h[15];
    v = capped(v, n, 9999);
    h[14] = v >> 8;
    h[15] = v;
    mirror(ee, HD1, HD2, h, 24);
}
int walkerSpend(uint8_t *ee, unsigned n) {
    WalkerInfo i;
    if (!walkerRead(ee, &i) || !i.registered || i.watts < n)
        return 0;
    uint8_t h[24];
    memcpy(h, reliable(ee, HD1, HD2, 24), 24);
    unsigned v = i.watts - n;
    h[14] = v >> 8;
    h[15] = v;
    mirror(ee, HD1, HD2, h, 24);
    return 1;
}
static void diary(uint8_t *ee, uint32_t now, unsigned action, unsigned encounter, unsigned item) {
    uint8_t h[24], d[136];
    const uint8_t *id = reliable(ee, ID1, ID2, 104);
    memcpy(h, reliable(ee, HD1, HD2, 24), 24);
    unsigned index = h[19] % 23;
    unsigned addr = 0xcf0c + index * 136;
    if (ee[addr + 132] == 25) {
        index = (index + 1) % 23;
        addr = 0xcf0c + index * 136;
    }
    memset(d, 0, sizeof(d));
    putbe(d, now);
    memcpy(d + 4, id, 4);
    memcpy(d + 8, id + 8, 2);
    memcpy(d + 10, ee + WALKER_ROUTE, 2);
    if (encounter < 3)
        memcpy(d + 12, ee + WALKER_ROUTE + 0x52 + encounter * 16, 2);
    d[14] = item;
    d[15] = item >> 8;
    memcpy(d + 32, ee + WALKER_ROUTE + 16, 22);
    memcpy(d + 76, ee + WALKER_ROUTE + 0x28, 42);
    d[118] = ee[WALKER_ROUTE + 0x27];
    d[119] = ee[WALKER_ROUTE + 0x26];
    if (reliable(ee, HD1, HD2, 24)[23] & 1)
        return;
    unsigned hs = meta(ee, M_HSTEPS);
    d[120] = hs >> 8;
    d[121] = hs;
    putbe(d + 124, meta(ee, M_DAILY));
    d[132] = action;
    d[133] = ee[WALKER_ROUTE + 13];
    memcpy(ee + addr, d, sizeof(d));
    h[19] = (index + 1) % 23;
    mirror(ee, HD1, HD2, h, 24);
}
static void event(uint8_t *ee, unsigned kind, unsigned value) {
    setmeta(ee, M_KIND, kind);
    setmeta(ee, M_VALUE, value);
    setmeta(ee, M_COUNT, meta(ee, M_COUNT) + 1);
    commitmeta(ee);
}
/* A 16-byte walker Pokemon summary as HGSS consumes it in overlay 112:
 * species, held item, four moves, level, form/gender byte. Real course data
 * always has a first move and packs moves to the front without repeats. */
static int validSummary(const uint8_t *p) {
    unsigned species = le16(p), item = le16(p + 2), level = p[12];
    if (!validSpecies(species) || level < 1 || level > 100 || (item && !validItem(item)))
        return 0;
    if ((p[13] & 0x1f) > 27) /* form index (Unown has the most) */
        return 0;
    int ended = 0;
    for (unsigned m = 0; m < 4; m++) {
        unsigned move = le16(p + 4 + m * 2);
        if (!move) { if (!m) return 0; ended = 1; continue; }
        if (ended || move > 467) return 0;
        for (unsigned k = 0; k < m; k++)
            if (le16(p + 4 + k * 2) == move) return 0;
    }
    return 1;
}
static int recordOk(const uint8_t *ee, unsigned a, unsigned n) { return sum(ee + a, n) == ee[a + n]; }
/* Mirrored records: if one copy is damaged, rewrite it from the good one. */
static int repairRecord(uint8_t *ee, unsigned a, unsigned b, unsigned n) {
    int ga = recordOk(ee, a, n), gb = recordOk(ee, b, n);
    if (ga && gb) {
        if (memcmp(ee + a, ee + b, n + 1)) memmove(ee + b, ee + a, n + 1);
        return 1;
    }
    if (!ga && !gb) return 0;
    if (ga) memmove(ee + b, ee + a, n + 1);
    else memmove(ee + a, ee + b, n + 1);
    return 1;
}
int walkerRepairRecords(uint8_t *ee) {
    WalkerInfo i;
    if (!walkerRead(ee, &i) || !i.setUp) return 0;
    /* Identity and health only; the unique-ID block at 0x83 carries no
     * checksum in real walker images. */
    return repairRecord(ee, ID1, ID2, 104) && repairRecord(ee, HD1, HD2, 24);
}
int walkerValidate(const uint8_t *ee) {
    WalkerInfo i;
    if (!walkerRead(ee, &i)) return 0;
    if (!i.setUp) return 1; /* blank, untouched walker */
    if (!recordOk(ee, ID1, 104) || !recordOk(ee, ID2, 104) || memcmp(ee + ID1, ee + ID2, 105) ||
        !recordOk(ee, HD1, 24) || !recordOk(ee, HD2, 24) || memcmp(ee + HD1, ee + HD2, 25))
        return 0;
    if (i.hasPokemon && !validSummary(ee + WALKER_ROUTE)) return 0;
    for (unsigned s = 0; s < 3; s++) {
        const uint8_t *p = ee + WALKER_POKEMON + s * 16;
        if (le16(p) && !validSummary(p)) return 0;
        unsigned item = walkerItemId(ee, s);
        if (item && !validItem(item)) return 0;
    }
    return 1;
}
int walkerStorePokemon(uint8_t *ee, unsigned choice, int slot, uint32_t now) {
    if (choice >= 3 || !validSpecies(walkerRoutePokemonId(ee, choice)) ||
        !validSummary(ee + WALKER_ROUTE + 0x52 + choice * 16))
        return 0;
    if (slot < 0)
        slot = emptyPokemon(ee);
    if (slot < 0 || slot >= 3)
        return -1;
    memcpy(ee + WALKER_POKEMON + slot * 16, ee + WALKER_ROUTE + 0x52 + choice * 16, 16);
    diary(ee, now, 13, choice, 0);
    event(ee, 3, walkerRoutePokemonId(ee, choice));
    return 1;
}
int walkerStoreItem(uint8_t *ee, unsigned choice, int slot, uint32_t now) {
    unsigned id = walkerRouteItemId(ee,choice);
    if (!validItem(id))
        return 0;
    if (slot < 0)
        slot = emptyItem(ee);
    if (slot < 0 || slot >= 3)
        return -1;
    uint8_t *p = ee + WALKER_ITEMS + slot * 4;
    p[0] = id;
    p[1] = id >> 8;
    p[2] = p[3] = 0;
    diary(ee, now, 11, 3, id);
    event(ee, 1, id);
    return 1;
}
int walkerChooseEncounter(uint8_t *ee, WalkerEncounter *out) {
    WalkerInfo i;
    if (!walkerRead(ee, &i) || !i.registered || !i.hasPokemon)
        return 0;
    unsigned roll = walkerRandom(ee) % 100, selected = 2;
    for (unsigned n = 0; n < 3; n++) {
        unsigned steps = le16(ee + WALKER_ROUTE + 0x82 + n * 2),
                 chance = ee[WALKER_ROUTE + 0x88 + n];
        if (i.todaySteps >= steps && roll < chance && validSpecies(walkerRoutePokemonId(ee, n))) {
            selected = n;
            break;
        }
    }
    unsigned id = walkerRoutePokemonId(ee, selected);
    if (!validSpecies(id))
        return 0;
    *out = (WalkerEncounter){selected, id, ee[WALKER_ROUTE + 0x52 + selected * 16 + 12],
                             le16(ee + WALKER_ROUTE + 0x82 + selected * 2),
                             ee[WALKER_ROUTE + 0x88 + selected]};
    return 1;
}
static int chooseRouteItem(uint8_t *ee, unsigned *out, int dowsing) {
    WalkerInfo i;
    if (!walkerRead(ee, &i) || !i.hasPokemon)
        return 0;
    unsigned selected = 9;
    for (unsigned n = 0; n < 10; n++) {
        if (i.todaySteps < le16(ee + WALKER_ROUTE + 0xa0 + n * 2))
            continue;
        unsigned roll = walkerRandom(ee) >> 3;
        if (dowsing)
            roll = (uint8_t)roll;
        if (roll % 100 < ee[WALKER_ROUTE + 0xb4 + n]) {
            selected = n;
            break;
        }
    }
    if (!validItem(le16(ee + WALKER_ROUTE + 0x8c + selected * 2)))
        return 0;
    *out = selected;
    return 1;
}
int walkerChooseItem(uint8_t *ee, unsigned *out) {
    return chooseRouteItem(ee, out, 0);
}
/* Optional Gen IV rewards. Weights are per selection out of 10,000.
 * Rows: Master Ball, PP Max, PP Up, TMs, vitamins, Rare Candy, EV training,
 * evolution items, battle-held items, Heart Scale; remainder = course items.
 * No key items, later-generation IDs or uploaded course edits are used. */
int walkerExpandedReward(uint8_t *ee,unsigned *out,int battle) {
    static const unsigned vitamins[]={45,46,47,48,49,52};
    static const unsigned ev[]={215,289,290,291,292,293,294};
    static const unsigned evolution[]={80,81,82,83,84,85,107,108,109,110,221,226,227,233,235,252,321,322,323,324,325,326,327};
    static const unsigned held[]={214,220,232,234,253,266,267,268,270,271,275,287,297};
    static const unsigned weights[2][10]={{1,9,20,170,300,50,50,100,50,50},{50,450,1000,3500,1000,500,500,500,500,500}};
    unsigned roll=(walkerRandom(ee)>>3)%10000,group=0,id;
    while(group<10 && roll>=weights[battle!=0][group]){roll-=weights[battle!=0][group];group++;}
    switch(group){
    case 0:id=1;break;case 1:id=53;break;case 2:id=51;break;
    case 3:id=328+(walkerRandom(ee)>>3)%92;break;
    case 4:id=vitamins[(walkerRandom(ee)>>3)%6];break;
    case 5:id=50;break;
    case 6:id=ev[(walkerRandom(ee)>>3)%7];break;
    case 7:id=evolution[(walkerRandom(ee)>>3)%23];break;
    case 8:id=held[(walkerRandom(ee)>>3)%13];break;
    case 9:id=93;break;
    default:return 0;
    }
    *out=0x8000|id;return 1;
}
int walkerChooseItemEx(uint8_t *ee,unsigned *out,int expanded) {
    if(!expanded)return chooseRouteItem(ee,out,1);
    WalkerInfo info;if(!walkerRead(ee,&info)||!info.hasPokemon)return 0;
    if(walkerExpandedReward(ee,out,0))return 1;
    return chooseRouteItem(ee,out,1);
}
static void social(uint8_t *ee, uint32_t stamp, WalkerReport *r) {
    WalkerInfo i;
    walkerRead(ee, &i);
    if ((walkerRandom(ee) % 100) >= 40)
        return;
    if (reliable(ee, HD1, HD2, 24)[23] & 1)
        return;
    unsigned hs = meta(ee, M_HSTEPS);
    if (!i.hasPokemon) {
        /* No solo join. HGSS reads an ID_FLAG_JOINED partner through its
         * event-distribution path (fateful encounter, OT/met/nickname taken
         * from walker records this app never had), so a partner made from
         * course data comes back as an illegal Pokemon. An empty walker still
         * earns steps and watts; send a partner from HGSS. */
        (void)hs;
        return;
    }
    /* Companion-app extension: an occasional free encounter can wait for the
     * player after an offline stroll. Never auto-catch or replace a packed reward. */
    if (hs >= 100 && !walkerPendingEncounter(ee) && (walkerRandom(ee) >> 3) % 100 < 25) {
        WalkerEncounter encounter;
        if (walkerChooseEncounter(ee, &encounter)) {
            setmeta(ee, M_PENDING, encounter.index + 1);
            commitmeta(ee);
            r->encounters++;
            r->events++;
        }
    }
    if (hs >= 100 && emptyItem(ee) >= 0 && (walkerRandom(ee) >> 3) % 100 < 10) {
        unsigned choice;
        if (walkerChooseItem(ee, &choice) && walkerStoreItem(ee, choice, -1, stamp) > 0) {
            r->items++;
            r->events++;
            return;
        }
    }
    if (i.friendship >= 90 && hs >= 500 && emptyItem(ee) >= 0) {
        unsigned idx = hs >= 4500 ? 0 : 9 - hs / 500;
        if (walkerStoreItem(ee, idx, -1, stamp) > 0) {
            r->items++;
            r->events++;
            return;
        }
    }
    unsigned w = i.friendship >= 80 && hs >= 250 ? 50 : hs >= 200 ? 20 : hs >= 100 ? 10 : 0;
    if (w) {
        wattsAdd(ee, w);
        event(ee, 4, w);
        r->watts += w;
        r->events++;
    }
}
static void rolloverDay(uint8_t *ee, uint32_t day) {
    if (day != meta(ee, M_DAY)) {
        if(day>meta(ee,M_DAY))for(unsigned n=1;n<=10;n++)memset(ee+0xdc00+n*548+8,0xff,40);
        uint32_t prev = meta(ee, M_DAILY);
        uint32_t gap = day > meta(ee, M_DAY) ? day - meta(ee, M_DAY) : 7;
        if (gap >= 7)
            memset(ee + 0xcef0, 0, 28);
        else {
            memmove(ee + 0xcef0 + gap * 4, ee + 0xcef0, (7 - gap) * 4);
            memset(ee + 0xcef0, 0, gap * 4);
            putbe(ee + 0xcef0 + (gap - 1) * 4, prev);
        }
        setmeta(ee, M_DAY, day);
        setmeta(ee, M_DAILY, 0);
    }
}
/* Start/resume an app session without awarding the interval since the app
 * was closed. Keep the fraction of a step and remaining event timer. Sleep
 * within the same app session is counted by the next walkerAdvance call. */
int walkerResume(uint8_t *ee, uint32_t now, uint32_t rate) {
    WalkerInfo i;
    if (!walkerRead(ee, &i) || !walkable(ee, &i))
        return 0;
    if (!rate || rate > 86400)
        rate = WALKER_DEFAULT_SECONDS_PER_STEP;
    if (!i.hasClock) {
        ee[CLOCK] = 'P';
        ee[CLOCK + 1] = 'T';
        putbe(ee + CLOCK + 2, now);
        i.walkStart = now;
    }
    uint32_t fraction = 0, delay = 3600;
    if (!metaValid(ee) || meta(ee, M_START) != i.walkStart) {
        initmeta(ee, i.walkStart, now, rate);
    } else {
        uint32_t stamp = meta(ee, M_TIME), next = meta(ee, M_EVENT);
        if (stamp >= i.lastCredited)
            fraction = (stamp - i.lastCredited) % rate;
        if (next > stamp && next - stamp <= 3600)
            delay = next - stamp;
        if (meta(ee, M_STEPS_MARK) != 0x53544550u) {
            unsigned oldRate = meta(ee, M_RATE);
            if (!oldRate || oldRate > 86400)
                oldRate = rate;
            uint32_t estimate =
                i.lastCredited >= i.walkStart ? (i.lastCredited - i.walkStart) / oldRate : 0;
            setmeta(ee, M_STEPS, estimate < i.totalSteps ? estimate : i.totalSteps);
            setmeta(ee, M_STEPS_MARK, 0x53544550u);
        }
    }
    putbe(ee + CLOCK + 6, now >= fraction ? now - fraction : now);
    setmeta(ee, M_TIME, now);
    setmeta(ee, M_EVENT, now + delay);
    setmeta(ee, M_RATE, rate);
    rolloverDay(ee, now / 86400);
    commitmeta(ee);
    return 1;
}
/* Persist the simulation and original PT clock in the SAME EEPROM file.
 * Reopening at the same time cannot credit the same interval twice. */
static uint32_t span(uint8_t *ee, uint32_t target, unsigned rate) {
    uint32_t start = be32(ee + CLOCK + 6);
    if (target <= start)
        return 0;
    uint32_t steps = (target - start) / rate;
    if (!steps)
        return 0;
    uint32_t aligned = start + steps * rate;
    uint8_t h[24];
    memcpy(h, reliable(ee, HD1, HD2, 24), 24);
    /* Attribute a step to the day on which its complete interval ends.
     * This also handles rates longer than an hour and fractional intervals
     * crossing midnight without moving the daily ledger backwards. */
    uint64_t stepTime = (uint64_t)start + rate;
    while (stepTime <= aligned) {
        uint32_t day = (uint32_t)(stepTime / 86400);
        uint64_t boundary = ((uint64_t)day + 1) * 86400;
        uint64_t end = boundary - 1 < aligned ? boundary - 1 : aligned;
        uint32_t count = (uint32_t)((end - stepTime) / rate + 1);
        rolloverDay(ee, day);
        setmeta(ee, M_DAILY, capped(meta(ee, M_DAILY), count, 99999));
        setmeta(ee, M_HSTEPS, capped(meta(ee, M_HSTEPS), count, 9999));
        stepTime += (uint64_t)count * rate;
    }
    setmeta(ee, M_STEPS, capped(meta(ee, M_STEPS), steps, WALKER_MAX_STEPS));
    putbe(h, capped(be32(h), steps, WALKER_MAX_STEPS));
    uint64_t toward = h[18] % 20 + (uint64_t)steps;
    unsigned w = capped(h[14] * 256 + h[15], (uint32_t)(toward / 20), 9999);
    h[14] = w >> 8;
    h[15] = w;
    h[18] = toward % 20;
    putbe(ee + CLOCK + 6, aligned);
    mirror(ee, HD1, HD2, h, 24);
    commitmeta(ee);
    return steps;
}
int walkerAdvance(uint8_t *ee, uint32_t now, uint32_t rate, WalkerReport *r) {
    memset(r, 0, sizeof(*r));
    WalkerInfo i;
    if (!walkerRead(ee, &i) || !walkable(ee, &i))
        return 0;
    if (!rate || rate > 86400)
        rate = WALKER_DEFAULT_SECONDS_PER_STEP;
    if (!i.hasClock) {
        ee[CLOCK] = 'P';
        ee[CLOCK + 1] = 'T';
        putbe(ee + CLOCK + 2, now);
        putbe(ee + CLOCK + 6, now);
        i.walkStart = i.lastCredited = now;
    }
    if (now < i.lastCredited) {
        r->clockBehind = 1;
        return 1;
    }
    if (!metaValid(ee) || meta(ee, M_START) != i.walkStart)
        initmeta(ee, i.walkStart, i.lastCredited, rate);
    if (meta(ee, M_STEPS_MARK) != 0x53544550u) {
        /* Older app versions kept time but had no per-stroll counter. */
        unsigned previousRate = meta(ee, M_RATE);
        if (!previousRate || previousRate > 86400)
            previousRate = rate;
        uint32_t estimate =
            i.lastCredited >= i.walkStart ? (i.lastCredited - i.walkStart) / previousRate : 0;
        setmeta(ee, M_STEPS, estimate < i.totalSteps ? estimate : i.totalSteps);
        setmeta(ee, M_STEPS_MARK, 0x53544550u);
    }
    uint32_t previous = meta(ee, M_TIME);
    if (now < previous) {
        r->clockBehind = 1;
        return 1;
    }
    if (now > previous) {
        uint8_t h[24];
        memcpy(h, reliable(ee, HD1, HD2, 24), 24);
        if (be32(h) < WALKER_MAX_STEPS)
            putbe(h + 4, capped(be32(h + 4), now / 3600 - previous / 3600, 9999999));
        unsigned minutes = capped(h[16] * 256 + h[17], now / 60 - previous / 60, 65535);
        h[16] = minutes >> 8;
        h[17] = minutes;
        mirror(ee, HD1, HD2, h, 24);
    }
    setmeta(ee, M_RATE, rate);
    uint32_t next = meta(ee, M_EVENT);
    unsigned events = 0;
    while (next <= now && next >= i.lastCredited && events++ < 8192) {
        r->steps += span(ee, next, rate);
        social(ee, next, r);
        setmeta(ee, M_HSTEPS, 0);
        setmeta(ee, M_HOUR, next / 3600);
        if (next > 0xffffffffu - 3600)
            break;
        next += 3600;
    }
    r->steps += span(ee, now, rate);
    if (next <= now)
        next = now + 3600;
    setmeta(ee, M_EVENT, next);
    rolloverDay(ee, now / 86400);
    setmeta(ee, M_TIME, now);
    commitmeta(ee);
    return 1;
}
/* Apply measured steps without deriving any steps from elapsed time. */
int walkerAddSteps(uint8_t *ee, uint32_t now, uint32_t steps, WalkerReport *r) {
    memset(r, 0, sizeof(*r));
    WalkerInfo i;
    if (!walkerRead(ee, &i) || !walkable(ee, &i))
        return 0;
    if (!i.hasClock || !metaValid(ee) || meta(ee, M_START) != i.walkStart)
        if (!walkerResume(ee, now, WALKER_DEFAULT_SECONDS_PER_STEP))
            return 0;
    if (!steps)
        return 1;
    uint32_t previous = meta(ee, M_TIME);
    if (now < previous) {
        r->clockBehind = 1;
        now = previous;
    }
    rolloverDay(ee, now / 86400);
    uint8_t h[24];
    memcpy(h, reliable(ee, HD1, HD2, 24), 24);
    if (now > previous) {
        if (be32(h) < WALKER_MAX_STEPS)
            putbe(h + 4, capped(be32(h + 4), now / 3600 - previous / 3600, 9999999));
        unsigned minutes = capped(h[16] * 256 + h[17], now / 60 - previous / 60, 65535);
        h[16] = minutes >> 8;
        h[17] = minutes;
    }
    putbe(h, capped(be32(h), steps, WALKER_MAX_STEPS));
    uint64_t toward = h[18] % 20 + (uint64_t)steps;
    unsigned w = capped(h[14] * 256 + h[15], (uint32_t)(toward / 20), 9999);
    h[14] = w >> 8;
    h[15] = w;
    h[18] = toward % 20;
    setmeta(ee, M_STEPS, capped(meta(ee, M_STEPS), steps, WALKER_MAX_STEPS));
    setmeta(ee, M_DAILY, capped(meta(ee, M_DAILY), steps, 99999));
    setmeta(ee, M_HSTEPS, capped(meta(ee, M_HSTEPS), steps, 9999));
    putbe(ee + CLOCK + 6, now);
    setmeta(ee, M_TIME, now);
    mirror(ee, HD1, HD2, h, 24);
    commitmeta(ee);
    r->steps = steps;
    if (now >= meta(ee, M_EVENT)) {
        social(ee, now, r);
        setmeta(ee, M_HSTEPS, 0);
        setmeta(ee, M_EVENT, now + 3600);
        commitmeta(ee);
    }
    return 1;
}
uint32_t walkerPendingSteps(const uint8_t *ee, uint32_t now, uint32_t rate) {
    WalkerInfo i;
    if (!rate)
        rate = WALKER_DEFAULT_SECONDS_PER_STEP;
    return walkerRead(ee, &i) && walkable(ee, &i) && i.hasClock && now > i.lastCredited
               ? (now - i.lastCredited) / rate
               : 0;
}
void walkerBattleStart(WalkerBattle *b, unsigned n) { *b = (WalkerBattle){4, 4, 0, 0, 0, n, 0}; }
void walkerBattleAction(uint8_t *ee, WalkerBattle *b, int action) {
    static const unsigned weights[5][3] = {
        {45, 35, 20}, {40, 30, 30}, {50, 40, 10}, {60, 30, 10}, {20, 30, 50}};
    static const unsigned capture[4] = {97, 79, 66, 56};
    if (b->finished || b->enemyHp < 1 || b->enemyHp > 4)
        return;
    if (action == 2) {
        int ok = 1;
        for (int i = 0; i < 3; i++)
            if ((walkerRandom(ee) >> 3) % 100 >= capture[b->enemyHp - 1])
                ok = 0;
        if (ok) {
            b->caught = b->finished = 1;
            b->lastOutcome = 5;
            return;
        }
        /* A broken ball ends the original battle: the opponent flees. */
        b->lastOutcome = 6;
        b->finished = 1;
        return;
    }
    const unsigned *w = weights[b->row % 5];
    unsigned roll = (walkerRandom(ee) >> 3) % 100;
    int result = roll < w[2] ? 2 : roll < w[2] + w[1] ? 1 : 0;
    if (action == 1) {
        if (result == 2) {
            b->finished = 1;
            b->lastOutcome = 4;
            return;
        }
        if (result == 1) {
            b->row = 4;
            b->lastOutcome = 3;
            return;
        }
        b->enemyHp--;
        b->row = 1;
        b->lastOutcome = 7;
    } else {
        if (result == 0)
            b->enemyHp--;
        else if (result == 2)
            b->enemyHp -= 2;
        if (b->enemyHp > 0)
            b->playerHp--;
        b->row = result == 0 ? 1 : result == 1 ? 3 : 2;
        b->lastOutcome = result == 0 ? 0 : result == 1 ? 1 : 2;
    }
    if (b->enemyHp <= 0) {
        b->enemyHp = 0;
        b->finished = 1;
        b->lastOutcome = 4;
    }
    if (b->playerHp <= 0) {
        b->playerHp = 0;
        b->finished = 1;
        b->lastOutcome = 8;
        WalkerInfo i;
        walkerRead(ee, &i);
        walkerSpend(ee, i.watts < 10 ? i.watts : 10);
    }
}
unsigned walkerFriendItemId(const uint8_t *ee,unsigned slot){
    return slot<10?le16(ee+0xcec8+slot*4):0;
}
int walkerVisit(uint8_t *ee,const uint8_t *peer,uint32_t now){
    WalkerInfo a,b;
    if(!walkerRead(ee,&a)||!walkerRead(peer,&b)||!a.hasPokemon||!b.hasPokemon)return 0;
    const uint8_t *ownId=reliable(ee,ID1,ID2,104),*otherId=reliable(peer,ID1,ID2,104);
    if(!ownId||!otherId||!memcmp(ownId+16,otherId+16,40))return 0;
    if(metaValid(ee)&&now/86400<meta(ee,M_DAY))return 0;
    if(!metaValid(ee))walkerResume(ee,now,WALKER_DEFAULT_SECONDS_PER_STEP);
    rolloverDay(ee,now/86400);
    for(unsigned n=1;n<=10;n++)if(!memcmp(ee+0xdc00+n*548+8,otherId+16,40))return -1;
    /* Original friend scoring, including 16-bit hourly arithmetic. */
    uint32_t ownDay=meta(ee,M_DAILY),peerDay=metaValid(peer)&&meta(peer,M_DAY)==now/86400?meta(peer,M_DAILY):0;
    unsigned ownHour=meta(ee,M_HOUR)==now/3600?meta(ee,M_HSTEPS):0;
    unsigned peerHour=metaValid(peer)&&meta(peer,M_HOUR)==now/3600?meta(peer,M_HSTEPS):0;
    uint32_t score=ownDay+peerDay+(uint16_t)((uint16_t)(ownHour+peerHour)*10);
    if(score>20000)score=20000;
    unsigned index=(score>=20000?0:score>=10000?2:score>=5000?4:score>=2500?6:8)+(ownDay>peerDay?0:1);
    unsigned id=walkerRouteItemId(ee,index),slot=0;
    while(slot<10&&walkerFriendItemId(ee,slot))slot++;
    if(slot<10&&!validItem(id))return 0;
    /* Preserve peer Trainer House data and the original ten-entry history. */
    memcpy(ee+0xdc00,peer+0xcc00,548);memcpy(ee+0xdc08,otherId+16,40);
    memmove(ee+0xdc00+548,ee+0xdc00,10*548);
    memcpy(ee+0xf400,peer+0x91be,384);memcpy(ee+0xf580,peer+0x993e,320);
    uint8_t *info=ee+0xf6c0;memset(info,0,56);putbe(info,peerDay);info[4]=peerHour>>8;info[5]=peerHour;
    memcpy(info+8,otherId,4);memcpy(info+12,otherId+8,2);memcpy(info+14,peer+WALKER_ROUTE,2);
    memcpy(info+16,peer+WALKER_ROUTE+16,22);memcpy(info+38,otherId+0x48,16);info[54]=peer[WALKER_ROUTE+13];
    int result;
    if(slot==10){unsigned watts=score/200;if(!watts)watts=1;if(watts>99)watts=99;wattsAdd(ee,watts);event(ee,4,watts);result=2;}
    else {uint8_t *item=ee+0xcec8+slot*4;item[0]=id;item[1]=id>>8;item[2]=item[3]=0;diary(ee,now,index+1,0,id);event(ee,1,id);result=1;}
    commitmeta(ee);return result;
}
