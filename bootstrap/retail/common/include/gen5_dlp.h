/* Exact HNDA autoload-source hook for the real received-client handoff.
 * Local experiment, GPL-3.0-or-later. No Nintendo executable included. */
#ifndef GEN5_DLP_H
#define GEN5_DLP_H
#include <stdint.h>
#include <stdbool.h>
#include "gen5_dlp_arm7_payload.h"

static inline bool gen5PatchDlpArm7(void *arm7, uint32_t size, uint32_t resetTarget) {
    if (size != 0x23634 || !resetTarget)
        return false;

    uint32_t *m = (uint32_t*)((unsigned char*)arm7 + 0x234);
    uint32_t *p = (uint32_t*)((unsigned char*)arm7 + 0x468);

    if (m[0] != 0x022e3604 || m[1] != 0x022e3634 ||
        m[2] != 0x022c024c)
        return false;

    bool original =
        p[0] == 0xe59f1074 &&
        p[1] == 0xe3a00c02 &&
        p[2] == 0xe1c100b0;

    bool prepatched = true;
    for (uint32_t i = 0; i < GEN5_ARM7_RESET_INDEX; i++) {
        if (p[i] != gen5DlpArm7BridgeCode[i]) {
            prepatched = false;
            break;
        }
    }

    if (!original && !prepatched)
        return false;

    for (uint32_t i = 0;
         i < sizeof(gen5DlpArm7BridgeCode) / sizeof(gen5DlpArm7BridgeCode[0]);
         i++)
        p[i] = gen5DlpArm7BridgeCode[i];

    p[GEN5_ARM7_RESET_INDEX] = resetTarget;
    return true;
}

static inline bool gen5PatchDlp(void *arm9, uint32_t size, const void *resetPatch) {
    if (size != 0x3fc44 || !resetPatch)
        return false;

    uint32_t *m = (uint32_t*)((unsigned char*)arm9 + 0xb64);
    uint32_t *a = (uint32_t*)((unsigned char*)arm9 + 0xda88);

    if (m[0] != 0x02040020 || m[1] != 0x02040044 ||
        m[2] != 0x02000fa0 ||
        m[6] != 0x05004e85 ||
        m[7] != 0xdec00621 ||
        m[8] != 0x2106c0de)
        return false;

    bool original =
        a[0] == 0xe3a00000 &&
        a[1] == 0xee070f16 &&
        a[2] == 0xe59f107c;

    bool prepatched =
        a[0] == 0xe59f0000 &&
        a[1] == 0xe59ff000 &&
        a[2] == 0x50544c44;

    if (!original && !prepatched)
        return false;

    uint32_t target = ((const uint32_t*)resetPatch)[1];
    if (!target)
        return false;

    a[0] = 0xe59f0000;
    a[1] = 0xe59ff000;
    a[2] = 0x50544c44;
    a[3] = target;

    return true;
}

#endif
