/*******************************************************************************************
 *
 *   raylib [core] example - 3d camera fps
 *
 *   Example complexity rating: [★★★☆] 3/4
 *
 *   Example originally created with raylib 5.5, last time updated with
 * raylib 5.5
 *
 *   Example contributed by Agnis Aldiņš (@nezvers) and reviewed by Ramon
 * Santamaria (@raysan5)
 *
 *   Example licensed under an unmodified zlib/libpng license, which is an
 * OSI-certified, BSD-like license that allows static linking with closed source
 * software
 *
 *   Copyright (c) 2025 Agnis Aldiņš (@nezvers)
 *
 ********************************************************************************************/
#include "enet.h"
#include "leevi_types.h"
#include "math.h"
#include "raylib.h"
#include "raymath.h"
#include "stdio.h"
//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Movement constants
#define GRAVITY 32.0f
#define MAX_SPEED 20.0f
#define CROUCH_SPEED 5.0f
#define JUMP_FORCE 12.0f
#define MAX_ACCEL 150.0f
// Grounded drag
#define FRICTION 0.86f
// Increasing air drag, increases strafing speed
#define AIR_DRAG 0.98f
// Responsiveness for turning movement direction to looked direction
#define CONTROL 15.0f
#define CROUCH_HEIGHT 0.0f
#define STAND_HEIGHT 1.0f
#define BOTTOM_HEIGHT 0.5f

#define NORMALIZE_INPUT 0

#define HOST_NAME "localhost"
#define HOST_PORT 1234

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Body structure

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static Vector2 sensitivity = { 0.001f, 0.001f };

// static Vector2 lookRotation = { 0 };
static float headTimer = 0.0f;
static float walkLerp = 0.0f;
static float headLerp = STAND_HEIGHT;
static Vector2 lean = { 0 };
Model map_model;
Matrix m = (Matrix) {
    3, 0, 0, 0,
    0, 3, 0, 0,
    0, 0, 3, 0,
    0, 0, 0, 1
};

Ray ray;
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void DrawLevel(Player players[], Player* player);
static void UpdateCameraFPS(Camera* camera, Player* player);
static void UpdateBody(Body* body, float rot, Vector2 input, bool jumpPressed,
    bool crouchHold);
Player* findPlayer(Player players[], int uuid);
int addPlayer(Player players[], Player playerToAdd);
ENetPeer* connectToServer(ENetHost* client);
void mainLoop(ENetHost* client, ENetPeer* server, Player players[],
    Player* player, Camera* camera);
void handleEvents(ENetHost* client, Player players[], int* myUUID);
void handlePackets(PacketEvent* packet, Player players[], int* myUUID);
void sendInputs(ENetPeer* server, Inputs inputs, int myUUID);
int sendPosition(ENetPeer* server, Body position, int myUUID);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    Player player = { 0 };
    player.body.position.x = 5.f;
    Player players[MAX_PLAYERS];
    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        players[i] = (Player) { 0 };
    }
    // init enet
    //--------------------------------------------------------------------------------------
    if (enet_initialize() != 0) {
        // //printf("An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    // Enet initialization
    //--------------------------------------------------------------------------------------
    ENetHost* client;

    client = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);

    if (client == NULL) {
        printf("An error occurred while trying to create an ENet client host.\n");
        exit(EXIT_FAILURE);
    }
    ENetPeer* server = connectToServer(client);

    // Game initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 1920;
    const int screenHeight = 1080;

    InitWindow(screenWidth, screenHeight,
        "raylib [core] example - 3d camera fps");
    // Initialize camera variables
    // NOTE: UpdateCameraFPS() takes care of the rest
    map_model = LoadModel("resource/dust/untitled.gltf");
    Camera3D camera = { 0 };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position = (Vector3) {
        player.body.position.x,
        player.body.position.y + (BOTTOM_HEIGHT + headLerp),
        player.body.position.z,
    };

    UpdateCameraFPS(&camera, &player); // Update camera parameters

    // DisableCursor(); // Limit cursor to relative movement inside the window
    // #define FLAG_VSYNC_HINT
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second

    ToggleFullscreen();
    DisableCursor();
    // Main game loop
    printf("Player: %p, players: %p, player.x = %f\n", &player, players,
        player.body.position.x);
    mainLoop(client, server, players, &player, &camera);

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
    enet_peer_disconnect(server, 0);
    enet_host_flush(client);
    enet_host_destroy(client);
    return 0;
}

