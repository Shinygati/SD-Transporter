#ifndef WALKER_SYNCNET_H
#define WALKER_SYNCNET_H
#define WALKER_SYNC_PORT 30820
typedef int (*SyncBeforeCheckout)(void);
typedef void (*SyncEvent)(int); /* 1=checked out, 2=returned */
int syncStart(const char *, SyncBeforeCheckout, SyncEvent);
void syncPoll(void);
void syncStop(void);
int syncReady(void);
const char *syncAddress(void);
const char *syncStatus(void);
#endif
