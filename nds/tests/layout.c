#include <stddef.h>
#include "ui.h"
#include <wfc.h>
const unsigned layout[]={sizeof(UIState),offsetof(UIState,screen),offsetof(UIState,wifiPhase),offsetof(UIState,wifiCount),offsetof(UIState,wifiCursor),offsetof(UIState,wifiNames),offsetof(UIState,wifiSecurity),offsetof(UIState,wifiKey),offsetof(UIState,wifiKeyCursor),sizeof(WlanBssDesc),offsetof(WlanBssDesc,ssid_len),offsetof(WlanBssDesc,ssid),offsetof(WlanBssDesc,auth_mask),offsetof(UIState,frame),offsetof(UIState,selection),offsetof(UIState,wifiSsid),offsetof(UIState,wifiTextPage),offsetof(WlanBssScanFilter,target_ssid_len),offsetof(WlanBssScanFilter,target_ssid)};

const unsigned handoffOffsets[]={offsetof(UIState,phoneLocked),offsetof(UIState,count),offsetof(UIState,cursor),UI_WIFI,UI_SYNC,UI_LOCKED,UI_HOME,UI_LIST};
const unsigned saveOffsets[]={offsetof(UIState,saving),offsetof(UIState,saveError),offsetof(UIState,loaded),offsetof(UIState,screen),UI_HOME};
