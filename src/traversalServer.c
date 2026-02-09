#include "traversalServer.h"

#define NBNET_IMPL

#include "nbnet.h"
#include "net_drivers/udp.h"

clientCount = 0;

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
		NBN_ConnectionHandle connectedClientHandle = NBN_GameServer_GetIncomingConnection();
		enqueue(connectedClientHandle);
		#if DEBUG_LOGGING >= 1
			printf("New client connected: %u\n", connectedClientHandle);
		#endif

		if (NBN_GameServer_AcceptIncomingConnection() < 0)
			printf("Warning: failed to accept incoming connection\n");

		NBN_ConnectionVector* clients = nbn_game_server.clients;
		clientCount = clients->count;
		break;
	}
	case NBN_CLIENT_DISCONNECTED:
	{
		NBN_ConnectionHandle disc = NBN_GameServer_GetDisconnectedClient();
		dequeue();
		#if DEBUG_LOGGING >= 1
			printf("Client disconnected: handle=%u\n", disc);
		#endif
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

					//struct LobbyQuery lobbyQuery = { 0 };
					//lobbyQuery.auth = 1;
					//lobbyQuery.isHost = 1;
					//lobbyQuery.isFull = 1;
					////lobbyQuery.hostIP = udp_conn->address.host; // Host is client1

					//// Send to host client
					//uint8_t buffer[1 + sizeof(struct LobbyQuery)];
					//buffer[0] = LobbyQuery; // Set message type
					//memcpy(buffer + 1, &lobbyQuery, sizeof(struct LobbyQuery));
					//TraversalServer_SendReliableByteArray(info.sender, buffer, sizeof(buffer));

					TraversalServer_PairHostClient();
				}
				break;
			case PositionSnapshot:  // Relay message to other client
			case IncomingPlayer:
			case MovementSnapshot:
				if (clientCount >= 2)
				{
					#if DEBUG_LOGGING >= 1
						//printf("Relaying reliable message of type %u from client %u\n", msg_type, info.sender);
					#endif
					if (info.sender == connectedClientsQueue[front]) {
						TraversalServer_SendReliableByteArray(connectedClientsQueue[rear], bmsg->bytes, bmsg->length);
					}
					else {
						TraversalServer_SendReliableByteArray(connectedClientsQueue[front], bmsg->bytes, bmsg->length);
					}
				}
				break;
			//case MovementSnapshot:
			//	if (clientCount >= 2)
			//	{
			//		#if DEBUG_LOGGING >= 1
			//			//printf("Relaying unreliable message of type %u from client %u\n", msg_type, info.sender);
			//		#endif
			//		if (info.sender == connectedClientsQueue[front]) {
			//			TraversalServer_SendUnreliableByteArray(connectedClientsQueue[rear], bmsg->bytes, bmsg->length);
			//		}
			//		else {
			//			TraversalServer_SendUnreliableByteArray(connectedClientsQueue[front], bmsg->bytes, bmsg->length);
			//		}
			//	}
			//	break;
			default:
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
	if (NBN_GameServer_SendReliableByteArrayTo(conn, data, length) < 0)
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

void TraversalServer_PairHostClient()
{
	NBN_ConnectionVector* clients = nbn_game_server.clients;

	if (clients != NULL && clients->count >= 2)
	{
		NBN_Connection* client1 = clients->connections[0];
		NBN_Connection* client2 = clients->connections[1];
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
	}
}
