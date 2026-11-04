/* JNI bridge to the same GPL-3.0-or-later core used by the consoles. */
#include <jni.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "walkerfile.h"
#include "walkerclassic.h"
#include "species.h"
#define JNI_NAME(n) Java_io_github_sdtransporter_pokewalker_Native_##n
static uint8_t *bytes(JNIEnv *env,jbyteArray data) {
    if(!data||(*env)->GetArrayLength(env,data)!=WALKER_FILE_SIZE) {
        jclass cls=(*env)->FindClass(env,"java/lang/IllegalArgumentException");
        (*env)->ThrowNew(env,cls,"Walker EEPROM must be exactly 65536 bytes");return 0;
    }
    return (uint8_t *)(*env)->GetByteArrayElements(env,data,0);
}
JNIEXPORT jlongArray JNICALL JNI_NAME(info)(JNIEnv *env,jclass cls,jbyteArray data) {
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return 0;WalkerInfo i;
    int ok=walkerRead(ee,&i);jlong v[14]={0};
    if(ok){v[0]=i.registered;v[1]=i.hasPokemon;v[2]=i.totalSteps;v[3]=i.strollSteps;v[4]=i.todaySteps;
        v[5]=i.watts;v[6]=i.species;v[7]=i.level;v[8]=i.friendship;v[9]=i.pokemonCount;v[10]=i.itemCount;
        v[11]=walkerPendingEncounter(ee);v[12]=i.lastEvent;v[13]=i.eventCount;}
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,JNI_ABORT);
    jlongArray out=(*env)->NewLongArray(env,14);if(out)(*env)->SetLongArrayRegion(env,out,0,14,v);return out;
}
JNIEXPORT jstring JNICALL JNI_NAME(text)(JNIEnv *env,jclass cls,jbyteArray data,jint kind) {
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return 0;WalkerInfo i;walkerRead(ee,&i);
    char trainer[128];if(kind==4){walkerTrainerKey(ee,trainer);jstring key=(*env)->NewStringUTF(env,trainer);(*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,JNI_ABORT);return key;}snprintf(trainer,sizeof(trainer),i.registered?"%s / ID %05lu":"Trainer unavailable",i.trainer,(unsigned long)i.trainerId);
    const char *s=kind==3 ? trainer : kind==0 ? i.trainer : kind==1 ? i.nickname : i.route;
    jstring out=(*env)->NewStringUTF(env,s);(*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,JNI_ABORT);return out;
}
JNIEXPORT jstring JNICALL JNI_NAME(species)(JNIEnv *env,jclass cls,jint id) {(void)cls;return (*env)->NewStringUTF(env,speciesName((unsigned)id));}
JNIEXPORT jstring JNICALL JNI_NAME(item)(JNIEnv *env,jclass cls,jint id) {(void)cls;return (*env)->NewStringUTF(env,itemName((unsigned)id));}
JNIEXPORT jboolean JNICALL JNI_NAME(walkable)(JNIEnv *env,jclass cls,jbyteArray data) {
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return JNI_FALSE;
    int ok=walkerCanWalk(ee);(*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,JNI_ABORT);
    return ok?JNI_TRUE:JNI_FALSE;
}
JNIEXPORT jint JNICALL JNI_NAME(resume)(JNIEnv *env,jclass cls,jbyteArray data,jlong now,jint rate) {
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return 0;int out=walkerResume(ee,(uint32_t)now,(unsigned)rate);
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,0);return out;
}
JNIEXPORT jint JNICALL JNI_NAME(timer)(JNIEnv *env,jclass cls,jbyteArray data,jlong now,jint rate) {
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return 0;WalkerReport r;walkerAdvance(ee,(uint32_t)now,(unsigned)rate,&r);
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,0);return (jint)r.steps;
}
JNIEXPORT jint JNICALL JNI_NAME(physical)(JNIEnv *env,jclass cls,jbyteArray data,jlong now,jlong steps) {
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return 0;WalkerReport r={0};
    if(steps>0 && steps<=0xffffffffLL)walkerAddSteps(ee,(uint32_t)now,(uint32_t)steps,&r);
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,0);return (jint)r.steps;
}
JNIEXPORT jint JNICALL JNI_NAME(command)(JNIEnv *env,jclass cls,jbyteArray data,jint op,jint arg) {
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return -1;int out=-1;unsigned choice;
    uint32_t now=(uint32_t)(time(0)-WALKER_EPOCH_UNIX);
    switch(op) {
    case 0:out=(int)((walkerRandom(ee)>>1)&0x7fffffff);break;
    case 1:out=walkerSpend(ee,(unsigned)arg);break;
    case 2:out=walkerChooseItemEx(ee,&choice,arg!=0)?(int)choice:-1;break;
    case 3:out=walkerStorePokemon(ee,(unsigned)arg&255,(arg>>8)-1,now);break;
    case 4:out=walkerStoreItem(ee,(unsigned)arg&255,(arg>>8)-1,now);break;
    case 12:out=walkerChooseBattleItem(ee,&choice,arg!=0)?(int)choice:-1;break;
    case 11:out=(int)walkerFriendItemId(ee,(unsigned)arg);break;
    case 10:out=walkerStoreItem(ee,(unsigned)arg&65535,(arg>>16)-1,now);break;
    case 5:walkerClearEncounter(ee);out=1;break;
    case 6:out=(int)walkerPokemonId(ee,(unsigned)arg);break;
    case 7:out=(int)walkerItemId(ee,(unsigned)arg);break;
    case 8:out=(int)walkerRouteItemId(ee,(unsigned)arg);break;
    case 9:out=(int)walkerRoutePokemonId(ee,(unsigned)arg);break;
    }
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,0);return out;
}
JNIEXPORT jintArray JNICALL JNI_NAME(encounter)(JNIEnv *env,jclass cls,jbyteArray data) {
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return 0;WalkerEncounter e;jint v[3]={-1,0,0};
    if(walkerChooseEncounter(ee,&e)){v[0]=(jint)e.index;v[1]=(jint)e.species;v[2]=(jint)e.level;}
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,0);
    jintArray out=(*env)->NewIntArray(env,3);if(out)(*env)->SetIntArrayRegion(env,out,0,3,v);return out;
}
JNIEXPORT void JNICALL JNI_NAME(battle)(JNIEnv *env,jclass cls,jbyteArray data,jintArray state,jint action) {
    (void)cls;if(!state||(*env)->GetArrayLength(env,state)!=7)return;
    uint8_t *ee=bytes(env,data);if(!ee)return;jint v[7];(*env)->GetIntArrayRegion(env,state,0,7,v);
    WalkerBattle b={v[0],v[1],v[2],v[3],v[4],(unsigned)v[5],v[6]};walkerBattleAction(ee,&b,action);
    v[0]=b.playerHp;v[1]=b.enemyHp;v[2]=b.row;v[3]=b.finished;v[4]=b.caught;v[5]=(jint)b.encounter;v[6]=b.lastOutcome;
    (*env)->SetIntArrayRegion(env,state,0,7,v);(*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,0);
}