void mainLoop(ENetHost* client, ENetPeer* server, Player players[],
    Player* player, Camera* camera)
{
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        if (IsKeyPressed(KEY_Q))
            DisableCursor();
        if (IsKeyPressed(KEY_E))
            EnableCursor();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            ray = (Ray) { camera->position, Vector3Subtract(camera->target, camera->position) };
        }

        handleEvents(client, players, &(player->uuid));
        // Update
        //----------------------------------------------------------------------------------
        Vector2 mouseDelta = GetMouseDelta();
        player->inputs.lookRotation.x -= mouseDelta.x * sensitivity.x;
        player->inputs.lookRotation.y += mouseDelta.y * sensitivity.y;

        char sideway = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
        char forward = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
        Vector2 input = (Vector2) { (IsKeyDown(KEY_D) - IsKeyDown(KEY_A)),
            (IsKeyDown(KEY_S) - IsKeyDown(KEY_W)) };
        player->inputs.walkInput = Vector2Normalize(input);
        player->inputs.isCrouching = IsKeyDown(KEY_LEFT_CONTROL);
        player->inputs.isJumpPressed = IsKeyPressed(KEY_SPACE);
        sendInputs(server, player->inputs, player->uuid);
        UpdateBody(&(player->body), player->inputs.lookRotation.x,
            player->inputs.walkInput, player->inputs.isJumpPressed,
            player->inputs.isCrouching);
        sendPosition(server, player->body, player->uuid);
        for (size_t i = 0; i < MAX_PLAYERS; i++) {
            Player* targetPlayer = &players[i];
            if (targetPlayer->uuid == 0)
                continue;
            UpdateBody(&(targetPlayer->body), targetPlayer->inputs.lookRotation.x,
                targetPlayer->inputs.lookRotation,
                targetPlayer->inputs.isJumpPressed,
                targetPlayer->inputs.isCrouching);

            targetPlayer->inputs.isJumpPressed = false;
        }
        float delta = GetFrameTime();
        headLerp = Lerp(headLerp,
            (player->inputs.isCrouching ? CROUCH_HEIGHT : STAND_HEIGHT),
            20.0f * delta);
        camera->position = (Vector3) {
            player->body.position.x,
            player->body.position.y + (BOTTOM_HEIGHT + headLerp),
            player->body.position.z,
        };
        if (player->body.isGrounded && ((forward != 0) || (sideway != 0))) {
            headTimer += delta * 3.0f;
            walkLerp = Lerp(walkLerp, 1.0f, 10.0f * delta);
            camera->fovy = Lerp(camera->fovy, 55.0f, 5.0f * delta);
        } else {
            walkLerp = Lerp(walkLerp, 0.0f, 10.0f * delta);
            camera->fovy = Lerp(camera->fovy, 60.0f, 5.0f * delta);
        }

        lean.x = Lerp(lean.x, sideway * 0.02f, 10.0f * delta);
        lean.y = Lerp(lean.y, forward * 0.015f, 10.0f * delta);
        UpdateCameraFPS(camera, player);
        //----------------------------------------------------------------------------------
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(*camera);
        DrawLevel(players, player);
        EndMode3D();

        // Draw info box
        DrawRectangle(5, 5, 330, 75, Fade(SKYBLUE, 0.5f));
        DrawRectangleLines(5, 5, 330, 75, BLUE);

        DrawText("Camera controls:", 15, 15, 10, BLACK);
        DrawText("- Move keys: W, A, S, D, Space, Left-Ctrl", 15, 30, 10, BLACK);
        DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);
        DrawText(TextFormat("- Velocity Len: (%06.3f)",
                     Vector2Length((Vector2) { player->body.velocity.x,
                         player->body.velocity.z })),
            15, 60, 10, BLACK);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Update body considering current world state
