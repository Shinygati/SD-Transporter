/* ARM9/DLDI runtime for the DS Lite walker. GPL-3.0-or-later. */
#include "walker_ds_context.h"
#include "walker_ds_io.h"
typedef unsigned u32;
typedef unsigned short u16;
static WalkerDsContext *context(WalkerControl *c) { return (WalkerDsContext *)((unsigned char *)c + WALKER_DS_CONTEXT_OFF); }
static unsigned readFile(void *arg, unsigned offset, void *data, unsigned size) {
    WalkerDsContext *ds = (WalkerDsContext *)arg;
    ((void(*)(void *))ds->reset)(ds->file);
    return ((u32(*)(void *,void *,u32,u32))ds->read)(data,ds->file,offset,size);
}
static unsigned writeFile(void *arg, unsigned offset, const void *data, unsigned size) {
    WalkerDsContext *ds = (WalkerDsContext *)arg;
    return ((u32(*)(const void *,void *,u32,u32))ds->write)(data,ds->file,offset,size);
}
static int flush(WalkerControl *c) {
    if (c->writeSeq == c->flushedSeq) return 1;
    WalkerDsContext *ds = context(c);
    volatile u32 *ime = (volatile u32 *)0x04000208;
    volatile u16 *exmem = (volatile u16 *)0x04000204;
    u32 oldIme = *ime; u16 oldOwner = *exmem; *ime = 0;
    ((void(*)(void))ds->owner)();
    ((void(*)(void))ds->initialize)();
    if (!ds->opened) { ((void(*)(void *,u32))ds->open)(ds->file,c->cluster); ds->opened=1; }
    int ok = walkerDsCommit(c,(const unsigned char *)c->eeprom,ds,readFile,writeFile);
    ds->lastError = !ok;
    *exmem=oldOwner; *ime=oldIme;
    return ok;
}
__attribute__((section(".text.entry")))
u32 walkerDsDispatch(WalkerControl *c, u32 op, u32 a, u32 b) {
    if (!c || c->magic != WALKER_MAGIC || c->version != WALKER_VERSION) return 0;
    WalkerDsContext *ds=context(c);
    u32 result=((u32(*)(WalkerControl *,u32,u32,u32))ds->protocolEntry)(c,op,a,b);
    if (!c->state && !flush(c)) { c->respLen=0; c->armed=0; return op==WALKER_OP_READ ? 0 : result; }
    return result;
}
