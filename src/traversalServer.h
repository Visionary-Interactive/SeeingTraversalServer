#ifndef TRAVERSALSERVER_H
#define TRAVERSALSERVER_H

#define SERVER_PORT 1337

#include "includes.h"
#include "Queue.h"

#ifndef DEBUG_LOGGING
#define DEBUG_LOGGING 2
#endif

#if DEBUG_LOGGING == 1
// Basic logging to console
#define NBN_LogInfo(...)   printf(__VA_ARGS__); printf("\n")
#define NBN_LogError(...)  printf(__VA_ARGS__); printf("\n")
#define NBN_LogDebug(...)  printf(__VA_ARGS__); printf("\n")
#define NBN_LogTrace(...)  printf(__VA_ARGS__); printf("\n")
#define NBN_LogWarning(...) printf(__VA_ARGS__); printf("\n")
#else
#define NBN_LogInfo(...)    ((void)0)
#define NBN_LogError(...)   ((void)0)
#define NBN_LogDebug(...)   ((void)0)
#define NBN_LogTrace(...)   ((void)0)
#define NBN_LogWarning(...) ((void)0)
#endif

typedef uint32_t NBN_ConnectionHandle;

typedef enum {
	MovementSnapshot,
	PositionSnapshot,
	IncomingPlayer,
	LobbyQuery
} MsgType;

struct LobbyQuery { // Sent upon joining lobby
	uint8_t auth : 1;	// Authenticated by server
	uint8_t isHost : 1;
	uint8_t isFull : 1;
	uint32_t hostIP;
};

extern NBN_ConnectionHandle connectedClientHandle;

void TraversalServer_Init();
bool TraversalServer_CreateServer(const char* protocol, uint16_t port);
void TraversalServer_StopServer();
int TraversalServer_HandleEvents();
bool TraversalServer_SendReliableByteArray(NBN_ConnectionHandle conn, uint8_t* data, unsigned int length);
int TraversalServer_SendPackets();
void TraversalServer_PairHostClient();

#endif // TRAVERSALSERVER_H