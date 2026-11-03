/* Verified ARM9/DLDI write-back for the DS-mode virtual walker. GPL-3.0-or-later. */
#ifndef WALKER_DS_IO_H
#define WALKER_DS_IO_H
#include "walker_emu.h"
typedef unsigned (*WalkerDsRead)(void *, unsigned, void *, unsigned);
typedef unsigned (*WalkerDsWrite)(void *, unsigned, const void *, unsigned);
static int walkerDsCommit(WalkerControl *c, const unsigned char *image, void *file,
                          WalkerDsRead read, WalkerDsWrite write) {
    if (!c || c->magic != WALKER_MAGIC || c->version != WALKER_VERSION || c->cluster < 2) return 0;
    if (c->writeSeq == c->flushedSeq) return 1;
    const unsigned seq = c->writeSeq;
    for (unsigned sector = 0; sector < WALKER_SECTORS; sector++) {
        if (!c->dirty[sector]) continue;
        unsigned base = sector * WALKER_SECTOR;
        if (write(file, base, image + base, WALKER_SECTOR) != WALKER_SECTOR) return 0;
        /* DLDI drivers may report success before media failure becomes visible.
         * Verify the stored bytes before letting HGSS receive completion. */
        for (unsigned offset = 0; offset < WALKER_SECTOR; offset += 512) {
            if (read(file, base + offset, c->scratch, 512) != 512) return 0;
            for (unsigned i = 0; i < 512; i++)
                if (c->scratch[i] != image[base + offset + i]) return 0;
        }
        c->dirty[sector] = 0;
    }
    c->flushedSeq = seq;
    c->flushes++;
    return 1;
}
#endif
