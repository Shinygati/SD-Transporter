/* GPL-3.0-or-later. */
#include "../../shared/console_audio.h"
#include "../../shared/walkerclassic.h"
#include <3ds.h>

enum { SAMPLE_RATE = 22050, SAMPLE_LIMIT = 220500 };
static enum { SILENT, NDSP, CSND } backend;
static int16_t *pcm[2];
static unsigned nextBuffer;
static ndspWaveBuf waves[2];
static int csndChannel;
static bool csndPlaying;

bool consoleAudioInit(void) {
    pcm[0] = linearAlloc(SAMPLE_LIMIT * sizeof(**pcm));
    pcm[1] = linearAlloc(SAMPLE_LIMIT * sizeof(**pcm));
    if (pcm[0] && pcm[1]) {
        if (R_SUCCEEDED(ndspInit())) {
            backend = NDSP;
            ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
            ndspChnSetRate(0, SAMPLE_RATE);
            ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
            return true;
        }
        /* Installed CIA builds can use CSND when no DSP component is present. */
        if (R_SUCCEEDED(csndInit())) {
            if (csndChannels) {
                backend = CSND;
                csndChannel = __builtin_ctz(csndChannels);
                return true;
            }
            csndExit();
        }
    }
    consoleAudioExit();
    return false;
}

void consoleAudioStop(void) {
    if (backend == NDSP) {
        ndspChnWaveBufClear(0);
    } else if (backend == CSND && csndPlaying) {
        CSND_SetPlayState(csndChannel, 0);
        csndExecCmds(true);
        csndPlaying = false;
    }
}

void consoleAudioPlay(const uint8_t *eeprom, int request, int volume) {
    if (!volume) {
        consoleAudioStop();
        return;
    }
    if (backend == SILENT || request <= 0)
        return;

    unsigned index = nextBuffer;
    unsigned count = walkerClassicSound(eeprom, request - 1, pcm[index], SAMPLE_LIMIT);
    consoleAudioStop();
    if (!count)
        return;

    if (backend == CSND && (count & 1))
        pcm[index][count++] = 0;
    unsigned bytes = count * sizeof(**pcm);
    float gain = volume == 1 ? .25f : .5f;
    nextBuffer ^= 1;
    if (backend == NDSP) {
        float mix[12] = {gain, gain};
        ndspChnSetMix(0, mix);
        DSP_FlushDataCache(pcm[index], bytes);
        waves[index] = (ndspWaveBuf){.data_pcm16 = pcm[index], .nsamples = count};
        ndspChnWaveBufAdd(0, &waves[index]);
    } else {
        GSPGPU_FlushDataCache(pcm[index], bytes);
        csndPlaying = csndPlaySound(csndChannel, SOUND_FORMAT_16BIT | SOUND_ONE_SHOT,
                                    SAMPLE_RATE, gain, 0, pcm[index], pcm[index], bytes) == 0;
    }
}

void consoleAudioExit(void) {
    consoleAudioStop();
    if (backend == NDSP)
        ndspExit();
    else if (backend == CSND)
        csndExit();
    backend = SILENT;
    for (unsigned i = 0; i < 2; i++) {
        if (pcm[i])
            linearFree(pcm[i]);
        pcm[i] = NULL;
    }
}
