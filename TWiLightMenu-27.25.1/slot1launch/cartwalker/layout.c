#include <stddef.h>
#include "cartwalker.h"
#include "walker_emu.h"
const unsigned cart_layout[]={sizeof(CartStage),offsetof(CartStage,imageCrc),offsetof(CartStage,storage),
 sizeof(CartStorage),offsetof(CartStorage,cid),offsetof(CartStorage,error),
 offsetof(WalkerControl,state),offsetof(WalkerControl,respLen),offsetof(WalkerControl,resp),
 offsetof(WalkerControl,eeprom),offsetof(WalkerControl,dirty),sizeof(WalkerControl)};
