#pragma once

#include "server_manager/NetworkUtils.h"
#include "Object.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <mutex>
#include <memory>
#include <atomic>

// Forward declarations
class Engine;

// Message types for Host-Client communication
enum class HostMessageType : uint8_t {
    CLIENT_CONNECT = 10,
    CLIENT_DISCONNECT = 11,
    INIT_PACKAGE = 12,
    OBJECT_UPDATE = 13,
    OBJECT_CREATE = 14,
    OBJECT_DESTROY = 15,
    CLIENT_INPUT = 16,
    HEARTBEAT = 17,
    ASSIGN_CONTROLLED_OBJECT = 18
};

// Message headers
struct HostMessageHeader {
    HostMessageType type;
    uint8_t reserved[3];
};

// Client connection message
struct ClientConnectMessage {
    HostMessageHeader header;
    char reserved[4];
};

// Initialization package header
struct InitPackageHeader {
    HostMessageHeader header;
    uint32_t backgroundLayerCount;
    uint32_t objectCount;
    uint8_t isCompressed;  // 1 if data is compressed, 0 if not
    uint8_t reserved[3];   // Padding
    // Followed by background layers JSON/compressed data, then objects JSON/compressed data
    // If compressed, each data block is prefixed with uint32_t size
};

// Object update message
struct ObjectUpdateHeader {
    HostMessageHeader header;
    uint32_t objectId;  // Unique ID for the object
    uint8_t hasBody : 1;
    uint8_t hasSprite : 1;
    uint8_t hasSound : 1;
    uint8_t hasViewGrab : 1;
    uint8_t isCompressed : 1;  // 1 if data is compressed, 0 if not
    uint8_t reserved : 3;
    // Followed by component data if flags are set
    // If compressed, all component data is compressed together as a single block
    // Format: uint32_t compressedSize, then compressed data
};

// Object body data (position, rotation, velocity)
struct ObjectBodyData {
    float posX;
    float posY;
    float rotation;  // in radians
    float velX;
    float velY;
    float angularVel;  // in radians per second
};

// Object create message
struct ObjectCreateHeader {
    HostMessageHeader header;
    uint32_t objectId;
    // Followed by JSON object definition
};

// Object destroy message
struct ObjectDestroyMessage {
    HostMessageHeader header;
    uint32_t objectId;
    char reserved[4];
};

// Client input message
struct ClientInputMessage {
    HostMessageHeader header;
    uint32_t objectId;  // Object ID that this input is for
    float moveUp;
    float moveDown;
    float moveLeft;
    float moveRight;
    float actionWalk;
    float actionInteract;
    float actionThrow;
};

// Assign controlled object message
struct AssignControlledObjectMessage {
    HostMessageHeader header;
    uint32_t objectId;
    char reserved[4];
};

// Client information
struct ClientInfo {
    std::string ip;
    uint16_t port;
    std::chrono::steady_clock::time_point lastHeartbeat;
    bool connected;
    uint32_t assignedObjectId;  // Object ID that this client controls
};

// HostManager manages multiplayer hosting
class HostManager {
public:
    HostManager(Engine* engine);
    ~HostManager();

    // Initialize and start hosting
    bool Initialize(uint16_t hostPort = 8889, const std::string& serverManagerIP = "127.0.0.1", uint16_t serverManagerPort = 8888);
    
    // Update (should be called every frame)
    void Update(float deltaTime);
    
    // Shutdown
    void Shutdown();

    // Get room code (empty if not registered yet)
    std::string GetRoomCode() const { return roomCode; }
    
    // Check if hosting
    bool IsHosting() const { return isHosting; }

    // Object synchronization (called by Engine)
    void SendObjectCreate(Object* obj);
    void SendObjectDestroy(Object* obj);

private:
    // Server Manager communication
    bool RegisterWithServerManager();
    void SendHeartbeatToServerManager();
    void UpdateServerManagerWithClients();

    // Client management
    void ProcessIncomingMessages();
    void HandleClientConnect(const std::string& fromIP, uint16_t fromPort);
    void HandleClientDisconnect(const std::string& fromIP, uint16_t fromPort);
    void HandleClientInput(const std::string& fromIP, uint16_t fromPort, const ClientInputMessage& msg);
    void HandleClientHeartbeat(const std::string& fromIP, uint16_t fromPort);
    void CleanupDisconnectedClients();
    
    // Object assignment
    uint32_t AssignObjectToClient(const std::string& clientKey);
    void SendControlledObjectAssignment(const std::string& clientIP, uint16_t clientPort, uint32_t objectId);

    // Object synchronization
    void SendInitializationPackage(const std::string& clientIP, uint16_t clientPort);
    void SendObjectUpdates();
    
    // Object ID management
    uint32_t GetOrAssignObjectId(Object* obj);
    Object* GetObjectById(uint32_t objectId);
    void CleanupObjectIds();

    // Message sending
    void SendToClient(const std::string& clientIP, uint16_t clientPort, const void* data, size_t length);
    void BroadcastToAllClients(const void* data, size_t length);

    // Serialization helpers
    nlohmann::json SerializeBackgroundLayers() const;
    nlohmann::json SerializeObjectForSync(Object* obj);  // Non-const because it may assign object IDs
    nlohmann::json SerializeObjectBody(Object* obj) const;
    nlohmann::json SerializeObjectSprite(Object* obj) const;
    nlohmann::json SerializeObjectSound(Object* obj) const;
    nlohmann::json SerializeObjectViewGrab(Object* obj) const;

    Engine* engine;
    SocketHandle socket;
    uint16_t hostPort;
    std::string serverManagerIP;
    uint16_t serverManagerPort;
    std::string roomCode;
    bool isHosting;

    // Client management
    std::unordered_map<std::string, ClientInfo> clients;  // Key: "IP:PORT"
    std::mutex clientsMutex;

    // Object ID tracking
    std::unordered_map<Object*, uint32_t> objectToId;
    std::unordered_map<uint32_t, Object*> idToObject;
    uint32_t nextObjectId;
    std::mutex objectIdsMutex;

    // Sync timing
    std::chrono::steady_clock::time_point lastSyncTime;
    std::chrono::milliseconds syncInterval;

    // Server Manager heartbeat
    std::chrono::steady_clock::time_point lastServerManagerHeartbeat;
    std::chrono::seconds serverManagerHeartbeatInterval;

    // Track objects that were created/destroyed this frame
    std::unordered_set<Object*> createdObjects;
    std::unordered_set<Object*> destroyedObjects;

    // Bandwidth tracking
    std::atomic<uint64_t> bytesSent;
    std::atomic<uint64_t> bytesReceived;
    std::chrono::steady_clock::time_point lastBandwidthLogTime;
};

