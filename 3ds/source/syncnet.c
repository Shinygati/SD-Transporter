/* Bounded local-WiFi HTTP handoff server. GPL-3.0-or-later. */
#include "syncnet.h"
#include "lease.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef NETWORK_HOST_TEST
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET NetSocket;
#define NO_SOCKET INVALID_SOCKET
#define netClose closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int NetSocket;
#define NO_SOCKET (-1)
#define netClose close
#ifdef __NDS__
#include <dswifi9.h>
#include <nds.h>
#else
#include <3ds.h>
#include <malloc.h>
static void *socMemory;
#endif
#endif
static NetSocket listener = NO_SOCKET, peer = NO_SOCKET;
static char selected[600], address[48], statusText[160];
static unsigned char request[WALKER_FILE_SIZE + 2048 + 1],
    snapshot[WALKER_FILE_SIZE];
static unsigned received, headLength, bodyLength;
static char response[640], returnReceipt[96];
static const unsigned char *responseBody;
static unsigned responseLength, sentHeader, sentBody;
static int networkState, responding, draining;
static time_t drainStarted;
#define SYNC_CHUNK 4096u
static time_t activity;
static SyncBeforeCheckout beforeCheckout;
static SyncEvent onEvent;
static int nonblock(NetSocket s) {
#ifdef NETWORK_HOST_TEST
  u_long one = 1;
  return ioctlsocket(s, FIONBIO, &one);
#else
  int one = 1;
  return ioctl(s, FIONBIO, &one);
#endif
}
static int wouldBlock(void) {
#ifdef NETWORK_HOST_TEST
  int e = WSAGetLastError();
  return e == WSAEWOULDBLOCK;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}
static int socketError(void) {
#ifdef NETWORK_HOST_TEST
  return WSAGetLastError();
#else
  return errno;
#endif
}
static void transferError(const char *stage, int code) {
  snprintf(statusText, sizeof(statusText),
           "%s error %d (%u/%u bytes). Retry Sync.", stage, code, sentBody,
           responseLength);
#ifdef NETWORK_HOST_TEST
  const char *logPath = "work/sync-network.log";
#elif defined(__NDS__)
  const char *logPath = "/data/pokewalker/sync-error.log";
#else
  const char *logPath = "sdmc:/3ds/pokewalker3ds/sync-error.log";
#endif
  FILE *log = fopen(logPath, "a");
  if (log) {
    fprintf(log, "%ld | %s | received=%u header=%u sent=%u/%u\n", (long)time(0),
            statusText, received, headLength, sentBody, responseLength);
    fclose(log);
  }
}
static void closePeer(void) {
  if (peer != NO_SOCKET)
    netClose(peer);
  peer = NO_SOCKET;
  received = headLength = bodyLength = 0;
  responding = draining = 0;
}
void syncStop(void) {
  closePeer();
  if (listener != NO_SOCKET)
    netClose(listener);
  listener = NO_SOCKET;
#ifdef NETWORK_HOST_TEST
  if (networkState)
    WSACleanup();
#elif defined(__NDS__)
  /* WiFi remains initialized; another session may reuse the configured AP. */
#else
  if (socMemory) {
    socExit();
    free(socMemory);
    socMemory = 0;
  }
#endif
  networkState = 0;
}
static int listenNow(void) {
  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener == NO_SOCKET)
    return 0;
  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = htons(WALKER_SYNC_PORT);
#ifdef NETWORK_HOST_TEST
  local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#else
  local.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
  int reuse = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse,
             sizeof(reuse));
  if (bind(listener, (struct sockaddr *)&local, sizeof(local)) ||
      listen(listener, 1) || nonblock(listener))
    return 0;
  snprintf(statusText, sizeof(statusText),
           "Ready for phone. Keep this app open.");
  networkState = 2;
  return 1;
}
int syncStart(const char *path, SyncBeforeCheckout before, SyncEvent event) {
  syncStop();
  snprintf(selected, sizeof(selected), "%s", path);
  beforeCheckout = before;
  onEvent = event;
#ifdef NETWORK_HOST_TEST
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data))
    return 0;
  networkState = 1;
  snprintf(address, sizeof(address), "127.0.0.1");
  if (!listenNow()) {
    snprintf(statusText, sizeof(statusText),
             "Could not listen on the sync port.");
    return 0;
  }
#elif defined(__NDS__)
  static int wifiInitialized;
  if (!wifiInitialized) {
    if (!Wifi_InitDefault(INIT_ONLY))
      return 0;
    wifiInitialized = 1;
  }
  Wifi_AutoConnect();
  networkState = 1;
  snprintf(address, sizeof(address), "Connecting...");
  snprintf(statusText, sizeof(statusText),
           "Connecting using saved Nintendo WFC settings.");
