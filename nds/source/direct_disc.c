/* Direct, aligned DS storage access. GPL-3.0-or-later.
 * libdvm's zero-cache path bypasses its alignment adapter as well as caching.
 * FatFs can pass unaligned or DTCM buffers; Calico's ARM9 block API cannot.
 * Bounce those transfers through main RAM without retaining dirty sectors. */
#include <calico.h>
#include <dvm.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    DvmDisc disc;
    DvmDisc *inner;
    Mutex mutex;
    unsigned char *bounce;
} DirectDisc;

static int aligned(const void *buffer, unsigned alignment) {
    uintptr_t p=(uintptr_t)buffer;
    return !(p&(alignment-1)) && p>=MM_MAINRAM && p<MM_DTCM;
}
static void destroy(DvmDisc *disc) {
    DirectDisc *d=(DirectDisc *)disc;
    d->inner->vt->destroy(d->inner);
    free(d->bounce);free(d);
}
static bool readSectors(DvmDisc *disc,void *buffer,sec_t first,sec_t count) {
    DirectDisc *d=(DirectDisc *)disc;bool ok=true;
    mutexLock(&d->mutex);
    if(aligned(buffer,ARM_CACHE_LINE_SZ))ok=d->inner->vt->read_sectors(d->inner,buffer,first,count);
    else {
        unsigned char *out=buffer;
        while(count && ok) {
            sec_t n=count>8?8:count;
            ok=d->inner->vt->read_sectors(d->inner,d->bounce,first,n);
            if(ok)memcpy(out,d->bounce,(size_t)n<<9);
            out+=(size_t)n<<9;first+=n;count-=n;
        }
    }
    mutexUnlock(&d->mutex);return ok;
}
static bool writeSectors(DvmDisc *disc,const void *buffer,sec_t first,sec_t count) {
    DirectDisc *d=(DirectDisc *)disc;bool ok=true;
    mutexLock(&d->mutex);
    if(aligned(buffer,4))ok=d->inner->vt->write_sectors(d->inner,buffer,first,count);
    else {
        const unsigned char *in=buffer;
        while(count && ok) {
            sec_t n=count>8?8:count;
            memcpy(d->bounce,in,(size_t)n<<9);
            ok=d->inner->vt->write_sectors(d->inner,d->bounce,first,n);
            in+=(size_t)n<<9;first+=n;count-=n;
        }
    }
    mutexUnlock(&d->mutex);return ok;
}
static void flush(DvmDisc *disc) {
    DirectDisc *d=(DirectDisc *)disc;
    mutexLock(&d->mutex);d->inner->vt->flush(d->inner);mutexUnlock(&d->mutex);
}
static const DvmDiscIface iface={destroy,readSectors,writeSectors,flush};
extern DvmDisc *__real_dvmDiscCacheCreate(DvmDisc *,unsigned,unsigned);
DvmDisc *__wrap_dvmDiscCacheCreate(DvmDisc *inner,unsigned pages,unsigned sectors) {
    if(pages)return __real_dvmDiscCacheCreate(inner,pages,sectors);
    if(!inner || inner->sector_shift!=9)return NULL;
    DirectDisc *d=calloc(1,sizeof(*d));if(!d)return NULL;
    d->bounce=memalign(ARM_CACHE_LINE_SZ,4096);
    if(!d->bounce){free(d);return NULL;}
    d->disc=*inner;d->disc.vt=&iface;d->disc.num_users=0;d->inner=inner;
    return &d->disc;
}