JNIEXPORT void JNICALL JNI_NAME(classicInput)(JNIEnv *env,jclass cls,jbyteArray data,jintArray state,jint button,jlong now,jlong ms,jboolean expanded){
    (void)cls;if(!state||(*env)->GetArrayLength(env,state)!=40)return;
    uint8_t *ee=bytes(env,data);if(!ee)return;jint values[40];(*env)->GetIntArrayRegion(env,state,0,40,values);
    walkerClassicInput(ee,values,button,(uint32_t)now,(uint32_t)ms,expanded);
    (*env)->SetIntArrayRegion(env,state,0,40,values);(*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,0);
}
JNIEXPORT jbyteArray JNICALL JNI_NAME(classicFrame)(JNIEnv *env,jclass cls,jbyteArray data,jintArray state,jlong ms){
    (void)cls;if(!state||(*env)->GetArrayLength(env,state)!=40)return 0;
    uint8_t *ee=bytes(env,data);if(!ee)return 0;jint values[40];uint8_t pixels[6144];
    (*env)->GetIntArrayRegion(env,state,0,40,values);walkerClassicFrame(ee,values,pixels,(uint32_t)ms);
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,JNI_ABORT);
    jbyteArray out=(*env)->NewByteArray(env,6144);if(out)(*env)->SetByteArrayRegion(env,out,0,6144,(jbyte *)pixels);return out;
}
JNIEXPORT jshortArray JNICALL JNI_NAME(classicSound)(JNIEnv *env,jclass cls,jbyteArray data,jint score){
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return 0;
    int16_t *pcm=malloc(220500*sizeof(*pcm));unsigned length=pcm?walkerClassicSound(ee,(unsigned)score,pcm,220500):0;
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,JNI_ABORT);
    jshortArray out=(*env)->NewShortArray(env,length);if(out&&length)(*env)->SetShortArrayRegion(env,out,0,length,(jshort *)pcm);free(pcm);return out;
}

JNIEXPORT jint JNICALL JNI_NAME(visit)(JNIEnv *env,jclass cls,jbyteArray data,jbyteArray peer,jlong now){
    (void)cls;uint8_t *a=bytes(env,data);if(!a)return 0;uint8_t *b=bytes(env,peer);if(!b){(*env)->ReleaseByteArrayElements(env,data,(jbyte *)a,JNI_ABORT);return 0;}
    int result=walkerVisit(a,b,(uint32_t)now);(*env)->ReleaseByteArrayElements(env,peer,(jbyte *)b,JNI_ABORT);(*env)->ReleaseByteArrayElements(env,data,(jbyte *)a,0);return result;
}

