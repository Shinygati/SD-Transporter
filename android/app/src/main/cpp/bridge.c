/* JNI bridge to the same GPL-3.0-or-later core used by the consoles. */
#include <jni.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include "walkerfile.h"
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
    case 2:out=walkerChooseItem(ee,&choice)?(int)choice:-1;break;
    case 3:out=walkerStorePokemon(ee,(unsigned)arg&255,(arg>>8)-1,now);break;
    case 4:out=walkerStoreItem(ee,(unsigned)arg&255,(arg>>8)-1,now);break;
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
