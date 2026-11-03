/* Downloaded-client SD adapter installer. GPL-3.0-or-later. */
#ifndef GEN5_ADAPTER_H
#define GEN5_ADAPTER_H
#include <stdint.h>
#include <stdbool.h>
#include "transfer_probe.h"
#include "gen5_adapter_payload.h"
#include "gen5_client_profiles.h"
static inline bool gen5ClassifyLayoutValid(const Gen5ClientProfile *p) {
    if(p->classify<p->constructor || p->classifyEnd<=p->classify+16 ||
       p->classifyEnd>=p->constructor+0x300)return false;
    for(uint32_t i=0;i<4;i++)
        if(((volatile uint32_t*)p->classify)[i]!=p->classifyWords[i])return false;
    const uint32_t headerReg=(p->classifyWords[0]>>16)&15;
    const uint32_t objectReg=(p->classifyWords[4]>>16)&15;
    return headerReg>=4 && headerReg<=11 && objectReg>=4 && objectReg<=11 &&
           (p->classifyWords[0]&0xfff00fff)==0xe590000c &&
           (p->classifyWords[4]&0xfff0ffff)==0xe5900228 &&
           *(volatile uint32_t*)p->classifyEnd==p->classifyWords[4];
}
static inline bool gen5LanguageLayoutValid(const Gen5ClientProfile *p) {
    const uint32_t addresses[2]={p->language,p->language2};
    const uint32_t stores[2]={p->languageStore,p->language2Store};
    for(uint32_t i=0;i<2;i++) {
        const volatile uint32_t *code=(const volatile uint32_t*)addresses[i];
        if((stores[i]&0xfff0ffff)!=0xe5c00006 ||
           code[0]!=(0xe3500000|p->nativeLanguage) || code[1]!=0x13a00003 ||
           code[2]!=(0x10000000|(stores[i]&0x0fffffff)))return false;
    }
    return true;
}
/* This installer runs once. Keep its code compact to preserve loader stack room. */
static __attribute__((noinline,optimize("Os"))) bool gen5Install(uint32_t *length) {
    Gen5ClientProfile candidate;
    const Gen5ClientProfile *profile=0;
    // The exact code CRC was checked before generic SDK patching. Recheck
    // the layout and untouched constructor/heap literal before any writes.
    for(uint32_t i=0;i<GEN5_CLIENT_PROFILE_COUNT;i++) {
        if(!gen5ReadProfile(i,&candidate))return false;
        const Gen5ClientProfile *p=&candidate;
        if(*length==p->length &&
           *(volatile uint32_t*)p->constructor==p->constructorWord && gen5ClassifyLayoutValid(p) && gen5LanguageLayoutValid(p) &&
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
    // Replace the region-specific family comparison, keeping the actual
    // cartridge header and native save readers intact. Japanese BW uses a
    // different register allocation, taken from each verified instruction pair.
    volatile uint32_t *classify=(volatile uint32_t*)profile->classify;
    classify[0]=0xe1a00000|((profile->classifyWords[0]>>16)&15);
    classify[1]=0xeb000000|(((profile->payloadBase+GEN5_CLASSIFY_OFFSET-profile->classify-12)>>2)&0xffffff);
    classify[2]=0xe5800228|(profile->classifyWords[4]&0x000f0000);
    classify[3]=0xea000000|(((profile->classifyEnd-profile->classify-20)>>2)&0xffffff);
    // Preserve trainer metadata, without rewriting source data.
    const uint32_t addresses[2]={profile->language,profile->language2};
    const uint32_t stores[2]={profile->languageStore,profile->language2Store};
    for(uint32_t i=0;i<2;i++) {
        volatile uint32_t *language=(volatile uint32_t*)addresses[i];
        language[0]=0xeb000000|(((profile->payloadBase+GEN5_LANGUAGE_OFFSET-addresses[i]-8)>>2)&0xffffff);
        language[1]=stores[i];language[2]=0xe1a00000;
    }
    *length=arena-profile->base;
    return true;
}
#endif
