/* Downloaded-client SD adapter installer. GPL-3.0-or-later. */
#ifndef GEN5_ADAPTER_H
#define GEN5_ADAPTER_H
#include <stdint.h>
#include <stdbool.h>
#include "transfer_probe.h"
#include "gen5_adapter_payload.h"
static inline bool gen5Install(uint32_t *length) {
    const Gen5ClientProfile *profile=0;
    // The exact code CRC was checked before generic SDK patching. Recheck
    // the layout and untouched constructor/heap literal before any writes.
    for(uint32_t i=0;i<GEN5_CLIENT_PROFILE_COUNT;i++) {
        const Gen5ClientProfile *p=&gen5ClientProfiles[i];
        if(*length==p->length &&
           *(volatile uint32_t*)p->constructor==p->constructorWord &&
           *(volatile uint32_t*)p->heapPointer==p->originalArena &&
           (!p->heapPointerTwl || *(volatile uint32_t*)p->heapPointerTwl==p->heapTwlOriginal)) {profile=p;break;}
    }
    if(!profile)return false;
    const uint32_t delta=profile->payloadBase-GEN5_PAYLOAD_BASE;
    unsigned char *dest=(unsigned char*)profile->payloadBase;
    for(uint32_t i=0;i<sizeof(gen5Payload);i++)dest[i]=gen5Payload[i];
    for(uint32_t i=0;i<sizeof(gen5Relocations)/sizeof(gen5Relocations[0]);i++)
        *(uint32_t*)(dest+gen5Relocations[i])+=delta;
    const uint32_t arena=profile->payloadBase+sizeof(gen5Payload);
    *(volatile uint32_t*)profile->heapPointer=arena;
    // Enhanced clients select a separate main-arena lower bound in TWL mode.
    // Reserve the adapter on both branches before the native heap initializes.
    if(profile->heapPointerTwl)*(volatile uint32_t*)profile->heapPointerTwl=arena;
    for(uint32_t i=0;i<15;i++) {
        volatile uint32_t *p=(volatile uint32_t*)profile->hooks[i].address;
        p[0]=0xe51ff004;p[1]=profile->payloadBase+profile->hooks[i].offset;
    }
    // Preserve Western trainer metadata, without rewriting source data.
    volatile uint32_t *language=(volatile uint32_t*)profile->language;
    language[0]=0xeb000000|(((profile->payloadBase+GEN5_LANGUAGE_OFFSET-profile->language-8)>>2)&0xffffff);
    language[1]=0xe5c70006;language[2]=0xe1a00000;
    *length=arena-profile->base;
    return true;
}
#endif
