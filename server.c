#include "enet.h"
#include "stdio.h"
#include "stdlib.h"
#include "leevi_types.h"

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
    ENetPeer *clients[MAX_PLAYERS] = {0};
    ENetEvent event;

    /* Wait up to 1000 milliseconds for an event. */
    while (enet_host_service(server, &event, 0) >= 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            printf("A new client connected from %x:%u.\n",
                   event.peer->address.host,
                   event.peer->address.port);
            /* Store any relevant client information here. */
            printf("before rand\n");
            {
                int uuid = rand();
                printf("before access data\n");
                event.peer->data = uuid;
                printf("before addclient\n");
                if (!addClient(clients, event.peer))
                {
                    printf("Could not add a new client!\n");
                    break;
                }
                printf("works\n");
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
                sendEveryone(
                    clients,
                    uuid,
                    p);
                // enet_peer_send()
                // enet_host_flush(server);
                enet_packet_destroy(p);
            }
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            // printf("A packet of length %u containing %d & %d was received from %u on channel %u.\n",
            //        event.packet->dataLength,
            //        event.packet->data,
            //        ((PacketEvent*)event.packet->data)->type,
            //        event.peer->data,
            //        event.channelID);
            PacketEvent *pe = (PacketEvent *)event.packet->data;
            enum PacketEventType type = pe->type;
            int packetUUID = pe->uuid;
            //printf("packet:%s, uuid: %d\n", eventTypeToString(type), packetUUID);
            if (type == E_UPDATEPOSITION)
            {
                Vector3 pos = pe->data.P_updatePosition.position;
                //printf("x:%f, y:%f, z:%f\n", pos.x, pos.y, pos.z);
            }
            sendEveryone(&clients, event.peer->data, event.packet);
            /* Clean up the packet now that we're done using it. */
            enet_packet_destroy(event.packet);

            break;

        case ENET_EVENT_TYPE_DISCONNECT:
        {
            int uuid = event.peer->data;
            printf("%u disconnected.\n", event.peer->data);

            /* Reset the peer's client information. */
            removeClient(&clients, event.peer);
            PacketEvent disconnect = (PacketEvent){
                E_PLAYERLEFT,
                uuid,
                {.P_PlayerLeft = uuid}};
            ENetPacket *disconnectPacket = enet_packet_create(&disconnect, sizeof(PacketEvent), ENET_PACKET_FLAG_RELIABLE);
            sendEveryone(&clients, event.peer->data, disconnectPacket);
            event.peer->data = NULL;
            enet_packet_destroy(disconnectPacket);
            break;
        }
        }
    }
}

int addClient(
    ENetPeer **clients,
    ENetPeer *newClient)
{
    printf("trying to add client...\n");
    for (size_t i = 0; i < MAX_PLAYERS; i++)
    {
        printf("i: %d\n", i);
        printf("*clients: %d\n", *clients);
        ENetPeer *client = (clients)[i];
        printf("Past indexing..\n");
        if (client == NULL)
        {
            (clients)[i] = newClient;
            printf("client added!\n");
            return true;
        }
    }
    printf("could not add client!\n");
    return false;
}

void removeClient(
    ENetPeer **clients,
    ENetPeer *clientToRemove)
{
    for (size_t i = 0; i < MAX_PLAYERS; i++)
    {

        ENetPeer *client = (clients)[i];
        if (client == clientToRemove)
        {
            (clients)[i] = NULL;
            return;
        }
    }
}

void sendEveryone(ENetPeer **clients, int uuidToIgnore, ENetPacket *packetToRelay)
{
    // printf("sending everyone!:\n");
    for (size_t i = 0; i < MAX_PLAYERS; i++)
    {
        ENetPeer *client = (clients)[i];
        // printf("client: %d\n", client);
        if (client == NULL || client->data == 0 || client->data == uuidToIgnore)
        {
            continue;
        }
        // printf("about to send!\n");
        PacketEvent pe = *(PacketEvent *)packetToRelay->data;
        ENetPacket *packet = enet_packet_create(&pe, sizeof(PacketEvent), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(client, 0, packet);
    }
    // printf("done sending\n");
}