void UpdateBody(Body* body, float rot, Vector2 input, bool jumpPressed,
    bool crouchHold)
{
    Vector3 oldPosition = body->position;
    float delta = GetFrameTime();

    // if (!body->isGrounded)
    body->velocity.y -= GRAVITY * delta;

    if (body->isGrounded && jumpPressed) {
        body->velocity.y = JUMP_FORCE;
        body->isGrounded = false;

        // Sound can be played at this moment
        // SetSoundPitch(fxJump, 1.0f + (GetRandomValue(-100, 100)*0.001));
        // PlaySound(fxJump);
    }
    Vector3 front = (Vector3) { sinf(rot), 0.f, cosf(rot) };
    Vector3 right = (Vector3) { cosf(-rot), 0.f, sinf(-rot) };

    Vector3 desiredDir = (Vector3) {
        input.x * right.x + input.y * front.x,
        0.0f,
        input.x * right.z + input.y * front.z,
    };
    body->dir = Vector3Lerp(body->dir, desiredDir, CONTROL * delta);

    float decel = (body->isGrounded ? FRICTION : AIR_DRAG);
    Vector3 hvel = (Vector3) { body->velocity.x * decel, 0.0f, body->velocity.z * decel };

    float hvelLength = Vector3Length(hvel); // Magnitude
    if (hvelLength < (MAX_SPEED * 0.01f))
        hvel = (Vector3) { 0 };

    // This is what creates strafing
    float speed = Vector3DotProduct(hvel, body->dir);

    // Whenever the amount of acceleration to add is clamped by the maximum
    // acceleration constant, a Player can make the speed faster by bringing the
    // direction closer to horizontal velocity angle More info here:
    // https://youtu.be/v3zT3Z5az§paM?t=165
    float maxSpeed = (crouchHold ? CROUCH_SPEED : MAX_SPEED);
    float accel = Clamp(maxSpeed - speed, 0.f, MAX_ACCEL * delta);
    hvel.x += body->dir.x * accel;
    hvel.z += body->dir.z * accel;

    body->velocity.x = hvel.x;
    body->velocity.z = hvel.z;

    body->position.x += body->velocity.x * delta;
    body->position.y += body->velocity.y * delta;
    body->position.z += body->velocity.z * delta;

    for (int i = 0; i < map_model.meshCount; i++) {
        Mesh mesh = map_model.meshes[i];
        Ray movementRay = (Ray) { oldPosition, Vector3Subtract(body->position, oldPosition) };
        RayCollision collision = GetRayCollisionMesh(
            movementRay,
            mesh,
            m);

        if (collision.hit && collision.distance < 5.0) {
            printf("HIT!\n");
            Vector3 reflectedVelocity = Vector3Scale(Vector3Reject(body->velocity, collision.normal), 1); // collision.distance);
            body->velocity = reflectedVelocity;
            if (collision.normal.y > 0.3) {
                body->isGrounded = true;
                if (body->velocity.y < 0) {
                    body->velocity.y = 0;
                }
                body->velocity = Vector3Subtract(
                    body->velocity,
                    Vector3Scale(
                        Vector3Normalize(body->velocity),
                        Clamp(delta * decel / 10, 0, delta * Vector3Length(body->velocity) / 2)));
                // float multiplier = pow(0.9, delta * 100);
                //  body->velocity = Vector3Scale(body->velocity, multiplier);
            }
            body->position = Vector3Add(oldPosition, Vector3Scale(body->velocity, delta));
        }
    }
    // Fancy collision system against the floor
    // if (body->position.y <= 0.0f) {
    //    body->position.y = 0.0f;
    //    body->velocity.y = 0.0f;
    //    body->isGrounded = true; // Enable jumping
    //}
}

void GroundMove(Body* body, float rot, Vector2 input, bool jumpPressed,
    bool crouchHold) { }

void AirMove(Body* body, float rot, Vector2 input, bool crouchHold) { }

