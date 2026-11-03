/* Local English B2/W2 Download Play client adapter. Original code, MIT license.
 * Native transfer logic and native asynchronous backup completion are retained.
 */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
#define V32(a) (*(volatile u32*)(a))
#define V16(a) (*(volatile u16*)(a))
#define SHARED ((volatile u32*)0x02fffa0c)
#define ROM_READ 0x50545244u
#define SAVE_READ 0x50545352u
#define SAVE_WRITE 0x50545357u
/* Save traffic is paced: between two save requests the ARM9 runs with its own
 * interrupts enabled for roughly a millisecond, so the ARM7 (which performs the
 * SD work inside its IPC interrupt) gets time to service the wireless hardware
 * instead of being re-entered back to back for the whole duration of loading or saving. */
#define SAVE_PACE_LOOPS 8000u
static u8 bounce[512] __attribute__((aligned(32)));
static u8 header[512] __attribute__((aligned(32)));
static u32 headerReady;
static u32 completed; /* requests the ARM7 answered */
static void timedOut(u32 command) __attribute__((noreturn));
static void cache(u32 address,u32 length,int writing) {
    u32 zero=0,end=address+length;address&=~31u;
    while(address<end) {
        if(writing)__asm__ volatile("mcr p15,0,%0,c7,c10,1"::"r"(address):"memory");
        else __asm__ volatile("mcr p15,0,%0,c7,c6,1"::"r"(address):"memory");
        address+=32;
    }
    __asm__ volatile("mcr p15,0,%0,c7,c10,4"::"r"(zero):"memory");
}
static int request(u32 command,u32 off,void *buffer,u32 length) {
    u32 ime=V16(0x04000208);V16(0x04000208)=0;
    // No second requester can reuse the shared bounce buffer while IRQs are masked.
    if(SHARED[3]){V16(0x04000208)=ime;return 0;}
    if(command==SAVE_WRITE)for(u32 i=0;i<length;i++)bounce[i]=((const u8*)buffer)[i];
    cache((u32)bounce,512,1);cache((u32)bounce,512,0);
    SHARED[0]=(u32)bounce;SHARED[1]=length;SHARED[2]=off;SHARED[4]=0;SHARED[3]=command;
    V16(0x04000180)=0x2400; // IPC sync 4, request ARM7 interrupt.
    u32 timeout=60000000;
    while(SHARED[3] && --timeout)__asm__ volatile("nop");
    if(!timeout)timedOut(command);
    completed++;
    int ok=SHARED[4]==1;
    if(ok && command!=SAVE_WRITE) {
        cache((u32)bounce,512,0);
        for(u32 i=0;i<length;i++)((u8*)buffer)[i]=bounce[i];
    }
    V16(0x04000208)=ime;return ok;
}
static void pace(void) {
    for(volatile u32 i=0;i<SAVE_PACE_LOOPS;i++);
}
static int transfer(u32 command,u32 off,void *data,u32 length,int paced) {
    if(command!=ROM_READ && (off>0x80000 || length>0x80000-off))return 0;
    while(length) {
        u32 n=length>512?512:length;
        // Save requests never straddle a 512-byte sector: the ARM7 then writes whole
        // sectors directly instead of read-modify-writing two partial ones.
        if(command!=ROM_READ) {u32 room=512-(off&511);if(n>room)n=room;}
        if(!request(command,off,data,n))return 0;
        off+=n;data=(u8*)data+n;length-=n;
        if(paced)pace();
    }
    return 1;
}
/* Diagnostic screen for a request the ARM7 never answered (it would otherwise
 * surface as a native "save data could not be accessed" after ~15 seconds).
 * Eight hex lines, top to bottom:
 *   0 command  1 requests answered  2 ARM7 served from IPC IRQ
 *   3 ARM7 served from halt hook  4 ARM7 VBlank ticks
 *   5 ARM7 IE (+80000000 if its IPC sync IRQ is enabled)  6 pending  7 status */
static const u8 hexFont[16][7]={
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{31,2,4,2,1,17,14},
 {2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},
 {14,17,17,14,17,17,14},{14,17,17,15,1,2,12},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16}};
