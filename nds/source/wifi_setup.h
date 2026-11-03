/* DS Lite local Wi-Fi configuration. GPL-3.0-or-later. */
#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include "ui.h"
int ndsWifiAvailable(void);
int ndsWifiConnectSaved(void);
void ndsWifiSetupBegin(UIState *);
void ndsWifiSetupPoll(UIState *);
int ndsWifiSetupInput(UIState *, unsigned down);
#endif
