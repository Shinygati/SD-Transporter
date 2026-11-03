#ifndef UI_H
#define UI_H
#include "walkerfile.h"
enum {
    UI_SCAN,
    UI_LIST,
    UI_HOME,
    UI_RADAR,
    UI_DOWSE,
    UI_BATTLE,
    UI_PACK,
    UI_REPLACE,
    UI_CARD,
    UI_SETTINGS,
    UI_RESULT,
    UI_SYNC,
    UI_LOCKED,
    UI_FORCE,
    UI_BACKUPS, UI_BACKUP_DELETE, UI_DELETE_ACTIVE, UI_RECOVER_SAVE, UI_RESTORE_BACKUP
};
typedef struct {
    int screen, cursor, count, page, selection, selectedPatch, activePatch, attempts, round, rounds;
    int validation, done, total, dirs, files, scanLimited, saveError, loaded, replaceKind;
    unsigned secondsPerStep, frame, now, pending, itemChoice, pendingEncounter;
    int discardPrompt, saveChoice, walkingEnabled, sensorEnabled, sensorAvailable, phoneLocked;
    char syncAddress[48], syncStatus[160];
    char saveStage[64], saveReason[160];
    float progress;
    char message[160], detail[192], filename[160], toast[160];
    uint64_t toastUntil, clockMs;
    int toastScreen;
    const char (*paths)[512];
    const uint8_t *statuses, *ee;
    WalkerInfo info;
    WalkerBattle battle;
    int backupIndex, backupCount, backupLimited;
    char backupName[160], backupDetail[96], backupWalker[160];
} UIState;
int uiInit(void);
void uiExit(void);
void uiBegin(int bottom);
void uiEnd(void);
void uiNotify(UIState *, const char *, uint64_t);
void uiUpdateClock(UIState *, uint64_t);
void uiTop(const UIState *);
void uiBottom(const UIState *);
#ifdef UI_PREVIEW
void uiPreviewOpen(const char *, int);
void uiPreviewClose(void);
#endif
#endif