static void hexLine(u32 row,u32 value) {
    volatile u16 *v=(volatile u16*)0x06800000;
    for(u32 d=0;d<9;d++) {
        u32 digit=d==0?row:(value>>(4*(8-d)))&15,x=8+d*24+(d?8:0),y=8+row*22;
        for(u32 r=0;r<7;r++)for(u32 c=0;c<5;c++)
            if(hexFont[digit][r]&(16u>>c))
                for(u32 k=0;k<4;k++)v[(y+r*2+(k>>1))*256+x+c*2+(k&1)]=d?0x7fff:0x03e0;
    }
}
static void timedOut(u32 command) {
    V16(0x04000208)=0;V16(0x04000304)=0x8203;V16(0x0400006c)=0;
    *(volatile u8*)0x04000240=0x80;V32(0x04000000)=0x20000;
    volatile u16 *v=(volatile u16*)0x06800000;
    for(u32 i=0;i<256*192;i++)v[i]=0x2800;
    const u32 values[8]={command,completed,SHARED[9],SHARED[10],SHARED[11],SHARED[12],SHARED[3],SHARED[4]};
    for(u32 row=0;row<8;row++)hexLine(row,values[row]);
    for(;;)__asm__ volatile("nop");
}
static void failed(void) __attribute__((noreturn));
static void failed(void) {
    V16(0x04000208)=0;V16(0x04000304)=0x8203;V16(0x0400006c)=0;
    *(volatile u8*)0x04000240=0x80;V32(0x04000000)=0x20000;
    volatile u16 *v=(volatile u16*)0x06800000;
    for(u32 i=0;i<256*192;i++)v[i]=0x001f;
    for(;;)__asm__ volatile("nop");
}
/* Western Gen 4 sources accepted by gen5Prepare(): Diamond, Pearl, Platinum,
 * HeartGold, SoulSilver in English (E/P), French, German, Italian, Spanish.
 * The native Black 2 client identifies the game itself from this header. */
static int supportedGameCode(u32 code) {
    u32 game=code&0x00ffffffu,region=code>>24;
    int knownGame=game==0x414441u || /* ADA Diamond    */
                  game==0x415041u || /* APA Pearl      */
                  game==0x555043u || /* CPU Platinum   */
                  game==0x4b5049u || /* IPK HeartGold  */
                  game==0x475049u;   /* IPG SoulSilver */
    int western=region=='E' || region=='P' || region=='F' ||
                region=='D' || region=='I' || region=='S';
    return knownGame && western;
}
void *gen5_header(void) {
    if(!headerReady) {
        if(!transfer(ROM_READ,0,header,512,0) || !supportedGameCode(V32((u32)header+12)))failed();
        headerReady=1;
    }
    return header;
}
u32 gen5_rom_raw(u32 dma,void *dst,u32 off,u32 len) {
    (void)dma;if(!transfer(ROM_READ,off,dst,len,0))failed();return len;
}
void gen5_rom_read(u32 dma,u32 off,void *dst,u32 len,void (*callback)(void*),void *arg,int async) {
    (void)async;gen5_rom_raw(dma,dst,off,len);if(callback)callback(arg);
}
u32 gen5_id(void) {return 0x00001fc2;}
u32 gen5_present(void) {return 0;}
u32 gen5_enabled(void) {return 1;}
void gen5_noop(void) {}
u32 gen5_language(u32 language) {
    // Western Gen4 save structures/character encoding share this source adapter.
    // Keep the stored trainer and individual Pokemon language fields unchanged.
    return language==2 || language==3 || language==4 || language==5 || language==7 ? 0 : 3;
}
int gen5_ack(u32 *common,u32 command,u32 tries) {
    (void)command;(void)tries;*(u32*)common[0]=0;return 1;
}
void gen5_backup_worker(u32 *common) {
    u32 command=common[0x510/4],mode=common[0x518/4];
    u32 source=common[0x4fc/4],dest=common[0x500/4],len=common[0x504/4];
    int ok=0;
    if(command==6 && mode==0)ok=transfer(SAVE_READ,source,(void*)dest,len,1);
    else if(command==7 && (mode==1 || mode==2))ok=transfer(SAVE_WRITE,dest,(void*)source,len,1);
    else if(command==9 && mode==3) {
        // Static (not stack) so a whole sector can be compared per request.
        static u8 check[512] __attribute__((aligned(32)));
        ok=dest<=0x80000 && len<=0x80000-dest;
        for(u32 off=0;ok && off<len;) {
            u32 n=len-off,room=512-((dest+off)&511);if(n>room)n=room;
            ok=transfer(SAVE_READ,dest+off,check,n,1);
            for(u32 i=0;ok && i<n;i++)if(check[i]!=((const u8*)source)[off+i])ok=0;
            off+=n;
        }
    }
    *(u32*)common[0]=ok?0:1; // Native CARDResult, read by the normal completion path.
    if(ok){common[0x4fc/4]+=len;common[0x500/4]+=len;common[0x504/4]=0;}
}