// Update camera for FPS behaviour
static void UpdateCameraFPS(Camera* camera, Player* player)
{
    const Vector3 up = (Vector3) { 0.0f, 1.0f, 0.0f };
    const Vector3 targetOffset = (Vector3) { 0.0f, 0.0f, -1.0f };

    // Left and right
    Vector3 yaw = Vector3RotateByAxisAngle(targetOffset, up, player->inputs.lookRotation.x);

    // Clamp view up
    float maxAngleUp = Vector3Angle(up, yaw);
    maxAngleUp -= 0.001f; // Avoid numerical errors
    if (-(player->inputs.lookRotation.y) > maxAngleUp) {
        player->inputs.lookRotation.y = -maxAngleUp;
    }

    // Clamp view down
    float maxAngleDown = Vector3Angle(Vector3Negate(up), yaw);
    maxAngleDown *= -1.0f; // Downwards angle is negative
    maxAngleDown += 0.001f; // Avoid numerical errors
    if (-(player->inputs.lookRotation.y) < maxAngleDown) {
        player->inputs.lookRotation.y = -maxAngleDown;
    }

    // Up and down
    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

    // Rotate view vector around right axis
    float pitchAngle = -player->inputs.lookRotation.y - lean.y;
    pitchAngle = Clamp(pitchAngle, -PI / 2 + 0.0001f,
        PI / 2 - 0.0001f); // Clamp angle so it doesn't go past
                           // straight up or straight down
    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    // Head animation
    // Rotate up direction around forward axis
    float headSin = sinf(headTimer * PI);
    float headCos = cosf(headTimer * PI);
    const float stepRotation = 0.01f;
    camera->up = Vector3RotateByAxisAngle(up, pitch, headSin * stepRotation + lean.x);

    // Camera BOB
    const float bobSide = 0.1f;
    const float bobUp = 0.15f;
    Vector3 bobbing = Vector3Scale(right, headSin * bobSide);
    bobbing.y = fabsf(headCos * bobUp);

    camera->position = Vector3Add(camera->position, Vector3Scale(bobbing, walkLerp));
    camera->target = Vector3Add(camera->position, pitch);
}

// Draw game level
static void DrawLevel(Player players[], Player* player)
{
    if (IsModelValid(map_model)) {
        DrawModel(map_model, Vector3Zero(), 3, WHITE);
        // DrawMesh(map_model.meshes[mesh_index], map_model.materials[material_index], m);
    } else {
        DrawGrid(10, 1);
    }

    // Red sun
    DrawSphere((Vector3) { 300.0f, 300.0f, 0.0f }, 100.0f, (Color) { 255, 0, 0, 255 });
    DrawRay(ray, RED);

    // Players
    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].uuid == 0) {
            continue;
        }
        float height = players[i].inputs.isCrouching ? 0.8f * STAND_HEIGHT : STAND_HEIGHT;
        Vector3 pos = (Vector3) { 0.f, 2 * height, 0.0f };
        // printf("drawing cube!\n");
        DrawCube(Vector3Add(players[i].body.position, pos), 2.0f, 4 * height, 2.0f,
            (Color) { 255, 255, 0, 255 });
        /* code */
    }
}

ENetPeer* connectToServer(ENetHost* client)
{
    ENetAddress address;
    ENetEvent event;
    ENetPeer* peer;

    /* Connect to some.server.net:1234. */
    enet_address_set_host(&address, HOST_NAME);
    address.port = HOST_PORT;

    /* Initiate the connection, allocating the two channels 0 and 1. */
    peer = enet_host_connect(client, &address, 2, 0);

    if (peer == NULL) {
        printf("No available peers for initiating an ENet connection.\n");
        exit(EXIT_FAILURE);
    }

    /* Wait up to 5 seconds for the connection attempt to succeed. */
    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        printf("Connection to %s:%d succeeded.\n", HOST_NAME, HOST_PORT);
    } else {
        /* Either the 5 seconds are up or a disconnect event was */
        /* received. Reset the peer in the event the 5 seconds   */
        /* had run out without any significant event.            */
        enet_peer_reset(peer);

        printf("Connection to %s:%d failed.", HOST_NAME, HOST_PORT);
    }
    return peer;
}