JNIEXPORT jintArray JNICALL JNI_NAME(menuIcon)(JNIEnv *env,jclass cls,jint kind){
    (void)cls;uint16_t rows[16];jint values[16];walkerMenuIcon((unsigned)kind,rows);for(int n=0;n<16;n++)values[n]=rows[n];
    jintArray out=(*env)->NewIntArray(env,16);if(out)(*env)->SetIntArrayRegion(env,out,0,16,values);return out;
}

JNIEXPORT jbyteArray JNICALL JNI_NAME(peerExport)(JNIEnv *env,jclass cls,jbyteArray data){
    (void)cls;uint8_t *ee=bytes(env,data);if(!ee)return 0;uint8_t packet[1688];int ok=walkerPeerExport(ee,packet);
    (*env)->ReleaseByteArrayElements(env,data,(jbyte *)ee,JNI_ABORT);if(!ok)return 0;jbyteArray out=(*env)->NewByteArray(env,1688);if(out)(*env)->SetByteArrayRegion(env,out,0,1688,(jbyte *)packet);return out;
}
JNIEXPORT jbyteArray JNICALL JNI_NAME(peerImport)(JNIEnv *env,jclass cls,jbyteArray data){
    (void)cls;if(!data||(*env)->GetArrayLength(env,data)!=1688)return 0;uint8_t packet[1688];(*env)->GetByteArrayRegion(env,data,0,1688,(jbyte *)packet);
    uint8_t *ee=malloc(65536);int ok=ee&&walkerPeerImport(packet,1688,ee);jbyteArray out=ok?(*env)->NewByteArray(env,65536):0;if(out)(*env)->SetByteArrayRegion(env,out,0,65536,(jbyte *)ee);free(ee);return out;
}
JNIEXPORT jintArray JNICALL JNI_NAME(friendlyStart)(JNIEnv *env,jclass cls,jbyteArray a,jbyteArray b,jint seed){
    (void)cls;uint8_t *x=bytes(env,a);if(!x)return 0;uint8_t *y=bytes(env,b);if(!y){(*env)->ReleaseByteArrayElements(env,a,(jbyte *)x,JNI_ABORT);return 0;}
    jint state[24];walkerFriendlyStart(x,y,state,(uint32_t)seed);(*env)->ReleaseByteArrayElements(env,b,(jbyte *)y,JNI_ABORT);(*env)->ReleaseByteArrayElements(env,a,(jbyte *)x,JNI_ABORT);
    jintArray out=(*env)->NewIntArray(env,24);if(out)(*env)->SetIntArrayRegion(env,out,0,24,state);return out;
}
JNIEXPORT jboolean JNICALL JNI_NAME(friendlyTurn)(JNIEnv *env,jclass cls,jbyteArray a,jbyteArray b,jintArray board,jint ca,jint cb){
    (void)cls;if(!board||(*env)->GetArrayLength(env,board)!=24)return 0;uint8_t *x=bytes(env,a);if(!x)return 0;uint8_t *y=bytes(env,b);if(!y){(*env)->ReleaseByteArrayElements(env,a,(jbyte *)x,JNI_ABORT);return 0;}
    jint state[24];(*env)->GetIntArrayRegion(env,board,0,24,state);int ok=walkerFriendlyTurn(x,y,state,(unsigned)ca,(unsigned)cb);if(ok)(*env)->SetIntArrayRegion(env,board,0,24,state);
    (*env)->ReleaseByteArrayElements(env,b,(jbyte *)y,JNI_ABORT);(*env)->ReleaseByteArrayElements(env,a,(jbyte *)x,JNI_ABORT);return ok;
}
JNIEXPORT jbyteArray JNICALL JNI_NAME(friendlyFrame)(JNIEnv *env,jclass cls,jbyteArray a,jbyteArray b,jintArray board,jint side,jint selected,jlong ms){
    (void)cls;if(!board||(*env)->GetArrayLength(env,board)!=24)return 0;uint8_t *x=bytes(env,a);if(!x)return 0;uint8_t *y=bytes(env,b);if(!y){(*env)->ReleaseByteArrayElements(env,a,(jbyte *)x,JNI_ABORT);return 0;}
    jint state[24];uint8_t pixels[6144];(*env)->GetIntArrayRegion(env,board,0,24,state);walkerFriendlyFrame(x,y,state,(unsigned)side,(unsigned)selected,pixels,(uint32_t)ms);
    (*env)->ReleaseByteArrayElements(env,b,(jbyte *)y,JNI_ABORT);(*env)->ReleaseByteArrayElements(env,a,(jbyte *)x,JNI_ABORT);
    jbyteArray out=(*env)->NewByteArray(env,6144);if(out)(*env)->SetByteArrayRegion(env,out,0,6144,(jbyte *)pixels);return out;
}
JNIEXPORT jstring JNICALL JNI_NAME(moveName)(JNIEnv *env,jclass cls,jint id){(void)cls;return (*env)->NewStringUTF(env,walkerMoveName((unsigned)id));}
