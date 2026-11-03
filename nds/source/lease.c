/* Shared, crash-recoverable console/phone checkout. GPL-3.0-or-later.
 * A phone-owned .walker is deliberately zero-length: existing bootstrap
 * releases already refuse it, so HGSS cannot use the checked-out copy.
 * The complete snapshot is retained at .walker.phone until return/recovery.
 */
#include "lease.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
static WalkerReplace replaceFile;
static int receipt(const char *, const uint8_t[16], uint32_t, unsigned, int);
void leaseSetReplace(WalkerReplace fn) { replaceFile = fn; }
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(n >> (8 * i));
}
static int fail(char *error, unsigned size, const char *why) {
  snprintf(error, size, "%s", why);
  return 0;
}
static int path(char *out, const char *base, const char *suffix) {
  int n = snprintf(out, 600, "%s%s", base, suffix);
  return n > 0 && n < 600;
}
static int readExact(const char *p, void *out, unsigned length) {
  FILE *f = fopen(p, "rb");
  if (!f)
    return 0;
  int ok = fread(out, 1, length, f) == length && fgetc(f) == EOF && !ferror(f);
  if (fclose(f))
    ok = 0;
  return ok;
}
static int writeExact(const char *p, const void *data, unsigned length) {
  FILE *f = fopen(p, "wb");
  if (!f)
    return 0;
  int ok = fwrite(data, 1, length, f) == length;
  if (fflush(f))
    ok = 0;
#ifndef _WIN32
  if (ok && fsync(fileno(f)))
    ok = 0;
#endif
  if (fclose(f))
    ok = 0;
  return ok;
}
static int publish(const char *p, const void *data, unsigned length) {
  char temporary[600];
  if (!path(temporary, p, ".tmp"))
    return 0;
  uint8_t *check = malloc(length ? length : 1);
  if (!check)
    return 0;
  int ok = writeExact(temporary, data, length) &&
           readExact(temporary, check, length) && !memcmp(check, data, length);
  free(check);
  return ok &&
         (replaceFile ? replaceFile(temporary, p) : rename(temporary, p)) == 0;
}
static int readMeta(const char *p, uint8_t meta[64]) {
  char name[600];
  if (!path(name, p, ".lease"))
    return 0;
  return readExact(name, meta, 64) && !memcmp(meta, "PWLEASE1", 8) &&
         get32(meta + 60) == walkerCrc(meta, 60);
}
int leaseInspect(const char *p, WalkerLease *out) {
  char name[600];
  uint8_t m[64];
  struct stat st;
  if (!path(name, p, ".lease"))
    return -1;
  if (stat(name, &st))
    return errno == ENOENT ? 0 : -1;
  if (!readMeta(p, m))
    return -1;
  if (out) {
    memcpy(out->token, m + 8, 16);
    out->baseCrc = get32(m + 24);
    out->walkStart = get32(m + 28);
    out->revoked = m[40] != 0 || receipt(p, m + 8, 0, 0, 0) == 2;
  }
  return 1;
}
static int receipt(const char *p, const uint8_t token[16], uint32_t crc,
                   unsigned kind, int write) {
  char name[600];
  uint8_t r[40] = {0};
  if (!path(name, p, ".returned"))
    return 0;
  if (write) {
    memcpy(r, "PWRECEIP", 8);
    memcpy(r + 8, token, 16);
    put32(r + 24, crc);
    put32(r + 28, kind);
    put32(r + 36, walkerCrc(r, 36));
    return publish(name, r, 40);
  }
  if (!readExact(name, r, 40) || memcmp(r, "PWRECEIP", 8) ||
      memcmp(r + 8, token, 16) || get32(r + 36) != walkerCrc(r, 36))
    return 0;
  if (get32(r + 28) == 2)
    return 2; /* revoked */
  return get32(r + 24) == crc ? 1 : 0;
}
static int compatible(const uint8_t *a, const uint8_t *b) {
  WalkerInfo x, y;
  return walkerRead(a, &x) && walkerRead(b, &y) && y.registered &&
         y.hasPokemon && x.walkStart == y.walkStart &&
         !memcmp(a + 0xed, b + 0xed, 104) && y.totalSteps >= x.totalSteps;
}
int leaseSessionRevoked(const char *p, const uint8_t token[16]) {
  return receipt(p, token, 0, 0, 0) == 2;
}
int leaseCheckout(const char *p, const uint8_t token[16], uint8_t *snapshot,
                  WalkerLease *out, char *error, unsigned size) {
  char name[600];
  uint8_t m[64] = {0};
  WalkerInfo info;
  WalkerLease existing;
  int locked = leaseInspect(p, &existing);
  if (locked < 0)
    return fail(error, size,
                "Lock metadata is damaged. Use recovery; do not overwrite it.");
  if (locked) {
    if (memcmp(existing.token, token, 16))
      return fail(error, size,
                  "This walker belongs to a different phone session.");
    if (existing.revoked)
      return fail(error, size,
                  "Recovery is pending. Finish force-unlock first.");
    path(name, p, ".phone");
    if (!readExact(name, snapshot, WALKER_FILE_SIZE) ||
        walkerCrc(snapshot, WALKER_FILE_SIZE) != existing.baseCrc)
      return fail(error, size,
                  "The local phone snapshot could not be verified.");
    struct stat st;
    if (stat(p, &st) || st.st_size != 0) {
      if (!writeExact(p, "", 0) || stat(p, &st) || st.st_size != 0)
        return fail(
            error, size,
            "Could not finish the console lock. Transfer was not sent.");
    }
    if (out)
      *out = existing;
    return 1; /* retry of the same handoff */
  }
  if (leaseSessionRevoked(p, token))
    return fail(error, size, "This session was revoked. Start a fresh handoff.");
  if (!readExact(p, snapshot, WALKER_FILE_SIZE) ||
      !walkerRead(snapshot, &info) || !info.registered || !info.hasPokemon)
    return fail(error, size, "Only a walker containing a Pokemon can be sent.");
  uint32_t crc = walkerCrc(snapshot, WALKER_FILE_SIZE);
  path(name, p, ".phone");
  if (!publish(name, snapshot, WALKER_FILE_SIZE))
    return fail(error, size, "Could not save the console recovery snapshot.");
  memcpy(m, "PWLEASE1", 8);
  memcpy(m + 8, token, 16);
  put32(m + 24, crc);
  put32(m + 28, info.walkStart);
  put32(m + 32, walkerCrc(snapshot + 0xed, 104));
  put32(m + 60, walkerCrc(m, 60));
  path(name, p, ".lease");
  if (!publish(name, m, 64))
    return fail(error, size, "Could not publish the phone lock.");
  /* Existing bootstrap checks exact 64 KiB length and rejects this placeholder.
   */
  if (!writeExact(p, "", 0))
    return fail(error, size,
                "Could not lock the console file. Transfer was not sent.");
  struct stat st;
  if (stat(p, &st) || st.st_size != 0)
    return fail(error, size, "Console lock verification failed.");
  if (out) {
    memcpy(out->token, token, 16);
    out->baseCrc = crc;
    out->walkStart = info.walkStart;
    out->revoked = 0;
  }
  return 1;
}
static void finish(const char *p) {
  char name[600];
  path(name, p, ".lease");
  if (remove(name) && errno != ENOENT)
    return;
  path(name, p, ".phone");
  remove(name);
}
int leaseReturn(const char *p, const uint8_t token[16], const uint8_t *incoming,
                char *error, unsigned size) {
  uint32_t crc = walkerCrc(incoming, WALKER_FILE_SIZE);
  WalkerLease loan;
  int locked = leaseInspect(p, &loan), ack = receipt(p, token, crc, 0, 0);
  if (ack == 2)
    return fail(
        error, size,
        "This phone session was force-unlocked and can no longer sync.");
  if (!locked && ack == 1)
    return 2; /* acknowledge retry, never overwrite current console progress */
  if (locked != 1 || loan.revoked || memcmp(token, loan.token, 16))
    return fail(
        error, size,
        "This is not the current phone session. Send the walker again.");
  char name[600];
  uint8_t *original = malloc(WALKER_FILE_SIZE);
  if (!original)
    return fail(error, size, "Not enough memory.");
  path(name, p, ".phone");
  int ok = readExact(name, original, WALKER_FILE_SIZE) &&
           walkerCrc(original, WALKER_FILE_SIZE) == loan.baseCrc &&
           compatible(original, incoming);
  free(original);
  if (!ok)
    return fail(error, size,
                "Return has a different walker, stroll, or invalid progress.");
  if (!publish(p, incoming, WALKER_FILE_SIZE))
    return fail(
        error, size,
        "Could not save the returned walker; the phone remains checked out.");
  if (!receipt(p, token, crc, 1, 1))
    return fail(
        error, size,
        "Return saved but acknowledgement failed. Retry before closing sync.");
  finish(p);
  if (leaseInspect(p, 0))
    return fail(error, size,
                "Return saved; could not remove the lock. Retry sync.");
  return 1;
}
int leaseForceUnlock(const char *p, char *error, unsigned size) {
  WalkerLease loan;
  if (leaseInspect(p, &loan) != 1)
    return fail(error, size, "A valid local recovery record is required.");
  char name[600];
  uint8_t *original = malloc(WALKER_FILE_SIZE);
  WalkerInfo info;
  if (!original)
    return fail(error, size, "Not enough memory.");
  path(name, p, ".phone");
  int ok = readExact(name, original, WALKER_FILE_SIZE) &&
           walkerCrc(original, WALKER_FILE_SIZE) == loan.baseCrc;
  if (!ok) {
    path(name, p, ".bak");
    ok = readExact(name, original, WALKER_FILE_SIZE) &&
         walkerCrc(original, WALKER_FILE_SIZE) == loan.baseCrc;
  }
  if (!ok || !walkerRead(original, &info)) {
    free(original);
    return fail(error, size, "No intact matching console backup was found.");
  }
  /* Revoke first. A stale phone must not regain authority after recovery. */
  if (!receipt(p, loan.token, loan.baseCrc, 2, 1)) {
    free(original);
    return fail(error, size, "Could not revoke the old phone session.");
  }
  uint8_t m[64];
  if (!readMeta(p, m)) {
    free(original);
    return fail(error, size, "Recovery metadata is damaged.");
  }
  m[40] = 1;
  put32(m + 60, walkerCrc(m, 60));
  path(name, p, ".lease");
  if (!publish(name, m, 64)) {
    free(original);
    return fail(error, size,
                "Could not mark the session revoked. Retry recovery.");
  }
  ok = publish(p, original, WALKER_FILE_SIZE);
  free(original);
  if (!ok)
    return fail(
        error, size,
        "Phone session revoked; retry to restore the console snapshot.");
  finish(p);
  return leaseInspect(p, 0) == 0
             ? 1
             : fail(error, size,
                    "Snapshot restored; lock cleanup needs retry.");
}
void leaseTokenText(const uint8_t *token, char out[33]) {
  static const char hex[] = "0123456789abcdef";
  for (unsigned i = 0; i < 16; i++) {
    out[i * 2] = hex[token[i] >> 4];
    out[i * 2 + 1] = hex[token[i] & 15];
  }
  out[32] = 0;
}
int leaseTokenParse(const char *text, uint8_t token[16]) {
  if (strlen(text) != 32)
    return 0;
  for (unsigned i = 0; i < 16; i++) {
    unsigned a, b;
    char x = text[i * 2], y = text[i * 2 + 1];
    a = x >= '0' && x <= '9'   ? x - '0'
        : x >= 'a' && x <= 'f' ? x - 'a' + 10
                               : 99;
    b = y >= '0' && y <= '9'   ? y - '0'
        : y >= 'a' && y <= 'f' ? y - 'a' + 10
                               : 99;
    if (a > 15 || b > 15)
      return 0;
    token[i] = (uint8_t)(a * 16 + b);
  }
  return 1;
}
