/* NDS adapter for the shared console application. GPL-3.0-or-later. */
#ifndef NDS_3DS_COMPAT_H
#define NDS_3DS_COMPAT_H
#include <nds.h>
#include <stdint.h>
#include <sys/time.h>
void gfxInitDefault(void);
void gfxExit(void);
int ndsStorageReady(void);
static inline int aptMainLoop(void){return pmMainLoop();}
static inline uint64_t osGetTime(void){struct timeval t;gettimeofday(&t,0);return (uint64_t)t.tv_sec*1000+t.tv_usec/1000;}
static inline void hidScanInput(void){scanKeys();}
static inline uint32_t hidKeysDown(void){return keysDown();}
static inline uint32_t hidKeysDownRepeat(void){return keysDownRepeat();}
static inline void hidSetRepeatParameters(unsigned delay,unsigned interval){keysSetRepeat(delay,interval);}
static inline void hidTouchRead(touchPosition *p){touchRead(p);p->px=p->px*320/256;p->py=p->py*240/192;}
#endif
