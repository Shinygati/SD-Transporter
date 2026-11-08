/* Original-style menus and three-button interaction. GPL-3.0-or-later.
 * Uploaded art and sound scores come from the user's walker, never this
 * distribution. Menu behaviour/layout cross-checked with lumirth/pw (CC0).
 * This is a presentation/controller for the companion core, not H8 emulation. */
#include "walkerclassic.h"
#include "walkerfile.h"
#include <string.h>
#include <stdio.h>
#include "species.h"
#include "menuicons.h"
#include "movefacts.h"

enum { HOME,MENU,RADAR,DOWSE,BATTLE,PACK,TRAINER,SETTINGS,CONNECT,RESULT,PEERS };
enum { VIEW,SEL,SUB,CHOICE,TARGET,ROUND,TRIES,EMPTY,DEADLINE,RESULT_ID,SOUND,BAT,
       B1,B2,B3,B4,B5,B6,REPLACE,VOLUME,CONTRAST,ANIM,PREV_HP,PREV_ENEMY,LAST_ACTION };
static unsigned rnd(uint8_t *ee,unsigned n){return (walkerRandom(ee)>>3)%n;}
static void result(int32_t *s,int value,int score){s[VIEW]=RESULT;s[RESULT_ID]=value;s[SOUND]=score+1;}
static void battleStart(int32_t *s,uint32_t ms){
    WalkerBattle b;walkerBattleStart(&b,(unsigned)s[CHOICE]);
    s[BAT]=b.playerHp;s[BAT+1]=b.enemyHp;s[BAT+2]=b.row;s[BAT+3]=b.finished;
    s[BAT+4]=b.caught;s[BAT+5]=b.encounter;s[BAT+6]=b.lastOutcome;
    s[VIEW]=BATTLE;s[ANIM]=ms;s[LAST_ACTION]=-1;s[SOUND]=11;
}
static void storeReward(uint8_t *ee,int32_t *s,int slot,uint32_t now){
    int item=s[REPLACE]==2;
    int ok=item?walkerStoreItem(ee,s[CHOICE],slot,now):walkerStorePokemon(ee,s[CHOICE],slot,now);
    if(ok<0){s[VIEW]=PACK;s[SEL]=0;s[SOUND]=2;}
    else {s[REPLACE]=0;result(s,ok?item?2:1:3,ok?5:4);}
}
int walkerClassicInput(uint8_t *ee,int32_t s[40],int button,uint32_t now,uint32_t ms,int expanded){
    WalkerInfo info;if(!walkerRead(ee,&info)||!walkerCanWalk(ee))return 0;
    if(s[VIEW]<HOME||s[VIEW]>PEERS){memset(s,0,40*sizeof(*s));s[VOLUME]=2;s[CONTRAST]=1;}
    if(s[VIEW]==RADAR && (int32_t)(ms-(uint32_t)s[DEADLINE])>=0){
        if(s[SUB]==0){s[SUB]=1;s[TARGET]=rnd(ee,4);s[DEADLINE]=ms+(s[ROUND]>2?800:2100-s[ROUND]*400);s[SOUND]=4;}
        else result(s,3,4);
    }
    /* A held centre button exits browsing, without abandoning paid activities. */
    if(button==3){
        if(s[VIEW]==MENU || (s[VIEW]==PACK && !s[REPLACE]) || s[VIEW]==TRAINER || s[VIEW]==SETTINGS || s[VIEW]==CONNECT){
            s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;s[SOUND]=2;
        }
        return 1;
    }
    if(button<0)return 1;
    if(button>2)return 0;
    s[SOUND]=3;
    switch(s[VIEW]){
    case HOME:s[VIEW]=MENU;s[SEL]=button==0?5:button==1?2:0;s[SOUND]=1;break;
    case MENU:
        if(button!=1){s[SEL]=(s[SEL]+(button==0?5:1))%6;break;}
        s[SEL]=s[SEL]<0||s[SEL]>5?0:s[SEL];s[SUB]=0;s[SOUND]=1;
        if(s[SEL]==0){
            unsigned visitor=walkerPendingEncounter(ee);WalkerEncounter encounter;
            if(!info.hasPokemon){result(s,4,4);break;}
            if(visitor){s[CHOICE]=visitor-1;walkerClearEncounter(ee);battleStart(s,ms);break;}
            if(info.watts<10||!walkerChooseEncounter(ee,&encounter)){result(s,5,4);break;}
            if(!walkerSpend(ee,10)){result(s,5,4);break;}
            s[CHOICE]=encounter.index;s[ROUND]=0;s[TRIES]=3-(int)encounter.index+(int)rnd(ee,2);
            s[VIEW]=RADAR;s[SEL]=0;s[SUB]=0;s[DEADLINE]=ms+600+rnd(ee,600);
        }else if(s[SEL]==1){
            unsigned choice;
            if(!info.hasPokemon){result(s,4,4);break;}
            if(info.watts<3||!walkerChooseItemEx(ee,&choice,expanded)||!walkerSpend(ee,3)){result(s,5,4);break;}
            s[CHOICE]=choice;s[VIEW]=DOWSE;s[TARGET]=rnd(ee,6);s[SEL]=0;s[TRIES]=2;s[EMPTY]=-1;
        }else{s[VIEW]=s[SEL]==2?CONNECT:s[SEL]==3?TRAINER:s[SEL]==4?PACK:SETTINGS;s[SEL]=0;s[REPLACE]=0;}
        break;
    case RADAR:
        if(button!=1){s[SEL]=(s[SEL]+(button==0?3:1))%4;break;}
        if(s[SUB]!=1)break;
        if(s[SEL]!=s[TARGET]){result(s,3,4);break;}
        if(++s[ROUND]>=s[TRIES])battleStart(s,ms);
        else{s[SUB]=0;s[DEADLINE]=ms+600+rnd(ee,600);s[SOUND]=4;}
        break;
    case DOWSE:
        if(button!=1){s[SEL]=(s[SEL]+(button==0?5:1))%6;break;}
        if(s[SEL]==s[EMPTY])break;
        if(s[SEL]==s[TARGET]){s[REPLACE]=2;storeReward(ee,s,-1,now);}
        else if(--s[TRIES]<=0)result(s,3,4);
        else{s[EMPTY]=s[SEL];s[SOUND]=2;}
        break;
    case BATTLE:{
        if((uint32_t)(ms-(uint32_t)s[ANIM])<650u && s[ANIM])break;
        WalkerBattle b={s[BAT],s[BAT+1],s[BAT+2],s[BAT+3],s[BAT+4],(unsigned)s[BAT+5],s[BAT+6]};
        s[PREV_HP]=b.playerHp;s[PREV_ENEMY]=b.enemyHp;s[LAST_ACTION]=button;s[ANIM]=ms;
        walkerBattleAction(ee,&b,button);
        s[BAT]=b.playerHp;s[BAT+1]=b.enemyHp;s[BAT+2]=b.row;s[BAT+3]=b.finished;
        s[BAT+4]=b.caught;s[BAT+5]=b.encounter;s[BAT+6]=b.lastOutcome;
        s[SOUND]=button==2?16:button==1?13:12;
        if(b.caught){s[REPLACE]=1;storeReward(ee,s,-1,now);}
        else if(b.finished)result(s,3,4);
        break;}
    case PACK:
        if(s[REPLACE]){
            if(button!=1){s[SEL]=(s[SEL]+(button==0?3:1))%4;break;}
            if(s[SEL]==3){s[REPLACE]=0;s[VIEW]=HOME;break;}
            storeReward(ee,s,s[SEL],now);
        }else if(button==1){s[VIEW]=HOME;s[SOUND]=2;}
        else s[SEL]=(s[SEL]+(button==0?15:1))%16;
        break;
    case TRAINER:
        if(button==1){s[VIEW]=HOME;s[SOUND]=2;}else s[SEL]=(s[SEL]+(button==0?3:1))%4;
        break;
    case SETTINGS:
        if(button==1){s[SUB]=(s[SUB]+1)%3;if(s[SUB]==0)s[VIEW]=HOME;}
        else if(s[SUB]==0)s[VOLUME]=(s[VOLUME]+(button==0?2:1))%3;
        else s[CONTRAST]=(s[CONTRAST]+(button==0?2:1))%3;
        break;
    case CONNECT:
        if(button!=1)s[SEL]=(s[SEL]+1)%2;
        else {s[VIEW]=s[SEL]?PEERS:CONNECT;s[SUB]=s[SEL]?0:1;s[SOUND]=1;}
        break;
    case PEERS:break; /* host supplies the available local profiles */
    case RESULT:s[VIEW]=HOME;s[SOUND]=2;break;
    }
    return 1;
}
static void pixel(uint8_t *out,int x,int y,unsigned color){if(x>=0&&x<96&&y>=0&&y<64)out[y*96+x]=(uint8_t)color;}
static void rect(uint8_t *out,int x,int y,int w,int h,unsigned color){for(int j=0;j<h;j++)for(int i=0;i<w;i++)pixel(out,x+i,y+j,color);}
static void raster(const uint8_t *ee,uint8_t *out,unsigned offset,int x,int y,int w,int h,int flip){
    if(offset+(unsigned)(w*h/4)>65536)return;
    for(int j=0;j<h;j++)for(int i=0;i<w;i++){
        unsigned n=offset+(j/8)*w*2+i*2,c=(((ee[n]>>(j&7))&1)<<1)|((ee[n+1]>>(j&7))&1);
        if(c)pixel(out,x+(flip?w-1-i:i),y+j,c);
    }
}
/* Original uploaded 8x16 number glyphs. */
static void number(const uint8_t *ee,uint8_t *out,uint32_t value,int x,int y,int digits){
    char text[16];snprintf(text,sizeof(text),"%0*lu",digits,(unsigned long)value);
    unsigned length=(unsigned)strlen(text);if(length>10)return;
    for(unsigned n=0;n<length;n++)raster(ee,out,0x280+(text[n]-'0')*32,x+n*8,y,8,16,0);
}
/* Small plain labels only where custom app features have no uploaded label. */
static void label(uint8_t *out,const char *text,int x,int y){
    static const char chars[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-/!?.";
    static const uint8_t glyph[][5]={
      {0,0,0,0,0},{126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},{65,65,127,65,65},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},{127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},{63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},{7,8,112,8,7},{97,81,73,69,67},{62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},{24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30},{0,54,54,0,0},{8,8,8,8,8},{32,16,8,4,2},{0,0,95,0,0},{2,1,81,9,6},{0,96,96,0,0}};
    for(;*text && x<94;text++,x+=6){char c=*text;if(c>='a'&&c<='z')c-=32;const char *p=strchr(chars,c);unsigned index=p?(unsigned)(p-chars):40;
        for(int j=0;j<7;j++)for(int i=0;i<5;i++)if(glyph[index][i]&(1u<<j))pixel(out,x+i,y+j,3);
    }
}
void walkerClassicFrame(const uint8_t *ee,const int32_t s[40],uint8_t out[6144],uint32_t ms){
    memset(out,0,6144);WalkerInfo i;if(!walkerRead(ee,&i))return;
    unsigned frame=(ms/400)&1;
    if(s[VIEW]==HOME){
        raster(ee,out,0x8fbe,0,24,32,24,0);
        if(i.hasPokemon)raster(ee,out,0x933e + frame*768,32,0,64,48,0);
        rect(out,0,47,96,1,3);
        unsigned steps=i.strollSteps>9999999?9999999:i.strollSteps,digits=1;
        for(unsigned rest=steps;rest>=10;rest/=10)digits++;
        number(ee,out,steps,96-(int)digits*8,48,(int)digits);
        uint16_t rows[16];
        if(i.pokemonCount){walkerMenuIcon(1,rows);for(int y=0;y<16;y++)for(int x=0;x<16;x++)if(rows[y]&(1u<<x))pixel(out,x,48+y,3);}
        if(i.itemCount){walkerMenuIcon(3,rows);for(int y=0;y<16;y++)for(int x=0;x<16;x++)if(rows[y]&(1u<<x))pixel(out,20+x,48+y,3);}
    }else if(s[VIEW]==MENU){
        unsigned sel=s[SEL]>=0&&s[SEL]<6?s[SEL]:0;
        raster(ee,out,0x910+sel*320,8,0,80,16,0);
        for(unsigned n=0;n<6;n++){raster(ee,out,0x1090+n*64,n*16,24,16,16,0);if(n==sel){pixel(out,n*16+7,18,3);rect(out,n*16+5,16,5,1,3);}}
        if(sel<2)number(ee,out,sel?3:10,8,48,2);
        number(ee,out,i.watts,44,48,4);raster(ee,out,0x420,80,48,16,16,0);
    }else if(s[VIEW]==RADAR || s[VIEW]==DOWSE){
        int radar=s[VIEW]==RADAR,cols=radar?2:3;
        label(out,radar?"POKE RADAR":"DOWSING",0,0);
        for(int n=0;n<(radar?4:6);n++){
            int x=8+(n%cols)*(radar?48:28),y=15+(n/cols)*21;
            raster(ee,out,0x1b50,x,y,16,16,0);
            if(n==s[SEL]){rect(out,x-2,y-1,20,1,3);rect(out,x-2,y+17,20,1,3);}
            if(radar && s[SUB]==1 && n==s[TARGET])label(out,"!",x+5,y+4);
            if(!radar && n==s[EMPTY])rect(out,x,y,16,16,0);
        }
        if(!radar){char value[16];snprintf(value,sizeof(value),"%ld TRIES",(long)s[TRIES]);label(out,value,0,57);}
    }else if(s[VIEW]==BATTLE){
        int anim=(uint32_t)(ms-(uint32_t)s[ANIM])<600u,shift=anim?(int)((ms/70)%5):0;
        raster(ee,out,0x9a7e + s[CHOICE]*384+frame*192,4+shift,5,32,24,!(ee[WALKER_ROUTE+0x52+s[CHOICE]*16+14]&1));
        raster(ee,out,0x91be + frame*192,60-shift,5,32,24,0);
        for(int n=0;n<4;n++){rect(out,4+n*7,33,5,3,n<s[BAT+1]?3:1);rect(out,60+n*7,33,5,3,n<s[BAT]?3:1);}
        label(out,"ATK DODGE CATCH",2,47);label(out,"<    O    >",8,57);
    }else if(s[VIEW]==PACK){
        label(out,s[REPLACE]?"REPLACE SLOT?":"INVENTORY",1,0);
        if(s[REPLACE] && s[SEL]==3)label(out,"LEAVE NEW FIND",3,25);
        else {
            int index=s[SEL],pokemon=s[REPLACE]?s[REPLACE]==1:index<3;
            /* Replacing: SEL is the slot itself (0-2) for Pokemon and items alike.
             * Browsing: 0-2 Pokemon, 3-5 items, 6-15 friend gifts. */
            unsigned id;
            if(pokemon)id=walkerPokemonId(ee,index%3);
            else if(s[REPLACE])id=walkerItemId(ee,index);
            else if(index<6)id=walkerItemId(ee,index-3);
            else if(index<16)id=ee[0xcec8+(index-6)*4]|ee[0xcec9+(index-6)*4]<<8;
            else id=0;
            char value[22];snprintf(value,sizeof(value),"%s %d",pokemon?"POKEMON":"ITEM",index+1);label(out,value,0,17);
            const char *name=id?(pokemon?speciesName(id):itemName(id)):"EMPTY";
            snprintf(value,sizeof(value),"%.16s",name);label(out,value,0,31);
            if(pokemon && id)for(unsigned candidate=0;candidate<3;candidate++)if(walkerRoutePokemonId(ee,candidate)==id){raster(ee,out,0x9a7e + candidate*384+frame*192,60,41,32,24,0);break;}
        }
        label(out,s[REPLACE]?"< SELECT >":"<   BACK   >",0,56);
    }else if(s[VIEW]==TRAINER){
        label(out,s[SEL]==0?"TRAINER":s[SEL]==1?"COURSE":s[SEL]==2?"TODAY":"TOTAL STEPS",0,0);
        if(s[SEL]==0){label(out,i.trainer,0,19);number(ee,out,i.trainerId,16,33,5);}
        else if(s[SEL]==1)raster(ee,out,0x907e,8,24,80,16,0);
        else number(ee,out,s[SEL]==2?i.todaySteps:i.totalSteps,8,24,7);
        label(out,"<   BACK   >",0,56);
    }else if(s[VIEW]==SETTINGS){
        label(out,"SETTINGS",0,0);label(out,s[SUB]==0?"SOUND":"CONTRAST",0,20);number(ee,out,s[SUB]==0?s[VOLUME]:s[CONTRAST],40,33,1);label(out,"<   NEXT   >",0,56);
    }else if(s[VIEW]==CONNECT || s[VIEW]==PEERS){
        raster(ee,out,0x2350,32,7,32,32,0);label(out,s[VIEW]==PEERS?"CHOOSE DEVICE":s[SEL]?"OTHER WALKER":"CONSOLE",1,43);label(out,"< CONNECT >",8,56);
    }else if(s[VIEW]==RESULT){
        const char *message=s[RESULT_ID]==1?"CAUGHT!":s[RESULT_ID]==2?"ITEM FOUND!":s[RESULT_ID]==4?"NO POKEMON":s[RESULT_ID]==5?"NOT ENOUGH WATTS":"IT GOT AWAY!";
        label(out,message,0,6);
        if(s[RESULT_ID]==1)raster(ee,out,0x9a7e + s[CHOICE]*384+frame*192,32,21,32,24,0);
        if(s[RESULT_ID]==1){char name[17];snprintf(name,sizeof(name),"%.16s",speciesName(walkerRoutePokemonId(ee,s[CHOICE])));label(out,name,0,46);}
        if(s[RESULT_ID]==2){char name[17];snprintf(name,sizeof(name),"%.16s",itemName(walkerRouteItemId(ee,s[CHOICE])));label(out,name,0,25);}
        label(out,"PRESS A BUTTON",4,56);
    }
}
unsigned walkerClassicSound(const uint8_t *ee,unsigned score,int16_t *out,unsigned cap){
    if(score>=16||cap>220500)return 0;
    unsigned at=0x8cb0+score*4,start=0x8cf0+(ee[at]|ee[at+1]<<8),bytes=ee[at+2],sum=0;
    if(bytes<2||bytes>128||start<0x8cf0||start+bytes>0x8ed0)return 0;
    for(unsigned j=0;j<bytes;j++)sum+=ee[start+j];
    if((sum&255)!=ee[at+3] || (ee[start+(bytes/2)*2-1]&127)<126)return 0;
    /* Equal-tempered compare values approximate the original piezo timbre.
     * Score order, tempo, durations and pitch changes use the uploaded data. */
    static const uint16_t frequency[]={131,139,147,156,165,175,185,196,208,220,233,247,262,277,294,311,330,349,370,392,415,440,466,494,523,554,587,622,659,698,740,784,831,880,932,988,1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976,2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,3951};
    unsigned length=0,tempo=120,loops=0,phase=0;
    for(unsigned n=0;n+1<bytes && length<cap && loops<128;n+=2,loops++){
        unsigned duration=ee[start+n],pitch=ee[start+n+1]&127;
        if(pitch==127)break;
        if(pitch==126){n=(unsigned)-2;continue;}
        if(pitch==123){if(!duration)return 0;tempo=duration;continue;}
        if(pitch>=60 && pitch!=125)return 0;
        unsigned samples=(unsigned)((uint64_t)22050*duration*60/(tempo*24));
        if(samples>cap-length)samples=cap-length;
        unsigned silence=(ee[start+n+1]&128)?0:216;if(silence>samples)silence=samples;
        for(unsigned k=0;k<samples;k++){
            if(pitch!=125)phase=(phase+frequency[pitch])%22050;
            out[length++]=(pitch==125||k+silence>=samples)?0:phase<11025?4200:-4200;
        }
    }
    return length;
}

