/* Independent Gen4 SD ROM/save service. GPL-3.0-or-later. */
#include <nds/ndstypes.h>
#include <string.h>
#include "my_fat.h"
#include "gen5_file.h"
#ifndef TWLSDK
extern u32 gen5RomCluster,gen5SaveCluster,gen5RomSize;
extern bool gen5CheckedIO(void *buffer,aFile *file,u32 off,u32 length,bool writing);
/* Virtual Slot-1 backend: ROM bytes and save bytes are served from two SD
 * files instead of a real Slot-1 cartridge. The existing Download Play hooks
 * and timing remain unchanged in this first experimental phase.
 * Source-game policy is in gen5Prepare(): English Diamond, Pearl, Platinum,
 * HeartGold and SoulSilver, all with 512 KiB saves. */
#define GEN5_VC_ROM_READ   0x50545244u
#define GEN5_VC_SAVE_READ  0x50545352u
#define GEN5_VC_SAVE_WRITE 0x50545357u
#define GEN5_VC_SAVE_BYTES 0x80000u
#define GEN5_VC_MAX_PACKET 512u

typedef struct {
    aFile rom;
    aFile save;
    u32 romCluster;
    u32 saveCluster;
    u32 romSize;
    bool opened;
} Gen5VirtualSlot1;

static Gen5VirtualSlot1 sGen5VirtualSlot1;
bool gen5Active=false;

/* Only the virtual cart backend touches FAT files. Return status through the
 * existing shared IPC request, retaining the original interrupt ownership. */
static bool gen5VirtualSlot1Dispatch(u32 command, u32 offset, void *buffer, u32 length) {
    Gen5VirtualSlot1 *cart=&sGen5VirtualSlot1;
    if(!gen5Active || gen5RomCluster<2 || gen5SaveCluster<2 ||
       gen5RomCluster==gen5SaveCluster || !gen5RomSize) return false;

    /* Configuration should be static during a transfer. Fail closed if an
     * unexpected change occurs after the FAT handles have been opened. */
    if(cart->opened && (cart->romCluster!=gen5RomCluster ||
       cart->saveCluster!=gen5SaveCluster || cart->romSize!=gen5RomSize))
        return false;

    const bool rom=(command==GEN5_VC_ROM_READ);
    const u32 limit=rom?gen5RomSize:GEN5_VC_SAVE_BYTES;
    if(length==0 || length>GEN5_VC_MAX_PACKET || offset>=limit ||
       length>limit-offset) return false;

    if(!cart->opened) {
        getFileFromCluster(&cart->rom,gen5RomCluster,false);
        getFileFromCluster(&cart->save,gen5SaveCluster,false);
        cart->romCluster=gen5RomCluster;
        cart->saveCluster=gen5SaveCluster;
        cart->romSize=gen5RomSize;
        cart->opened=true;
    }

    aFile *file=rom?&cart->rom:&cart->save;
    return gen5CheckedIO(buffer,file,offset,length,command==GEN5_VC_SAVE_WRITE);
}

bool gen5FileRequest(volatile u32 *r) {
    const u32 command=r[3];
    if(command!=GEN5_VC_ROM_READ && command!=GEN5_VC_SAVE_READ &&
       command!=GEN5_VC_SAVE_WRITE) return false;

    const u32 dst=r[0], length=r[1], off=r[2];
    bool ok=dst>=0x02000000u && dst<0x02400000u &&
       length>0 && length<=GEN5_VC_MAX_PACKET &&
       length<=0x02400000u-dst;
    if(ok) ok=gen5VirtualSlot1Dispatch(command,off,(void*)dst,length);
    r[4]=ok?1:0;
    r[3]=0;
    return true;
}
#endif
