#include "raylib.h"
#define MAX_PLAYERS     10

typedef struct {
    Vector3 position;
    Vector3 velocity;
    Vector3 dir;
    bool isGrounded;
} Body;

typedef struct {
    Vector2 lookRotation;
    Vector2 walkInput;
    bool isJumpPressed;
    bool isCrouching;
} Inputs;

typedef struct {
    Body body;
    Inputs inputs;
    int uuid;
} Player;


typedef Body P_UpdatePosition;
typedef Inputs P_UpdateInputs;
typedef int P_UuidReceived; //TODO: redundancy
typedef int P_PlayerLeft;
typedef Player P_PlayerJoined;

enum PacketEventType {
    E_NOTHING,
    E_UPDATEINPUTS,
    E_UPDATEPOSITION,
    E_UUIDRECEIVED,
    E_PLAYERJOINED,
    E_PLAYERLEFT
};

typedef union PacketEventData {
    P_UpdateInputs P_updateInputs;
    P_UpdatePosition P_updatePosition;
    P_UuidReceived P_UuidReceived;
    P_PlayerJoined P_PlayerJoined;
    P_PlayerLeft P_PlayerLeft;
} PacketEventData;

typedef struct {
    enum PacketEventType type;
    int uuid;
    PacketEventData data;
} PacketEvent;



const char* eventTypeToString(enum PacketEventType e) {
    if (e == E_UPDATEINPUTS) return "E_UPDATEINPUTS";
    if (e == E_UPDATEPOSITION) return "E_UPDATEPOSITION";
    if (e == E_UUIDRECEIVED) return "E_UUIDRECEIVED";
    if (e == E_PLAYERJOINED) return "E_PLAYERJOINED";
    if (e == E_PLAYERLEFT) return "E_PLAYERLEFT";
    return "unknown";
}