void walkerMenuIcon(unsigned kind,uint16_t rows[16]){
    if(kind>=sizeof(walkerMenuIcons)/sizeof(walkerMenuIcons[0]))kind=0;
    memcpy(rows,walkerMenuIcons[kind],16*sizeof(*rows));
}
static const unsigned peerOffset[]={0xed,0x156,0x272,0x10,0x8f00,0x91be,0x993e,0xcc00};
static const unsigned peerLength[]={104,24,10,96,190,384,320,548};
static void peerSeal(uint8_t *ee,unsigned a,unsigned b,unsigned bytes){
    unsigned sum=1;for(unsigned n=0;n<bytes;n++)sum+=ee[a+n];
    ee[a+bytes]=(uint8_t)sum;memcpy(ee+b,ee+a,bytes+1);
}
int walkerPeerExport(const uint8_t *ee,uint8_t out[1688]){
    WalkerInfo info;if(!walkerRead(ee,&info)||!info.hasPokemon||!info.registered)return 0;
    memcpy(out,"PWPEER15",8);unsigned at=12;
    for(unsigned n=0;n<8;n++){memcpy(out+at,ee+peerOffset[n],peerLength[n]);at+=peerLength[n];}
    out[12+14]=out[12+15]=0; /* Secret ID is not needed for a phone meeting. */
    uint32_t crc=walkerCrc(out+12,1676);for(int n=0;n<4;n++)out[8+n]=crc>>(n*8);
    return 1;
}
int walkerPeerImport(const uint8_t *packet,unsigned bytes,uint8_t out[65536]){
    if(bytes!=1688||memcmp(packet,"PWPEER15",8))return 0;
    uint32_t crc=(uint32_t)packet[8]|(uint32_t)packet[9]<<8|(uint32_t)packet[10]<<16|(uint32_t)packet[11]<<24;
    if(walkerCrc(packet+12,1676)!=crc)return 0;
    memset(out,0,65536);memcpy(out,"nintendo",8);unsigned at=12;
    for(unsigned n=0;n<8;n++){memcpy(out+peerOffset[n],packet+at,peerLength[n]);at+=peerLength[n];}
    peerSeal(out,0xed,0x1ed,104);peerSeal(out,0x156,0x256,24);
    WalkerInfo info;if(!walkerRead(out,&info)||!info.hasPokemon||!info.registered||!walkerCanWalk(out))return 0;
    for(unsigned n=0;n<4;n++)if(walkerPartnerMove(out,n)>467)return 0;
    return 1;
}
unsigned walkerPartnerMove(const uint8_t *ee,unsigned slot){
    if(slot>=4)return 0;unsigned at=WALKER_ROUTE+4+slot*2;
    return ee[at]|ee[at+1]<<8;
}
const char *walkerMoveName(unsigned id){return id<468?walkerMoves[id].name:"Unknown move";}
void walkerFriendlyStart(const uint8_t *a,const uint8_t *b,int32_t s[24],uint32_t seed){
    memset(s,0,24*sizeof(*s));s[0]=s[1]=100;s[11]=(int32_t)(seed?seed:0x51504231);
    for(unsigned side=0;side<2;side++)for(unsigned n=0;n<4;n++){
        unsigned move=walkerPartnerMove(side?b:a,n);s[2+side*4+n]=move<468?walkerMoves[move].pp:0;
    }
    s[13]=s[14]=-1;
}
static uint32_t friendlyRandom(int32_t *s){uint32_t value=(uint32_t)s[11];value^=value<<13;value^=value>>17;value^=value<<5;s[11]=(int32_t)value;return value;}
static unsigned selectedMove(const uint8_t *ee,const int32_t *s,unsigned side,unsigned choice){
    int available=0;for(unsigned n=0;n<4;n++)if(s[2+side*4+n]>0)available=1;
    if(!available)return 165; /* Struggle when all retained moves are exhausted. */
    unsigned move=walkerPartnerMove(ee,choice);
    return choice<4&&s[2+side*4+choice]>0&&move&&move<468?move:0;
}
int walkerFriendlyTurn(const uint8_t *a,const uint8_t *b,int32_t s[24],unsigned choiceA,unsigned choiceB){
    if(s[12]||choiceA>3||choiceB>3||s[0]<1||s[0]>100||s[1]<1||s[1]>100)return 0;
    unsigned move[2]={selectedMove(a,s,0,choiceA),selectedMove(b,s,1,choiceB)};if(!move[0]||!move[1])return 0;
    unsigned first;
    if(walkerMoves[move[0]].priority!=walkerMoves[move[1]].priority)first=walkerMoves[move[0]].priority>walkerMoves[move[1]].priority?0:1;
    else if(a[WALKER_ROUTE+12]!=b[WALKER_ROUTE+12])first=a[WALKER_ROUTE+12]>b[WALKER_ROUTE+12]?0:1;
    else first=friendlyRandom(s)&1;
    s[13]=choiceA;s[14]=choiceB;s[15]=s[16]=0;
    for(unsigned n=0;n<2;n++){
        unsigned side=first^n,other=side^1,id=move[side],choice=side?choiceB:choiceA;const WalkerMoveFact *fact=walkerMoves+id;
        if(s[2+side*4+choice]>0)s[2+side*4+choice]--;
        if(fact->accuracy && friendlyRandom(s)%100>=fact->accuracy)continue;
        if(fact->category==2){s[18+side]=1;continue;} /* Simplified brace effect. */
        unsigned damage=4+fact->power/8+(side?b:a)[WALKER_ROUTE+12]/10;
        if((friendlyRandom(s)&15)==0)damage+=damage/2;
        if(s[18+other]){damage=(damage+1)/2;s[18+other]=0;}
        if(damage>(unsigned)s[other])damage=(unsigned)s[other];s[other]-=damage;s[15+side]=damage;s[17]=side;
        if(!s[other]){s[12]=side+1;break;}
    }
    if(++s[10]>=30 && !s[12])s[12]=s[0]==s[1]?3:s[0]>s[1]?1:2;
    return 1;
}
void walkerFriendlyFrame(const uint8_t *a,const uint8_t *b,const int32_t s[24],unsigned side,unsigned selected,uint8_t out[6144],uint32_t ms){
    memset(out,0,6144);if(side>1||selected>3)return;
    const uint8_t *own=side?b:a,*peer=side?a:b;unsigned frame=(ms/400)&1,other=side^1;
    char text[16];snprintf(text,sizeof(text),"%.7s",speciesName(own[WALKER_ROUTE]|own[WALKER_ROUTE+1]<<8));label(out,text,54,0);
    snprintf(text,sizeof(text),"%.7s",speciesName(peer[WALKER_ROUTE]|peer[WALKER_ROUTE+1]<<8));label(out,text,0,0);
    rect(out,1,9,39,4,1);rect(out,56,9,39,4,1);rect(out,1,9,(s[other]*39+99)/100,4,3);rect(out,56,9,(s[side]*39+99)/100,4,3);
    int motion=(int)((ms/100)%3)-1;
    raster(peer,out,0x91be + frame*192,4+motion,17,32,24,!(peer[WALKER_ROUTE+14]&1));
    raster(own,out,0x91be + frame*192,60-motion,17,32,24,0);
    unsigned move=walkerPartnerMove(own,selected);snprintf(text,sizeof(text),"%u.%.12s",selected+1,walkerMoveName(move));label(out,text,0,46);
    snprintf(text,sizeof(text),"PP %ld",(long)s[2+side*4+selected]);label(out,text,0,56);
    snprintf(text,sizeof(text),"TURN %ld",(long)s[10]+1);label(out,text,50,56);
}
int walkerChooseBattleItem(uint8_t *ee,unsigned *out,int expanded){
    WalkerInfo info;if(!walkerRead(ee,&info)||!info.hasPokemon)return 0;
    if(expanded && walkerExpandedReward(ee,out,1))return 1;
    /* Course slots are ordered from rarer finds to common finds. Use the
     * original table only, with 70% weight on its first three entries. Invalid
     * entries are omitted and the remaining weights are normalized. */
    static const unsigned weight[]={30,25,15,12,8,5,2,1,1,1};
    unsigned total=0;
    for(unsigned n=0;n<10;n++){unsigned id=walkerRouteItemId(ee,n);if(id>0&&id<=536)total+=weight[n];}
    if(!total)return 0;
    unsigned roll=(walkerRandom(ee)>>3)%total;
    for(unsigned n=0;n<10;n++){
        unsigned id=walkerRouteItemId(ee,n);if(!id||id>536)continue;
        if(roll<weight[n]){*out=n;return 1;}roll-=weight[n];
    }
    return 0;
}
