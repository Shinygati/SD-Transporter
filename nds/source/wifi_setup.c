/* DS Lite access-point setup using devkitPro WFC APIs. GPL-3.0-or-later.
 * App profiles live on the card; the console firmware is never written. */
#include "wifi_setup.h"
#include "sdrename.h"
#include <dswifi9.h>
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <wfc.h>
#define PROFILE "/data/pokewalker/wifi.bin"
static int initialized, scanStarted, connectedSaved;
static time_t started;
static WlanBssDesc accessPoints[WLAN_MAX_BSS_ENTRIES], chosen;
static WlanAuthData auth;
static int editingScanned;
static WlanBssScanFilter manualFilter;
static int targetedScan;
static const WlanBssScanFilter filter = {
    .channel_mask = UINT32_MAX, .target_bssid = {255, 255, 255, 255, 255, 255}};
typedef struct {
  char magic[8];
  WfcConnSlot slot;
  uint32_t crc;
} Profile;
static Profile profile;
int ndsWifiAvailable(void) {
  unsigned type = g_envExtraInfo->nvram_console_type;
  return type == EnvConsoleType_DSLite || type == EnvConsoleType_iQueDSLite;
}
static int initialize(void) {
  if (!initialized)
    initialized = Wifi_InitDefault(INIT_ONLY);
  return initialized;
}
static int loadProfile(void) {
  memset(&profile, 0, sizeof(profile));
  FILE *f = fopen(PROFILE, "rb");
  if (!f)
    return 0;
  int ok = fread(&profile, 1, sizeof(profile), f) == sizeof(profile) &&
           fgetc(f) == EOF;
  fclose(f);
  return ok && !memcmp(profile.magic, "PWIFI01", 8) &&
         profile.slot.ssid_len > 0 && profile.slot.ssid_len <= 32 &&
         profile.slot.conn_type == WfcConnType_WepNormal &&
         profile.slot.wep_mode <= 3 &&
         profile.crc ==
             walkerCrc((const uint8_t *)&profile, sizeof(profile) - 4);
}
static int saveProfile(void) {
  memset(&profile, 0, sizeof(profile));
  memcpy(profile.magic, "PWIFI01", 8);
  profile.slot.ssid_len = chosen.ssid_len;
  memcpy(profile.slot.ssid, chosen.ssid, chosen.ssid_len);
  profile.slot.conn_type = WfcConnType_WepNormal;
  profile.slot.wep_mode = chosen.auth_type;
  memcpy(profile.slot.wep_keys[0], auth.wep_key, sizeof(auth.wep_key));
  profile.crc = walkerCrc((const uint8_t *)&profile, sizeof(profile) - 4);
  mkdir("/data", 0777);
  mkdir("/data/pokewalker", 0777);
  FILE *f = fopen(PROFILE ".tmp", "wb");
  if (!f)
    return 0;
  int ok = fwrite(&profile, 1, sizeof(profile), f) == sizeof(profile);
  if (fflush(f))
    ok = 0;
  if (fclose(f))
    ok = 0;
  uint32_t native = 0;
  return ok && !sdRenameReplace(PROFILE ".tmp", PROFILE, &native);
}
int ndsWifiConnectSaved(void) {
  if (!initialize())
    return 0;
  if (Wifi_AssocStatus() == ASSOCSTATUS_ASSOCIATED)
    return 1;
  if (ndsWifiAvailable() && loadProfile()) {
    wfcClearConnSlots();
    if (!wfcLoadSlot(&profile.slot))
      return 0;
  } else
    wfcLoadFromNvram();
  return wfcBeginAutoConnect();
}
static void status(UIState *s, const char *message) {
  snprintf(s->wifiMessage, sizeof(s->wifiMessage), "%s", message);
}
void ndsWifiSetupBegin(UIState *s) {
  s->screen = UI_WIFI;
  s->wifiPhase = 0;
  s->wifiCount = 0;
  s->wifiCursor = 0;
  s->wifiKey[0] = 0;
  s->wifiKeyCursor = 0;
  scanStarted = connectedSaved = 0;
  targetedScan = 0;
  started = time(NULL);
  if (!ndsWifiAvailable() || !initialize()) {
    s->wifiPhase = 5;
    status(s, "Wi-Fi could not initialize.");
    return;
  }
  if (wlmgrGetState() >= WlMgrState_Scanning)
    wlmgrStop();
  status(s, "Scanning for nearby networks...");
}
static void connect(UIState *s) {
  memset(&auth, 0, sizeof(auth));
  unsigned n = strlen(s->wifiKey);
  if (chosen.auth_type != WlanBssAuthType_Open) {
    if (n != 10 && n != 26 && n != 32) {
      status(s, "Use a 10, 26 or 32 digit hexadecimal WEP key.");
      return;
    }
    chosen.auth_type = n == 10   ? WlanBssAuthType_WEP_40
                       : n == 26 ? WlanBssAuthType_WEP_104
                                 : WlanBssAuthType_WEP_128;
    for (unsigned i = 0; i < n / 2; i++) {
      unsigned value;
      char pair[3] = {s->wifiKey[i * 2], s->wifiKey[i * 2 + 1], 0};
      if (sscanf(pair, "%2x", &value) != 1) {
        status(s, "The WEP key contains an invalid digit.");
        return;
      }
      auth.wep_key[i] = value;
    }
  }
  if (!wfcBeginConnect(&chosen, &auth)) {
    s->wifiPhase = 5;
    status(s, "Wi-Fi is busy. Press R to scan again.");
    return;
  }
  s->wifiPhase = 3;
  started = time(NULL);
  connectedSaved = 0;
  status(s, "Connecting to the network...");
}

