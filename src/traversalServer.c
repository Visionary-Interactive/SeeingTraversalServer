#include "traversalServer.h"

#define NBNET_IMPL

#include "nbnet.h"
#include "net_drivers/udp.h"

clientCount = 0;
NBN_Connection* connectedPairs[PAIR_LIMIT][2];
pairCount = 0;
bool pairingDone[PAIR_LIMIT];

void TraversalServer_Init()
{
	clientCount = 0;
	NBN_UDP_Register();
}

bool TraversalServer_CreateServer(const char* protocol, uint16_t port)
{
	if (NBN_GameServer_Start(protocol, port) < 0)
	{
		printf("Failed to start server\n");
		return false;
	}
	#if DEBUG_LOGGING >= 1
		printf("Server started on port %d\n", port);
	#endif
	return true;
}

void TraversalServer_StopServer()
{
	NBN_GameServer_Stop();
	#if DEBUG_LOGGING >= 1
		printf("Server stopped\n");
	#endif
}

int TraversalServer_HandleEvents()
{
	int ev = NBN_GameServer_Poll();

	switch (ev)
	{
	case NBN_NEW_CONNECTION:
	{
		//NBN_ConnectionHandle connectedClientHandle = NBN_GameServer_GetIncomingConnection();
		//enqueue(connectedClientHandle);

		NBN_ConnectionVector* clients = nbn_game_server.clients;

		// Pair Clients as they connect to the server
		if (connectedPairs[pairCount][0] == NULL) {
			connectedPairs[pairCount][0] = clients->connections[clients->count - 1];
		}
		else {
			connectedPairs[pairCount][1] = clients->connections[clients->count - 1];
			pairCount++;
		}
		#if DEBUG_LOGGING >= 1
			printf("New client connected: %u\n", clients->connections[clients->count - 1]->id);
			printf("Current pair count: %d\n", pairCount);
		#endif

		if (NBN_GameServer_AcceptIncomingConnection() < 0)
			printf("Warning: failed to accept incoming connection\n");

		//NBN_ConnectionVector* clients = nbn_game_server.clients;
		clientCount++;
		break;
	}
	case NBN_CLIENT_DISCONNECTED:
	{
		if (nbn_game_server.last_event.type == NBN_CLIENT_DISCONNECTED)
		{
			NBN_ConnectionHandle sender = NBN_GameServer_GetDisconnectedClient();
			//NBN_UDP_Connection* conn = (NBN_UDP_Connection*)nbn_game_server.last_event.data.connection->driver_data;

			// Check if IP address matches either client in a pair and remove the pair if so
			for (int i = 0; i < PAIR_LIMIT; i++)
			{
				NBN_UDP_Connection* connTemp;

				if (pairingDone[i] && connectedPairs[i][0] != NULL && connectedPairs[i][1] != NULL)
				{
					if (connectedPairs[i][0]->id == 0)
					{
						NBN_GameServer_CloseClientWithCode(connectedPairs[i][1]->id, 4000);
					}

					if (connectedPairs[i][1]->id == 0)
					{
						NBN_GameServer_CloseClientWithCode(connectedPairs[i][0]->id, 4000);
					}

					if (connectedPairs[i][1]->id == 0 || connectedPairs[i][0]->id == 0)
					{
						pairingDone[i] = false;
						connectedPairs[i][0] = NULL;
						connectedPairs[i][1] = NULL;
					}
				}
			}
			//dequeue();
			#if DEBUG_LOGGING >= 1
				printf("Client disconnected: handle=%u\n", sender);
			#endif
			clientCount--;
		}
		break;
	}
	case NBN_CLIENT_MESSAGE_RECEIVED:
	{
		NBN_MessageInfo info = NBN_GameServer_GetMessageInfo();

		if (info.type == NBN_BYTE_ARRAY_MESSAGE_TYPE)
		{
			NBN_ByteArrayMessage* bmsg = (NBN_ByteArrayMessage*)info.data;
			if (bmsg->length < 1) return 1; // Invalid

			MsgType msg_type = bmsg->bytes[0];
			switch (msg_type)
			{
			case LobbyQuery:
				if (bmsg->length >= 1 + sizeof(struct LobbyQuery)) {

					struct LobbyQuery recv_query;
					memcpy(&recv_query, bmsg->bytes + 1, sizeof(struct LobbyQuery));
					#if DEBUG_LOGGING >= 1
						printf("Received LobbyQuery from client %u: auth=%u isHost=%u isFull=%u\n",
							info.sender,
							recv_query.auth,
							recv_query.isHost,
							recv_query.isFull);
					#endif
					NBN_ConnectionVector* clients = nbn_game_server.clients;
					for (unsigned int i = 0; i < clients->count; i++) {
						NBN_Connection* conn = clients->connections[i];
						NBN_UDP_Connection* udp_conn = (NBN_UDP_Connection*)conn->driver_data;
						#if DEBUG_LOGGING >= 1
							printf("%u IP: %u.%u.%u.%u:%u\n",
								i,
								(udp_conn->address.host >> 24) & 0xFF,
								(udp_conn->address.host >> 16) & 0xFF,
								(udp_conn->address.host >> 8) & 0xFF,
								(udp_conn->address.host) & 0xFF,
								udp_conn->address.port);
						#endif
					}

					// Find pair
					int pairIndex = -1;
					printf("Finding pair for client %u...\n", info.sender);

					for (int i = 0; i < pairCount; i++)
					{
						NBN_UDP_Connection* connTemp;

						if (connectedPairs[i][0] != NULL)
						{
							if (connectedPairs[i][0]->id == info.sender) {
								pairIndex = i;
								break;
							}
						}

						if (connectedPairs[i][1] != NULL)
						{
							if (connectedPairs[i][1]->id == info.sender) {
								pairIndex = i;
								break;
							}
						}
					}

					if (pairIndex == -1) {
						#if DEBUG_LOGGING >= 1
							printf("No pair found for client %u, ignoring LobbyQuery\n", info.sender);
						#endif
						break;
					}

					if (pairingDone[pairIndex]) {
						#if DEBUG_LOGGING >= 1
							printf("Pair already done for client %u, ignoring LobbyQuery\n", info.sender);
						#endif
						break;
					}
					TraversalServer_PairHostClient(pairIndex);
				}
				break;
			default:  // Route message to paired client
				#if DEBUG_LOGGING >= 1
					//printf("Relaying reliable message of type %u from client %u\n", msg_type, info.sender);
				#endif

				for (int i = 0; i < pairCount; i++) {
					if (!pairingDone[i]) continue; // Skip clients that aren't done pairing

					if (!connectedPairs[i][0]->is_closed && !connectedPairs[i][0]->is_stale &&
						!connectedPairs[i][1]->is_closed && !connectedPairs[i][1]->is_stale)
					{
						if (connectedPairs[i][0]->id == info.sender)
						{
							TraversalServer_SendReliableByteArray(connectedPairs[i][1]->id, bmsg->bytes, bmsg->length);
							break;
						}

						if (connectedPairs[i][1]->id == info.sender)
						{
							TraversalServer_SendReliableByteArray(connectedPairs[i][0]->id, bmsg->bytes, bmsg->length);
							break;
						}
					}
				}
				break;
			}
		}
		break;
	}
	case NBN_NO_EVENT:
	default:
		break;
	}

	return ev;
}

