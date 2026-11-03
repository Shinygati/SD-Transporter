"""Emit a compact, lossless regional hook table from verified metadata."""
import struct

def emit(profiles, hooks, syms, base):
    rows=[]
    languages={'E':2,'F':3,'D':5,'I':4,'S':7,'J':1,'K':8}
    for p in profiles:
        a=p['addresses']
        row=[p[x] for x in ('base','entry','length','module','bss_start','payload_base','code_crc','sdk')]
        row += [a['constructor'],p['constructor_word'],a['heap_pointer'],a['language'],a.get('heap_pointer_twl',0),p.get('heap_twl_original',0),p['original_arena'],a['classify'],a['classify_end'],a['language2'],p['language_store_word'],p['language2_store_word'],languages[p['region']]]+p['classify_words']
        for name,fn in hooks.items():row += [a[name],syms[fn]-base]
        rows.append(row)
    count=len(rows[0]);assert all(len(r)==count for r in rows)
    data=bytearray();offsets=[]
    def varint(value):
        signed=value if value<0x80000000 else value-0x100000000
        v=(signed<<1)^(signed>>31)
        while v>=128:data.append((v&127)|128);v>>=7
        data.append(v)
    for p,row in zip(profiles[4:],rows[4:]):
        family=p['key'].rsplit('_',1)[0]
        idx=next(i for i,q in enumerate(profiles[:4]) if q['key']==family)
        diff=[(x-y)&0xffffffff for x,y in zip(row,rows[idx])]
        dictionary=list(dict.fromkeys(diff));assert dictionary[0]==0 and len(dictionary)<=16
        offsets.append(len(data));data += bytes([idx,len(dictionary)])
        for d in dictionary[1:]:varint(d)
        indices=[dictionary.index(x) for x in diff]
        for i in range(0,count,2):data.append(indices[i]|((indices[i+1] if i+1<count else 0)<<4))
    h='/* Generated lossless regional client hook metadata. */\n#ifndef GEN5_CLIENT_PROFILES_H\n#define GEN5_CLIENT_PROFILES_H\n#include <stdint.h>\n#include <stdbool.h>\n'
    h+='typedef struct { uint32_t address, offset; } Gen5Hook;\n'
    h+='typedef struct { uint32_t base, entry, length, module, bssStart, payloadBase, codeCRC, sdk, constructor, constructorWord, heapPointer, language, heapPointerTwl, heapTwlOriginal, originalArena, classify, classifyEnd, language2, languageStore, language2Store, nativeLanguage, classifyWords[5]; Gen5Hook hooks[15]; } Gen5ClientProfile;\n'
    h+='static const uint32_t gen5ProfileTemplates[4][%d]={\n'%count
    h+='\n'.join('{'+','.join('0x%xu'%v for v in row)+'},' for row in rows[:4])+'\n};\n'
    h+='static const uint16_t gen5ProfileOffsets[]={'+','.join(str(x) for x in offsets)+'};\n'
    h+='static const uint8_t gen5ProfileData[]={'+','.join(str(x) for x in data)+'};\n'
    h+='#define GEN5_CLIENT_PROFILE_COUNT %du\n'%len(rows)
    h+='''/* Decode one row on demand instead of storing every layout in ARM7 RAM. */
static __attribute__((noinline,optimize("Os"))) bool gen5ReadProfile(uint32_t index, Gen5ClientProfile *out) {
    if(index>=GEN5_CLIENT_PROFILE_COUNT)return false;
    const uint8_t *p=0,*end=gen5ProfileData+sizeof(gen5ProfileData);
    uint32_t which=index,n=0,deltas[16];
    deltas[0]=0;
    if(index>=4) {
        p=gen5ProfileData+gen5ProfileOffsets[index-4];
        if(p+2>end)return false;
        which=*p++;n=*p++;
        if(which>=4 || !n || n>16)return false;
        for(uint32_t k=1;k<n;k++) {
            uint32_t v=0,shift=0,byte;
            do {
                if(p==end || shift>28)return false;
                byte=*p++;v|=(byte&127)<<shift;shift+=7;
            } while(byte&128);
            deltas[k]=(v>>1)-(v&1)*(v|1);
        }
    }
    volatile uint32_t *words=(volatile uint32_t*)out;
    for(uint32_t k=0;k<sizeof(*out)/4;k++)words[k]=gen5ProfileTemplates[which][k];
    if(p) {
        if(p+(sizeof(*out)/4+1)/2>end)return false;
        for(uint32_t k=0;k<sizeof(*out)/4;k++) {
            uint32_t d=(p[k/2]>>((k&1)*4))&15;
            if(d>=n)return false;
            words[k]+=deltas[d];
        }
    }
    return true;
}
#endif
'''
    # Zigzag's odd case is -(v+1)/2; decoder expression above has that value.
    print('Hook table:',4*count*4+len(data)+len(offsets)*2,'bytes vs',len(rows)*count*4,'raw')
    return h
