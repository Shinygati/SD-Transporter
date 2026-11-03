/* Host preview uses demonstration counters, with no game artwork. */
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char ee[WALKER_FILE_SIZE];
int main(int argc, char **argv) {
    UIState s = {0};
    s.ee = ee;
    s.loaded = 1;
    s.info.totalSteps = 12840;
    s.info.strollSteps = 2840;
    s.info.todaySteps = 2840;
    s.info.watts = 642;
    s.info.stepsThisWatt = 12;
    s.secondsPerStep = WALKER_DEFAULT_SECONDS_PER_STEP;
    s.info.lastCredited = s.now = 100000;
    s.info.species = 25;
    s.info.level = 15;
    strcpy(s.info.route, "Refreshing Field");
    strcpy(s.filename, "HeartGold.walker");
    strcpy(s.message, "Walking active. Sleep mode counts too.");
    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        if (!f || fread(ee, 1, sizeof(ee), f) != sizeof(ee))
            return 1;
        fclose(f);
        if (!walkerRead(ee, &s.info))
            return 1;
    }
    s.screen = UI_HOME;
    uiNotify(&s, "Walking active. Sleep mode counts too.", 1000);
    uiUpdateClock(&s, 1000);
    uiPreviewOpen("work/walker-top.svg", 0);
    uiTop(&s);
    uiPreviewClose();
    uiPreviewOpen("work/walker-home.svg", 1);
    uiBottom(&s);
    uiPreviewClose();
    s.screen = UI_SCAN;
    s.frame = 42;
    s.files = 481;
    s.dirs = 18;
    s.validation = 1;
    s.done = 7;
    s.total = 10;
    s.progress = .7f;
    uiPreviewOpen("work/walker-scan.svg", 1);
    uiBottom(&s);
    uiPreviewClose();
    s.screen = UI_DOWSE;
    s.activePatch = -1;
    s.selectedPatch = 1;
    s.attempts = 2;
    strcpy(s.message, "An item is hidden in one of these six patches.");
    uiPreviewOpen("work/walker-dowse.svg", 1);
    uiBottom(&s);
    uiPreviewClose();
    s.screen = UI_CARD;
    uiPreviewOpen("work/walker-card.svg", 1);
    uiBottom(&s);
    uiPreviewClose();
    s.screen = UI_SETTINGS;
    strcpy(s.message, "A result title must stay intact");
    uiNotify(&s, "Rate saved. Previously earned progress is kept.", 1000);
    uiUpdateClock(&s, 1000);
    uiPreviewOpen("work/walker-settings-notice.svg", 1);
    uiBottom(&s);
    uiPreviewClose();
    uiUpdateClock(&s, 5999);
    assert(s.toast[0]);
    uiUpdateClock(&s, 6000);
    assert(!s.toast[0]);
    assert(!strcmp(s.message, "A result title must stay intact"));
    uiPreviewOpen("work/walker-settings-clear.svg", 1);
    uiBottom(&s);
    uiPreviewClose();
    uiNotify(&s, "First", 7000);
    uiNotify(&s, "Latest", 7001);
    assert(!strcmp(s.toast, "Latest"));
    s.screen = UI_HOME;
    uiUpdateClock(&s, 7002);
    assert(!s.toast[0]);
    puts("notice expiry, replacement, screen scoping and modal title checks passed");
    s.saveError = 1;
    s.screen = UI_HOME;
    strcpy(s.saveStage, "Writing backup");
    strcpy(s.saveReason, "Error 28: No space left on device. Unsaved changes remain in memory.");
    uiPreviewOpen("work/walker-save-error.svg", 1);
    uiBottom(&s);
    uiPreviewClose();
    return 0;
}