bool TraversalServer_SendReliableByteArray(NBN_ConnectionHandle conn, uint8_t* data, unsigned int length)
{
	if (conn != 0 && NBN_GameServer_SendReliableByteArrayTo(conn, data, length) < 0)
	{
		printf("Failed to send Reliable Byte Array to client %u\n", conn);
		return false;
	}
	return true;
}

bool TraversalServer_SendUnreliableByteArray(NBN_ConnectionHandle conn, uint8_t* data, unsigned int length)
{
	if (NBN_GameServer_SendUnreliableByteArrayTo(conn, data, length) < 0)
	{
		printf("Failed to send Unreliable Byte Array to client %u\n", conn);
		return false;
	}
	return true;
}

int TraversalServer_SendPackets()
{
	return NBN_GameServer_SendPackets();
}

void TraversalServer_PairHostClient(int pairIndex)
{
	if ((connectedPairs[pairIndex][0] != NULL) &&
		(connectedPairs[pairIndex][1] != NULL))
	{
		NBN_Connection* client1 = connectedPairs[pairIndex][0];
		NBN_Connection* client2 = connectedPairs[pairIndex][1];
		NBN_UDP_Connection* udp_conn1 = (NBN_UDP_Connection*)client1->driver_data;
		NBN_UDP_Connection* udp_conn2 = (NBN_UDP_Connection*)client2->driver_data;
		#if DEBUG_LOGGING >= 1
			printf("Pairing clients:\n");
			printf("Client 1: %u.%u.%u.%u:%u\n",
				(udp_conn1->address.host >> 24) & 0xFF,
				(udp_conn1->address.host >> 16) & 0xFF,
				(udp_conn1->address.host >> 8) & 0xFF,
				(udp_conn1->address.host) & 0xFF,
				udp_conn1->address.port);
			printf("Client 2: %u.%u.%u.%u:%u\n",
				(udp_conn2->address.host >> 24) & 0xFF,
				(udp_conn2->address.host >> 16) & 0xFF,
				(udp_conn2->address.host >> 8) & 0xFF,
				(udp_conn2->address.host) & 0xFF,
				udp_conn2->address.port);
		#endif

		#if DEBUG_LOGGING >= 1
				printf("Sending Lobby Response...\n");
		#endif
		struct LobbyQuery lobbyQuery = { 0 };
		lobbyQuery.auth = 1;
		lobbyQuery.isHost = 1;
		lobbyQuery.isFull = 1;
		lobbyQuery.hostIP = udp_conn2->address.host; // Host is client1
		lobbyQuery.hostPort = udp_conn2->address.port;

		// Send to host client
		uint8_t buffer[1 + sizeof(struct LobbyQuery)];
		buffer[0] = LobbyQuery; // Set message type
		memcpy(buffer + 1, &lobbyQuery, sizeof(struct LobbyQuery));
		TraversalServer_SendReliableByteArray(client1->id, buffer, sizeof(buffer));

		// Send to joining client
		lobbyQuery.isHost = 0;
		lobbyQuery.hostIP = udp_conn1->address.host; // Host is client1
		lobbyQuery.hostPort = udp_conn1->address.port;
		memcpy(buffer + 1, &lobbyQuery, sizeof(struct LobbyQuery));
		TraversalServer_SendReliableByteArray(client2->id, buffer, sizeof(buffer));

		pairingDone[pairIndex] = true;
	}
}
