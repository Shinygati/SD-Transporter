/* Synthetic core and flat-ABI checks; no retail walker or ROM required. */
#define main portable_core_tests
#include "../../3ds/tests/test_walkerfile.c"
#undef main
#include "../Walker/Bridge.h"
#include <assert.h>
int main(void){
    assert(portable_core_tests()==0);
    int64_t info[14];char text[128];
    makeStrolling(1,847775796);
    assert(pw_info(ee,info) && info[1]==1 && info[2]==1000);
    pw_text(ee,0,text);assert(text[0]=='K');
    assert(!strcmp(pw_species(250),"Ho-Oh"));
    assert(!strcmp(pw_species(21),"Spearow"));
    assert(!strcmp(pw_item(21),"Awakening"));
    assert(!strcmp(pw_item(22),"Parlyz Heal"));
    assert(!strcmp(pw_item(536),"Enigma Stone"));
    ee[0xed+12]=0x39;ee[0xed+13]=0x30;seal(0xed,0x1ed,104);
    char trainerLabel[128];pw_text(ee,3,trainerLabel);assert(strstr(trainerLabel,"12345"));
    char trainerKey[128],sameKey[128];pw_text(ee,4,trainerKey);assert(strlen(trainerKey)==36);
    ee[0xed+14]^=1;seal(0xed,0x1ed,104);pw_text(ee,4,sameKey);assert(!strcmp(trainerKey,sameKey)); /* SID is not public Trainer ID. */
    ee[0xed+12]^=1;seal(0xed,0x1ed,104);pw_text(ee,4,sameKey);assert(strcmp(trainerKey,sameKey));
    ee[0xed+12]^=1;ee[0xed+0x48]^=1;seal(0xed,0x1ed,104);pw_text(ee,4,sameKey);assert(strcmp(trainerKey,sameKey));
    ee[0xed+0x48]^=1;seal(0xed,0x1ed,104);

    assert(!strcmp(pw_item(0xffff),"Empty"));
    assert(!strcmp(pw_item(537),"Unknown item"));
    ee[WALKER_ROUTE+0x8c]=21;ee[WALKER_ROUTE+0x8d]=0;
    assert(pw_command(ee,8,0,847775796)==21);
    assert(pw_command(ee,8,10,847775796)==0);
    assert(pw_crc((const unsigned char*)"123456789",9)==0xcbf43926u);
    pw_resume(ee,847775796,10);assert(pw_timer(ee,847775895,10)==9);
    assert(pw_timer(ee,847775895,10)==0);
    pw_steps(ee,847775896,100);assert(pw_info(ee,info) && info[2]==1109);
    assert(pw_command(ee,1,1,847775896)==1);
    assert(pw_command(ee,99,0,847775896)==-1);
    puts("iOS bridge: shared synthetic core tests, info/text/species ABI, CRC32, timer, physical steps and commands passed.");
    return 0;
}