static void authenticate(UIState *s) {
  if (chosen.auth_type == WlanBssAuthType_Open)
    connect(s);
  else {
    s->wifiPhase = 2;
    s->wifiKey[0] = 0;
    s->wifiKeyCursor = 0;
    status(s, "Enter the hexadecimal WEP key.");
  }
}

static void editSsid(UIState *s, int scanned) {
  editingScanned = scanned;
  s->wifiSsid[0] = 0;
  s->wifiTextPage = 0;
  s->wifiKeyCursor = 0;
  s->wifiPhase = 6;
  status(s, "Enter the exact network name.");
  if (!scanned && wlmgrGetState() >= WlMgrState_Scanning)
    wlmgrStop();
}

static void submitSsid(UIState *s) {
  unsigned length = strlen(s->wifiSsid);
  if (!length || length > WLAN_MAX_SSID_LEN) {
    status(s, "Enter a name of 1 to 32 bytes.");
    return;
  }
  if (editingScanned) {
    memset(chosen.ssid, 0, sizeof(chosen.ssid));
    memcpy(chosen.ssid, s->wifiSsid, length);
    chosen.ssid_len = length;
    authenticate(s);
  } else {
    manualFilter = filter;
    manualFilter.target_ssid_len = length;
    memcpy(manualFilter.target_ssid, s->wifiSsid, length);
    targetedScan = 1;
    scanStarted = 0;
    started = time(NULL);
    s->wifiPhase = 0;
    s->wifiCount = s->wifiCursor = 0;
    status(s, "Searching for the entered SSID...");
  }
}
void ndsWifiSetupPoll(UIState *s) {
  if (s->screen != UI_WIFI)
    return;
  if (s->wifiPhase == 0) {
    WlMgrState state = wlmgrGetState();
    if (!scanStarted &&
        (state == WlMgrState_Stopped || state == WlMgrState_Idle))
      scanStarted = wfcBeginScan(targetedScan ? &manualFilter : &filter);
    unsigned count = 0;
    WlanBssDesc *found = scanStarted ? wfcGetScanBssList(&count) : NULL;
    if (found) {
      if (count > WLAN_MAX_BSS_ENTRIES)
        count = WLAN_MAX_BSS_ENTRIES;
      memcpy(accessPoints, found, count * sizeof(*found));
      s->wifiCount = count;
      s->wifiPhase = 1;
      for (unsigned i = 0; i < count; i++) {
        if (targetedScan && !accessPoints[i].ssid_len) {
          accessPoints[i].ssid_len = manualFilter.target_ssid_len;
          memcpy(accessPoints[i].ssid, manualFilter.target_ssid,
                 manualFilter.target_ssid_len);
        }
        unsigned len = accessPoints[i].ssid_len;
        if (len > 32)
          len = 32;
        memcpy(s->wifiNames[i], accessPoints[i].ssid, len);
        s->wifiNames[i][len] = 0;
        if (!len)
          strcpy(s->wifiNames[i], "[Hidden SSID]");
        unsigned mask = accessPoints[i].auth_mask;
        s->wifiSecurity[i] = (mask & 1) ? 0 : (mask & 14) ? 1 : 2;
      }
      status(s,
             count ? "A Select. Y Enter SSID." : "No networks. Y Enter SSID.");
    } else if (time(NULL) - started > 30) {
      wlmgrStop();
      s->wifiPhase = 5;
      status(s, "Scan timed out. Press R to retry.");
    }
  } else if (s->wifiPhase == 3) {
    int value = Wifi_AssocStatus();
    if (value == ASSOCSTATUS_ASSOCIATED) {
      s->wifiPhase = 4;
      connectedSaved = saveProfile();
      snprintf(s->wifiMessage, sizeof(s->wifiMessage), "Connected. IP %s. %s",
               inet_ntoa(Wifi_GetIPInfo(NULL, NULL, NULL, NULL)),
               connectedSaved ? "Network saved for phone sync."
                              : "Could not save network to the card.");
      memset(s->wifiKey, 0, sizeof(s->wifiKey));
    } else if (value == ASSOCSTATUS_CANNOTCONNECT ||
               time(NULL) - started > 40) {
      wlmgrStop();
      s->wifiPhase = 5;
      status(s, "Connection failed. Check the network and key, then press R to "
                "retry.");
    } else
      status(s, value == ASSOCSTATUS_ACQUIRINGDHCP
                    ? "Obtaining an IP address..."
                    : "Connecting to the network...");
  }
}
int ndsWifiSetupInput(UIState *s, unsigned down) {
  if (down & KEY_B) {
    if (s->wifiPhase == 6) {
      s->wifiPhase = 1;
      s->wifiKeyCursor = 0;
      status(s, "Choose a network. Y Enter SSID.");
      return 0;
    }
    if (s->wifiPhase == 2) {
      s->wifiPhase = 1;
      memset(s->wifiKey, 0, sizeof(s->wifiKey));
      status(s, "Choose a network.");
      return 0;
    }
    if (s->wifiPhase != 4)
      wlmgrStop();
    memset(s->wifiKey, 0, sizeof(s->wifiKey));
    return 1;
  }
  if ((s->wifiPhase == 0 || s->wifiPhase == 1 || s->wifiPhase == 5) &&
      (down & KEY_Y)) {
    editSsid(s, 0);
    return 0;
  }
  if (s->wifiPhase != 2 && s->wifiPhase != 6 && (down & KEY_R)) {
    ndsWifiSetupBegin(s);
    return 0;
  }
  if (s->wifiPhase == 1) {
    if ((down & KEY_UP) && s->wifiCursor > 0)
      s->wifiCursor--;
    if ((down & KEY_DOWN) && s->wifiCursor + 1 < s->wifiCount)
      s->wifiCursor++;
    if (down & KEY_TOUCH) {
      touchPosition p;
      touchRead(&p);
      int row = (p.py - 38) / 21;
      if (p.py >= 38 && row < 6 && s->wifiCursor / 6 * 6 + row < s->wifiCount) {
        s->wifiCursor = s->wifiCursor / 6 * 6 + row;
        down |= KEY_A;
      }
    }
    if ((down & KEY_A) && s->wifiCount) {
      int index = s->wifiCursor;
      if (s->wifiSecurity[index] == 2) {
        status(s,
               "DS Lite supports open/WEP only. WPA networks cannot connect.");
        return 0;
      }
      chosen = accessPoints[index];
      chosen.auth_type = s->wifiSecurity[index] ? WlanBssAuthType_WEP_40
                                                : WlanBssAuthType_Open;
      if (!chosen.ssid_len)
        editSsid(s, 1);
      else
        authenticate(s);
    }
  } else if (s->wifiPhase == 6) {
    if ((down & KEY_LEFT) && s->wifiKeyCursor % 8)
      s->wifiKeyCursor--;
    if ((down & KEY_RIGHT) && s->wifiKeyCursor % 8 < 7)
      s->wifiKeyCursor++;
    if ((down & KEY_UP) && s->wifiKeyCursor >= 8)
      s->wifiKeyCursor -= 8;
    if ((down & KEY_DOWN) && s->wifiKeyCursor < 40)
      s->wifiKeyCursor += 8;
    if (down & KEY_R)
      s->wifiTextPage ^= 1;
    if (down & KEY_TOUCH) {
      touchPosition p;
      touchRead(&p);
      if (p.px >= 8 && p.px < 248 && p.py >= 68 && p.py < 164) {
        s->wifiKeyCursor = (p.py - 68) / 16 * 8 + (p.px - 8) / 30;
        down |= KEY_A;
      }
    }
    unsigned length = strlen(s->wifiSsid);
    if ((down & KEY_X) && length)
      s->wifiSsid[--length] = 0;
    unsigned ch = 32 + s->wifiTextPage * 48 + s->wifiKeyCursor;
    if ((down & KEY_A) && length < 32 && ch < 127) {
      s->wifiSsid[length++] = ch;
      s->wifiSsid[length] = 0;
    }
    if (down & KEY_START)
      submitSsid(s);
  } else if (s->wifiPhase == 2) {
    static const char digits[] = "0123456789ABCDEF";
    if ((down & KEY_LEFT) && s->wifiKeyCursor % 4)
      s->wifiKeyCursor--;
    if ((down & KEY_RIGHT) && s->wifiKeyCursor % 4 < 3)
      s->wifiKeyCursor++;
    if ((down & KEY_UP) && s->wifiKeyCursor >= 4)
      s->wifiKeyCursor -= 4;
    if ((down & KEY_DOWN) && s->wifiKeyCursor < 12)
      s->wifiKeyCursor += 4;
    if (down & KEY_TOUCH) {
      touchPosition p;
      touchRead(&p);
      if (p.px >= 8 && p.px < 248 && p.py >= 65 && p.py < 161) {
        int col = (p.px - 8) / 60, row = (p.py - 65) / 24;
        if (col < 4 && row < 4) {
          s->wifiKeyCursor = row * 4 + col;
          down |= KEY_A;
        }
      }
    }
    unsigned length = strlen(s->wifiKey);
    if ((down & KEY_X) && length)
      s->wifiKey[--length] = 0;
    if ((down & KEY_A) && length < 32) {
      s->wifiKey[length++] = digits[s->wifiKeyCursor];
      s->wifiKey[length] = 0;
    }
    if (down & KEY_START)
      connect(s);
  }
  return 0;
}
