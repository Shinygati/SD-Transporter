/* Pokéwalker companion: graphical UI, /roms-only discovery and open-session
 * walking (including sleep). GPL-3.0-or-later. */
#include "lease.h"
#include "sdrename.h"
#include "syncnet.h"
#include "ui.h"
#include "walkerfile.h"
#include "species.h"
#include "backups.h"
#include <3ds.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifdef __NDS__
#define CONFIG_DIR "/data/pokewalker"
#define ROMS_ROOT "/roms"
#else
#define CONFIG_DIR "sdmc:/3ds/pokewalker3ds"
#define ROMS_ROOT "sdmc:/roms"
#endif
#define CONFIG_PATH CONFIG_DIR "/config.ini"
#define MAX_WALKERS 128
#define MAX_DIRS 1024
static UIState ui;
static char recoveryPaths[MAX_WALKERS][512], backupTarget[512];
static int recoveryCount;
static char paths[MAX_WALKERS][512], folders[MAX_DIRS][512];
static uint8_t statuses[MAX_WALKERS], ee[WALKER_FILE_SIZE],
    scratch[WALKER_FILE_SIZE];
static int folderCount, folderIndex, validateIndex, scanPhase, opened = -1,
                                                               dirty, quit;
static DIR *directory;
static uint32_t diskCrc, lastSync, lastAdvance, nativeFsError, sensorBaseline;
static int sensorBaselineValid;
static int phase, hidden, emptyPatch, pendingKind = -1;
static unsigned reward, gameChoice;
static uint64_t deadline;
static int resultReturn = UI_HOME;

