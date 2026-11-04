/* Synthetic-only feature/state/render/save tests. No game data is required. */
#define main existingCoreTests
#include "test_walkerfile.c"
#undef main
#include "walkerclassic.h"
#include "hgss_courses.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
static unsigned checksum16(const uint8_t *p,unsigned count){unsigned c=65535;while(count--){c^=*p++<<8;for(int n=0;n<8;n++)c=((c<<1)^((c&32768)?0x1021:0))&65535;}return c;}
static void le(uint8_t *p,uint32_t v){for(int n=0;n<4;n++)p[n]=v>>(n*8);}
static void saveFixture(uint8_t *save,int language){
    memset(save,0,0x80000);
    for(int n=0;n<2;n++){
        uint8_t *p=save+n*0x40000;p[0x80]=7;p[0x7d]=language;le(p+0xe70c,3);
        le(p+0xf618,n+1);le(p+0xf61c,0xf628);le(p+0xf620,0x20060623);
        unsigned c=checksum16(p,0xf618);p[0xf626]=c;p[0xf627]=c>>8;
    }
}
int main(void){
    static uint8_t peer[65536],baseline[65536],route[0xbe],frame[6144];
    makeStrolling(1,1000000);walkerResume(ee,1000000,10);
    for(int n=0;n<10;n++){ee[WALKER_ROUTE+0x8c+n*2]=4;ee[WALKER_ROUTE+0x8d+n*2]=0;ee[WALKER_ROUTE+0xb4+n]=100;}
    memset(ee+0xce8c,0,0xcef0-0xce8c);
    memcpy(baseline,ee,65536);memcpy(route,ee+WALKER_ROUTE,sizeof(route));
    for(unsigned id=1;id<537;id++)if(id==1||id==51||id==53||(id>=328&&id<=419)){
        assert(walkerStoreItem(ee,0x8000|id,0,1000000)==1);assert(walkerItemId(ee,0)==id);assert(walkerRouteItemId(ee,0x8000|id)==id);
    }
    assert(!memcmp(route,ee+WALKER_ROUTE,sizeof(route)));
    memcpy(ee,baseline,65536);unsigned regular,expanded,count=0,master=0;
    for(unsigned n=0;n<200000;n++){
        assert(walkerChooseItemEx(ee,&expanded,1));unsigned id=walkerRouteItemId(ee,expanded);
        if(expanded&0x8000){count++;if(id==1)master++;assert(id>0&&id<=419);}
    }
    assert(count>14000&&count<18000&&master>0&&master<60);
    memcpy(ee,baseline,65536);memcpy(peer,baseline,65536);
    assert(walkerChooseItemEx(ee,&regular,0));unsigned direct;assert(walkerChooseItem(peer,&direct));assert(regular==direct&&!memcmp(ee,peer,65536));
    /* Two real core images: gifts, once/day, separate ten-item bag and health. */
    memcpy(ee,baseline,65536);ee[0xed+16]=1;seal(0xed,0x1ed,104);memcpy(baseline,ee,65536);
    ee[0xed+16]=2;seal(0xed,0x1ed,104);memcpy(peer,ee,65536);memcpy(ee,baseline,65536);
    assert(walkerVisit(ee,peer,1000010)==1);assert(walkerFriendItemId(ee,0)>0);assert(walkerVisit(ee,peer,1000010)==-1);
    assert(!memcmp(route,ee+WALKER_ROUTE,sizeof(route)));
    assert(walkerVisit(ee,peer,1086410)==1);assert(walkerFriendItemId(ee,1)>0);
    assert(walkerVisit(ee,ee,1086410)==0);
    for(int n=0;n<10;n++){ee[0xcec8+n*4]=4;ee[0xcec9+n*4]=0;}
    WalkerInfo before,after;assert(walkerRead(ee,&before));assert(walkerVisit(ee,peer,1172810)==2);assert(walkerRead(ee,&after));assert(after.watts>before.watts);
    /* Exercise all presentation states/choices and the menu's original ends. */
    int32_t state[40]={0};state[19]=2;state[20]=1;
    assert(walkerClassicInput(ee,state,0,1172810,1000,0));assert(state[0]==1&&state[1]==5);
    walkerClassicInput(ee,state,2,1172810,1100,0);assert(state[0]==0);
    walkerClassicInput(ee,state,2,1172810,1200,0);assert(state[0]==1&&state[1]==0);
    walkerClassicInput(ee,state,0,1172810,1300,0);assert(state[0]==0);
    for(int view=0;view<=10;view++)for(int selection=0;selection<16;selection++){
        state[0]=view;state[1]=selection;state[3]=0;state[18]=0;walkerClassicFrame(ee,state,frame,1000);
        for(unsigned n=0;n<sizeof(frame);n++)assert(frame[n]<=3);
    }
    /* A valid uploaded sound directory can synthesize; bad sums refuse audio. */
    int16_t pcm[22050];ee[0x8cb0]=ee[0x8cb1]=0;ee[0x8cb2]=6;
    unsigned char notes[]={120,123,6,12,0,127};memcpy(ee+0x8cf0,notes,6);unsigned sum=0;for(int n=0;n<6;n++)sum+=notes[n];ee[0x8cb3]=sum;
    assert(walkerClassicSound(ee,0,pcm,22050)>0);ee[0x8cb3]^=1;assert(!walkerClassicSound(ee,0,pcm,22050));
    /* Compact Bluetooth packets are validated before creating a read-only peer.
     * Battle state must be deterministic and never mutate either active image. */
    static uint8_t packet[1688],decoded[65536],frozen[65536];
    memcpy(ee,baseline,65536);
    unsigned moves[]={33,45,98,182};
    for(unsigned n=0;n<4;n++){ee[WALKER_ROUTE+4+n*2]=moves[n];ee[WALKER_ROUTE+5+n*2]=0;}
    ee[0xed+14]=12;ee[0xed+15]=34;seal(0xed,0x1ed,104);
    assert(walkerPeerExport(ee,packet));assert(!packet[26]&&!packet[27]);
    assert(walkerPeerImport(packet,sizeof(packet),decoded));assert(walkerPartnerMove(decoded,2)==98);
    assert(!walkerPeerImport(packet,sizeof(packet)-1,decoded));packet[1600]^=1;assert(!walkerPeerImport(packet,sizeof(packet),decoded));packet[1600]^=1;
    assert(walkerPeerImport(packet,sizeof(packet),decoded));memcpy(peer,decoded,65536);memcpy(frozen,ee,65536);
    int32_t a[24],b[24];walkerFriendlyStart(ee,peer,a,150);memcpy(b,a,sizeof(a));
    assert(!walkerFriendlyTurn(ee,peer,a,4,0));assert(!memcmp(a,b,sizeof(a)));
    unsigned turns=0;
    while(!a[12]){
        assert(walkerFriendlyTurn(ee,peer,a,0,0));assert(walkerFriendlyTurn(ee,peer,b,0,0));assert(!memcmp(a,b,sizeof(a)));
        assert(++turns<=30);
        walkerFriendlyFrame(ee,peer,a,turns&1,turns%4,frame,turns*400);
        for(unsigned n=0;n<sizeof(frame);n++)assert(frame[n]<=3);
    }
    assert(!memcmp(frozen,ee,65536));assert(a[12]>=1&&a[12]<=3);
    /* Both frames and both viewpoints use the same inward-facing rule.
     * An asymmetric marker catches an accidental extra/missing mirror.
     * Fixed-facing artwork retains its serialized orientation. */
    for(unsigned fixed=0;fixed<2;fixed++)for(unsigned side=0;side<2;side++)for(unsigned pose=0;pose<2;pose++){
        memset(ee+0x91be,0,384);memset(peer+0x91be,0,384);
        ee[WALKER_ROUTE+14]=peer[WALKER_ROUTE+14]=(uint8_t)fixed;
        unsigned at=0x91be+pose*192+4;
        ee[at]=ee[at+1]=peer[at]=peer[at+1]=64;
        unsigned ms=pose?400:1000; /* Both have zero motion offset. */
        walkerFriendlyFrame(ee,peer,b,side,0,frame,ms);
        assert(frame[23*96+(fixed?6:33)]==3); /* left opponent */
        assert(frame[23*96+62]==3);           /* right partner */
        assert(frame[23*96+(fixed?33:6)]==0);
        assert(frame[23*96+89]==0);
    }
    unsigned rare=0;
    for(unsigned n=0;n<10000;n++){unsigned choice;assert(walkerChooseBattleItem(ee,&choice,1));if(choice&0x8000)rare++;}
    assert(rare>8000&&rare<9000);
    for(unsigned n=0;n<100;n++){unsigned choice;assert(walkerChooseBattleItem(ee,&choice,0));assert(!(choice&0x8000));}
    static uint8_t save[0x80000],original[0x80000];uint32_t added;
    for(int language=1;language<=8;language++)if(language!=6){
        saveFixture(save,language);memcpy(original,save,sizeof(save));assert(hgssUnlockCourses(save,sizeof(save),&added)==1);
        uint32_t expected=(language==1?0x07ffffff:language==8?0x037fffff:0x027fffff)&0x07f00000;assert(added==expected);
        for(unsigned n=0;n<sizeof(save);n++)if(n%0x40000<0xe70c||n%0x40000>0xe70f)if(n%0x40000!=0xf626&&n%0x40000!=0xf627)assert(save[n]==original[n]);
        assert(hgssUnlockCourses(save,sizeof(save),&added)==2&&added==0);
        save[0x80]=10;save[0x40080]=10;assert(!hgssUnlockCourses(save,sizeof(save),&added));
    }
    saveFixture(save,2);save[100]^=1;save[0x40000+100]^=1;memcpy(original,save,sizeof(save));assert(!hgssUnlockCourses(save,sizeof(save),&added));assert(!memcmp(save,original,sizeof(save)));
    assert(!hgssUnlockCourses(save,65536,&added));
    printf("1.5 core: expanded pool %u/200000 (Master Ball %u), opt-out byte parity, peer gift/daily/history/full-pack rules, classic menu/render/sound validation and regional save masks/checksums passed.\n",count,master);
    return 0;
}
