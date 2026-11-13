/* GPL-3.0-or-later. */
#include "../../shared/console_audio.h"
#include "../../shared/walkerclassic.h"
#include <nds.h>

enum { SAMPLE_RATE = 22050, SAMPLE_LIMIT = 220500 };
/* Pad each buffer to a cache line, and pad odd sample counts to a word. */
static int16_t pcm[2][(SAMPLE_LIMIT + 15) & ~15] __attribute__((aligned(32)));
static unsigned nextBuffer;
static int channel = -1;

bool consoleAudioInit(void) {
  soundEnable();
  return true;
}

void consoleAudioStop(void) {
  if (channel < 0)
    return;
  soundKill(channel);
  channel = -1;
}

void consoleAudioPlay(const uint8_t *eeprom, int request, int volume) {
  if (!volume) {
    consoleAudioStop();
    return;
  }
  if (request <= 0)
    return;
  int16_t *samples = pcm[nextBuffer];
  unsigned count = walkerClassicSound(eeprom, request - 1, samples, SAMPLE_LIMIT);
  consoleAudioStop();
  if (!count)
    return;
  if (count & 1)
    samples[count++] = 0;
  unsigned bytes = count * sizeof(*samples);
  DC_FlushRange(samples, bytes);
  channel = soundPlaySample(samples, SoundFormat_16Bit, bytes, SAMPLE_RATE,
                            volume == 1 ? 32 : 64, 64, false, 0);
  nextBuffer ^= 1;
}

void consoleAudioExit(void) {
  consoleAudioStop();
  soundDisable();
}
