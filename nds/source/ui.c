/* Native DS interface, designed for 256x192 screens. GPL-3.0-or-later. */
#include "ui.h"
#include "nds_ui.h"
#include "species.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define RGB(r, g, b)                                                           \
  (0x8000 | ((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10))
static const uint16_t BG = RGB(12, 24, 32), PANEL = RGB(24, 45, 53),
                      INK = RGB(242, 250, 242), MUTED = RGB(175, 198, 191),
                      TEAL = RGB(84, 232, 188), GOLD = RGB(255, 213, 112),
                      RED = RGB(255, 154, 154), LCD = RGB(198, 219, 168);
static UIState cached[2];
static int valid[2];
static void text(int x, int y, uint16_t color, const char *s, int columns) {
  ndsUiText(x, y, 1, color, s, columns);
}
static void fmt(int x, int y, uint16_t color, int columns, const char *format,
                ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  text(x, y, color, buffer, columns);
}
static void plain(char *out, const char *in, unsigned size) {
  unsigned n = 0;
  while (*in && n + 1 < size) {
    unsigned char c = *in++;
    if (c < 128)
      out[n++] = c;
    else if (c == 0xc3 && *in) {
      unsigned char d = *in++;
      out[n++] = (d == 0xa9 ? 'e' : d == 0x89 ? 'E' : '?');
    } else {
      while (((unsigned char)*in & 0xc0) == 0x80)
        in++;
      out[n++] = '.';
    }
  }
  out[n] = 0;
}
static void wrap(int x, int y, uint16_t color, const char *value, int columns,
                 int lines) {
  char buffer[512];
  plain(buffer, value, sizeof(buffer));
  const char *p = buffer;
  for (int row = 0; row < lines && *p; row++) {
    while (*p == ' ' || *p == '\n')
      p++;
    unsigned n = 0, space = 0;
    while (p[n] && p[n] != '\n' && n < (unsigned)columns) {
      if (p[n] == ' ')
        space = n;
      n++;
    }
    if (p[n] && p[n] != '\n' && space)
      n = space;
    char line[40];
    memcpy(line, p, n);
    line[n] = 0;
    if (row == lines - 1 && p[n] && n >= 3)
      memcpy(line + n - 3, "...", 3);
    text(x, y + row * 11, color, line, columns);
    p += n;
  }
}
static void panel(int x, int y, int w, int h, int selected) {
  ndsUiRect(x, y, w, h, selected ? TEAL : PANEL);
  if (selected)
    ndsUiRect(x + 2, y + 2, w - 4, h - 4, RGB(28, 73, 67));
}
static void header(const char *title) {
  ndsUiText(8, 6, 2, INK, title, 15);
  ndsUiRect(8, 27, 240, 1, TEAL);
}
static void footer(const char *hint) {
  ndsUiRect(0, 177, 256, 15, PANEL);
  text(8, 181, INK, hint, 30);
}
static void bar(int x, int y, int w, int h, float progress, uint16_t color) {
  if (progress < 0)
    progress = 0;
  if (progress > 1)
    progress = 1;
  ndsUiRect(x, y, w, h, PANEL);
  ndsUiRect(x + 1, y + 1, (int)((w - 2) * progress), h - 2, color);
}
static void portrait(const uint8_t *ee, unsigned offset, int w, int h, int x,
                     int y, int scale) {
  if (!ee || offset + (unsigned)(w * h / 4) > WALKER_FILE_SIZE)
    return;
  const uint16_t shades[] = {LCD, RGB(153, 180, 132), RGB(92, 126, 91),
                             RGB(43, 69, 57)};
  int flip=w==32 && h==24 && offset>=0x91be && offset<0x933e && !(ee[WALKER_ROUTE+14]&1);
  for (int py = 0; py < h; py++)
    for (int px = 0; px < w; px++) {
      unsigned pos = offset + ((py / 8) * w + px) * 2, bit = py & 7;
      unsigned c = ((ee[pos] >> bit) & 1) * 2 + ((ee[pos + 1] >> bit) & 1);
      if (c)
        ndsUiRect(x + (flip?w-1-px:px) * scale, y + py * scale, scale, scale, shades[c]);
    }
}
int uiInit(void) {
  memset(valid, 0, sizeof(valid));
  return ndsUiInit();
}
void uiExit(void) {}
void uiBegin(int bottom) { (void)bottom; }
void uiEnd(void) { ndsUiEnd(); }
void uiNotify(UIState *s, const char *value, uint64_t now) {
  snprintf(s->toast, sizeof(s->toast), "%s", value);
  s->toastScreen = s->screen;
  s->toastUntil = now + 5000;
}
void uiUpdateClock(UIState *s, uint64_t now) {
  s->clockMs = now;
  if (now >= s->toastUntil || s->screen != s->toastScreen) {
    s->toast[0] = 0;
    s->toastUntil = 0;
  }
}
void uiTop(const UIState *s) {
  UIState key = {0};
  key.info = s->info;
  key.loaded = s->loaded;
  key.pending = s->pending;
  key.walkingEnabled = s->walkingEnabled;
  key.secondsPerStep = s->secondsPerStep;
  key.phoneLocked = s->phoneLocked;
  key.frame = (s->frame / 24) & 1;
  key.battle = s->battle;
  key.screen = s->screen == UI_BATTLE || s->screen == UI_LIST ? s->screen : 0;
  snprintf(key.filename, sizeof(key.filename), "%s", s->filename);
  if (valid[0] && !memcmp(&key, &cached[0], sizeof(key)))
    return;
  cached[0] = key;
  valid[0] = 1;
  ndsUiBegin(0, BG);
  text(8, 5, TEAL, "NDS POKEWALKER", 30);
  if(s->screen==UI_LIST && s->loaded) {
    text(8,30,INK,s->filename,30);
    text(8,57,TEAL,"TRAINER",30);
    text(8,75,INK,s->info.registered&&s->info.trainer[0]?s->info.trainer:"Unavailable",30);
    if(s->info.registered)fmt(8,100,GOLD,30,"Trainer ID: %05lu",(unsigned long)s->info.trainerId);
    else text(8,100,GOLD,"Trainer ID unavailable",30);
    text(8,129,INK,s->info.hasPokemon?speciesName(s->info.species):"No companion",30);
    return;
  }
  if (!s->loaded || !s->info.hasPokemon) {
    ndsUiRect(60, 38, 136, 99, PANEL);
    text(88, 80, GOLD, "NO PARTNER", 13);
    if (s->loaded && s->info.registered) {
      fmt(8, 112, TEAL, 30, "%lu steps  %u watts", (unsigned long)(s->info.strollSteps+s->pending),s->info.watts);
    }
    wrap(16, 149, INK,
         s->phoneLocked ? "Walker is checked out to a phone."
                        : s->loaded && s->info.registered ? "No partner. Send a Pokémon from HGSS." : "Choose an active walker below.",
         28, 2);
    return;
  }
  fmt(8, 20, INK, 30, "%s  Lv %u", speciesName(s->info.species), s->info.level);
  if (s->screen == UI_BATTLE) {
    ndsUiRect(40, 37, 176, 75, LCD);
    portrait(s->ee, 0x91be + key.frame * 192, 32, 24, 46, 49, 2);
    portrait(s->ee, 0x9a7e + s->battle.encounter * 384 + key.frame * 192, 32,
             24, 144, 49, 2);
    fmt(8, 123, INK, 30, "Partner HP %d/4", s->battle.playerHp);
    fmt(8, 138, GOLD, 30, "Wild HP    %d/4", s->battle.enemyHp);
    text(8, 158, MUTED,
         speciesName(walkerRoutePokemonId(s->ee, s->battle.encounter)), 30);
  } else {
    /* Integer 2x scale preserves every pixel of the original walker portrait.
     */
    ndsUiRect(60, 34, 136, 100, PANEL);
    ndsUiRect(64, 36, 128, 96, LCD);
    portrait(s->ee, 0x933e + key.frame * 768, 64, 48, 64, 36, 2);
    bar(8, 135, 240, 3, (s->info.stepsThisWatt + s->pending) % 20 / 20.f, TEAL);
    text(8, 140, MUTED, "STROLL STEPS", 16);
    text(176, 140, MUTED, "WATTS", 9);
    char value[32];
    snprintf(value, sizeof(value), "%lu",
             (unsigned long)(s->info.strollSteps + s->pending));
    ndsUiText(8, 151, 2, INK, value, 9);
    snprintf(value, sizeof(value), "%u", s->info.watts);
    ndsUiText(176, 151, 2, GOLD, value, 4);
  }
  fmt(8, 171, TEAL, 30, "Today %lu  %s",
      (unsigned long)(s->info.todaySteps + s->pending),
      s->walkingEnabled ? "Walking" : "Paused");
  text(8, 183, MUTED, s->info.route[0] ? s->info.route : "Your next adventure",
       30);
}
void uiBottom(const UIState *s) {
  UIState key = *s;
  key.frame = s->screen == UI_SCAN ? s->frame / 8 : 0;
  key.clockMs = key.toastUntil = 0;
  key.now = 0;
  key.progress = (int)(s->progress * 100) / 100.f;
  if (valid[1] && !memcmp(&key, &cached[1], sizeof(key)))
    return;
  cached[1] = key;
  valid[1] = 1;
  ndsUiBegin(1, BG);
  if (s->saving) {
    header("Saving...");
    wrap(8, 42, INK, s->saveStage, 30, 3);
    wrap(8, 90, GOLD, "Please wait. Keep the console on until saving finishes.", 30, 4);
    footer("Controls locked while saving");
    return;
  }
  if (s->saveError) {
    header(s->discardPrompt ? "Discard changes" : "Save failed");
    if (s->discardPrompt) {
      wrap(8, 40, INK,
           "Discard unsaved changes and reload the file on the card?", 30, 4);
      wrap(8, 105, GOLD, "Progress still in memory will be lost.", 30, 3);
      footer("A Discard   B Keep");
    } else {
      wrap(8, 38, RED, s->saveStage, 30, 2);
      wrap(8, 66, INK, s->saveReason, 30, 7);
      panel(8, 150, 116, 23, s->saveChoice == 0);
      panel(132, 150, 116, 23, s->saveChoice == 1);
      text(16, 158, INK, "Retry save", 12);
      text(140, 158, INK, "Discard...", 12);
      footer("D-pad A Select Y Retry");
    }
    return;
  }
  if (s->screen == UI_CLASSIC && s->classicPixels) {
    ndsUiWalkerShell(s->classicPixels, s->classicContrast);
    return;
  }
  if (s->screen == UI_FOLDER_PICKER) {
    header("Choose search folder");
    wrap(8, 36, TEAL, s->folderPath, 30, 2);
    if (!s->folderCount)
      text(8, 63, INK, "No subfolders", 30);
    int first = s->folderCursor / 6 * 6;
    for (int row = 0; row < 6 && first + row < s->folderCount; row++) {
      int index = first + row, y = 58 + row * 17;
      panel(8, y, 240, 15, index == s->folderCursor);
      text(12, y + 4, INK, strrchr(s->folderPaths[index], '/') + 1, 28);
    }
    text(8, 163, s->folderLimited ? GOLD : MUTED,
         s->folderLimited ? "Folder list limit reached" : "Y Scan /roms  X Cancel", 30);
    footer("B Up  START Scan here");
    return;
  }
  if (s->screen == UI_SCAN) {
    header(s->validation ? "Checking files" : "Finding walkers");
    wrap(8, 40, INK,
         s->validation ? "Reading your walker files."
                       : s->scanRoot,
         30, 3);
    float progress = s->validation ? s->progress : (s->frame % 120) / 119.f;
    bar(8, 94, 240, 12, progress, TEAL);
    fmt(8, 119, INK, 30, "%d folders  %d files", s->dirs, s->files);
    fmt(8, 137, TEAL, 30, "%d walkers found", s->count);
    if (s->validation)
      fmt(8, 155, MUTED, 30, "Checked %d of %d", s->done, s->total);
    footer("X Folders  START Cancel");
    return;
  }
  if (s->screen == UI_LIST) {
    header("Your walkers");
    if (!s->count)
      wrap(8, 42, INK,
           "No .walker files found. Press X to choose a search folder.",
           30, 7);
    for (int row = 0; row < 7 && s->page + row < s->count; row++) {
      int i = s->page + row, y = 35 + row * 19;
      panel(8, y, 240, 17, i == s->cursor);
      const char *p = strrchr(s->paths[i], '/');
      p = p ? p + 1 : s->paths[i];
      char name[512];
      snprintf(name, sizeof(name), "%s", p);
      char *ext = strrchr(name, '.');
      if (ext)
        *ext = 0;
      text(12, y + 5, INK, name, s->statuses[i] >= 2 ? 22 : 28);
      if (s->statuses[i] >= 2)
        text(196, y + 5, GOLD, s->statuses[i] == 3 ? "Phone" : "Empty", 6);
    }
    if (s->scanLimited)
      text(8, 169, GOLD, "Search limit reached", 30);
    footer("X Folder  SEL Backups");
    return;
  }
  if(s->screen==UI_COURSE_CONFIRM){
    header("Special courses");
    wrap(8,35,INK,"Activate special/event courses supported by this HGSS save's language. Normal watt unlocks stay unchanged. The game save is verified and backed up first.",30,8);
    wrap(8,132,GOLD,s->phonePaths[s->phoneCursor],30,3);footer("A Activate B Cancel");return;
  }
  if(s->screen==UI_PHONE_TARGET || s->screen==UI_PHONE_RECEIVE_CONFIRM){
    header(s->screen==UI_PHONE_TARGET?"Return destination":"Receive phone copy?");
    if(s->screen==UI_PHONE_TARGET){
      for(int row=0;row<6;row++){int n=(s->phoneCursor/6)*6+row;if(n>=s->phoneCount)break;
        const char *name=strrchr(s->phonePaths[n],'/');panel(8,36+row*22,240,20,n==s->phoneCursor);text(12,42+row*22,INK,name?name+1:s->phonePaths[n],29);
      }
      footer("A Phone X Courses B Cancel");
    }else{
      wrap(8,35,INK,"Use only if the console walker is missing. Receive the phone's current copy into this game's saves folder. An existing active walker cannot be overwritten.",30,8);
      wrap(8,132,GOLD,s->phonePaths[s->phoneCursor],30,3);footer("A Receive phone B Cancel");
    }return;
  }
  if(s->screen==UI_REWARD_SETTINGS) {
    header("Dowsing rewards");
    wrap(8,36,INK,"Optional expanded items: Master Ball 0.01%, PP Max 0.09%, PP Up 0.20%, TM01-92 1.70%. Remaining 98% uses original course rewards.",30,9);
    fmt(8,141,TEAL,30,"Currently %s",s->expandedItems?"enabled":"disabled");
    footer("A Opt in X Disable B Back");return;
  }
  if(s->screen==UI_RESTORE_BACKUP) {
    header("Restore selected backup?");
    wrap(8,36,INK,"A: Restore backup (revokes phone). X: Receive matching phone Pokemon into a missing target. Old data may duplicate Pokemon.",30,6);
    wrap(8,126,GOLD,s->backupName,30,3);
    footer("A Restore X Phone B Cancel");return;
  }
  if(s->screen==UI_RECOVER_SAVE) {
    header("Recover interrupted save?");
    wrap(8,34,INK,"Final walker missing. Choose a file. Originals are kept.",30,3);
    wrap(8,80,GOLD,s->backupName,30,3);
    wrap(8,128,INK,s->backupDetail,30,3);
    footer("L/R File A Recover B Cancel");return;
  }
  if(s->screen==UI_BACKUPS || s->screen==UI_BACKUP_DELETE || s->screen==UI_DELETE_ACTIVE) {
    int active=s->screen==UI_DELETE_ACTIVE,confirm=s->screen!=UI_BACKUPS;
    header(active?"Delete active walker?":confirm?"Delete recovery copy?":"Recovery copies");
    if(confirm) {
      wrap(8,40,INK,active?"Delete the active walker? A recovery copy is kept first. A walker on the phone keeps its return permission.":"Permanently delete this snapshot? Active data and ownership records are kept.",30,5);
      wrap(8,118,GOLD,active?s->backupWalker:s->backupName,30,3);
      footer("A Confirm delete B Cancel");
    } else {
      fmt(8,32,TEAL,30,"%d / %d copies%s",s->backupCount?s->backupIndex+1:0,s->backupCount,s->backupLimited?" (first 128)":"");
      wrap(8,48,INK,s->backupCount?s->backupName:"No recovery snapshots yet.",30,2);
      wrap(8,84,MUTED,s->backupDetail,30,2);
      text(8,110,TEAL,"START Restore SELECT Missing",30);
        panel(8,124,240,24,0);text(16,132,INK,"A Delete selected backup",28);
      panel(8,152,240,24,0);text(16,160,GOLD,"X Delete active walker",28);
      footer("L/R Copy Y Walker B Back");
    }
    return;
  }
  if (s->screen == UI_SYNC) {
    header("Wi-Fi sync");
    text(8, 39, MUTED, "Console IP address", 30);
    fmt(8, 55, TEAL, 30, "%s:30820", s->syncAddress);
    wrap(8, 77, INK, s->syncStatus, 30, 6);
    wrap(8, 151, MUTED, "Use the same local Wi-Fi on both devices.", 30, 2);
    footer(s->phoneLocked ? "B Back  X Force unlock" : "B Back  START Save and exit");
    return;
  }
  if (s->screen == UI_LOCKED || s->screen == UI_FORCE) {
    int force = s->screen == UI_FORCE;
    header(force ? "Force unlock?" : "On phone");
    wrap(
        8, 40, INK,
        force
            ? "Restore the console snapshot? Recovery copies are kept. "
              "The old phone session cannot sync back until a fresh handoff."
            : "This walker is locked while a phone owns it. Return it over "
              "Wi-Fi, or recover the local snapshot if the phone is "
              "unavailable.",
        30, 11);
    footer(force              ? "A Restore and revoke B Cancel"
           : s->wifiAvailable ? "A Sync X Recover R Backups"
                              : "A Sync X Recover R Backups");
    return;
  }
  if (s->screen == UI_WIFI) {
    if (s->wifiPhase == 6) {
      header("Network name");
      text(8, 33, INK, s->wifiSsid, 30);
      if (strlen(s->wifiSsid) > 30)
        text(8, 44, INK, s->wifiSsid + 30, 2);
      unsigned length = strlen(s->wifiSsid);
      if (length < 32)
        text(8 + (length % 30) * 8, 33 + (length / 30) * 11, TEAL, "|", 1);
      if (!strncmp(s->wifiMessage, "Enter a name", 12))
        text(8, 56, GOLD, "A name is required", 22);
      else
        fmt(8, 56, MUTED, 22, "R Keyboard page %d/2", s->wifiTextPage + 1);
      fmt(208, 56, TEAL, 5, "%u/32", length);
      for (int i = 0; i < 48; i++) {
        int x = 8 + (i % 8) * 30, y = 68 + (i / 8) * 16;
        unsigned ch = 32 + s->wifiTextPage * 48 + i;
        panel(x, y, 28, 14, i == s->wifiKeyCursor);
        char glyph[2] = {ch < 127 ? (char)ch : ' ', 0};
        text(x + (ch == 32 ? 6 : 10), y + 3, INK, ch == 32 ? "SP" : glyph, 2);
      }
      footer("A Add X Del R Page START Done");
      return;
    }
    if (s->wifiPhase == 2) {
      header("WEP key");
      text(8, 33, MUTED, "Use 10, 26 or 32 hex digits", 30);
      char masked[33];
      unsigned len = strlen(s->wifiKey);
      memset(masked, '*', len);
      masked[len] = 0;
      fmt(8, 48, INK, 30, "%u: %s", len, masked);
      for (int i = 0; i < 16; i++) {
        int x = 8 + (i % 4) * 60, y = 65 + (i / 4) * 24;
        panel(x, y, 56, 21, i == s->wifiKeyCursor);
        char ch[2] = {"0123456789ABCDEF"[i], 0};
        text(x + 24, y + 7, INK, ch, 1);
      }
      if (s->wifiMessage[0] != 'E')
        wrap(8, 164, GOLD, s->wifiMessage, 30, 1);
      footer("A Add X Delete START Connect");
      return;
    }
    header("Wi-Fi setup");
    if (s->wifiPhase == 1) {
      for (int row = 0; row < 6; row++) {
        int i = s->wifiCursor / 6 * 6 + row;
        if (i >= s->wifiCount)
          break;
        int y = 38 + row * 21;
        panel(8, y, 240, 19, i == s->wifiCursor);
        text(12, y + 6, INK, s->wifiNames[i], 22);
        text(192, y + 6, s->wifiSecurity[i] == 2 ? RED : TEAL,
             s->wifiSecurity[i] == 2 ? "WPA"
             : s->wifiSecurity[i]    ? "WEP"
                                     : "Open",
             6);
      }
      wrap(8, 165, GOLD, s->wifiMessage, 30, 1);
      footer("A Select Y SSID R Scan B Back");
    } else {
      wrap(8, 42, s->wifiPhase == 4 ? TEAL : INK, s->wifiMessage, 30, 9);
      wrap(8, 151, MUTED, "DS Lite: open or WEP networks.", 30, 2);
      footer(s->wifiPhase == 0 || s->wifiPhase == 5
                 ? "Y Enter SSID R Rescan B Back"
                 : "R Rescan B Back");
    }
    return;
  }
  const char *title = s->screen == UI_HOME       ? "Adventure menu"
                      : s->screen == UI_SETTINGS ? "Settings 1.7.0"
                      : s->screen == UI_RADAR    ? "Poke Radar"
                      : s->screen == UI_DOWSE    ? "Dowsing"
                      : s->screen == UI_BATTLE   ? "Choose a move"
                      : s->screen == UI_PACK     ? "Your pack"
                      : s->screen == UI_REPLACE  ? "Choose a slot"
                      : s->screen == UI_CARD     ? "Trainer card"
                                                 : "Journal";
  header(title);
  if (s->screen == UI_HOME) {
    const char *labels[] = {"Pokewalker", "Update steps", "Settings"};
    for (int i = 0; i < 3; i++) {
      int y = 36 + i * 40;
      panel(8,y,240,36,i==s->selection);
      text(16,y+12,INK,labels[i],25);
    }
    wrap(8, 158, TEAL, s->toast, 30, 1);
  } else if (s->screen == UI_SETTINGS) {
    const char *labels[] = {"Time-based steps", "Pedometer", "Step rate",
                            "Phone sync", "Wi-Fi setup"};
    for (int i = 0; i < (s->wifiAvailable ? 5 : 4); i++) {
      int y = 32 + i * (s->wifiAvailable ? 25 : 31);
      panel(8, y, 240, s->wifiAvailable ? 23 : 28, i == s->selection);
      text(16, y + 8, INK, labels[i], 21);
      char value[24];
      if (i == 2)
        snprintf(value, sizeof(value),
                 s->secondsPerStep >= 3600 ? "%uh" : "%us",
                 s->secondsPerStep >= 3600 ? s->secondsPerStep / 3600
                                           : s->secondsPerStep);
      if (i != 2)
        snprintf(value, sizeof(value), "%s",
                 i >= 3              ? "Open"
                 : i == 1            ? "N/A"
                 : s->walkingEnabled ? "On"
                                     : "Off");
      text(200, y + 8, TEAL, value, 5);
    }
    panel(8,156,240,20,0);text(16,162,TEAL,"X Backups  SELECT Rewards",28);

  } else if (s->screen == UI_PACK || s->screen == UI_REPLACE) {
    int replace = s->screen == UI_REPLACE;
    for (int i = 0; i < 3; i++) {
      unsigned id = (replace && s->replaceKind) ? walkerItemId(s->ee, i)
                                                : walkerPokemonId(s->ee, i);
      int y = 38 + i * (replace ? 32 : 18);
      if (replace)
        panel(8, y, 240, 28, i == s->selection);
      if (replace && s->replaceKind)
        fmt(16, y + 8, INK, 28, "%d. %s", i + 1, itemName(id));
      else
        fmt(16, y + (replace ? 8 : 0), INK, 28, "%d. %s", i + 1,
            id && id != 65535 ? speciesName(id) : "Empty");
    }
    if (!replace) {
      text(8, 101, GOLD, "Items", 30);
      for (int i = 0; i < 3; i++) {
        unsigned id = walkerItemId(s->ee, i);
        if (id && id != 65535)
          fmt(16, 118 + i * 16, INK, 28, "%d. %s", i + 1, itemName(id));
        else
          fmt(16, 118 + i * 16, MUTED, 28, "%d. Empty", i + 1);
      }
    } else
      wrap(8, 144, INK, "A Replace selected slot. B Leave the new reward.", 30,
           2);
  } else if (s->screen == UI_CARD) {
    fmt(8, 40, INK, 30, "Trainer: %s",
        s->info.trainer[0] ? s->info.trainer : "Not registered");
    fmt(8, 60, TEAL, 30, "%s  Lv %u", speciesName(s->info.species),
        s->info.level);
    fmt(8, 80, INK, 30, "Bond: %u / 255", s->info.friendship);
    fmt(8, 100, INK, 30, "%u Pokemon / %u items packed", s->info.pokemonCount,
        s->info.itemCount);
    fmt(8, 120, INK, 30, "Lifetime steps: %lu",
        (unsigned long)s->info.totalSteps);
    fmt(8, 140, INK, 30, "Today: %lu", (unsigned long)s->info.todaySteps);
    text(8, 160, MUTED, s->info.route, 30);
  } else if (s->screen == UI_RESULT || s->screen == UI_DISCARD_WALKER) {
    wrap(8, 39, GOLD, s->message, 30, 3);
    wrap(8, 79, INK, s->detail, 30, 8);
    footer(s->screen == UI_DISCARD_WALKER ? "A Discard  B Keep" : "A Continue B Back");
    return;
  }
  footer(s->screen == UI_HOME ? "START Save and exit  B Back"
         : s->screen == UI_SETTINGS ? "D-pad/A Change B Back"
                                  : "D-pad/Touch A Select B Back");
}
