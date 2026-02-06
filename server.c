#include "enet.h"
#include "stdio.h"
#include "stdlib.h"
#include "leevi_types.h"

// Declarations
// -------------------------------------------------------------------------
void mainLoop(ENetHost *server);

int main()
{
    printf("ghello\n");
    if (enet_initialize() != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);
    printf("something is happening!\n");
    ENetAddress address;
    ENetHost *server;

    /* Bind the server to the default localhost.     */
    /* A specific host address can be specified by   */
    /* enet_address_set_host (& address, "x.x.x.x"); */

    address.host = ENET_HOST_ANY;
    /* Bind the server to port 1234. */
    address.port = 1234;

    server = enet_host_create(&address /* the address to bind the server host to */,
                              32 /* allow up to 32 clients and/or outgoing connections */,
                              2 /* allow up to 2 channels to be used, 0 and 1 */,
                              0 /* assume any amount of incoming bandwidth */,
                              0 /* assume any amount of outgoing bandwidth */);
    if (server == NULL)
    {
        fprintf(stderr,
                "An error occurred while trying to create an ENet server host.\n");
        exit(EXIT_FAILURE);
    }

    mainLoop(server);

    enet_host_destroy(server);
    return 0;
}

void mainLoop(ENetHost *server)
{
    ENetEvent event;

    while (enet_host_service(server, &event, 100) >= 0 || true)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
        {
            printf("A new client connected from %x:%u.\n",
                   event.peer->address.host,
                   event.peer->address.port);
            /* Store any relevant client information here. */
            int uuid = rand();
            event.peer->data = uuid;
            PacketEvent outGoingEvent = (PacketEvent){
                E_UUIDRECEIVED,
                uuid,
                {.P_UuidReceived = uuid}};
            ENetPacket *packet = enet_packet_create(&outGoingEvent, sizeof(PacketEvent), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(event.peer, 0, packet);
            PacketEvent newPlayer = (PacketEvent){
                E_PLAYERJOINED,
                uuid,
                {.P_PlayerJoined = {0}}};
            newPlayer.data.P_PlayerJoined.uuid = uuid;
            ENetPacket *p = enet_packet_create(&newPlayer, sizeof(PacketEvent), ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(server, 0, p);
            break;
        }

        case ENET_EVENT_TYPE_RECEIVE:
        {
            // printf("A packet of length %u containing %d & %d was received from %u on channel %u.\n",
            //        event.packet->dataLength,
            //        event.packet->data,
            //        ((PacketEvent *)event.packet->data)->type,
            //        event.peer->data,
            //        event.channelID);
            PacketEvent pe = *(PacketEvent *)event.packet->data;
            enum PacketEventType type = pe.type;
            int packetUUID = pe.uuid;
            ENetPacket *packet = enet_packet_create(&pe, sizeof(PacketEvent), 0);
            enet_host_broadcast(server, 0, packet);
            break;
        }

        case ENET_EVENT_TYPE_DISCONNECT:
        {
            int uuid = event.peer->data;
            printf("%u disconnected.\n", event.peer->data);
            PacketEvent disconnect = (PacketEvent){
                E_PLAYERLEFT,
                uuid,
                {.P_PlayerLeft = uuid}};
            ENetPacket *disconnectPacket = enet_packet_create(&disconnect, sizeof(PacketEvent), ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(server, 0, disconnectPacket);
            event.peer->data = NULL;
            break;
        }
        }
    }
}
