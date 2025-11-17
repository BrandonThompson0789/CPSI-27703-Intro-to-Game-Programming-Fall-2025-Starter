#pragma once

#include "server_manager/NetworkUtils.h"
#include "HostManager.h"  // Use message types and structs from HostManager
#include "Object.h"
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <memory>
#include <atomic>

// Forward declarations
class Engine;
class BodyComponent;

// Use HostMessageType and message structs from HostManager.h
// They are compatible and avoid duplication - just use the types directly

struct ObjectSmoothingState {
    float startPosX = 0.0f;
    float startPosY = 0.0f;
    float startAngle = 0.0f;
    float targetPosX = 0.0f;
    float targetPosY = 0.0f;
    float targetAngle = 0.0f;
    float startVelX = 0.0f;
    float startVelY = 0.0f;
    float startVelAngle = 0.0f;
    float targetVelX = 0.0f;
    float targetVelY = 0.0f;
    float targetVelAngle = 0.0f;
    float elapsed = 0.0f;
    float duration = 0.0f;
    uint64_t revision = 0;
};

// ClientManager manages multiplayer client connection
class ClientManager {
public:
    ClientManager(Engine* engine);
    ~ClientManager();

    // Initialize and connect to a room
    // Returns true on success, false on failure
    bool Connect(const std::string& roomCode, 
                 const std::string& serverManagerIP = "127.0.0.1", 
                 uint16_t serverManagerPort = 8888);
    
    // Update (should be called every frame)
    void Update(float deltaTime);
    
    // Shutdown and disconnect
    void Disconnect();

    // Check if connected
    bool IsConnected() const { return isConnected; }
    
    // Get room code
    std::string GetRoomCode() const { return roomCode; }
    
    // Get host IP and port
    std::string GetHostIP() const { return hostIP; }
    uint16_t GetHostPort() const { return hostPort; }

    // Set which player ID this client controls (for sending input)
    void SetPlayerId(int playerId) { assignedPlayerId = playerId; }
    int GetPlayerId() const { return assignedPlayerId; }

    void SetSmoothingEnabled(bool enabled);
    bool IsSmoothingEnabled() const { return smoothingEnabled; }

private:
    // Server Manager communication
    bool LookupRoom(const std::string& roomCode, 
                    const std::string& serverManagerIP, 
                    uint16_t serverManagerPort);

    // Host communication
    void ProcessIncomingMessages();
    void HandleInitPackage(const void* data, size_t length);
    void HandleObjectUpdate(const void* data, size_t length);
    void HandleObjectCreate(const void* data, size_t length);
    void HandleObjectDestroy(const ObjectDestroyMessage& msg);
    
    // Object synchronization
    void CreateObjectFromJson(uint32_t objectId, const nlohmann::json& objJson);
    void UpdateObjectFromJson(uint32_t objectId, const nlohmann::json& bodyJson, 
                              const nlohmann::json& spriteJson, 
                              const nlohmann::json& soundJson,
                              const nlohmann::json& viewGrabJson);
    void DestroyObject(uint32_t objectId);
    
    // Input sending
    void SendInput();
    void SendHeartbeat();
    
    // Message sending
    void SendToHost(const void* data, size_t length);

    // Object ID management
    Object* GetObjectById(uint32_t objectId);
    void CleanupObjectIds();
    void ApplySmoothing(float deltaTime);
    bool UpdateSmoothingState(uint32_t objectId, BodyComponent* body, const nlohmann::json& bodyJson);
    void ClearSmoothingState(uint32_t objectId);
    void ClearAllSmoothingStates();

    Engine* engine;
    SocketHandle socket;
    std::string serverManagerIP;
    uint16_t serverManagerPort;
    std::string hostIP;
    uint16_t hostPort;
    std::string roomCode;
    bool isConnected;

    // Object ID tracking
    std::unordered_map<uint32_t, Object*> idToObject;
    std::mutex objectIdsMutex;

    // Input tracking
    int assignedPlayerId;  // Player ID assigned to this client
    std::mutex inputMutex;

    // Smoothing
    bool smoothingEnabled;
    float hostSyncIntervalSeconds;
    std::unordered_map<uint32_t, ObjectSmoothingState> smoothingStates;
    std::mutex smoothingMutex;
    std::atomic<uint64_t> smoothingRevisionCounter;

    // Heartbeat timing
    std::chrono::steady_clock::time_point lastHeartbeat;
    std::chrono::seconds heartbeatInterval;

    // Input send timing
    std::chrono::steady_clock::time_point lastInputSend;
    std::chrono::milliseconds inputSendInterval;

    // Bandwidth tracking
    std::atomic<uint64_t> bytesSent;
    std::atomic<uint64_t> bytesReceived;
    std::chrono::steady_clock::time_point lastBandwidthLogTime;
};