#else
  socMemory = memalign(0x1000, 0x100000);
  if (!socMemory)
    return 0;
  if (R_FAILED(socInit(socMemory, 0x100000))) {
    free(socMemory);
    socMemory = 0;
    return 0;
  }
  networkState = 1;
  struct in_addr ip;
  ip.s_addr = (uint32_t)gethostid();
  if (!ip.s_addr) {
    snprintf(statusText, sizeof(statusText), "Connect the 3DS to Wi-Fi first.");
    return 0;
  }
  snprintf(address, sizeof(address), "%s", inet_ntoa(ip));
  if (!listenNow()) {
    snprintf(statusText, sizeof(statusText),
             "Could not listen on the sync port.");
    return 0;
  }
#endif
  return 1;
}
int syncReady(void) { return networkState == 2; }
int syncBusy(void) { return peer != NO_SOCKET; }
const char *syncAddress(void) { return address; }
const char *syncStatus(void) { return statusText; }
static void respond(int code, const char *text, const unsigned char *body,
                    unsigned length, const char *token, uint32_t storedCRC) {
  responseLength = length;
  responseBody = body;
  sentHeader = sentBody = 0;
  responding = 1;
  snprintf(
      response, sizeof(response),
      "HTTP/1.1 %d %s\r\nContent-Length: %u\r\nContent-Type: %s\r\nConnection: "
      "close\r\nCache-Control: no-store\r\nX-Walker-Session: "
      "%s\r\nX-Walker-CRC32: %08lX\r\nX-Walker-Stored-CRC32: %08lX\r\n\r\n",
      code, text, length,
      code == 200 && length == WALKER_FILE_SIZE ? "application/octet-stream"
                                                : "text/plain",
      token ? token : "",
      (unsigned long)(length == WALKER_FILE_SIZE ? walkerCrc(body, length)
                                                 : 0), (unsigned long)storedCRC);
}
static void errorResponse(int code, const char *why) {
  snprintf(statusText, sizeof(statusText), "%s", why);
  respond(code, "Error", (const unsigned char *)statusText,
          (unsigned)strlen(statusText), 0, 0);
}
static int header(const char *name, char *out, unsigned capacity) {
  unsigned n = (unsigned)strlen(name);
  const char *p = (const char *)request;
  while ((p = strstr(p, "\r\n")) != 0) {
    p += 2;
    if (!*p || *p == '\r')
      break;
    unsigned i;
    for (i = 0; i < n; i++)
      if (tolower((unsigned char)p[i]) != tolower((unsigned char)name[i]))
        break;
    if (i == n && p[n] == ':') {
      p += n + 1;
      while (*p == ' ' || *p == '\t')
        p++;
      const char *end = strstr(p, "\r\n");
      if (!end || (unsigned)(end - p) >= capacity)
        return 0;
      memcpy(out, p, end - p);
      out[end - p] = 0;
      return 1;
    }
  }
  return 0;
}
static void handle(void) {
  char session[64], length[32];
  uint8_t token[16];
  int get = !strncmp((char *)request, "GET /walker HTTP/1.", 19);
  int post = !strncmp((char *)request, "POST /walker HTTP/1.", 20);
  if (!get && !post) {
    errorResponse(404, "Unknown sync endpoint.");
    return;
  }
  if (!header("X-Walker-Session", session, sizeof(session)) ||
      !leaseTokenParse(session, token)) {
    errorResponse(400, "A 16-byte phone session token is required.");
    return;
  }
  char trainer[64];
  if(header("X-Walker-Trainer",trainer,sizeof(trainer)) && !leaseMatchesTrainer(selected,trainer)) {
    errorResponse(409,"Trainer name or Trainer ID does not match this phone profile. Select the matching walker.");return;
  }
  if (post) {
    if (!header("Content-Length", length, sizeof(length)) ||
        strtoul(length, 0, 10) != WALKER_FILE_SIZE) {
      errorResponse(400, "A return must contain exactly 65536 bytes.");
      return;
    }
    bodyLength = WALKER_FILE_SIZE;
    if (received < headLength + bodyLength)
      return;
    char error[160];
    const uint32_t returnedCRC=walkerCrc(request+headLength,WALKER_FILE_SIZE);
    int returned=leaseReturn(selected,token,request+headLength,error,sizeof(error));
    if (!returned) {
      WalkerLease active;
      int state = leaseInspect(selected, &active);
      int same = state == 1 && !memcmp(active.token, token, 16);
      int revoked =
          leaseSessionRevoked(selected, token) || (same && active.revoked);
      errorResponse(revoked ? 410 : (same || state < 0 ? 503 : 409), error);
      return;
    }
    if (returned==1 && onEvent)
      onEvent(2);
    snprintf(statusText,sizeof(statusText),"%s",returned==1?
             "Returned. Console walker unlocked.":"Return already acknowledged. Current walker unchanged.");
    snprintf(returnReceipt,sizeof(returnReceipt),"PWRETURN1 %s %08lX\n",session,(unsigned long)returnedCRC);
    respond(200, "OK", (unsigned char *)returnReceipt,
            (unsigned)strlen(returnReceipt), session, returnedCRC);
  } else {
    if (leaseSessionRevoked(selected, token)) {
      errorResponse(
          410,
          "This handoff was revoked. Try Receive again for a fresh handoff.");
      return;
    }
    WalkerLease loan;
    int locked = leaseInspect(selected, &loan);
    if (locked == 1 && memcmp(loan.token, token, 16)) {
      errorResponse(
          409, "This walker is already checked out to a different session.");
      return;
    }
    if (!locked && beforeCheckout && !beforeCheckout()) {
      errorResponse(500, "Console save failed. Resolve the save error first.");
      return;
    }
    char error[160];
    if (!leaseCheckout(selected, token, snapshot, &loan, error,
                       sizeof(error))) {
      errorResponse(409, error);
      return;
    }
    if (onEvent)
      onEvent(1);
    snprintf(statusText, sizeof(statusText),
             "On phone. Console access is locked.");
    respond(200, "OK", snapshot, WALKER_FILE_SIZE, session, 0);
  }
}
void syncPoll(void) {
#ifdef __NDS__
  if (networkState == 1) {
    int state = Wifi_AssocStatus();
    if (state == ASSOCSTATUS_ASSOCIATED) {
      struct in_addr ip, gateway, mask, dns1, dns2;
      ip = Wifi_GetIPInfo(&gateway, &mask, &dns1, &dns2);
      snprintf(address, sizeof(address), "%s", inet_ntoa(ip));
      if (!listenNow()) {
        snprintf(statusText, sizeof(statusText),
                 "Could not start sync listener.");
        networkState = 3;
      }
    } else if (state == ASSOCSTATUS_CANNOTCONNECT) {
      snprintf(statusText, sizeof(statusText),
               "WFC connection failed. Check compatible Wi-Fi settings.");
      networkState = 3;
    }
  }
#endif
  if (networkState != 2)
    return;
  if (peer == NO_SOCKET) {
    peer = accept(listener, 0, 0);
    if (peer == NO_SOCKET)
      return;
    if (nonblock(peer)) {
      closePeer();
      return;
    }
    activity = time(0);
  }
  if (time(0) - activity > 60) {
    transferError("Timeout", 0);
    closePeer();
    return;
  }
  /* Half-close only after every byte was queued; allow the client to consume
   * the framed response and close before releasing the console socket. */
  if (draining) {
    char discard[256];
    int n = recv(peer, discard, sizeof(discard), 0);
    if (n == 0 || (n < 0 && !wouldBlock()) || time(0) - drainStarted >= 5)
      closePeer();
    return;
  }
  if (!responding) {
    unsigned freeBytes = sizeof(request) - 1 - received;
    if (!freeBytes) {
      errorResponse(413, "Request is too large.");
    } else {
      int n = recv(peer, (char *)request + received,
                   freeBytes > SYNC_CHUNK ? SYNC_CHUNK : freeBytes, 0);
      if (n > 0) {
        received += (unsigned)n;
        activity = time(0);
        if (!headLength) {
          request[received] = 0;
          char *end = strstr((char *)request, "\r\n\r\n");
          if (end) {
            headLength = (unsigned)(end - (char *)request) + 4;
            if (headLength > 2048) {
              errorResponse(413, "Headers are too large.");
              return;
            }
          } else if (received >= 2048) {
            errorResponse(413, "Headers are too large.");
            return;
          }
        }
        if (headLength) {
          /* Headers remain NUL terminated without modifying binary payload. */
          unsigned char saved = request[headLength - 1];
          request[headLength - 1] = 0;
          handle();
          request[headLength - 1] = saved;
        }
      } else if (n == 0 || !wouldBlock()) {
        if (n < 0)
          transferError("Receive", socketError());
        closePeer();
        return;
      }
    }
  }
  if (responding) {
    unsigned h = (unsigned)strlen(response);
    int n;
    if (sentHeader < h) {
      n = send(peer, response + sentHeader, h - sentHeader, 0);
      if (n > 0)
        sentHeader += n;
    } else if (sentBody < responseLength) {
      unsigned remain = responseLength - sentBody;
      n = send(peer, (const char *)responseBody + sentBody,
               remain > SYNC_CHUNK ? SYNC_CHUNK : remain, 0);
      if (n > 0)
        sentBody += n;
    } else {
#ifdef NETWORK_HOST_TEST
      int result = shutdown(peer, SD_SEND);
#else
      int result = shutdown(peer, SHUT_WR);
#endif
      if (result < 0 && !wouldBlock()) {
        transferError("Finish", socketError());
        closePeer();
        return;
      }
      draining = 1;
      drainStarted = time(0);
      return;
    }
    if (n < 0 && !wouldBlock()) {
      transferError("Send", socketError());
      closePeer();
      return;
    }
    if (n > 0) {
      activity = time(0);
      if (responseBody == snapshot)
        snprintf(statusText, sizeof(statusText),
                 "Sending to phone: %u / %u bytes.", sentBody,
                 responseLength);
    }
  }
}
