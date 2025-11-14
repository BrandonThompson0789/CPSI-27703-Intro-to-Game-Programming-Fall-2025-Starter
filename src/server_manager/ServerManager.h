#pragma once

#include "NetworkUtils.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <random>

struct RoomInfo {
    std::string roomCode;
    std::string hostIP;
    uint16_t hostPort;
    int connectedPlayers;
    std::vector<std::string> playerIPs;
    std::chrono::steady_clock::time_point lastPing;
    std::chrono::steady_clock::time_point createdAt;
};

enum class MessageType : uint8_t {
    HOST_REGISTER = 1,
    HOST_HEARTBEAT = 2,
    HOST_UPDATE = 3,
    CLIENT_LOOKUP = 4,
    RESPONSE_REGISTER = 5,
    RESPONSE_ROOM_INFO = 6,
    RESPONSE_ERROR = 7
};

struct MessageHeader {
    MessageType type;
    uint8_t reserved[3];
};

struct HostRegisterMessage {
    MessageHeader header;
    uint16_t hostPort;
    char reserved[2];
};

struct HostHeartbeatMessage {
    MessageHeader header;
};

struct HostUpdateMessage {
    MessageHeader header;
    uint16_t playerCount;
    uint16_t reserved;
    // Followed by playerCount IP addresses as null-terminated strings
};

struct ClientLookupMessage {
    MessageHeader header;
    char roomCode[7]; // 6 chars + null terminator
};

struct RegisterResponse {
    MessageHeader header;
    char roomCode[7]; // 6 chars + null terminator
    uint16_t hostPort;
    char reserved[1];
};

struct RoomInfoResponse {
    MessageHeader header;
    uint16_t hostPort;
    uint16_t playerCount;
    char hostIP[16]; // IPv4 max length
    // Followed by playerCount IP addresses as null-terminated strings
};

struct ErrorResponse {
    MessageHeader header;
    char errorMessage[64];
};

class ServerManager {
public:
    ServerManager(uint16_t port = 8888);
    ~ServerManager();

    bool Initialize();
    void Run();
    void Shutdown();

private:
    void ProcessMessage(const std::string& fromIP, uint16_t fromPort, 
                       const void* data, size_t length);
    
    void HandleHostRegister(const std::string& fromIP, uint16_t fromPort,
                           const HostRegisterMessage& msg);
    void HandleHostHeartbeat(const std::string& fromIP, uint16_t fromPort);
    void HandleHostUpdate(const std::string& fromIP, uint16_t fromPort,
                         const void* data, size_t length);
    void HandleClientLookup(const std::string& fromIP, uint16_t fromPort,
                           const ClientLookupMessage& msg);
    
    std::string GenerateRoomCode();
    void CleanupStaleRooms();
    void SendResponse(const std::string& toIP, uint16_t toPort,
                     const void* data, size_t length);

    SocketHandle m_socket;
    uint16_t m_port;
    bool m_running;
    
    std::unordered_map<std::string, RoomInfo> m_rooms; // Key: roomCode
    std::unordered_map<std::string, std::string> m_hostToRoom; // Key: hostIP:port, Value: roomCode
    std::mutex m_roomsMutex;
    
    std::random_device m_randomDevice;
    std::mt19937 m_randomGenerator;
    std::uniform_int_distribution<int> m_codeDistribution;
    
    static constexpr int ROOM_CODE_LENGTH = 6;
    static constexpr int HEARTBEAT_TIMEOUT_SECONDS = 30;
    static constexpr int CLEANUP_INTERVAL_SECONDS = 10;
};

