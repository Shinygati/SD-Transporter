#include "Bridge.h"
#include "walkerfile.h"
#include "species.h"
#include <string.h>
#include <stdio.h>
int pw_info(const uint8_t *data,int64_t v[14]) {
    WalkerInfo i;memset(v,0,14*sizeof(*v));
    if(!walkerRead(data,&i))return 0;
    int64_t values[14]={i.registered,i.hasPokemon,i.totalSteps,i.strollSteps,i.todaySteps,
        i.watts,i.species,i.level,i.friendship,i.pokemonCount,i.itemCount,
        walkerPendingEncounter(data),i.lastEvent,i.eventCount};
    memcpy(v,values,sizeof(values));return 1;
}
const char *pw_species(unsigned id){return speciesName(id);}
const char *pw_item(unsigned id){return itemName(id);}
int pw_visit(uint8_t *own,const uint8_t *peer,uint32_t now){return walkerVisit(own,peer,now);}
int pw_walkable(const uint8_t *data){return walkerCanWalk(data);}
void pw_text(const uint8_t *data,int kind,char out[128]){
    WalkerInfo i;memset(&i,0,sizeof(i));walkerRead(data,&i);
    if(kind==4){walkerTrainerKey(data,out);return;}
    if(kind==3){snprintf(out,128,i.registered?"%s / ID %05lu":"Trainer unavailable",i.trainer,(unsigned long)i.trainerId);return;}
    snprintf(out,128,"%s",kind==0?i.trainer:kind==1?i.nickname:i.route);
}
int pw_resume(uint8_t *data,uint32_t now,unsigned rate){return walkerResume(data,now,rate);}
int pw_timer(uint8_t *data,uint32_t now,unsigned rate){WalkerReport r={0};walkerAdvance(data,now,rate,&r);return (int)r.steps;}
int pw_steps(uint8_t *data,uint32_t now,uint32_t steps){WalkerReport r={0};walkerAddSteps(data,now,steps,&r);return (int)r.steps;}
int pw_command(uint8_t *data,int op,int arg,uint32_t now){
    unsigned choice;
    switch(op){
    case 0:return (int)((walkerRandom(data)>>1)&0x7fffffff);
    case 1:return walkerSpend(data,(unsigned)arg);
    case 2:return walkerChooseItemEx(data,&choice,arg!=0)?(int)choice:-1;
    case 3:return walkerStorePokemon(data,(unsigned)arg&255,(arg>>8)-1,now);
    case 4:return walkerStoreItem(data,(unsigned)arg&255,(arg>>8)-1,now);
    case 12:return walkerChooseBattleItem(data,&choice,arg!=0)?(int)choice:-1;
    case 11:return (int)walkerFriendItemId(data,(unsigned)arg);
    case 10:return walkerStoreItem(data,(unsigned)arg&65535,(arg>>16)-1,now);
    case 5:walkerClearEncounter(data);return 1;
    case 6:return (int)walkerPokemonId(data,(unsigned)arg);
    case 7:return (int)walkerItemId(data,(unsigned)arg);
    case 8:return (int)walkerRouteItemId(data,(unsigned)arg);
    case 9:return (int)walkerRoutePokemonId(data,(unsigned)arg);
    default:return -1;
    }
}
void pw_encounter(uint8_t *data,int v[3]){WalkerEncounter e;v[0]=-1;v[1]=v[2]=0;if(walkerChooseEncounter(data,&e)){v[0]=(int)e.index;v[1]=(int)e.species;v[2]=(int)e.level;}}
void pw_battle(uint8_t *data,int v[7],int action){
    WalkerBattle b={v[0],v[1],v[2],v[3],v[4],(unsigned)v[5],v[6]};walkerBattleAction(data,&b,action);
    v[0]=b.playerHp;v[1]=b.enemyHp;v[2]=b.row;v[3]=b.finished;v[4]=b.caught;v[5]=(int)b.encounter;v[6]=b.lastOutcome;
}
uint32_t pw_crc(const uint8_t *data,unsigned length){return walkerCrc(data,length);}

#include "../../shared/walkerclassic.c"
int pw_check(uint8_t *data){walkerRepairRecords(data);return walkerValidate(data);}
