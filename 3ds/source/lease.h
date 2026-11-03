#ifndef WALKER_LEASE_H
#define WALKER_LEASE_H
#include "walkerfile.h"
#include <stdint.h>
typedef struct {
    uint8_t token[16];
    uint32_t baseCrc, walkStart;
    int revoked;
} WalkerLease;
typedef int (*WalkerReplace)(const char *, const char *);
void leaseSetReplace(WalkerReplace);
/* A positive result means phone-owned; -1 means damaged lock metadata. */
int leaseInspect(const char *, WalkerLease *);
int leaseSessionRevoked(const char *, const uint8_t token[16]);
int leaseCheckout(const char *, const uint8_t token[16], uint8_t *, WalkerLease *, char *,
                  unsigned);
int leaseReturn(const char *, const uint8_t token[16], const uint8_t *, char *, unsigned);
int leaseForceUnlock(const char *, char *, unsigned);
void leaseTokenText(const uint8_t *, char[33]);
int leaseTokenParse(const char *, uint8_t[16]);
int leaseMatchesTrainer(const char *path,const char *key);
#endif
