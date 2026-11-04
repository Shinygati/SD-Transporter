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
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "recovery_limit.h"
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
  if(ok)ok=(replaceFile ? replaceFile(temporary,p) : rename(temporary,p))==0;
  /* Verify the published name too: a successful rename alone is not a saved
   * return acknowledgement. Retain phone ownership on any verification error. */
  if(ok)ok=readExact(p,check,length) && !memcmp(check,data,length);
  free(check);
  return ok;
}
/* Keep immutable ownership-boundary snapshots. Later HGSS resets, new strolls
 * and fresh checkouts must not destroy the last copy of an earlier Pokemon.
 * These are recovery data, never automatically merged into a newer session. */
static int ensureDirectory(const char *p) {
  struct stat st;
  if (!stat(p, &st)) return (st.st_mode & S_IFMT) == S_IFDIR;
#ifdef _WIN32
  return _mkdir(p) == 0;
#else
  return mkdir(p, 0777) == 0;
#endif
}
static const char *fileName(const char *p) {
  const char *slash=strrchr(p,'/'), *backslash=strrchr(p,'\\');
  if(backslash && (!slash || backslash>slash))slash=backslash;
  return slash?slash+1:p;
}
static int backupDirectory(char directory[600], const char *p, int create) {
  unsigned parent=(unsigned)(fileName(p)-p);
  if(parent+sizeof("transfers/backups")>=600)return 0;
  memcpy(directory,p,parent);strcpy(directory+parent,"transfers");
  if(create && !ensureDirectory(directory))return 0;
  strcat(directory,"/backups");
  return !create || ensureDirectory(directory);
}
static int archive(const char *p, const uint8_t token[16], const char *kind,
                   const uint8_t *data) {
  char directory[600], name[600], text[33];
  if(!backupDirectory(directory,p,1))return 0;
  leaseTokenText(token,text);
  int n=snprintf(name,sizeof(name),"%s/%s.%s-%s-%08lx.bak",directory,
      fileName(p),kind,text,(unsigned long)walkerCrc(data,WALKER_FILE_SIZE));
  if(n<0 || n>=(int)sizeof(name))return 0;
  struct stat st;
  uint8_t *check=malloc(WALKER_FILE_SIZE);
  if(!check)return 0;
  int status=stat(name,&st), exists=status==0;
  if(status && errno!=ENOENT){free(check);return 0;}
  int ok=exists ? readExact(name,check,WALKER_FILE_SIZE) &&
      !memcmp(check,data,WALKER_FILE_SIZE) : publish(name,data,WALKER_FILE_SIZE);
  if(ok)ok=readExact(name,check,WALKER_FILE_SIZE) && !memcmp(check,data,WALKER_FILE_SIZE);
  free(check);return ok && walkerPruneRecovery(p,name);
}
/* A nonempty console file can be a new stroll copied in while a lease remains.
 * Never silently replace it. The only permitted nonempty return target is an
 * identical already-published return (acknowledgement/crash retry). */
static int sameFile(const char *p, const uint8_t *data) {
  uint8_t *check=malloc(WALKER_FILE_SIZE);
  if(!check)return 0;
  int ok=readExact(p,check,WALKER_FILE_SIZE) && !memcmp(check,data,WALKER_FILE_SIZE);
  free(check);return ok;
}
static int emptyOrSame(const char *p, const uint8_t *data) {
  struct stat st;
  if(stat(p,&st))return errno==ENOENT;
  return st.st_size==0 || (st.st_size==WALKER_FILE_SIZE && sameFile(p,data));
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
    int record=receipt(p,m+8,0,0,0);
    if(record<0)return -1;
    out->revoked = m[40] != 0 || record == 2;
  }
  return 1;
}
static int sessionReceiptPath(char name[600],const char *p,const uint8_t token[16],int create) {
  char directory[600],text[33];
  if(!backupDirectory(directory,p,create))return 0;
  leaseTokenText(token,text);
  int n=snprintf(name,600,"%s/%s.session-%s.receipt",directory,fileName(p),text);
  return n>0 && n<600;
}
static int validReceipt(const uint8_t r[40],const uint8_t token[16]) {
  return !memcmp(r,"PWRECEIP",8) && !memcmp(r+8,token,16) &&
      get32(r+36)==walkerCrc(r,36) && (get32(r+28)==1 || get32(r+28)==2);
}
/* Session receipts survive later handoffs. A stale phone never regains write
 * authority merely because another session has replaced the latest receipt. */