void handleEvents(ENetHost* client, Player players[], int* myUUID)
{
    ENetEvent event;
    /* Wait up to 1000 milliseconds for an event. */
    while (enet_host_service(client, &event, 0) > 0) {
        switch (event.type) {

        case ENET_EVENT_TYPE_RECEIVE:
            // //printf("A packet of length %u containing %s was received from %s on
            // channel %u.\n",
            //        event.packet->dataLength,
            //        event.packet->data,
            //        event.peer->data,
            //        event.channelID);
            // //printf("peer:%d\n", event.peer);
            handlePackets(event.packet->data, players, myUUID);
            /* Clean up the packet now that we're done using it. */
            enet_packet_destroy(event.packet);

            break;

        default:
            break;
        }
    }
}

void handlePackets(PacketEvent* packet, Player players[], int* myUUID)
{
    if (*myUUID == packet->uuid && packet->type != E_UUIDRECEIVED) {
        // ignore packets regarding me
        return;
    }
    Player* targetPlayer = findPlayer(players, packet->uuid);

    switch (packet->type) {

    case E_UPDATEINPUTS:
        if (packet->uuid == *myUUID)
            break;
        if (targetPlayer == NULL) {
            Player newPlayer = { 0 };
            newPlayer.uuid = packet->uuid;
            if (!addPlayer(players, newPlayer)) {
                break;
            } else {
                targetPlayer = findPlayer(players, packet->uuid);
            }
        }
        targetPlayer->inputs = packet->data.P_updateInputs;
        break;

    case E_UPDATEPOSITION:
        if (packet->uuid == *myUUID) {
            break;
        }
        if (targetPlayer == NULL) {
            Player newPlayer = { 0 };
            newPlayer.uuid = packet->uuid;
            if (!addPlayer(players, newPlayer)) {
                break;
            } else {
                targetPlayer = findPlayer(players, packet->uuid);
            }
        }
        targetPlayer->body = packet->data.P_updatePosition;
        break;

    case E_PLAYERJOINED:
        printf("NEW PLAYER!\n");
        if (targetPlayer == NULL) {
            if (!addPlayer(players, packet->data.P_PlayerJoined)) {
                break;
            } else {
                targetPlayer = findPlayer(players, packet->uuid);
            }
        }
        *targetPlayer = packet->data.P_PlayerJoined;
        break;

    case E_PLAYERLEFT:
        printf("PLAYER LEFT!\n");
        if (targetPlayer != NULL) {
            Player p = { 0 };
            *targetPlayer = p;
        }
        break;
    case E_UUIDRECEIVED:

        *myUUID = packet->data.P_UuidReceived;
        printf("MY UUID RECEIVED:%d\n", packet->data.P_UuidReceived);
        break;
    }
}

Player* findPlayer(Player players[], int uuid)
{
    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].uuid == uuid) {
            return &(players[i]);
        }
    }
    return NULL;
}

int addPlayer(Player players[], Player playerToAdd)
{
    for (size_t i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].uuid == 0) {
            players[i] = playerToAdd;
            return true;
        }
    }
    return false;
}

void sendInputs(ENetPeer* server, Inputs inputs, int myUUID)
{
    PacketEvent event = (PacketEvent) { E_UPDATEINPUTS, myUUID, { .P_updateInputs = inputs } };
    ENetPacket* packet = enet_packet_create(&event, sizeof(event), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(server, 1, packet);
}

int sendPosition(ENetPeer* server, Body position, int myUUID)
{
    PacketEvent event = (PacketEvent) { E_UPDATEPOSITION, myUUID, { .P_updatePosition = position } };
    ENetPacket* packet = enet_packet_create(&event, sizeof(event), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(server, 1, packet);
}

void printPlayer(Player* player)
{
    if (player->uuid == 0)
        return;
    Vector3 pos = player->body.position;

    printf("Player: uuid=%d, x=%f y=%f z=%f\n", player->uuid, pos.x, pos.y,
        pos.z);
}
void debug() { }