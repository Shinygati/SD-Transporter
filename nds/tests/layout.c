#include <stddef.h>
#include "ui.h"
#include <wfc.h>
const unsigned layout[]={sizeof(UIState),offsetof(UIState,screen),offsetof(UIState,wifiPhase),offsetof(UIState,wifiCount),offsetof(UIState,wifiCursor),offsetof(UIState,wifiNames),offsetof(UIState,wifiSecurity),offsetof(UIState,wifiKey),offsetof(UIState,wifiKeyCursor),sizeof(WlanBssDesc),offsetof(WlanBssDesc,ssid_len),offsetof(WlanBssDesc,ssid),offsetof(WlanBssDesc,auth_mask),offsetof(UIState,frame),offsetof(UIState,selection),offsetof(UIState,wifiSsid),offsetof(UIState,wifiTextPage),offsetof(WlanBssScanFilter,target_ssid_len),offsetof(WlanBssScanFilter,target_ssid)};