static int readRecordedReceipt(const char *p,const uint8_t token[16],uint8_t r[40]) {
  char name[600];struct stat st;
  if(!sessionReceiptPath(name,p,token,0))return -1;
  if(!stat(name,&st))return readExact(name,r,40) && validReceipt(r,token)?1:-1;
  if(errno!=ENOENT)return -1;
  if(!path(name,p,".returned"))return -1;
  if(readExact(name,r,40) && validReceipt(r,token))return 1;
  return 0;
}
static int receipt(const char *p, const uint8_t token[16], uint32_t crc,
                   unsigned kind, int write) {
  char name[600];uint8_t r[40]={0};
  if(write) {
    memcpy(r,"PWRECEIP",8);memcpy(r+8,token,16);put32(r+24,crc);put32(r+28,kind);
    put32(r+36,walkerCrc(r,36));
    if(!sessionReceiptPath(name,p,token,1))return 0;
    struct stat st;uint8_t check[40];
    if(!stat(name,&st)) {
      if(!readExact(name,check,40))return 0;
      if(memcmp(check,r,40)) {
        /* Explicit force recovery may revoke a completed-but-not-cleaned return;
         * a revocation is permanent and can never be downgraded. */
        if(kind!=2 || !validReceipt(check,token) || get32(check+28)!=1 || !publish(name,r,40))return 0;
      }
    } else {
      if(errno!=ENOENT || !publish(name,r,40))return 0;
    }
    if(!readExact(name,check,40) || memcmp(check,r,40))return 0;
    return path(name,p,".returned") && publish(name,r,40);
  }
  int found=readRecordedReceipt(p,token,r);
  if(found<=0)return found;
  if(get32(r+28)==2)return 2;
  return get32(r+24)==crc?1:0;
}
static const uint8_t *leaseIdentity(const uint8_t *ee) {
  for(unsigned copy=0;copy<2;copy++) {
    const uint8_t *p=ee+(copy?0x1ed:0xed);uint8_t sum=1;
    for(unsigned n=0;n<104;n++)sum+=p[n];
    if(sum==p[104])return p;
  }
  return 0;
}
static int compatible(const uint8_t *a, const uint8_t *b) {
  WalkerInfo x, y;
  if (!walkerRead(a,&x) || !walkerRead(b,&y) || !walkerCanWalk(a) ||
      !walkerCanWalk(b) || x.walkStart!=y.walkStart || y.totalSteps<x.totalSteps)
    return 0;
  const uint8_t *originalId=leaseIdentity(a),*returnedId=leaseIdentity(b);
  if(!originalId || !returnedId)return 0;
  if (x.hasPokemon)
    return y.hasPokemon && x.species==y.species && !memcmp(originalId,returnedId,104);
  if (!y.hasPokemon) return !memcmp(originalId,returnedId,104);
  /* The sole permitted identity transition is the original empty-walker join.
   * Preserve trainer/device identity and course; verify the designated reward. */
  uint8_t identity[104],pokemon[16];
  memcpy(identity,originalId,sizeof(identity));
  identity[0x5b]|=6;
  memcpy(identity+4,identity,4);memcpy(identity+10,identity+8,2);
  memcpy(pokemon,a+WALKER_ROUTE+0x52+32,sizeof(pokemon));pokemon[13]&=0x7f;
  return !memcmp(identity,returnedId,sizeof(identity)) &&
         !memcmp(pokemon,b+WALKER_ROUTE,sizeof(pokemon)) &&
         !memcmp(a+WALKER_ROUTE+0x28,b+WALKER_ROUTE+0x28,0xbe - 0x28);
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
                  "This Pokémon belongs to a different phone session.");
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
      if (!emptyOrSame(p, snapshot))
        return fail(error,size,"Console walker changed since checkout. No data was replaced; use recovery.");
      if (!writeExact(p, "", 0) || stat(p, &st) || st.st_size != 0)
        return fail(
            error, size,
            "Could not finish the console lock. Transfer was not sent.");
    }
    if (out)
      *out = existing;
    return 1; /* retry of the same handoff */
  }
  uint8_t previousReceipt[40];
  if (readRecordedReceipt(p,token,previousReceipt)!=0)
    return fail(error,size,"This session is finished, revoked, or damaged. Start a fresh handoff.");
  if (!readExact(p, snapshot, WALKER_FILE_SIZE) ||
      !walkerRead(snapshot, &info) || !walkerCanWalk(snapshot))
    return fail(error, size, "A registered walker with valid companion or joining-course data is required.");
  uint32_t crc = walkerCrc(snapshot, WALKER_FILE_SIZE);
  if (!archive(p, token, "checkout", snapshot))
    return fail(error, size, "Could not preserve walker recovery history. Nothing was sent.");
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
static int validFinalWalker(const char *p) {
  uint8_t *data=malloc(WALKER_FILE_SIZE);WalkerInfo info;if(!data)return 0;
  int ok=readExact(p,data,WALKER_FILE_SIZE)&&walkerRead(data,&info)&&info.setUp;free(data);return ok;
}
/* A separately named receipt records an explicitly confirmed phone recovery.
 * It does not remove the revoked session receipt or permit another checkout. */
