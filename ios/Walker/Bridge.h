/* GPL-3.0-or-later. Flat bridge to the shared console/Android game core. */
#ifndef PW_IOS_BRIDGE_H
#define PW_IOS_BRIDGE_H
#include <stdint.h>
#include "walkerclassic.h"
int pw_info(const uint8_t *data, int64_t values[14]);
int pw_visit(uint8_t *,const uint8_t *,uint32_t);
int pw_walkable(const uint8_t *data);
const char *pw_species(unsigned species);
const char *pw_item(unsigned item);
void pw_text(const uint8_t *data, int kind, char output[128]);
int pw_resume(uint8_t *data, uint32_t now, unsigned rate);
int pw_timer(uint8_t *data, uint32_t now, unsigned rate);
int pw_steps(uint8_t *data, uint32_t now, uint32_t steps);
int pw_command(uint8_t *data, int op, int arg, uint32_t now);
void pw_encounter(uint8_t *data, int values[3]);
void pw_battle(uint8_t *data, int values[7], int action);
uint32_t pw_crc(const uint8_t *data, unsigned length);
/* Repair mirrored records in place, then 1 if the walker is safe to save. */
int pw_check(uint8_t *data);
#endif