static uint32_t now(void) {
  time_t t = time(NULL);
  return t > WALKER_EPOCH_UNIX ? (uint32_t)(t - WALKER_EPOCH_UNIX) : 0;
}
static int readPedometer(uint32_t *value) {
#ifndef __NDS__
  return ui.sensorAvailable && R_SUCCEEDED(PTMU_GetTotalStepCount(value));
#else
  (void)value;
  return 0;
#endif
}
static void rebaseSensor(void) {
  sensorBaselineValid = readPedometer(&sensorBaseline);
}
static void sampleSensor(void) {
  uint32_t measured;
  if (!ui.sensorEnabled || !readPedometer(&measured))
    return;
  if (sensorBaselineValid && measured >= sensorBaseline) {
    WalkerReport report;
    walkerAddSteps(ee, now(), measured - sensorBaseline, &report);
    if (report.steps || report.events)
      dirty = 1;
  }
  sensorBaseline = measured;
  sensorBaselineValid = 1;
}
static void notify(const char *s) { uiNotify(&ui, s, osGetTime()); }
static void message(const char *s) {
  snprintf(ui.message, sizeof(ui.message), "%s", s);
}
static const char *nameOf(const char *p) {
  const char *s = strrchr(p, '/');
  return s ? s + 1 : p;
}
static int ends(const char *s, const char *tail) {
  size_t a = strlen(s), b = strlen(tail);
  return a >= b && !strcasecmp(s + a - b, tail);
}
static int readFile(const char *path, uint8_t *out) {
  errno = 0;
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  size_t n = fread(out, 1, WALKER_FILE_SIZE, f);
  int extra = fgetc(f),
      ok = !ferror(f) && n == WALKER_FILE_SIZE && extra == EOF;
  int failure = ok ? 0 : (errno ? errno : EIO);
  if (fclose(f) && !failure)
    failure = errno ? errno : EIO;
  errno = failure;
  return failure == 0;
}
static int writeFile(const char *path, const uint8_t *data) {
  errno = 0;
  FILE *f = fopen(path, "wb");
  if (!f)
    return 0;
  int failure = 0;
  if (fwrite(data, 1, WALKER_FILE_SIZE, f) != WALKER_FILE_SIZE)
    failure = errno ? errno : EIO;
  if (fflush(f) && !failure)
    failure = errno ? errno : EIO;
  /* Flush through the 3DS filesystem before reopening for verification. */
  if (!failure && fsync(fileno(f)))
    failure = errno ? errno : EIO;
  if (fclose(f) && !failure)
    failure = errno ? errno : EIO;
  errno = failure;
  return failure == 0;
}
static void ensureConfigDir(void) {
#ifdef __NDS__
  mkdir("/data", 0777);
#else
  mkdir("sdmc:/3ds", 0777);
#endif
  mkdir(CONFIG_DIR, 0777);
}
static int saveFailure(const char *stage, int code) {
  ui.saveError = 1;
  snprintf(ui.saveStage, sizeof(ui.saveStage), "%s", stage);
  snprintf(ui.saveReason, sizeof(ui.saveReason),
           "Error %d: %s. Unsaved changes remain in memory.", code,
           strerror(code));
  if (nativeFsError)
    snprintf(ui.saveReason, sizeof(ui.saveReason),
             "FS %08lX, error %d. Unsaved changes remain in memory.",
             (unsigned long)nativeFsError, code);
  ensureConfigDir();
  FILE *log = fopen(CONFIG_DIR "/save-error.log", "a");
  if (log) {
    fprintf(log, "%lu | %s | errno %d (%s) | %s | FS %08lX\n",
            (unsigned long)now(), stage, code, strerror(code),
            opened >= 0 ? paths[opened] : "no file",
            (unsigned long)nativeFsError);
    fclose(log);
  }
  return 0;
}
static int save(void) {
  if (!dirty || opened < 0 || ui.phoneLocked)
    return 1;
  nativeFsError = 0;
  const char *p = paths[opened];
  char backup[544], temp[544], baktemp[544];
  snprintf(backup, sizeof(backup), "%s.bak", p);
  snprintf(temp, sizeof(temp), "%s.tmp", p);
  snprintf(baktemp, sizeof(baktemp), "%s.bak.tmp", p);
  if (!readFile(p, scratch))
    return saveFailure("Reading original file", errno);
  if (walkerCrc(scratch, sizeof(scratch)) != diskCrc)
    return saveFailure("File changed outside this app", EAGAIN);
  if (!writeFile(baktemp, scratch))
    return saveFailure("Writing backup", errno);
  if (!readFile(baktemp, scratch))
    return saveFailure("Reading backup", errno);
  if (walkerCrc(scratch, sizeof(scratch)) != diskCrc)
    return saveFailure("Backup verification", EIO);
  if (sdRenameReplace(baktemp, backup, &nativeFsError))
    return saveFailure("Replacing backup", errno);
  if (!writeFile(temp, ee))
    return saveFailure("Writing updated walker", errno);
  if (!readFile(temp, scratch))
    return saveFailure("Reading updated walker", errno);
  if (memcmp(scratch, ee, sizeof(ee)))
    return saveFailure("Walker verification", EIO);
  if (sdRenameReplace(temp, p, &nativeFsError))
    return saveFailure("Replacing walker file", errno);
  if (!readFile(p, scratch) || memcmp(scratch, ee, sizeof(ee)))
    return saveFailure("Published walker verification", EIO);
  diskCrc = walkerCrc(ee, sizeof(ee));
  dirty = 0;
  ui.saveError = 0;
  return 1;
}
static void refresh(void) {
  ui.loaded = walkerRead(ee, &ui.info);
  ui.ee = ee;
  ui.pendingEncounter = walkerPendingEncounter(ee);
  ui.now = now();
  ui.pending = ui.loaded && ui.walkingEnabled
                   ? walkerPendingSteps(ee, ui.now, ui.secondsPerStep)
                   : 0;
}
static int load(int index) {
  ui.loaded = 0;
  memset(&ui.info, 0, sizeof(ui.info));
  ui.pending = ui.pendingEncounter = 0;
  if (index < 0 || index >= ui.count || leaseInspect(paths[index], 0) ||
      !readFile(paths[index], ee) || !walkerRead(ee, &ui.info))
    return 0;
  diskCrc = walkerCrc(ee, sizeof(ee));
  ui.loaded = 1;
  ui.ee = ee;
  snprintf(ui.filename, sizeof(ui.filename), "%s", nameOf(paths[index]));
  return 1;
}
static int syncSteps(int show) {
  if (opened < 0 || !ui.loaded || ui.saveError)
    return 0;
  sampleSensor();
  memcpy(scratch, ee, sizeof(ee));
  WalkerReport report;
  memset(&report, 0, sizeof(report));
  if (ui.walkingEnabled)
    walkerAdvance(ee, now(), ui.secondsPerStep, &report);
  if (memcmp(scratch, ee, sizeof(ee)))
    dirty = 1;
  if (!save())
    return 0;
  refresh();
  if (show || report.events) {
    if (report.joined)
      notify("A wild Pokémon joined you!");
    else if (!ui.info.hasPokemon)
      notify("Walking solo. A wild Pokémon may join you.");
    else if (walkerPendingEncounter(ee))
      notify("A wild Pokémon is nearby! Choose the visitor tile.");
    else if (report.clockBehind)
      notify("Clock is behind the last sync; progress is preserved.");
    else if (show && !report.steps && !report.events)
      notify("Progress saved.");
    else {
      char text[160];
      snprintf(text, sizeof(text),
               "+%lu steps  ·  %u items  ·  %u new companion",
               (unsigned long)report.steps, report.items, report.joined);
      notify(text);
    }
  }
  lastSync = now();
  refresh();
  return 1;
}
static int configSave(void) {
  ensureConfigDir();
  FILE *f = fopen(CONFIG_PATH ".tmp", "w");
  if (!f)
    return 0;
  int ok = fprintf(f,
                   "; One step source at a "
                   "time.\nCONFIG_VERSION=3\nSECONDS_PER_STEP=%u\nSTEP_COUNTER="
                   "%d\nPEDOMETER=%d\n",
                   ui.secondsPerStep, ui.walkingEnabled, ui.sensorEnabled) > 0;
  if (fclose(f))
    ok = 0;
  return ok &&
         sdRenameReplace(CONFIG_PATH ".tmp", CONFIG_PATH, &nativeFsError) == 0;
}
static void configLoad(void) {
  ui.secondsPerStep = WALKER_DEFAULT_SECONDS_PER_STEP;
#ifdef __NDS__
  ui.walkingEnabled = 1;
  ui.sensorEnabled = 0;
#else
  ui.walkingEnabled = 0;
  ui.sensorEnabled = 1;
#endif
  FILE *f = fopen(CONFIG_PATH, "r");
  if (!f) {
    configSave();
    return;
  }
  int version = 0;
  char line[160];
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    char *value = strchr(p, '=');
    if (!value)
      continue;
    long n = strtol(value + 1, 0, 10);
    if (!strncasecmp(p, "CONFIG_VERSION", 14))
      version = (int)n;
    if (!strncasecmp(p, "SECONDS_PER_STEP", 16) && n >= 1 && n <= 86400)
      ui.secondsPerStep = (unsigned)n;
    if (!strncasecmp(p, "STEP_COUNTER", 12))
      ui.walkingEnabled = n != 0;
    if (!strncasecmp(p, "PEDOMETER", 9))
      ui.sensorEnabled = n != 0;
  }
  fclose(f);
  if (version < 3) {
#ifdef __NDS__
    ui.walkingEnabled = 1;
    ui.sensorEnabled = 0;
#else
    ui.walkingEnabled = 0;
    ui.sensorEnabled = 1;
#endif
    if (version < 2 && ui.secondsPerStep == 1)
      ui.secondsPerStep = WALKER_DEFAULT_SECONDS_PER_STEP;
    configSave();
  }
#ifdef __NDS__
  ui.sensorEnabled = 0;