static int recoveredReceipt(const char *p,const uint8_t token[16],uint32_t crc,int write) {
  char name[600],regular[600];uint8_t record[40]={0};struct stat st;
  if(!sessionReceiptPath(regular,p,token,write))return -1;
  if(snprintf(name,sizeof(name),"%s.phone-recovery",regular)>=(int)sizeof(name))return -1;
  if(write) {
    memcpy(record,"PWRECEIP",8);memcpy(record+8,token,16);put32(record+24,crc);put32(record+28,1);
    put32(record+36,walkerCrc(record,36));
    if(stat(name,&st) && errno==ENOENT) {if(!publish(name,record,sizeof(record)))return -1;}
  }
  if(stat(name,&st))return errno==ENOENT?0:-1;
  return readExact(name,record,sizeof(record)) && validReceipt(record,token) && get32(record+28)==1 && get32(record+24)==crc?1:-1;
}
int leaseRecoverPhone(const char *p,const uint8_t token[16],const uint8_t *incoming,
                      const uint8_t *baseline,char *error,unsigned size) {
  /* Only the separate, explicitly opened console recovery listener calls this.
   * The original backup is used as evidence, never loaded as an active walker. */
  if(leaseInspect(p,0))return fail(error,size,"A phone session already owns this path. Use its normal return screen.");
  uint32_t crc=walkerCrc(incoming,WALKER_FILE_SIZE);
  int done=recoveredReceipt(p,token,crc,0);
  if(done<0)return fail(error,size,"Phone recovery receipt differs or is damaged. Existing files kept.");
  if(done==1)return validFinalWalker(p)?2:fail(error,size,"Final walker missing; recovery is not confirmed.");
  if(baseline) {
    if(!compatible(baseline,incoming))return fail(error,size,"Phone does not match this backup's trainer, stroll and companion.");
  } else {
    WalkerInfo info;char trainer[37];
    if(!walkerRead(incoming,&info)||!info.registered||!walkerCanWalk(incoming)||!walkerTrainerKey(incoming,trainer))
      return fail(error,size,"The phone copy is not a valid registered walker.");
  }
  if(!emptyOrSame(p,incoming))return fail(error,size,"An active console walker exists. Phone recovery will not overwrite it.");
  if(!archive(p,token,"return",incoming) || !publish(p,incoming,WALKER_FILE_SIZE))
    return fail(error,size,"Could not verify the recovered walker. Keep the phone copy and retry.");
  if(recoveredReceipt(p,token,crc,1)!=1 || !sameFile(p,incoming))
    return fail(error,size,"Recovery confirmation failed. Keep the phone copy and retry.");
  return 1;
}
int leaseReturn(const char *p, const uint8_t token[16], const uint8_t *incoming,
                char *error, unsigned size) {
  uint32_t crc = walkerCrc(incoming, WALKER_FILE_SIZE);
  int recovered=recoveredReceipt(p,token,crc,0);
  if(recovered<0)return fail(error,size,"Phone recovery receipt differs or is damaged.");
  if(recovered==1)return validFinalWalker(p)?2:fail(error,size,"Final walker missing; return is not confirmed.");
  WalkerLease loan;
  int locked = leaseInspect(p, &loan), ack = receipt(p, token, crc, 0, 0);
  if (ack == 2)
    return fail(
        error, size,
        "This phone session was force-unlocked and can no longer sync.");
  if (ack == 1 && (locked != 1 || memcmp(token,loan.token,16))) {
    if(!validFinalWalker(p))return fail(error,size,"The final .walker is missing or invalid. Return is not confirmed; keep the phone copy for recovery.");
    return 2;
  } /* acknowledge retry, never overwrite current console progress */
  if (locked != 1 || loan.revoked || memcmp(token, loan.token, 16))
    return fail(
        error, size,
        "This is not the current phone session. Send the Pokémon again.");
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
  if (!archive(p, token, "return", incoming))
    return fail(error, size, "Could not preserve returned walker history. The phone keeps ownership.");
  if (!emptyOrSame(p, incoming))
    return fail(error, size, "A different walker is on the console. Return refused; both copies are kept.");
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
                "Pokémon saved. Retry sync to finish unlocking the console.");
  if(!sameFile(p,incoming))return fail(error,size,"Final .walker verification failed after lock cleanup. The phone keeps its copy.");
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
  if (!archive(p, loan.token, "recovery", original)) {
    free(original);
    return fail(error,size,"Could not preserve recovery history. No walker was replaced.");
  }
  struct stat current;
  int currentStatus=stat(p,&current);
  if (!currentStatus && current.st_size) {
    uint8_t *other=malloc(WALKER_FILE_SIZE);
    ok=other && readExact(p,other,WALKER_FILE_SIZE) && archive(p,loan.token,"conflict",other);
    free(other);
    if(!ok){free(original);return fail(error,size,"Could not preserve the current console walker. Recovery stopped.");}
  } else if (currentStatus && errno!=ENOENT) {
    free(original);return fail(error,size,"Cannot inspect the current console walker. Recovery stopped.");
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

int leaseMatchesTrainer(const char *p,const char *key) {
    if(strlen(key)!=36)return 0;
    char source[600],actual[37];int locked=leaseInspect(p,0);if(locked<0)return 0;
    if(!path(source,p,locked?".phone":""))return 0;
    uint8_t *data=malloc(WALKER_FILE_SIZE);if(!data)return 0;
    int ok=readExact(source,data,WALKER_FILE_SIZE)&&walkerTrainerKey(data,actual)&&!strcmp(key,actual);free(data);return ok;
}
