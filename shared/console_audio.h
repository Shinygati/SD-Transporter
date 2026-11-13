/* Native playback of uploaded Pokéwalker scores. GPL-3.0-or-later. */
#ifndef CONSOLE_AUDIO_H
#define CONSOLE_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

bool consoleAudioInit(void);
void consoleAudioPlay(const uint8_t *eeprom, int request, int volume);
void consoleAudioStop(void);
void consoleAudioExit(void);

#endif