#endif
  if (ui.walkingEnabled)
    ui.sensorEnabled = 0;
}
static void result(const char *a, const char *b) {
  message(a);
  snprintf(ui.detail, sizeof(ui.detail), "%s", b);
  ui.screen = UI_RESULT;
  resultReturn = ui.phoneLocked ? UI_LOCKED : opened >= 0 ? UI_HOME : UI_LIST;
  deadline = osGetTime() + 250;
}
static void rememberOriginal(const char *path) {
  for(int i=0;i<recoveryCount;i++)if(!strcmp(recoveryPaths[i],path))return;
  if(recoveryCount<MAX_WALKERS)snprintf(recoveryPaths[recoveryCount++],512,"%.511s",path);
}
static __attribute__((noinline)) void rememberRecovery(const char *path) {
  const char *suffixes[]={".walker.tmp",".walker.bak",".walker.bak.tmp",".walker.replace-old"};
  for(unsigned i=0;i<4;i++)if(ends(path,suffixes[i])) {
    char original[512];size_t length=strlen(path)-strlen(suffixes[i])+7;
    if(length<sizeof(original)){memcpy(original,path,length);original[length]=0;struct stat st;if(stat(original,&st)&&errno==ENOENT)rememberOriginal(original);}
    break;
  }
  const char *folder=strstr(path,"/transfers/backups/");
  if(!folder || !ends(path,".bak"))return;
  const char *base=folder+19,*part=0;
  for(const char *p=base;*p;p++)if(!strncasecmp(p,".walker.",8))part=p;
  if(!part)return;
  const char *kind=part+8;
  if(strncmp(kind,"prelaunch-",10)&&strncmp(kind,"checkout-",9)&&strncmp(kind,"return-",7)&&strncmp(kind,"recovery-",9)&&strncmp(kind,"conflict-",9)&&strncmp(kind,"deleted-",8)&&strncmp(kind,"reward-",7))return;
  char original[512];
  if(snprintf(original,sizeof(original),"%.*s%.*s",(int)(folder-path+1),path,(int)(part-base+7),base)>=(int)sizeof(original))return;
  for(int i=0;i<recoveryCount;i++)if(!strcmp(recoveryPaths[i],original))return;
  if(recoveryCount<MAX_WALKERS)snprintf(recoveryPaths[recoveryCount++],512,"%s",original);
}
static void scanBegin(void) {
  syncStop();
  recoveryCount=0;
  if (directory) {
    closedir(directory);
    directory = 0;
  }
  ui.screen = UI_SCAN;
  ui.count = ui.cursor = ui.page = ui.dirs = ui.files = ui.done = ui.total =
      ui.validation = ui.scanLimited = 0;
  ui.progress = 0;
  ui.loaded = 0;
  folderCount = 1;
  folderIndex = validateIndex = scanPhase = 0;
  opened = -1;
  memset(statuses, 0, sizeof(statuses));
  snprintf(folders[0], 512, "%s", ROMS_ROOT);
}
static void scanStep(void) {
  if (scanPhase == 0) {
    for (int budget = 0; budget < 28; budget++) {
      if (folderIndex >= folderCount) {
        scanPhase = 1;
        ui.validation = 1;
        ui.total = ui.count;
        return;
      }
      if (!directory) {
        directory = opendir(folders[folderIndex]);
        ui.dirs++;
        if (!directory) {
          folderIndex++;
          continue;
        }
      }
      struct dirent *e = readdir(directory);
      if (!e) {
        closedir(directory);
        directory = 0;
        folderIndex++;
        continue;
      }
      if (e->d_name[0] == '.')
        continue;
      ui.files++;
      char path[512];
      int n = snprintf(path, sizeof(path), "%s/%s", folders[folderIndex],
                       e->d_name);
      if (n < 0 || n >= (int)sizeof(path)) {
        ui.scanLimited = 1;
        continue;
      }
      struct stat st;
      if (lstat(path, &st))
        continue;
      if (S_ISLNK(st.st_mode))
        continue;
      if (S_ISDIR(st.st_mode)) {
        if (folderCount < MAX_DIRS)
          snprintf(folders[folderCount++], 512, "%s", path);
        else
          ui.scanLimited = 1;
      } else if(S_ISREG(st.st_mode) && (ends(e->d_name,".bak") || ends(e->d_name,".tmp") || ends(e->d_name,".replace-old"))) {
        rememberRecovery(path);
      } else if (S_ISREG(st.st_mode) && ends(e->d_name, ".walker")) {
        if (ui.count < MAX_WALKERS)
          snprintf(paths[ui.count++], 512, "%s", path);
        else
          ui.scanLimited = 1;
      }
    }
  } else {
    if (validateIndex < ui.count) {
      WalkerInfo i;
      statuses[validateIndex] =
          leaseInspect(paths[validateIndex], 0) ? 3
          : readFile(paths[validateIndex], scratch) && walkerRead(scratch, &i)
              ? (walkerCanWalk(scratch) ? 1 : 2)
              : 0;
      validateIndex++;
      ui.done = validateIndex;
      ui.progress = ui.count ? (float)validateIndex / ui.count : 1;
    } else {
      ui.screen = UI_LIST;
      ui.selection = 0;
      if (ui.count)
        load(0);
      if (ui.scanLimited)
        message("Some paths exceeded the folder/file limits.");
    }
  }
}
static void openWalker(void) {
  if (!ui.count)
    return;
  if (leaseInspect(paths[ui.cursor], 0)) {
    opened = ui.cursor;
    ui.phoneLocked = 1;
    ui.loaded = 0;
    dirty = 0;
    memset(&ui.info, 0, sizeof(ui.info));
    ui.screen = UI_LOCKED;
    return;
  }
  ui.phoneLocked = 0;
  if (!load(ui.cursor)) {
    result("This walker could not be opened.",
           "It must be exactly 64 KiB with valid mirrored records.");
    return;
  }
  if (!walkerCanWalk(ee)) {
    result("This walker is not ready.",
           "Register it with HGSS first. A valid uploaded course is "
           "needed for an empty walker to find a companion.");
    return;
  }
  opened = ui.cursor;
  dirty = 0;
  ui.saveError = 0;
  ui.screen = UI_HOME;
  ui.selection = 0;
  pendingKind = -1;
  /* Rebase the clock before any accrual: closed-app time earns nothing. */
  memcpy(scratch, ee, sizeof(ee));
  walkerResume(ee, now(), ui.secondsPerStep);
  dirty = memcmp(scratch, ee, sizeof(ee)) != 0;
  lastAdvance = lastSync = now();
  rebaseSensor();
  refresh();
  notify(!ui.info.hasPokemon ? "Walking solo. A wild Pokémon may join."
         : ui.walkingEnabled ? "Walking started. Sleep counts too."
                           : "Step counter is paused.");
}
static int replaceForLease(const char *a, const char *b) {
  return sdRenameReplace(a, b, &nativeFsError);
}
static void transferredBack(const char *text) {
  ui.phoneLocked = 0;
  if (opened >= 0 && load(opened)) {
    walkerResume(ee, now(), ui.secondsPerStep);
    dirty = 1;
    rebaseSensor();
    lastAdvance = lastSync = now();
    ui.screen = UI_HOME;
    refresh();
    notify(text);
  } else {
    opened = -1;
    scanBegin();
  }
}
static int beforePhoneCheckout(void) { return opened >= 0 && syncSteps(0); }
static void onPhoneEvent(int event) {
  if (event == 1) {
    ui.phoneLocked = 1;
    dirty = 0;
    ui.pending = ui.pendingEncounter = 0;
    ui.loaded=0;memset(&ui.info,0,sizeof(ui.info));memset(ee,0xff,sizeof(ee));
  }
  if (event == 2)
    transferredBack("Returned from phone. Walker unlocked.");
}
static void startPhoneSync(void) {
  if (opened < 0)
    return;
  if (!ui.phoneLocked && !syncSteps(0))
    return;
  ui.screen = UI_SYNC;
  if (!syncStart(paths[opened], beforePhoneCheckout, onPhoneEvent))
    snprintf(ui.syncStatus, sizeof(ui.syncStatus),
             "Could not start Wi-Fi. Check console network settings.");
}
static void endGame(const char *a, const char *b) {
  dirty = 1;
  if (!save()) { refresh(); return; }
  refresh();
  result(a, b);
}
static int ready(void) {
  if (ui.phoneLocked) {
    ui.screen = UI_LOCKED;
    return 0;
  }
  refresh();
  if (!ui.info.registered || !ui.info.hasPokemon) {
    result("Send a Pokémon first.",
           "Keep walking until a Pokémon joins, or send one from HGSS.");
    return 0;
  }
  if (ee[0x156 + 23] & 1) {
    result("Special-event course active.",
           "Event gift transfers are not supported yet. Normal route games "
           "remain available "
           "after changing course in HGSS.");
    return 0;
  }
  return !ui.saveError;
}
static void nextRustle(void) {
  phase = 0;
  ui.activePatch = -1;
  ui.progress = 0;
  deadline = osGetTime() + 500 + (walkerRandom(ee) % 900);
  dirty = 1;
}
static void radarStart(void) {
  if (!ready())
    return;
  unsigned visitor = walkerPendingEncounter(ee);
  if (visitor) {
    walkerClearEncounter(ee);
    dirty = 1;
    if (!save())
      return;
    walkerBattleStart(&ui.battle, visitor - 1);
    ui.screen = UI_BATTLE;
    ui.selection = 0;
    deadline = osGetTime() + 200;
    message("Your visitor is here! This encounter costs no watts.");
    return;
  }
  if (ui.info.watts < 10) {
    result("Poké Radar needs 10 watts.",
           "Keep strolling to earn another watt every 20 steps.");
    return;
  }
  WalkerEncounter encounter;
  if (!walkerChooseEncounter(ee, &encounter)) {
    result("This route has no usable encounters.",
           "Start a new stroll from HGSS to refresh the route data.");
    return;
  }
  walkerSpend(ee, 10);
  dirty = 1;
  if (!save())
    return;
  gameChoice = encounter.index;
  ui.round = 0;
  ui.rounds = 3 - (int)encounter.index + ((walkerRandom(ee) >> 3) & 1);
  ui.selectedPatch = 0;
  ui.screen = UI_RADAR;
  message("Wait for a rustle, then choose that patch.");
  nextRustle();
}
static void dowseStart(void) {
  if (!ready())
    return;
  if (ui.info.watts < 3) {
    result("Dowsing needs 3 watts.",
           "Keep strolling to earn another watt every 20 steps.");
    return;
  }
  if (!walkerChooseItem(ee, &reward)) {
    result("This route has no usable item data.",
           "Start a new stroll from HGSS to refresh the route data.");
    return;
  }
  walkerSpend(ee, 3);
  hidden = walkerRandom(ee) % 6;
  emptyPatch = -1;
  ui.selectedPatch = 0;
  ui.attempts = 2;
  ui.activePatch = -1;
  dirty = 1;
  if (!save())
    return;
  ui.screen = UI_DOWSE;
  message("An item is hidden in one of these six patches.");
}
static void rewardStore(int kind, unsigned which) {
  int ok = kind ? walkerStoreItem(ee, which, -1, now())
                : walkerStorePokemon(ee, which, -1, now());
  dirty = 1;
  if (ok < 0) {
    pendingKind = kind;
    reward = which;
    ui.replaceKind = kind;
    ui.selection = 0;
    ui.screen = UI_REPLACE;
    char text[160];
    snprintf(text,sizeof(text),kind?"Found %s. Choose a slot, or B to leave it.":"Caught %s! Choose a slot, or B to leave it.",kind?itemName(walkerRouteItemId(ee,which)):speciesName(walkerRoutePokemonId(ee,which)));
    message(text);
    return;
  }
  if (ok) {
    if (!backupsPreserveReward(paths[opened],ee)) { saveFailure("Preserving caught reward", EIO); refresh(); return; }
    char title[96];
    snprintf(title,sizeof(title),kind?"You found %s!":"You caught %s!",
             kind?itemName(walkerRouteItemId(ee,which)):speciesName(walkerRoutePokemonId(ee,which)));
    endGame(title,"Saved to card. It is in your pack, ready for HGSS.");
  } else
    endGame("The reward data was not usable.", "No Pokémon or item was added.");
}
static void radarPick(void) {
  if (phase != 1)
    return;
  if (ui.selectedPatch != ui.activePatch) {
    if (!ui.round) {
      message("Nothing here. Try the rustling patch!");
      nextRustle();
    } else
      endGame("It got away!", "Try another Poké Radar search.");
    return;
  }
  ui.round++;
  if (ui.round >= ui.rounds) {
    walkerBattleStart(&ui.battle, gameChoice);
    ui.screen = UI_BATTLE;
    ui.selection = 0;
    message("Attack, evade, or throw a Poké Ball.");
    deadline = osGetTime() + 200;
    return;
  }
  message(ui.round > 1 ? "!! Something rare is close!"
                       : "! Keep following the rustles.");
  nextRustle();
}
static void dowsePick(void) {
  if (ui.selectedPatch == emptyPatch) {
    message("You already searched that patch.");
    return;
  }
  if (ui.selectedPatch == hidden) {
    rewardStore(1, reward);
    return;
  }
  emptyPatch = ui.selectedPatch;
  ui.activePatch = emptyPatch;
  ui.attempts--;
  if (!ui.attempts) {
    endGame("Nothing was found this time.",
            "The item stayed hidden. Try again on your next stroll.");
    return;
  }
  int a = ui.selectedPatch, b = hidden,
      dist = abs(a % 3 - b % 3) + abs(a / 3 - b / 3);
  message(dist <= 1 ? "It's near! One attempt left."
                    : "It's far away… One attempt left.");
}
static void battleAction(void) {
  if (osGetTime() < deadline)
    return;
  walkerBattleAction(ee, &ui.battle, ui.selection);
  dirty = 1;
  deadline = osGetTime() + 350;
  const char *lines[] = {"You attacked! The wild Pokémon replied.",
                         "It dodged, then attacked!",
                         "A critical hit! It attacked back.",
                         "A stare-down…",
                         "The Pokémon fled!",
                         "Caught!",
                         "Almost had it…",
                         "You evaded and counter-attacked!",
                         "Your partner was defeated."};
  message(lines[ui.battle.lastOutcome]);
  if (ui.battle.caught) {
    rewardStore(0, ui.battle.encounter);
    return;
  }
  if (ui.battle.finished)
    endGame(lines[ui.battle.lastOutcome],
            ui.battle.lastOutcome == 8
                ? "Up to 10 watts were lost. Your partner is still safe."
                : "Your partner is safe. Try another search.");
}
static void homeAction(void) {
  switch (ui.selection) {
  case 0:
    radarStart();
    break;
  case 1:
    dowseStart();
    break;
  case 2:
    ui.screen = UI_PACK;
    break;
  case 3:
    ui.screen = UI_CARD;
    break;
  case 4:
    syncSteps(1);
    break;
  case 5:
    ui.screen = UI_SETTINGS;
    ui.selection = 0;
    break;
  }
}
static void changeRate(int direction) {
  static const unsigned rates[] = {1, 2, 5, 10, 30, 60, 300, 3600, 86400};
  if (!syncSteps(0))
    return;
  int i = 0;
  while (i < 8 && rates[i] < ui.secondsPerStep)
    i++;
  i += direction;
  if (i < 0)
    i = 0;
  if (i > 8)
    i = 8;
  unsigned old = ui.secondsPerStep;
  ui.secondsPerStep = rates[i];
  if (!configSave()) {
    ui.secondsPerStep = old;
    notify("Could not write the settings file.");
  } else
    notify("Rate saved.");
}
static void changeStepSource(int sensor) {
  if (!syncSteps(0))
    return;
  int oldTime = ui.walkingEnabled, oldSensor = ui.sensorEnabled;
  if (sensor) {
    if (!ui.sensorAvailable) {
      notify("Pedometer unavailable on this device.");
      return;
    }
    ui.sensorEnabled = !ui.sensorEnabled;
    if (ui.sensorEnabled)
      ui.walkingEnabled = 0;
  } else {
    ui.walkingEnabled = !ui.walkingEnabled;
    ui.sensorEnabled = !ui.walkingEnabled && ui.sensorAvailable;
  }
  if (!configSave()) {
    ui.walkingEnabled = oldTime;
    ui.sensorEnabled = oldSensor;
    notify("Could not save settings.");
    return;
  }
  walkerResume(ee, now(), ui.secondsPerStep);
  dirty = 1;
  rebaseSensor();
  lastAdvance = lastSync = now();
  refresh();
  notify(ui.walkingEnabled  ? "Time-based walking enabled."
         : ui.sensorEnabled ? "Pedometer walking enabled."
                            : "Walking paused.");
}
static void back(void) {
  if (ui.saveError) {
    message("Save failed. Y retries; X offers to discard changes.");
    return;
  }
  if (ui.screen == UI_REPLACE) {
    pendingKind = -1;
    endGame("You left the new reward behind.",
            "The existing pack was kept unchanged.");
    return;
  }
  if (ui.screen == UI_HOME) {
    if (!syncSteps(0))
      return;
    syncStop();
    opened = -1;
    ui.screen = UI_LIST;
    load(ui.cursor);
    message("");
    return;
  }
  if (ui.screen == UI_RADAR || ui.screen == UI_DOWSE ||
      ui.screen == UI_BATTLE) {
    dirty = 1;
    save();
  }
  ui.screen = UI_HOME;
  ui.selection = 0;
  refresh();
}
static int backupReturnScreen;
static void refreshBackups(void) {
  ui.backupCount=backupsCount();ui.backupLimited=backupsLimited();
  if(ui.backupIndex>=ui.backupCount)ui.backupIndex=ui.backupCount?ui.backupCount-1:0;
  ui.selection=ui.backupIndex;
  snprintf(ui.backupWalker,sizeof(ui.backupWalker),"%.*s",(int)sizeof(ui.backupWalker)-1,nameOf(backupTarget));
  ui.backupName[0]=ui.backupDetail[0]=0;
  if(ui.backupCount && !backupsDescribe(ui.backupIndex,ui.backupName,sizeof(ui.backupName),ui.backupDetail,sizeof(ui.backupDetail)))
    snprintf(ui.backupDetail,sizeof(ui.backupDetail),"This snapshot could not be read.");
  snprintf(ui.message,sizeof(ui.message),"%s",ui.backupName);
}
static void openBackups(void) {
  if(opened<0)return;
  if(syncBusy()){notify("Wait for the transfer to finish first.");return;}
  if(!ui.phoneLocked && !syncSteps(0))return;
  backupReturnScreen=ui.phoneLocked?UI_LOCKED:UI_SETTINGS;
  syncStop();snprintf(backupTarget,sizeof(backupTarget),"%s",paths[opened]);backupsScan(backupTarget);ui.backupIndex=0;refreshBackups();ui.screen=UI_BACKUPS;
}
static void openGlobalBackups(void) {
  if(!recoveryCount){notify("No recovery snapshots found under /roms.");return;}
  if(syncBusy()){notify("Wait for sync to finish first.");return;}
  backupReturnScreen=UI_LIST;syncStop();
  snprintf(backupTarget,sizeof(backupTarget),"%s",recoveryPaths[0]);
  backupsScan(backupTarget);ui.backupIndex=0;refreshBackups();ui.screen=UI_BACKUPS;
}
static void backupNotice(const char *text) {
  backupsScan(backupTarget);ui.backupIndex=0;refreshBackups();result("Recovery",text);resultReturn=UI_BACKUPS;
}
static int backupInput(u32 down) {
  if(ui.screen!=UI_BACKUPS && ui.screen!=UI_BACKUP_DELETE && ui.screen!=UI_DELETE_ACTIVE && ui.screen!=UI_RECOVER_SAVE && ui.screen!=UI_RESTORE_BACKUP)return 0;
  if((down & KEY_B) && ui.screen==UI_RECOVER_SAVE){backupsScan(backupTarget);ui.backupIndex=0;refreshBackups();ui.screen=UI_BACKUPS;return 1;}
  if(down & KEY_B){int browsing=ui.screen==UI_BACKUPS;ui.screen=browsing?backupReturnScreen:UI_BACKUPS;if(browsing)ui.selection=0;return 1;}
  if(ui.screen==UI_RESTORE_BACKUP) {
    if(down & KEY_A) {
      if(opened>=0 && !ui.phoneLocked && ui.loaded && !syncSteps(0))return 1;
      if(!backupsLoad(ui.backupIndex,scratch)){backupNotice("This recovery copy could not be read.");return 1;}
      int lock=leaseInspect(backupTarget,0);char error[192],key[37];
      if(lock<0){backupNotice("Phone lock is damaged. Preserve the files for recovery.");return 1;}
      if(lock) {
        if(!walkerTrainerKey(scratch,key)||!leaseMatchesTrainer(backupTarget,key)){backupNotice("Trainer name or ID differs. Restore refused.");return 1;}
        if(!leaseForceUnlock(backupTarget,error,sizeof(error))){backupNotice(error);return 1;}
        if(opened>=0&&!strcmp(paths[opened],backupTarget)){ui.phoneLocked=0;load(opened);dirty=0;}
      }
      if(!backupsRestoreImage(backupTarget,scratch,replaceForLease)){backupNotice("Restore failed. Existing recovery copies kept.");return 1;}
      dirty=0;ui.saveError=ui.phoneLocked=0;opened=-1;scanBegin();notify("Backup restored. Open it from the walker list.");
    }
    return 1;
  }
  if(ui.screen==UI_RECOVER_SAVE) {
    int count=backupsInterruptedCount(backupTarget);
    if((down & (KEY_LEFT|KEY_L)) && ui.backupIndex>0)ui.backupIndex--;
    if((down & (KEY_RIGHT|KEY_R)) && ui.backupIndex+1<count)ui.backupIndex++;
    backupsInterruptedDescribe(backupTarget,ui.backupIndex,ui.backupName,sizeof(ui.backupName),ui.backupDetail,sizeof(ui.backupDetail));
    if(down & KEY_A) {
      if(backupsRestoreInterrupted(backupTarget,ui.backupIndex)){scanBegin();notify("File recovered. Open it from the list.");}
      else notify("Recovery refused. Original copies kept.");
    }
    return 1;
  }
  if((down & KEY_START) && ui.backupCount){ui.screen=UI_RESTORE_BACKUP;return 1;}
  if(down & (KEY_START|KEY_SELECT)) {
    int count=backupsInterruptedCount(backupTarget);
    if(!count){backupNotice("No missing-file recovery is available. Use START on a selected history snapshot to restore it.");return 1;}
    ui.backupIndex=0;ui.backupCount=count;
    backupsInterruptedDescribe(backupTarget,0,ui.backupName,sizeof(ui.backupName),ui.backupDetail,sizeof(ui.backupDetail));
    ui.screen=UI_RECOVER_SAVE;return 1;
  }
  if(ui.screen==UI_DELETE_ACTIVE) {
    if(down & KEY_A) {
      char error[192];const char *path=backupTarget;
      int current=opened>=0 && !strcmp(paths[opened],path);
      unsigned char *data=current?ee:scratch;
      if((current && ui.phoneLocked) || leaseInspect(path,0)) {
        if(!leaseForceUnlock(path,error,sizeof(error))){notify(error);return 1;}
        if(current)ui.phoneLocked=0;
        if(!readFile(path,data)){notify("Could not read recovered walker. Deletion stopped.");return 1;}
      } else if(!current && !readFile(path,data)) {notify("No active file exists for this recovery history.");return 1;}
      if(!backupsPreserveActive(path,data)){notify("Could not preserve recovery copy. Deletion stopped.");return 1;}
      if(remove(path)){notify("Could not delete active walker. Recovery copy kept.");return 1;}
      if(current || opened<0){opened=-1;dirty=0;ui.loaded=ui.phoneLocked=0;scanBegin();}
      else {backupsScan(backupTarget);refreshBackups();ui.screen=UI_BACKUPS;}
      notify("Active walker deleted. Recovery copy kept.");
    }
    return 1;
  }
  if(ui.screen==UI_BACKUP_DELETE) {
    if(down & KEY_A){if(!backupsDelete(ui.backupIndex))notify("Could not delete this recovery copy.");backupsScan(backupTarget);refreshBackups();ui.screen=UI_BACKUPS;}
    return 1;
  }
  if((down & KEY_Y) && recoveryCount) {
    int index=-1;for(int i=0;i<recoveryCount;i++)if(!strcmp(recoveryPaths[i],backupTarget))index=i;
    snprintf(backupTarget,sizeof(backupTarget),"%.511s",recoveryPaths[(index+1)%recoveryCount]);
    backupsScan(backupTarget);ui.backupIndex=0;refreshBackups();return 1;
  }
  int before=ui.backupIndex;
  if((down & (KEY_UP|KEY_LEFT|KEY_L)) && ui.backupIndex>0)ui.backupIndex--;
  if((down & (KEY_DOWN|KEY_RIGHT|KEY_R)) && ui.backupIndex+1<ui.backupCount)ui.backupIndex++;
  if(before!=ui.backupIndex)refreshBackups();
  if(down & KEY_TOUCH){touchPosition touch;hidTouchRead(&touch);if(touch.py>=150 && touch.py<178)down|=KEY_A;else if(touch.py>=181 && touch.py<208)down|=KEY_X;}
  if((down & KEY_A) && ui.backupCount)ui.screen=UI_BACKUP_DELETE;
  if(down & KEY_X)ui.screen=UI_DELETE_ACTIVE;
  return 1;
}
static void input(u32 down) {
  if(backupInput(down))return;
  if(ui.screen==UI_LIST && (down & KEY_SELECT)){openGlobalBackups();return;}
  if (down & KEY_Y) {
    if (ui.saveError) {
      if (save()) {
        ui.discardPrompt = 0;
        ui.screen = UI_HOME;
        refresh();
        notify("Saved. Your progress is safe.");
      }
      return;
    }
  }
  if (ui.saveError) {
    if (!ui.discardPrompt) {
      if (down & (KEY_LEFT | KEY_UP))
        ui.saveChoice = 0;
      if (down & (KEY_RIGHT | KEY_DOWN))
        ui.saveChoice = 1;
      if ((down & KEY_X) || ((down & KEY_A) && ui.saveChoice == 1)) {
        ui.discardPrompt = 1;
      } else if ((down & KEY_A) && ui.saveChoice == 0) {
        if (save()) {
          ui.screen = UI_HOME;
          refresh();
          notify("Saved. Your progress is safe.");
        }
      }
    } else if (down & KEY_A) {
      ui.discardPrompt = 0;
      dirty = 0;
      ui.saveError = 0;
      if (!load(opened) || !walkerCanWalk(ee))
        scanBegin();
      else {
        walkerResume(ee, now(), ui.secondsPerStep);
        dirty = 1;
        lastAdvance = lastSync = now();
        ui.screen = UI_HOME;
      }
    } else if (down & KEY_B) {
      ui.discardPrompt = 0;
    }
    return;
  }
  if (ui.phoneLocked && ui.screen != UI_SYNC && ui.screen != UI_LOCKED &&
      ui.screen != UI_FORCE && ui.screen != UI_RESULT)
    ui.screen = UI_LOCKED;
  if (ui.screen == UI_LOCKED) {
    if(down & KEY_R){openBackups();return;}
    if (down & KEY_A)
      startPhoneSync();
    if (down & KEY_X)
      ui.screen = UI_FORCE;
    if (down & KEY_B) {
      syncStop();
      opened = -1;
      ui.phoneLocked = 0;
      ui.screen = UI_LIST;
    }
    return;
  }
  if (ui.screen == UI_FORCE) {
    if (down & KEY_B)
      ui.screen = UI_LOCKED;
    if (down & KEY_A) {
      char error[192];
      if (leaseForceUnlock(paths[opened], error, sizeof(error)))
        transferredBack("Recovered. Old phone session revoked.");
      else
        result("Recovery could not finish.", error);
    }
    return;
  }
  if (ui.screen == UI_SYNC) {
    if (down & KEY_X && ui.phoneLocked)
      ui.screen = UI_FORCE;
    if (down & KEY_B)
      ui.screen = ui.phoneLocked ? UI_LOCKED : UI_HOME;
    if (down & KEY_START) {
      syncStop();
      if (!ui.phoneLocked && !syncSteps(0))
        return;
      quit = 1;
    }
    return;
  }
  if (down & KEY_START) {
    if (opened >= 0 && !syncSteps(0))
      return;
    quit = 1;
    return;
  }
  if (ui.screen == UI_SCAN)
    return;
  if (ui.screen == UI_LIST) {
    int old = ui.cursor;
    if ((down & KEY_DOWN) && ui.cursor + 1 < ui.count)
      ui.cursor++;
    if ((down & KEY_UP) && ui.cursor > 0)
      ui.cursor--;
    if (down & KEY_R)
      ui.cursor = ui.cursor + 7 < ui.count ? ui.cursor + 7
                  : ui.count               ? ui.count - 1
                                           : 0;
    if (down & KEY_L)
      ui.cursor = ui.cursor > 7 ? ui.cursor - 7 : 0;
    if (down & KEY_TOUCH) {
      touchPosition p;
      hidTouchRead(&p);
      if (p.py >= 43 && p.py < 211) {
        int n = ui.page + (p.py - 43) / 24;
        if (n < ui.count) {
          ui.cursor = n;
          down |= KEY_A;
        }
      }
    }
    ui.page = ui.cursor / 7 * 7;
    if (old != ui.cursor)
      load(ui.cursor);
    if (down & KEY_A)
      openWalker();
    if (down & KEY_X)
      scanBegin();
    return;
  }
  if (ui.screen == UI_RESULT) {
    if ((down & (KEY_A | KEY_B)) && osGetTime() >= deadline) {
      ui.screen = resultReturn;
      if (opened >= 0)
        syncSteps(0);
      else if (ui.count)
        load(ui.cursor);
      message("");
    }
    return;
  }
  if (down & KEY_B) {
    back();
    return;
  }
  if (ui.screen == UI_HOME) {
    if ((down & KEY_LEFT) && ui.selection % 2)
      ui.selection--;
    if ((down & KEY_RIGHT) && !(ui.selection % 2))
      ui.selection++;
    if ((down & KEY_UP) && ui.selection >= 2)
      ui.selection -= 2;
    if ((down & KEY_DOWN) && ui.selection < 4)
      ui.selection += 2;
    if (down & KEY_TOUCH) {
      touchPosition p;
      hidTouchRead(&p);
      if (p.py >= 44 && p.py < 190) {
        int row = (p.py - 44) / 50, col = p.px >= 166;
        if (row < 3) {
          ui.selection = row * 2 + col;
          down |= KEY_A;
        }
      }
    }
    if (down & KEY_A)
      homeAction();
  } else if (ui.screen == UI_SETTINGS) {
    if(down & KEY_X){openBackups();return;}
    if(down & KEY_TOUCH){touchPosition touch;hidTouchRead(&touch);if(touch.py>=198 && touch.py<219){openBackups();return;}}
    if ((down & KEY_UP) && ui.selection > 0)
      ui.selection--;
    if ((down & KEY_DOWN) && ui.selection < 3)
      ui.selection++;
    if (down & KEY_TOUCH) {
      touchPosition p;
      hidTouchRead(&p);
      if (p.py >= 40 && p.py < 190) {
        ui.selection = (p.py - 40) / 39;
        if (ui.selection != 2)
          down |= KEY_A;
        else
          down |= p.px < 160 ? KEY_LEFT : KEY_RIGHT;
      }
    }
    if (ui.selection < 2 && (down & (KEY_A | KEY_LEFT | KEY_RIGHT)))
      changeStepSource(ui.selection == 1);
    if (ui.selection == 2 && (down & KEY_LEFT))
      changeRate(-1);
    if (ui.selection == 2 && (down & KEY_RIGHT))
      changeRate(1);
    if (ui.selection == 3 && (down & KEY_A))
      startPhoneSync();
  } else if (ui.screen == UI_RADAR || ui.screen == UI_DOWSE) {
    int cols = ui.screen == UI_RADAR ? 2 : 3, n = cols * 2;
    if ((down & KEY_LEFT) && ui.selectedPatch % cols)
      ui.selectedPatch--;
    if ((down & KEY_RIGHT) && ui.selectedPatch % cols < cols - 1)
      ui.selectedPatch++;
    if ((down & KEY_UP) && ui.selectedPatch >= cols)
      ui.selectedPatch -= cols;
    if ((down & KEY_DOWN) && ui.selectedPatch < cols)
      ui.selectedPatch += cols;
    if (down & KEY_TOUCH) {
      touchPosition p;
      hidTouchRead(&p);
      if (p.py >= 57 && p.py < 181) {
        int row = (p.py - 57) / 62, col = (p.px - 14) / (cols == 2 ? 155 : 101);
        if (p.px >= 14 && col >= 0 && col < cols && row < 2) {
          int index = row * cols + col;
          if (index < n) {
            ui.selectedPatch = index;
            down |= KEY_A;
          }
        }
      }
    }
    if (down & KEY_A) {
      if (ui.screen == UI_RADAR)
        radarPick();
      else
        dowsePick();
    }
  } else if (ui.screen == UI_BATTLE) {
    if ((down & KEY_LEFT) && ui.selection > 0)
      ui.selection--;
    if ((down & KEY_RIGHT) && ui.selection < 2)
      ui.selection++;
    if (down & KEY_TOUCH) {
      touchPosition p;
      hidTouchRead(&p);
      if (p.py >= 57 && p.py < 124) {
        ui.selection = p.px / 106;
        if (ui.selection > 2)
          ui.selection = 2;
        down |= KEY_A;
      }
    }
    if (down & KEY_A)
      battleAction();
  } else if (ui.screen == UI_REPLACE) {
    if ((down & (KEY_LEFT|KEY_UP)) && ui.selection > 0)
      ui.selection--;
    if ((down & (KEY_RIGHT|KEY_DOWN)) && ui.selection < 2)
      ui.selection++;
    if (down & KEY_TOUCH) {
      touchPosition p;
      hidTouchRead(&p);
      if (p.py >= 48 && p.py < 174) {
        ui.selection = (p.py-48) / 44;
        if (ui.selection > 2)
          ui.selection = 2;
        down |= KEY_A;
      }
    }
    if ((down & KEY_A) && pendingKind >= 0) {
      int ok = pendingKind
                   ? walkerStoreItem(ee, reward, ui.selection, now())
                   : walkerStorePokemon(ee, reward, ui.selection, now());
      int kind=pendingKind;
      pendingKind = -1;
      if (ok > 0) {
        dirty=1;
        if (!backupsPreserveReward(paths[opened],ee)) { saveFailure("Preserving caught reward", EIO); refresh(); return; }
        char title[96];
        snprintf(title,sizeof(title),kind?"You found %s!":"You caught %s!",
                 kind?itemName(walkerRouteItemId(ee,reward)):speciesName(walkerRoutePokemonId(ee,reward)));
        endGame(title,"The new reward replaced your selected slot.");
      }
    }
  }
}
int main(void) {
  if (!uiInit())
    return 1;
  ui.paths = paths;
  ui.statuses = statuses;
  ui.ee = ee;
#ifndef __NDS__
  ui.sensorAvailable = R_SUCCEEDED(ptmuInit());
#endif
  configLoad();
  leaseSetReplace(replaceForLease);
  hidSetRepeatParameters(20, 6);
  scanBegin();
#ifdef __NDS__
  if (!ndsStorageReady())
    result(
        "Storage unavailable.",
        "Launch with working DLDI/SD access. The app needs access to /roms.");
#endif
  static const unsigned windows[4] = {64, 48, 32, 24};
  while (aptMainLoop() && !quit) {
    hidScanInput();
    u32 down = hidKeysDown() | (hidKeysDownRepeat() &
                                (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT));
    input(down);
    if (ui.screen == UI_SCAN)
      scanStep();
    if (ui.screen == UI_RADAR) {
      uint64_t t = osGetTime();
      if (phase == 0 && t >= deadline) {
        phase = 1;
        ui.activePatch = walkerRandom(ee) % 4;
        dirty = 1;
        deadline =
            t + (uint64_t)windows[ui.round < 4 ? ui.round : 3] * 1000 / 30;
      }
      if (phase == 1) {
        uint64_t span =
            (uint64_t)windows[ui.round < 4 ? ui.round : 3] * 1000 / 30;
        ui.progress = t < deadline ? (float)(deadline - t) / span : 0;
        if (t >= deadline)
          endGame("The rustling stopped…", "Try another Poké Radar search.");
      }
    }
    syncPoll();
    if (ui.screen == UI_SYNC) {
      snprintf(ui.syncAddress, sizeof(ui.syncAddress), "%s", syncAddress());
      snprintf(ui.syncStatus, sizeof(ui.syncStatus), "%s", syncStatus());
    }
    if (opened >= 0 && !ui.phoneLocked) {
      uint32_t stamp = now();
      if (ui.walkingEnabled && stamp != lastAdvance) {
        memcpy(scratch, ee, sizeof(ee));
        WalkerReport progress;
        walkerAdvance(ee, stamp, ui.secondsPerStep, &progress);
        if (memcmp(scratch, ee, sizeof(ee)))
          dirty = 1;
        lastAdvance = stamp;
        if (progress.joined)
          notify("A wild Pokémon joined you!");
        else if (progress.encounters)
          notify("A wild visitor is waiting!");
        else if (progress.items)
          notify("Your Pokémon found an item!");
      }
      if (ui.sensorEnabled && stamp != lastAdvance) {
        sampleSensor();
        lastAdvance = stamp;
      }
      if ((ui.screen == UI_HOME || ui.screen == UI_CARD ||
           ui.screen == UI_PACK) &&
          !ui.saveError && now() - lastSync >= 60)
        syncSteps(0);
      refresh();
    }
    uiUpdateClock(&ui, osGetTime());
    ui.frame++;
    uiBegin(0);
    uiTop(&ui);
    uiBegin(1);
    uiBottom(&ui);
    uiEnd();
  }
  syncStop();
  if (directory)
    closedir(directory);
  if (opened >= 0 && !ui.phoneLocked)
    syncSteps(0);
#ifndef __NDS__
  if (ui.sensorAvailable)
    ptmuExit();
#endif
  uiExit();
  return 0;
}
