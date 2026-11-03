/* DS-mode storage services; all code/data lives in the reserved walker area. */
#ifndef WALKER_DS_CONTEXT_H
#define WALKER_DS_CONTEXT_H
#include "walker_emu.h"
#define WALKER_DS_CONTEXT_OFF 0x800u
#define WALKER_DS_BLOB_OFF 0x12800u
#define WALKER_DS_BLOB_MAX (WALKER_RESERVE-WALKER_DS_BLOB_OFF)
typedef struct WalkerDsContext {
    unsigned protocolEntry;
    unsigned initialize, read, write, open, owner, reset;
    unsigned file[6];
    unsigned opened, lastError;
} WalkerDsContext;
WALKER_ASSERT(ds_control_space, sizeof(WalkerControl) <= WALKER_DS_CONTEXT_OFF);
WALKER_ASSERT(ds_context_space, sizeof(WalkerDsContext)+WALKER_DS_CONTEXT_OFF <= 0x1000);
#endif
