/* Host test for the Pokewalker 3DS step crediting (source/walkerfile.c).
 * gcc -Isource tests/test_walkerfile.c source/walkerfile.c -o t && ./t [file.walker] */
#include "walkerfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(name, got, want)                                                                     \
    do {                                                                                           \
        long long g_ = (long long)(got), w_ = (long long)(want);                                   \
        if (g_ != w_) {                                                                            \
            printf("FAIL %s: got %lld want %lld\n", name, g_, w_);                                 \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static unsigned char ee[WALKER_FILE_SIZE];
static void put32(unsigned char *p, unsigned v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}
static void seal(unsigned a1, unsigned a2, unsigned len) {
    unsigned char s = 1;
    for (unsigned i = 0; i < len; i++)
        s += ee[a1 + i];
    ee[a1 + len] = s;
    memcpy(ee + a2, ee + a1, len + 1);
}

static void makeStrolling(int withClock, unsigned start) {
    memset(ee, 0xFF, sizeof(ee));
    memcpy(ee, "nintendo", 8);
    memset(ee + 0xED, 0, 0x68);
    ee[0xED + 0x5B] = 3; /* registered, Pokemon on board */
    ee[0xED + 0x48] = 0x35;
    ee[0xED + 0x49] = 0x01; /* 'K' */
    ee[0xED + 0x4A] = 0xFF;
    ee[0xED + 0x4B] = 0xFF;
    seal(0xED, 0x1ED, 0x68);
    memset(ee + 0x156, 0, 0x18);
    put32(ee + 0x156, 1000); /* lifetime steps */
    ee[0x156 + 0x12] = 15;   /* 15 steps toward the next watt */
    seal(0x156, 0x256, 0x18);
    ee[0x8F00] = 74;
    ee[0x8F01] = 0;
    ee[0x8F0C] = 15;
    if (withClock) {
        ee[0x272] = 'P';
        ee[0x273] = 'T';
        put32(ee + 0x274, start);
        put32(ee + 0x278, start);
    }
}

int main(void) {
    WalkerInfo info;
    const unsigned t0 = 847775796;

    makeStrolling(1, t0);
    CHECK("read", walkerRead(ee, &info), 1);
    CHECK("strolling", info.hasPokemon, 1);
    CHECK("clock", info.hasClock, 1);
    CHECK("trainer[0]", info.trainer[0], 'K');
    CHECK("pending 1 s/step", walkerPendingSteps(ee, t0 + 100, 1), 100);
    CHECK("pending 7 s/step", walkerPendingSteps(ee, t0 + 100, 7), 14);
    CHECK("credit 7 s/step", walkerCredit(ee, t0 + 100, 7), 14);
    walkerRead(ee, &info);
    CHECK("total after credit", info.totalSteps, 1014);
    CHECK("today after credit", info.todaySteps, 14);
    CHECK("watts (15+14 -> 1 W)", info.watts, 1);
    CHECK("toward next watt", info.stepsThisWatt, 9);
    CHECK("remainder kept", info.lastCredited, t0 + 98);
    CHECK("no double credit", walkerCredit(ee, t0 + 100, 7), 0);
    CHECK("remainder used next time", walkerCredit(ee, t0 + 105, 7), 1);
    CHECK("both health copies equal", memcmp(ee + 0x156, ee + 0x256, 0x19), 0);
    CHECK("long stroll", walkerCredit(ee, t0 + 98 + 7 + 86400 * 3, 1), 86400 * 3);
    walkerRead(ee, &info);
    CHECK("watts after 3 days", info.watts,
          (unsigned)((9 + 1 + 86400 * 3) / 20 + 1) > 9999 ? 9999 : (9 + 1 + 86400 * 3) / 20 + 1);
    CHECK("clock backwards: nothing", walkerCredit(ee, t0, 1), 0);

    makeStrolling(0, 0);
    CHECK("no clock: nothing pending", walkerPendingSteps(ee, t0, 1), 0);
    CHECK("no clock: starts counting", walkerCredit(ee, t0, 1), 0);
    CHECK("no clock: then counts", walkerCredit(ee, t0 + 60, 1), 60);

    makeStrolling(1, t0);
    ee[0xED + 0x5B] = 1;
    seal(0xED, 0x1ED, 0x68); /* Empty slots must never earn app progress. */
    unsigned emptyCrc = walkerCrc(ee, sizeof(ee));
    CHECK("empty walker paused", walkerCredit(ee, t0 + 500, 1), 0);
    CHECK("empty walker pending", walkerPendingSteps(ee, t0 + 500, 1), 0);
    CHECK("empty walker untouched", walkerCrc(ee, sizeof(ee)), emptyCrc);
    memset(ee, 0xFF, sizeof(ee));
    CHECK("blank walker readable", walkerRead(ee, &info), 1);
    CHECK("blank walker not set up", info.setUp, 0);

    /* Returning resets the visible stroll count, not the lifetime total. */
    makeStrolling(1, t0);
    walkerCredit(ee, t0 + 100, 1);
    walkerRead(ee, &info);
    CHECK("first stroll count", info.strollSteps, 100);
    CHECK("first lifetime count", info.totalSteps, 1100);
    ee[0xed + 0x5b] = 1;
    seal(0xed, 0x1ed, 104);
    memset(ee + 0x272, 0xff, 10);
    unsigned returnedCrc = walkerCrc(ee, sizeof(ee));
    walkerCredit(ee, t0 + 10000, 1);
    walkerRead(ee, &info);
    CHECK("returned stroll is zero", info.strollSteps, 0);
    CHECK("returned lifetime kept", info.totalSteps, 1100);
    CHECK("returned file untouched", walkerCrc(ee, sizeof(ee)), returnedCrc);
    ee[0xed + 0x5b] = 3;
    seal(0xed, 0x1ed, 104);
    ee[0x272] = 'P';
    ee[0x273] = 'T';
    put32(ee + 0x274, t0 + 20000);
    put32(ee + 0x278, t0 + 20000);
    walkerCredit(ee, t0 + 20050, 1);
    walkerRead(ee, &info);
    CHECK("new stroll starts fresh", info.strollSteps, 50);
    CHECK("new stroll lifetime kept", info.totalSteps, 1150);

    /* Open sessions exclude closed-app time, but sleep in one session counts. */
    makeStrolling(1, t0);
    CHECK("start session", walkerResume(ee, t0 + 86400, 1), 1);
    walkerRead(ee, &info);
    CHECK("closed time not credited", info.totalSteps, 1000);
    CHECK("session pending starts zero", walkerPendingSteps(ee, t0 + 86400, 1), 0);
    walkerCredit(ee, t0 + 86400 + 100, 1);
    walkerRead(ee, &info);
    CHECK("open seconds counted", info.strollSteps, 100);
    unsigned beforeSleep = info.totalSteps;
    walkerCredit(ee, t0 + 86400 + 100 + 3600, 1);
    walkerRead(ee, &info);
    CHECK("sleep seconds counted", info.totalSteps, beforeSleep + 3600);
    unsigned beforeClose = info.totalSteps;
    walkerResume(ee, t0 + 86400 * 7, 1);
    walkerRead(ee, &info);
    CHECK("reopening does not catch up", info.totalSteps, beforeClose);
    walkerCredit(ee, t0 + 86400 * 7 + 20, 1);
    walkerRead(ee, &info);
    CHECK("new open interval counted", info.totalSteps, beforeClose + 20);
    makeStrolling(1, t0);
    walkerResume(ee, t0, 7);
    walkerCredit(ee, t0 + 2, 7);
    walkerResume(ee, t0 + 86400, 7);
    CHECK("fraction kept across sessions", walkerCredit(ee, t0 + 86400 + 5, 7), 1);

    /* Walking visitors and items persist without an automatic Pokemon catch. */
    makeStrolling(1, 86400u * 9000);
    memset(ee + WALKER_ROUTE + 0x52, 0, 0x6c);
    memset(ee + WALKER_POKEMON, 0, 48);
    memset(ee + WALKER_ITEMS, 0, 12);
    for (unsigned n = 0; n < 3; n++) {
        ee[WALKER_ROUTE + 0x52 + n * 16] = 25 + n;
        ee[WALKER_ROUTE + 0x88 + n] = 100;
    }
    for (unsigned n = 0; n < 10; n++) {
        ee[WALKER_ROUTE + 0x8c + n * 2] = 17 + n;
        ee[WALKER_ROUTE + 0xb4 + n] = 100;
    }
    WalkerReport visitors;
    walkerAdvance(ee, 86400u * 9003, 1, &visitors);
    CHECK("visitor generated", walkerPendingEncounter(ee) > 0, 1);
    CHECK("random walking item generated", visitors.items > 0, 1);
    walkerRead(ee, &info);
    CHECK("visitor is not an automatic catch", info.pokemonCount, 0);
    CHECK("walking item capacity", info.itemCount <= 3, 1);
    unsigned visitor = walkerPendingEncounter(ee);
    walkerAdvance(ee, 86400u * 9003, 1, &visitors);
    CHECK("visitor survives reopen", walkerPendingEncounter(ee), visitor);
    walkerClearEncounter(ee);
    CHECK("visitor explicitly consumed", walkerPendingEncounter(ee), 0);

    /* Midnight attribution, including rates longer than an hour. */
    const unsigned midnight = 86400u * 9000;
    makeStrolling(1, midnight - 10);
    walkerCredit(ee, midnight, 1);
    walkerRead(ee, &info);
    CHECK("midnight total", info.totalSteps, 1010);
    CHECK("new day includes midnight step", info.todaySteps, 1);
    CHECK("previous day history", ee[0xcef3], 9);
    makeStrolling(1, midnight - 43200);
    walkerCredit(ee, midnight, 86400);
    walkerRead(ee, &info);
    CHECK("slow rate keeps partial interval", info.totalSteps, 1000);
    walkerCredit(ee, midnight + 43200, 86400);
    walkerRead(ee, &info);
    CHECK("slow step belongs to current day", info.todaySteps, 1);
    CHECK("slow rate total", info.totalSteps, 1001);
    unsigned savedCrc = walkerCrc(ee, sizeof(ee));
    CHECK("restart no duplicate", walkerCredit(ee, midnight + 43200, 86400), 0);
    CHECK("restart identical bytes", walkerCrc(ee, sizeof(ee)), savedCrc);
    WalkerReport report;
    walkerAdvance(ee, midnight, 86400, &report);
    CHECK("backward clock reported", report.clockBehind, 1);
    CHECK("backward clock preserves bytes", walkerCrc(ee, sizeof(ee)), savedCrc);

    /* Bulk offline replay matches hourly reopenings, including event RNG. */
    static unsigned char bulk[WALKER_FILE_SIZE];
    makeStrolling(1, midnight);
    walkerAdvance(ee, midnight, 1, &report);
    memcpy(bulk, ee, sizeof(ee));
    walkerAdvance(bulk, midnight + 86400 * 2 + 123, 1, &report);
    for (unsigned n = 3600; n <= 86400 * 2; n += 3600)
        walkerAdvance(ee, midnight + n, 1, &report);
    walkerAdvance(ee, midnight + 86400 * 2 + 123, 1, &report);
    CHECK("offline cadence-independent save", memcmp(ee, bulk, sizeof(ee)), 0);
    CHECK("mirrored health intact", memcmp(ee + 0x156, ee + 0x256, 25), 0);
    CHECK("read after offline replay", walkerRead(ee, &info), 1);

    /* Rewards preserve the entire route record and never silently overwrite. */
    makeStrolling(1, midnight);
    memset(ee + WALKER_POKEMON, 0, 48);
    memset(ee + WALKER_ITEMS, 0, 12);
    for (unsigned n = 0; n < 3; n++) {
        unsigned char *p = ee + WALKER_ROUTE + 0x52 + n * 16;
        for (unsigned j = 0; j < 16; j++)
            p[j] = (unsigned char)(n * 16 + j);
        p[0] = 25 + n;
        p[1] = 0;
        CHECK("store route Pokemon", walkerStorePokemon(ee, n, -1, midnight), 1);
        CHECK("full native record retained", memcmp(p, ee + WALKER_POKEMON + n * 16, 16), 0);
    }
    savedCrc = walkerCrc(ee, sizeof(ee));
    CHECK("full Pokemon pack requires choice", walkerStorePokemon(ee, 0, -1, midnight), -1);
    CHECK("full pack unchanged", walkerCrc(ee, sizeof(ee)), savedCrc);
    CHECK("explicit replacement", walkerStorePokemon(ee, 2, 0, midnight), 1);
    CHECK("replacement native record",
          memcmp(ee + WALKER_POKEMON, ee + WALKER_ROUTE + 0x52 + 32, 16), 0);
    ee[WALKER_ROUTE + 0x8c] = 17;
    ee[WALKER_ROUTE + 0x8d] = 0;
    for (int n = 0; n < 3; n++)
        CHECK("store item", walkerStoreItem(ee, 0, -1, midnight), 1);
    savedCrc = walkerCrc(ee, sizeof(ee));
    CHECK("full item pack requires choice", walkerStoreItem(ee, 0, -1, midnight), -1);
    CHECK("full item pack unchanged", walkerCrc(ee, sizeof(ee)), savedCrc);
    CHECK("malformed marker refused", (ee[0] = 0, walkerRead(ee, &info)), 0);
    makeStrolling(1, t0);
    walkerResume(ee, t0, 10);
    CHECK("sensor initial state", walkerRead(ee, &info), 1);
    unsigned beforeSteps = info.totalSteps;
    unsigned beforeWatts = info.watts;
    walkerAddSteps(ee, t0 + 3600, 0, &report);
    CHECK("sensor zero delta adds no time steps", report.steps, 0);
    walkerAddSteps(ee, t0 + 3600, 23, &report);
    CHECK("sensor credits exact measured delta", report.steps, 23);
    walkerRead(ee, &info);
    CHECK("sensor lifetime", info.totalSteps, beforeSteps + 23);
    CHECK("sensor watts", info.watts, beforeWatts + 1);
    CHECK("sensor elapsed hours", info.elapsedHours, (t0+3600)/3600-t0/3600);
    ee[0xed + 0x5b] = 1;
    seal(0xed, 0x1ed, 0x68);
    walkerAddSteps(ee, t0 + 7200, 99, &report);
    CHECK("empty walker has no physical progress", report.steps, 0);
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all Pokewalker core checks passed\n");
    return 0;
}
